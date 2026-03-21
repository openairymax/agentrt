/**
 * @file audit_logger.c
 * @brief 审计日志器主实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "audit.h"
#include "audit_queue.h"
#include "audit_rotator.h"
#include "logger.h"
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>

struct audit_logger {
    audit_queue_t*      queue;
    audit_rotator_t*    rotator;
    pthread_t           writer_thread;
    volatile int        running;
    audit_config_t      config;
};

/* 默认配置 */
static const audit_config_t DEFAULT_CONFIG = {
    .log_path = "/var/log/agentos/audit.log",
    .max_size_bytes = 100 * 1024 * 1024,  // 100MB
    .max_files = 5,
    .format = "json",
    .queue_size = 1024
};

/* 格式化审计事件为 JSON 字符串 */
static char* format_event_json(const audit_event_t* event) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddNumberToObject(root, "timestamp", (double)event->timestamp);
    if (event->agent_id) cJSON_AddStringToObject(root, "agent_id", event->agent_id);
    if (event->tool_name) cJSON_AddStringToObject(root, "tool", event->tool_name);
    if (event->input) cJSON_AddStringToObject(root, "input", event->input);
    if (event->output) cJSON_AddStringToObject(root, "output", event->output);
    cJSON_AddNumberToObject(root, "duration_ms", (double)event->duration_ms);
    cJSON_AddBoolToObject(root, "success", event->success);
    if (event->error_msg) cJSON_AddStringToObject(root, "error", event->error_msg);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* 写入线程主循环 */
static void* writer_thread_func(void* arg) {
    audit_logger_t* logger = (audit_logger_t*)arg;
    char* event_str = NULL;
    int ret;

    while (logger->running) {
        ret = audit_queue_pop(logger->queue, &event_str, 1000);
        if (ret == 0 && event_str) {
            size_t len = strlen(event_str);
            ssize_t written = audit_rotator_write(logger->rotator, event_str, len);
            if (written < 0) {
                AGENTOS_LOG_ERROR("audit: failed to write to log, error %d", errno);
            } else {
                audit_rotator_write(logger->rotator, "\n", 1);
            }
            free(event_str);
            event_str = NULL;
        }
    }
    return NULL;
}

/* 创建审计日志器 */
audit_logger_t* audit_logger_create(const audit_config_t* user_config) {
    audit_logger_t* logger = (audit_logger_t*)calloc(1, sizeof(audit_logger_t));
    if (!logger) {
        AGENTOS_LOG_ERROR("audit: failed to allocate logger");
        return NULL;
    }

    if (user_config) {
        logger->config = *user_config;
    } else {
        logger->config = DEFAULT_CONFIG;
    }

    if (logger->config.log_path == NULL) {
        AGENTOS_LOG_ERROR("audit: log_path cannot be NULL");
        free(logger);
        return NULL;
    }
    if (logger->config.queue_size == 0) logger->config.queue_size = DEFAULT_CONFIG.queue_size;
    if (logger->config.max_files == 0) logger->config.max_files = 1;

    logger->queue = audit_queue_create(logger->config.queue_size);
    if (!logger->queue) {
        AGENTOS_LOG_ERROR("audit: failed to create queue");
        free(logger);
        return NULL;
    }

    logger->rotator = audit_rotator_create(logger->config.log_path,
                                           logger->config.max_size_bytes,
                                           logger->config.max_files);
    if (!logger->rotator) {
        AGENTOS_LOG_ERROR("audit: failed to create rotator for %s", logger->config.log_path);
        audit_queue_destroy(logger->queue);
        free(logger);
        return NULL;
    }

    logger->running = 1;
    if (pthread_create(&logger->writer_thread, NULL, writer_thread_func, logger) != 0) {
        AGENTOS_LOG_ERROR("audit: failed to create writer thread");
        audit_rotator_destroy(logger->rotator);
        audit_queue_destroy(logger->queue);
        free(logger);
        return NULL;
    }

    AGENTOS_LOG_INFO("audit: logger initialized, path=%s", logger->config.log_path);
    return logger;
}

/* 销毁审计日志器 */
void audit_logger_destroy(audit_logger_t* logger) {
    if (!logger) return;

    logger->running = 0;
    audit_queue_shutdown(logger->queue);
    pthread_join(logger->writer_thread, NULL);

    audit_rotator_destroy(logger->rotator);
    audit_queue_destroy(logger->queue);

    free(logger);
    AGENTOS_LOG_INFO("audit: logger destroyed");
}

/* 记录一条审计事件 */
int audit_logger_record(audit_logger_t* logger, const audit_event_t* event) {
    if (!logger || !logger->running) return -1;
    if (!event) {
        AGENTOS_LOG_ERROR("audit: event is NULL");
        return -1;
    }

    char* json = format_event_json(event);
    if (!json) {
        AGENTOS_LOG_ERROR("audit: failed to format event as JSON");
        return -1;
    }

    int ret = audit_queue_push(logger->queue, json, 0);
    if (ret != 0) {
        AGENTOS_LOG_WARN("audit: queue is full, dropping event");
        free(json);
        return -1;
    }

    free(json);  // 队列已复制，可以释放
    return 0;
}

