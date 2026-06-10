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

#include "memory_compat.h"
#include "platform.h"
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../error/include/error.h"



#define MAX_OUTPUTTERS 16
#define MAX_FILTERS 32

static const int DEFAULT_CONFIG_RELOAD_INTERVAL = 30;

typedef log_monitoring_stats_t service_logging_stats_t;

typedef struct outputter {
    int type;
    char name[64];
    int (*output)(struct outputter *self, const log_record_t *record);
    void (*destroy)(struct outputter *self);
    void *user_data;
} outputter_t;

typedef struct filter {
    int type;
    char name[64];
    bool (*filter)(struct filter *self, const log_record_t *record);
    void (*destroy)(struct filter *self);
    void *user_data;
} filter_t;

typedef struct {
    service_logging_config_t manager;
    bool initialized;
    outputter_t *outputters[MAX_OUTPUTTERS];
    int outputter_count;
    filter_t *filters[MAX_FILTERS];
    int filter_count;
    agentos_mutex_t mutex;
    service_logging_stats_t stats;
    log_rotation_config_t rotation_config;
    log_transport_config_t transport_config;
} service_logging_state_t;

static service_logging_state_t g_service_state = {
    .initialized = false, .outputter_count = 0, .filter_count = 0};

static int console_outputter_output(outputter_t *self, const log_record_t *record)
{
    (void)self;
    if (!record)
        return AGENTOS_EINVAL;
    FILE *stream = (record->level >= LOG_LEVEL_ERROR) ? stderr : stdout;
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(stream, "[SERVICE] %s:%d %s\n", record->module, record->line, record->message);
    return 0;
}

static void console_outputter_destroy(outputter_t *self)
{
    AGENTOS_FREE(self);
}

static int file_outputter_output(outputter_t *self, const log_record_t *record)
{
    if (!self || !record)
        return AGENTOS_EINVAL;

    FILE *fp = (FILE *)self->user_data;
    if (!fp) {
        fp = fopen(self->name, "a");
        if (!fp)
            return AGENTOS_EINVAL;
        self->user_data = fp;
    }

    char time_buf[64];
    time_t now = (time_t)(record->timestamp / 1000);
    struct tm tm_info_buf;
    struct tm *tm_info = localtime_r(&now, &tm_info_buf);
    if (tm_info) {
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(time_buf, sizeof(time_buf), "UNKNOWN");
    }

    const char *level_str = "UNKNOWN";
    switch (record->level) {
    case LOG_LEVEL_DEBUG:
        level_str = "DEBUG";
        break;
    case LOG_LEVEL_INFO:
        level_str = "INFO";
        break;
    case LOG_LEVEL_WARN:
        level_str = "WARN";
        break;
    case LOG_LEVEL_ERROR:
        level_str = "ERROR";
        break;
    case LOG_LEVEL_FATAL:
        level_str = "FATAL";
        break;
    case LOG_LEVEL_COUNT:
        level_str = "UNKNOWN";
        break;
    }

    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "[%s] [%s] %s:%d - %s\n", time_buf, level_str, record->module, record->line,
            record->message);
    fflush(fp);
    return 0;
}

static void file_outputter_destroy(outputter_t *self)
{
    if (self && self->user_data) {
        fclose((FILE *)self->user_data);
        self->user_data = NULL;
    }
    AGENTOS_FREE(self);
}

static bool level_filter_filter(filter_t *self, const log_record_t *record)
{
    if (!self || !record)
        return false;
    int min_level = (int)(intptr_t)self->user_data;
    if (min_level <= 0)
        min_level = LOG_LEVEL_INFO;
    return (int)record->level >= min_level;
}

static void level_filter_destroy(filter_t *self)
{
    AGENTOS_FREE(self);
}

void service_log_output_record(const log_record_t *record)
{
    if (!record || !g_service_state.initialized)
        return;

    agentos_mutex_lock(&g_service_state.mutex);

    bool passed = true;
    for (int i = 0; i < g_service_state.filter_count; i++) {
        filter_t *f = g_service_state.filters[i];
        if (f && f->filter && !f->filter(f, record)) {
            passed = false;
            break;
        }
    }

    if (passed) {
        for (int i = 0; i < g_service_state.outputter_count; i++) {
            outputter_t *o = g_service_state.outputters[i];
            if (o && o->output) {
                o->output(o, record);
            }
        }
        g_service_state.stats.throughput.total_records++;
    }

    g_service_state.stats.throughput.total_records++;
    agentos_mutex_unlock(&g_service_state.mutex);
}

int service_logging_init(const service_logging_config_t *manager)
{
    if (g_service_state.initialized) {
        return AGENTOS_EINVAL;
    }

    if (agentos_mutex_init(&g_service_state.mutex) != 0) {
        return AGENTOS_ERR_SYS_NOT_INIT;
    }

    if (manager) {
        __builtin_memcpy(&g_service_state.manager, manager, sizeof(service_logging_config_t));
    } else {
        g_service_state.manager.enable_rotation = false;
        g_service_state.manager.enable_filtering = false;
        g_service_state.manager.enable_transport = false;
        g_service_state.manager.enable_monitoring = false;
        g_service_state.manager.enable_management = false;
        g_service_state.manager.worker_threads = 0;
        g_service_state.manager.max_outputters = MAX_OUTPUTTERS;
        g_service_state.manager.max_filters = MAX_FILTERS;
        g_service_state.manager.config_reload_interval = DEFAULT_CONFIG_RELOAD_INTERVAL;
    }

    AGENTOS_MEMSET(&g_service_state.stats, 0, sizeof(service_logging_stats_t));

    outputter_t *console_outputter = (outputter_t *)AGENTOS_CALLOC(1, sizeof(outputter_t));
    if (console_outputter) {
        console_outputter->type = 1;
        snprintf(console_outputter->name, sizeof(console_outputter->name), "%s", "console");
        console_outputter->output = console_outputter_output;
        console_outputter->destroy = console_outputter_destroy;
        g_service_state.outputters[g_service_state.outputter_count++] = console_outputter;
    }

    g_service_state.initialized = true;
    return 0;
}

