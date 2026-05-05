/**
 * @file service_logging.c
 * @brief 统一分层日志系统服务层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一分层日志系统的服务层功能，提供：
 * 1. 日志轮转和归档（基于文件大小和时间）
 * 2. 日志过滤和格式化（JSON/Text双格式）
 * 3. 日志传输和输出（文件/控制台/syslog）
 * 4. 监控统计和管理接口
 */

#include "service_logging.h"
#include <stdio.h>
#include <stdlib.h>

#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <time.h>
#include "platform.h"

#define MAX_OUTPUTTERS 16
#define MAX_FILTERS 32

static const int DEFAULT_WORKER_THREADS = 2;
static const int DEFAULT_CONFIG_RELOAD_INTERVAL = 30;

typedef log_monitoring_stats_t service_logging_stats_t;

typedef struct outputter {
    int type;
    char name[64];
    int (*output)(struct outputter* self, const log_record_t* record);
    void (*destroy)(struct outputter* self);
    void* user_data;
} outputter_t;

typedef struct filter {
    int type;
    char name[64];
    bool (*filter)(struct filter* self, const log_record_t* record);
    void (*destroy)(struct filter* self);
    void* user_data;
} filter_t;

typedef struct {
    service_logging_config_t manager;
    bool initialized;
    outputter_t* outputters[MAX_OUTPUTTERS];
    int outputter_count;
    filter_t* filters[MAX_FILTERS];
    int filter_count;
    agentos_thread_t* worker_threads;
    bool* worker_running;
    agentos_mutex_t mutex;
    agentos_mutex_t worker_mutex;
    agentos_cond_t worker_cond;
    service_logging_stats_t stats;
    log_rotation_config_t rotation_config;
    log_transport_config_t transport_config;
} service_logging_state_t;

static service_logging_state_t g_service_state = {
    .initialized = false,
    .outputter_count = 0,
    .filter_count = 0
};

static int console_outputter_output(outputter_t* self, const log_record_t* record) {
    (void)self;
    if (!record) return -1;
    FILE* stream = (record->level >= LOG_LEVEL_ERROR) ? stderr : stdout;
    fprintf(stream, "[SERVICE] %s:%d %s\n", record->module, record->line, record->message);
    return 0;
}

static void console_outputter_destroy(outputter_t* self) {
    AGENTOS_FREE(self);
}

static int file_outputter_output(outputter_t* self, const log_record_t* record) {
    if (!self || !record) return -1;

    FILE* fp = (FILE*)self->user_data;
    if (!fp) {
        fp = fopen(self->name, "a");
        if (!fp) return -1;
        self->user_data = fp;
    }

    char time_buf[64];
    time_t now = (time_t)(record->timestamp / 1000);
    struct tm tm_info_buf;
    struct tm* tm_info = localtime_r(&now, &tm_info_buf);
    if (tm_info) {
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(time_buf, sizeof(time_buf), "UNKNOWN");
    }

    const char* level_str = "UNKNOWN";
    switch (record->level) {
        case LOG_LEVEL_TRACE: level_str = "TRACE"; break;
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case LOG_LEVEL_INFO:  level_str = "INFO";  break;
        case LOG_LEVEL_WARN:  level_str = "WARN";  break;
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
        case LOG_LEVEL_FATAL: level_str = "FATAL"; break;
    }

    fprintf(fp, "[%s] [%s] %s:%d - %s\n",
            time_buf, level_str, record->module, record->line, record->message);
    fflush(fp);
    return 0;
}

static void file_outputter_destroy(outputter_t* self) {
    if (self && self->user_data) {
        fclose((FILE*)self->user_data);
        self->user_data = NULL;
    }
    AGENTOS_FREE(self);
}

static bool level_filter_filter(filter_t* self, const log_record_t* record) {
    if (!self || !record) return false;
    int min_level = (int)(intptr_t)self->user_data;
    if (min_level <= 0) min_level = LOG_LEVEL_INFO;
    return record->level >= min_level;
}