/* 查询审计日志（同步，支持多文件和JSON解析） */
int audit_logger_query(audit_logger_t* logger,
                       const char* agent_id,
                       uint64_t start_time,
                       uint64_t end_time,
                       uint32_t limit,
                       char*** out_events,
                       size_t* out_count) {
    if (!logger || !out_events || !out_count) return -1;

    char** results = NULL;
    size_t capacity = 0;
    size_t count = 0;

    // 定义辅助函数：从指定文件读取并填充结果数组
    // 使用宏或内联函数，但为了清晰，在循环内直接实现

    // 先查询当前文件
    FILE* f = fopen(logger->config.log_path, "r");
    if (f) {
        char line[8192];
        while (fgets(line, sizeof(line), f) != NULL && count < limit) {
            // 去除末尾换行符
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

            // 解析JSON
            cJSON* root = cJSON_Parse(line);
            if (!root) continue;

            // 获取时间戳
            cJSON* ts_item = cJSON_GetObjectItem(root, "timestamp");
            if (!cJSON_IsNumber(ts_item)) {
                cJSON_Delete(root);
                continue;
            }
            uint64_t ts = (uint64_t)cJSON_GetNumberValue(ts_item);
            if ((start_time > 0 && ts < start_time) || (end_time > 0 && ts > end_time)) {
                cJSON_Delete(root);
                continue;
            }

            // 获取agent_id（如果指定）
            if (agent_id) {
                cJSON* agent_item = cJSON_GetObjectItem(root, "agent_id");
                if (!cJSON_IsString(agent_item) || strcmp(agent_item->valuestring, agent_id) != 0) {
                    cJSON_Delete(root);
                    continue;
                }
            }

            cJSON_Delete(root);

            // 添加到结果数组
            if (count >= capacity) {
                capacity = capacity == 0 ? 16 : capacity * 2;
                char** new_results = realloc(results, capacity * sizeof(char*));
                if (!new_results) {
                    AGENTOS_LOG_ERROR("audit: out of memory during query");
                    for (size_t i = 0; i < count; i++) free(results[i]);
                    free(results);
                    fclose(f);
                    return -1;
                }
                results = new_results;
            }
            results[count] = strdup(line);
            if (!results[count]) {
                AGENTOS_LOG_ERROR("audit: out of memory during query");
                for (size_t i = 0; i < count; i++) free(results[i]);
                free(results);
                fclose(f);
                return -1;
            }
            count++;
        }
        fclose(f);
    }

    // 如果还没达到 limit，继续查询归档文件
    if (count < limit) {
        for (uint32_t i = 1; i < logger->config.max_files; i++) {
            // 构造归档文件名
            char archive_path[1024];
            snprintf(archive_path, sizeof(archive_path), "%s.%u", logger->config.log_path, i);

            struct stat st;
            if (stat(archive_path, &st) != 0) {
                continue; // 文件不存在，跳过
            }

            FILE* af = fopen(archive_path, "r");
            if (!af) continue;

            char line[8192];
            while (fgets(line, sizeof(line), af) != NULL && count < limit) {
                size_t len = strlen(line);
                if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

                cJSON* root = cJSON_Parse(line);
                if (!root) continue;

                cJSON* ts_item = cJSON_GetObjectItem(root, "timestamp");
                if (!cJSON_IsNumber(ts_item)) {
                    cJSON_Delete(root);
                    continue;
                }
                uint64_t ts = (uint64_t)cJSON_GetNumberValue(ts_item);
                if ((start_time > 0 && ts < start_time) || (end_time > 0 && ts > end_time)) {
                    cJSON_Delete(root);
                    continue;
                }

                if (agent_id) {
                    cJSON* agent_item = cJSON_GetObjectItem(root, "agent_id");
                    if (!cJSON_IsString(agent_item) || strcmp(agent_item->valuestring, agent_id) != 0) {
                        cJSON_Delete(root);
                        continue;
                    }
                }

                cJSON_Delete(root);

                if (count >= capacity) {
                    capacity = capacity == 0 ? 16 : capacity * 2;
                    char** new_results = realloc(results, capacity * sizeof(char*));
                    if (!new_results) {
                        AGENTOS_LOG_ERROR("audit: out of memory during query");
                        for (size_t j = 0; j < count; j++) free(results[j]);
                        free(results);
                        fclose(af);
                        return -1;
                    }
                    results = new_results;
                }
                results[count] = strdup(line);
                if (!results[count]) {
                    AGENTOS_LOG_ERROR("audit: out of memory during query");
                    for (size_t j = 0; j < count; j++) free(results[j]);
                    free(results);
                    fclose(af);
                    return -1;
                }
                count++;
            }
            fclose(af);
            if (count >= limit) break;
        }
    }

    *out_events = results;
    *out_count = count;
    return 0;
}