int service_logging_configure_rotation(const log_rotation_config_t *manager)
{
    if (!g_service_state.initialized || !manager)
        return AGENTOS_EINVAL;
    agentos_mutex_lock(&g_service_state.mutex);
    __builtin_memcpy(&g_service_state.rotation_config, manager, sizeof(log_rotation_config_t));
    agentos_mutex_unlock(&g_service_state.mutex);
    return 0;
}

int service_logging_configure_transport(const log_transport_config_t *manager)
{
    if (!g_service_state.initialized || !manager)
        return AGENTOS_EINVAL;
    agentos_mutex_lock(&g_service_state.mutex);
    __builtin_memcpy(&g_service_state.transport_config, manager, sizeof(log_transport_config_t));
    agentos_mutex_unlock(&g_service_state.mutex);
    return 0;
}

int service_logging_add_outputter(const char *name, int type, void *user_data)
{
    if (!g_service_state.initialized || !name ||
        g_service_state.outputter_count >= MAX_OUTPUTTERS) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(&g_service_state.mutex);

    outputter_t *outputter = (outputter_t *)AGENTOS_CALLOC(1, sizeof(outputter_t));
    if (!outputter) {
        agentos_mutex_unlock(&g_service_state.mutex);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    outputter->type = type;
    AGENTOS_STRNCPY_TERM(outputter->name, name, sizeof(outputter->name));
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
        return AGENTOS_ERR_NULL_POINTER;
    }

    g_service_state.outputters[g_service_state.outputter_count++] = outputter;
    agentos_mutex_unlock(&g_service_state.mutex);
    return 0;
}

int service_logging_add_filter(const char *name, int type, void *user_data)
{
    if (!g_service_state.initialized || !name || g_service_state.filter_count >= MAX_FILTERS) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(&g_service_state.mutex);

    filter_t *filter = (filter_t *)AGENTOS_CALLOC(1, sizeof(filter_t));
    if (!filter) {
        agentos_mutex_unlock(&g_service_state.mutex);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    filter->type = type;
    AGENTOS_STRNCPY_TERM(filter->name, name, sizeof(filter->name));
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
        return AGENTOS_ERR_NULL_POINTER;
    }

    g_service_state.filters[g_service_state.filter_count++] = filter;
    agentos_mutex_unlock(&g_service_state.mutex);
    return 0;
}

int service_logging_process_record(const log_record_t *record)
{
    if (!g_service_state.initialized || !record)
        return AGENTOS_EINVAL;

    agentos_mutex_lock(&g_service_state.mutex);

    bool passed = true;
    for (int i = 0; i < g_service_state.filter_count; i++) {
        filter_t *f = g_service_state.filters[i];
        if (f && f->filter && !f->filter(f, record)) {
            passed = false;
            break;
        }
    }

    int success_count = 0;
    if (passed) {
        for (int i = 0; i < g_service_state.outputter_count; i++) {
            outputter_t *o = g_service_state.outputters[i];
            if (o && o->output && o->output(o, record) == 0) {
                success_count++;
            }
        }
    }

    g_service_state.stats.throughput.total_records++;
    agentos_mutex_unlock(&g_service_state.mutex);

    return success_count > 0 ? 0 : -2;
}

int service_logging_get_stats(service_logging_stats_t *stats)
{
    if (!g_service_state.initialized || !stats)
        return AGENTOS_EINVAL;
    agentos_mutex_lock(&g_service_state.mutex);
    __builtin_memcpy(stats, &g_service_state.stats, sizeof(service_logging_stats_t));
    agentos_mutex_unlock(&g_service_state.mutex);
    return 0;
}

int service_logging_reload_config(const char *config_path)
{
    if (!g_service_state.initialized || !config_path)
        return AGENTOS_EINVAL;

    FILE *f = fopen(config_path, "r");
    if (!f)
        return AGENTOS_EINVAL;

    agentos_mutex_lock(&g_service_state.mutex);

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';
        if (line[0] == '#' || line[0] == '\0')
            continue;
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;
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

void service_logging_cleanup(void)
{
    if (!g_service_state.initialized)
        return;

    agentos_mutex_lock(&g_service_state.mutex);

    for (int i = 0; i < g_service_state.outputter_count; i++) {
        outputter_t *outputter = g_service_state.outputters[i];
        if (outputter && outputter->destroy) {
            outputter->destroy(outputter);
        }
    }
    g_service_state.outputter_count = 0;

    for (int i = 0; i < g_service_state.filter_count; i++) {
        filter_t *filter = g_service_state.filters[i];
        if (filter && filter->destroy) {
            filter->destroy(filter);
        }
    }
    g_service_state.filter_count = 0;

    agentos_mutex_unlock(&g_service_state.mutex);

    agentos_mutex_destroy(&g_service_state.mutex);

    AGENTOS_MEMSET(&g_service_state, 0, sizeof(g_service_state));
}