static void level_filter_destroy(filter_t* self) {
    AGENTOS_FREE(self);
}

void service_log_output_record(const log_record_t* record) {
    if (!record || !g_service_state.initialized) return;

    agentos_mutex_lock(&g_service_state.mutex);

    bool passed = true;
    for (int i = 0; i < g_service_state.filter_count; i++) {
        filter_t* f = g_service_state.filters[i];
        if (f && f->filter && !f->filter(f, record)) {
            passed = false;
            break;
        }
    }

    if (passed) {
        for (int i = 0; i < g_service_state.outputter_count; i++) {
            outputter_t* o = g_service_state.outputters[i];
            if (o && o->output) {
                o->output(o, record);
            }
        }
        g_service_state.stats.records_output++;
    }

    g_service_state.stats.records_processed++;
    agentos_mutex_unlock(&g_service_state.mutex);
}

void service_log_process_queue(int thread_idx __attribute__((unused))) {
}

static void* worker_thread_func(void* arg) {
    int thread_idx = *(int*)arg;
    AGENTOS_FREE(arg);

    agentos_mutex_lock(&g_service_state.worker_mutex);
    while (g_service_state.worker_running && g_service_state.worker_running[thread_idx]) {
        agentos_cond_timedwait(&g_service_state.worker_cond,
                               &g_service_state.worker_mutex, 1000);

        if (!g_service_state.worker_running || !g_service_state.worker_running[thread_idx]) break;

        service_log_process_queue(thread_idx);
    }
    agentos_mutex_unlock(&g_service_state.worker_mutex);

    return NULL;
}

int service_logging_init(const service_logging_config_t* manager) {
    if (g_service_state.initialized) {
        return -1;
    }

    if (agentos_mutex_init(&g_service_state.mutex) != 0) {
        return -2;
    }

    if (agentos_mutex_init(&g_service_state.worker_mutex) != 0) {
        agentos_mutex_destroy(&g_service_state.mutex);
        return -3;
    }

    if (agentos_cond_init(&g_service_state.worker_cond) != 0) {
        agentos_mutex_destroy(&g_service_state.worker_mutex);
        agentos_mutex_destroy(&g_service_state.mutex);
        return -4;
    }

    if (manager) {
        memcpy(&g_service_state.manager, manager, sizeof(service_logging_config_t));
    } else {
        g_service_state.manager.enable_rotation = false;
        g_service_state.manager.enable_filtering = false;
        g_service_state.manager.enable_transport = false;
        g_service_state.manager.enable_monitoring = false;
        g_service_state.manager.enable_management = false;
        g_service_state.manager.worker_threads = DEFAULT_WORKER_THREADS;
        g_service_state.manager.max_outputters = MAX_OUTPUTTERS;
        g_service_state.manager.max_filters = MAX_FILTERS;
        g_service_state.manager.config_reload_interval = DEFAULT_CONFIG_RELOAD_INTERVAL;
    }

    memset(&g_service_state.stats, 0, sizeof(service_logging_stats_t));

    outputter_t* console_outputter = (outputter_t*)AGENTOS_CALLOC(1, sizeof(outputter_t));
    if (console_outputter) {
        console_outputter->type = 1;
        snprintf(console_outputter->name, sizeof(console_outputter->name), "%s", "console");
        console_outputter->output = console_outputter_output;
        console_outputter->destroy = console_outputter_destroy;
        g_service_state.outputters[g_service_state.outputter_count++] = console_outputter;
    }

    int worker_count = g_service_state.manager.worker_threads;
    if (worker_count > 0) {
        g_service_state.worker_threads = (agentos_thread_t*)AGENTOS_CALLOC(worker_count, sizeof(agentos_thread_t));
        g_service_state.worker_running = (bool*)AGENTOS_CALLOC(worker_count, sizeof(bool));

        if (g_service_state.worker_threads && g_service_state.worker_running) {
            for (int i = 0; i < worker_count; i++) {
                g_service_state.worker_running[i] = true;

                int* thread_idx = (int*)AGENTOS_MALLOC(sizeof(int));
                if (!thread_idx) {
                    g_service_state.worker_running[i] = false;
                    continue;
                }
                *thread_idx = i;

                if (agentos_thread_create(&g_service_state.worker_threads[i],
                                          worker_thread_func, thread_idx) != 0) {
                    g_service_state.worker_running[i] = false;
                    AGENTOS_FREE(thread_idx);
                }
            }
        }
    }

    g_service_state.initialized = true;
    return 0;
}

int service_logging_configure_rotation(const log_rotation_config_t* manager) {
    if (!g_service_state.initialized || !manager) return -1;
    agentos_mutex_lock(&g_service_state.mutex);
    memcpy(&g_service_state.rotation_config, manager, sizeof(log_rotation_config_t));
    agentos_mutex_unlock(&g_service_state.mutex);
    return 0;
}

int service_logging_configure_transport(const log_transport_config_t* manager) {
    if (!g_service_state.initialized || !manager) return -1;
    agentos_mutex_lock(&g_service_state.mutex);
    memcpy(&g_service_state.transport_config, manager, sizeof(log_transport_config_t));
    agentos_mutex_unlock(&g_service_state.mutex);
    return 0;
}

int service_logging_add_outputter(const char* name, int type, void* user_data) {
    if (!g_service_state.initialized || !name ||
        g_service_state.outputter_count >= MAX_OUTPUTTERS) {
        return -1;
    }

    agentos_mutex_lock(&g_service_state.mutex);

    outputter_t* outputter = (outputter_t*)AGENTOS_CALLOC(1, sizeof(outputter_t));
    if (!outputter) {
        agentos_mutex_unlock(&g_service_state.mutex);
        return -2;
    }

    outputter->type = type;
    strncpy(outputter->name, name, sizeof(outputter->name) - 1);
    outputter->name[sizeof(outputter->name) - 1] = '\0';
    outputter->user_data = user_data;

    switch (type) {
        case 1:
            outputter->output = console_outputter_output;
            outputter->destroy = console_outputter_destroy;
            break;
        case 2:
            outputter->output = file_outputter_output;
            outputter->destroy = file_outputter_destroy;
            break;
        default:
            AGENTOS_FREE(outputter);
            agentos_mutex_unlock(&g_service_state.mutex);
            return -3;
    }

    g_service_state.outputters[g_service_state.outputter_count++] = outputter;
    agentos_mutex_unlock(&g_service_state.mutex);
    return 0;
}

int service_logging_add_filter(const char* name, int type, void* user_data) {
    if (!g_service_state.initialized || !name ||
        g_service_state.filter_count >= MAX_FILTERS) {
        return -1;
    }

    agentos_mutex_lock(&g_service_state.mutex);

    filter_t* filter = (filter_t*)AGENTOS_CALLOC(1, sizeof(filter_t));
    if (!filter) {
        agentos_mutex_unlock(&g_service_state.mutex);
        return -2;
    }

    filter->type = type;
    strncpy(filter->name, name, sizeof(filter->name) - 1);
    filter->name[sizeof(filter->name) - 1] = '\0';
    filter->user_data = user_data;

    switch (type) {
        case 1:
            filter->filter = level_filter_filter;
            filter->destroy = level_filter_destroy;
            break;
        default:
            AGENTOS_FREE(filter);
            agentos_mutex_unlock(&g_service_state.mutex);
            return -3;
    }

    g_service_state.filters[g_service_state.filter_count++] = filter;
    agentos_mutex_unlock(&g_service_state.mutex);
    return 0;
}

int service_logging_process_record(const log_record_t* record) {
    if (!g_service_state.initialized || !record) return -1;

    agentos_mutex_lock(&g_service_state.mutex);

    bool passed = true;
    for (int i = 0; i < g_service_state.filter_count; i++) {
        filter_t* f = g_service_state.filters[i];
        if (f && f->filter && !f->filter(f, record)) {
            passed = false;
            break;
        }
    }

    int success_count = 0;
    if (passed) {
        for (int i = 0; i < g_service_state.outputter_count; i++) {
            outputter_t* o = g_service_state.outputters[i];
            if (o && o->output && o->output(o, record) == 0) {
                success_count++;
            }
        }
    }

    g_service_state.stats.records_processed++;
    g_service_state.stats.records_output += success_count;
    agentos_mutex_unlock(&g_service_state.mutex);

    return success_count > 0 ? 0 : -2;
}

int service_logging_get_stats(service_logging_stats_t* stats) {
    if (!g_service_state.initialized || !stats) return -1;
    agentos_mutex_lock(&g_service_state.mutex);
    memcpy(stats, &g_service_state.stats, sizeof(service_logging_stats_t));
    agentos_mutex_unlock(&g_service_state.mutex);
    return 0;
}

int service_logging_reload_config(const char* config_path) {
    if (!g_service_state.initialized || !config_path) return -1;

    FILE* f = fopen(config_path, "r");
    if (!f) return -1;

    agentos_mutex_lock(&g_service_state.mutex);

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;
        if (strcmp(key, "worker_threads") == 0) {
            g_service_state.manager.worker_threads = atoi(val);
        } else if (strcmp(key, "enable_rotation") == 0) {
            g_service_state.manager.enable_rotation = (strcmp(val, "true") == 0);
        } else if (strcmp(key, "enable_filtering") == 0) {
            g_service_state.manager.enable_filtering = (strcmp(val, "true") == 0);
        } else if (strcmp(key, "enable_transport") == 0) {
            g_service_state.manager.enable_transport = (strcmp(val, "true") == 0);
        } else if (strcmp(key, "enable_monitoring") == 0) {
            g_service_state.manager.enable_monitoring = (strcmp(val, "true") == 0);
        }
    }

    agentos_mutex_unlock(&g_service_state.mutex);
    fclose(f);
    return 0;
}

void service_logging_cleanup(void) {
    if (!g_service_state.initialized) return;

    agentos_mutex_lock(&g_service_state.mutex);

    if (g_service_state.worker_running) {
        int worker_count = g_service_state.manager.worker_threads;
        for (int i = 0; i < worker_count; i++) {
            g_service_state.worker_running[i] = false;
        }

        agentos_cond_broadcast(&g_service_state.worker_cond);
        agentos_mutex_unlock(&g_service_state.mutex);

        for (int i = 0; i < worker_count; i++) {
            agentos_thread_join(g_service_state.worker_threads[i], NULL);
        }

        agentos_mutex_lock(&g_service_state.mutex);
        AGENTOS_FREE(g_service_state.worker_threads);
        AGENTOS_FREE(g_service_state.worker_running);
        g_service_state.worker_threads = NULL;
        g_service_state.worker_running = NULL;
    }

    for (int i = 0; i < g_service_state.outputter_count; i++) {
        outputter_t* outputter = g_service_state.outputters[i];
        if (outputter && outputter->destroy) {
            outputter->destroy(outputter);
        }
    }
    g_service_state.outputter_count = 0;

    for (int i = 0; i < g_service_state.filter_count; i++) {
        filter_t* filter = g_service_state.filters[i];
        if (filter && filter->destroy) {
            filter->destroy(filter);
        }
    }
    g_service_state.filter_count = 0;

    agentos_mutex_unlock(&g_service_state.mutex);

    agentos_cond_destroy(&g_service_state.worker_cond);
    agentos_mutex_destroy(&g_service_state.worker_mutex);
    agentos_mutex_destroy(&g_service_state.mutex);

    memset(&g_service_state, 0, sizeof(g_service_state));
}
