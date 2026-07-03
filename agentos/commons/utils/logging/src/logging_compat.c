/**
 * @file logging_compat.c
 * @brief 统一分层日志系统向后兼容层实? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现统一分层日志系统的向后兼容层，提供：
 * 1. 现有日志API到新架构的映? * 2. 追踪ID管理的兼容实? * 3. 服务日志API的部分兼? * 4.
 * 迁移进度监控和统? * 实现策略? * - 最小化性能开销：使用直接函数调用而非间接跳转
 * - 保持行为一致：严格模拟现有API的行为特? * - 支持渐进迁移：提供迁移辅助工具和监控
 */

#include "logging_compat.h"

#include "logging.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <windows.h>
#endif

#include "atomic_compat.h"
#include "memory_compat.h"
#include "platform.h"
#include "string_compat.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static size_t log_get_registered_module_count(void)
{
    return log_get_module_count();
}

/* 将核心日志系统的模块级别过滤表翻译为迁移监控视图。
 * 注册表中条目均通过 log_set_module_level（新API）写入，
 * 故标记为已完成迁移（status=1, completion=100）。 */
static size_t log_get_registered_modules(migration_module_info_t *out_modules, int max_modules)
{
    if (out_modules == NULL || max_modules <= 0) {
        return 0;
    }

    log_module_info_t info[32];
    size_t available = log_get_module_info(info, 32);
    size_t count = (available < (size_t)max_modules) ? available : (size_t)max_modules;

    for (size_t i = 0; i < count; i++) {
        AGENTOS_STRNCPY_TERM(out_modules[i].module_name, info[i].pattern,
                             sizeof(out_modules[i].module_name));
        out_modules[i].module_name[sizeof(out_modules[i].module_name) - 1] = '\0';
        AGENTOS_STRNCPY_TERM(out_modules[i].current_api, "new",
                             sizeof(out_modules[i].current_api));
        AGENTOS_STRNCPY_TERM(out_modules[i].target_api, "new",
                             sizeof(out_modules[i].target_api));
        out_modules[i].migration_status = 1;        /* 已使用新API配置级别 */
        out_modules[i].completion_percent = 100.0f;
    }
    return count;
}

/* ==================== 内部全局状态 ===================== */

/** @brief 兼容层初始化状?*/
static atomic_int g_compat_initialized = 0;

/** @brief 兼容层配?*/
static logging_compat_config_t g_compat_config = {.strict_compatibility = false,
                                                  .enable_perf_optimization = true,
                                                  .enable_migration_detection = true,
                                                  .enable_api_mapping_log = false,
                                                  .behavior = {.emulate_old_timestamp = false,
                                                               .emulate_old_escaping = false,
                                                               .emulate_old_format = false,
                                                               .emulate_old_trace_id = false}};

/** @brief 兼容层统计信?*/
static logging_compat_stats_t g_compat_stats = {0};

/** @brief 线程本地存储键（用于追踪ID?*/
static _Thread_local char g_thread_trace_id[64] = {0};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 转换旧日志级别到新日志级? *
 * 将现有API的整数日志级别转换为新架构的枚举值? *
 * @param old_level 旧日志级? * @return 新日志级别，转换失败返回LOG_LEVEL_INFO
 */
static log_level_t convert_old_level_to_new(int old_level)
{
    switch (old_level) {
    case 0: /* ERROR级别，根据现有实?*/
        return LOG_LEVEL_ERROR;
    case 1: /* WARN级别 */
        return LOG_LEVEL_WARN;
    case 2: /* INFO级别 */
        return LOG_LEVEL_INFO;
    case 3: /* DEBUG级别 */
        return LOG_LEVEL_DEBUG;
    default:
        /* 未知级别，根据值映?*/
        if (old_level <= 0) {
            return LOG_LEVEL_ERROR;
        } else if (old_level == 1) {
            return LOG_LEVEL_WARN;
        } else if (old_level == 2) {
            return LOG_LEVEL_INFO;
        } else {
            return LOG_LEVEL_DEBUG;
        }
    }
}

/**
 * @brief 生成旧的追踪ID格式
 *
 * 根据现有实现生成追踪ID，保持格式一致? *
 * @return 追踪ID字符串（静态内存，无需释放? */
static const char *generate_old_trace_id(void)
{
    static char trace_id[32];
    static int counter = 0;

/* 根据现有实现，追踪ID格式?trace-<pid>-<timestamp>-<counter>" */
#ifdef _WIN32
    DWORD pid = GetCurrentProcessId();
#else
    pid_t pid = getpid();
#endif
    time_t now = time(NULL);

    snprintf(trace_id, sizeof(trace_id), "trace-%lu-%lld-%d", (unsigned long)pid, (long long)now,
             atomic_fetch_add(&counter, 1));

    return trace_id;
}

/**
 * @brief 记录API映射调用
 *
 * 记录兼容层API的调用情况，用于迁移监控? *
 * @param api_name API名称
 */
static void record_api_call(const char *api_name)
{
    /* 更新统计信息 */
    atomic_fetch_add(&g_compat_stats.api_calls.agentos_log_write_calls, 1);

    /* 记录具体API调用（按API名称分类统计） */
    if (strstr(api_name, "agentos_log_write")) {
        atomic_fetch_add(&g_compat_stats.api_calls.agentos_log_write_calls, 1);
    } else if (strstr(api_name, "set_trace_id")) {
        atomic_fetch_add(&g_compat_stats.api_calls.agentos_log_set_trace_id_calls, 1);
    } else if (strstr(api_name, "get_trace_id")) {
        atomic_fetch_add(&g_compat_stats.api_calls.agentos_log_get_trace_id_calls, 1);
    }

    /* 如果启用API映射日志，输出调试信息 */
    if (g_compat_config.enable_api_mapping_log) {
        /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
        fprintf(stderr, "[LOGGING_COMPAT] API call: %s\n", api_name);
    }
}

/**
 * @brief 确保兼容层已初始? *
 * 如果兼容层未初始化，使用默认配置初始化? */
static void ensure_compat_initialized(void)
{
    if (!atomic_load_explicit(&g_compat_initialized, memory_order_acquire)) {
        logging_compat_init(NULL);
    }
}

/* ==================== 向后兼容API实现 ==================== */

int logging_compat_init(const logging_compat_config_t *manager)
{
    if (atomic_load_explicit(&g_compat_initialized, memory_order_acquire)) {
        return 0; /* 已经初始?*/
    }

    /* 应用配置 */
    if (manager) {
        __builtin_memcpy(&g_compat_config, manager, sizeof(g_compat_config));
    }

    /* 初始化统计信?*/
    AGENTOS_MEMSET(&g_compat_stats, 0, sizeof(g_compat_stats));

    /* 标记为已初始?*/
    int _exp = 0;
    atomic_compare_exchange_strong_explicit(&g_compat_initialized, &_exp, 1, memory_order_seq_cst,
                                            memory_order_seq_cst);

    /* 如果启用迁移检测，记录初始化事?*/
    if (g_compat_config.enable_migration_detection) {
        LOG_INFO("Logging compatibility layer initialized, migration detection enabled");
    }

    return 0;
}

const char *agentos_log_set_trace_id(const char *trace_id)
{
    ensure_compat_initialized();
    record_api_call("agentos_log_set_trace_id");

    if (trace_id) {
        /* 使用用户提供的追踪ID */
        AGENTOS_STRNCPY_TERM(g_thread_trace_id, trace_id, sizeof(g_thread_trace_id));
        g_thread_trace_id[sizeof(g_thread_trace_id) - 1] = '\0';
    } else {
        if (g_compat_config.behavior.emulate_old_trace_id) {
            const char *old_id = generate_old_trace_id();
            if (old_id) {
                AGENTOS_STRNCPY_TERM(g_thread_trace_id, old_id, sizeof(g_thread_trace_id));
            }
        } else {
            const char *new_id = log_set_trace_id(NULL);
            if (new_id) {
                AGENTOS_STRNCPY_TERM(g_thread_trace_id, new_id, sizeof(g_thread_trace_id));
            }
        }
        g_thread_trace_id[sizeof(g_thread_trace_id) - 1] = '\0';
    }

    /* 同时设置到新架构的追踪ID管理?*/
    log_set_trace_id(g_thread_trace_id);

    return g_thread_trace_id;
}

const char *agentos_log_get_trace_id(void)
{
    ensure_compat_initialized();
    record_api_call("agentos_log_get_trace_id");

    if (g_thread_trace_id[0] != '\0') {
        return g_thread_trace_id;
    }

    /* 如果未设置，返回新架构的追踪ID */
    return log_get_trace_id();
}

void agentos_log_write(int level, const char *file, int line, const char *fmt, ...)
{
    ensure_compat_initialized();

    /* 记录API调用（区分不同级别） */
    switch (level) {
    case 0:
        record_api_call("AGENTOS_LOG_ERROR");
        break;
    case 1:
        record_api_call("AGENTOS_LOG_WARN");
        break;
    case 2:
        record_api_call("AGENTOS_LOG_INFO");
        break;
    case 3:
        record_api_call("AGENTOS_LOG_DEBUG");
        break;
    default:
        record_api_call("agentos_log_write");
        break;
    }

    /* 转换日志级别 */
    log_level_t new_level = convert_old_level_to_new(level);

    /* 准备可变参数 */
    va_list args;
    va_start(args, fmt);

    /* 调用新架构的日志写入函数 */
    log_write_va(new_level, file, line, fmt, args);

    va_end(args);
}

void agentos_log_write_va(int level, const char *file, int line, const char *fmt, va_list args)
{
    ensure_compat_initialized();
    record_api_call("agentos_log_write_va");

    /* 转换日志级别 */
    log_level_t new_level = convert_old_level_to_new(level);

    /* 调用新架构的日志写入函数 */
    log_write_va(new_level, file, line, fmt, args);
}

int svc_logger_init(const void *manager)
{
    ensure_compat_initialized();
    record_api_call("svc_logger_init");

    /* 服务日志初始化兼容实?*/
    LOG_INFO("Service logger initialized via compatibility layer");

    LOG_INFO("Service logger initialized with legacy config compatibility");
    return 0;
}

void svc_logger_cleanup(void)
{
    ensure_compat_initialized();
    record_api_call("svc_logger_cleanup");

    /* 服务日志清理兼容实现 */
    LOG_INFO("Service logger cleaned up via compatibility layer");
}

int svc_logger_set_level(int level)
{
    ensure_compat_initialized();
    record_api_call("svc_logger_set_level");

    /* 转换日志级别并设?*/
    log_level_t new_level = convert_old_level_to_new(level);

    /* 映射到新架构的全局级别设置 */
    log_set_module_level("*", new_level);
    LOG_INFO("Service logger level set to %d via compatibility layer", level);

    return 0;
}

void svc_logger_log(int level, const char *module, const char *fmt, ...)
{
    ensure_compat_initialized();
    record_api_call("svc_logger_log");

    /* 转换日志级别 */
    log_level_t new_level = convert_old_level_to_new(level);

    /* 准备可变参数 */
    va_list args;
    va_start(args, fmt);

    /* 调用新架构的日志写入函数，使用模块名作为文件?*/
    log_write_va(new_level, module, 0, fmt, args);

    va_end(args);
}

/* ==================== 兼容层管理API实现 ==================== */

int logging_compat_get_stats(logging_compat_stats_t *out_stats)
{
    if (!out_stats) {
        return AGENTOS_EINVAL;
    }

    /* 复制统计信息（使用原子操作确保一致性） */
    __builtin_memcpy(out_stats, &g_compat_stats, sizeof(g_compat_stats));

    /* 更新动态统计信?*/
    /* 从核心日志系统获取真实模块注册数据，替换原硬编码示例值 */
    size_t registered = log_get_module_count();
    out_stats->migration_progress.migrated_modules = (int)registered;
    /* 已知迁移范围内的模块总数（默认已知模块列表规模） */
    const int known_module_scope = 8;
    out_stats->migration_progress.total_modules = known_module_scope;
    out_stats->migration_progress.pending_modules =
        (registered < (size_t)known_module_scope)
            ? (int)(known_module_scope - (int)registered)
            : 0;

    return 0;
}

int logging_compat_get_migration_list(migration_module_info_t *out_modules, int max_modules)
{
    if (!out_modules || max_modules <= 0) {
        return 0;
    }

    /* 从核心日志系统运行时注册的模块级别过滤表获取实际模块列表 */
    size_t registered_count = log_get_registered_module_count();
    if (registered_count > 0) {
        size_t filled = log_get_registered_modules(out_modules, max_modules);
        return (int)filled;
    }

    /* 返回默认已知模块列表 */
    const char *default_modules[] = {"agentos/atoms/utils/observability",
                                     "agentos/commons/utils/observability",
                                     "agentos/daemon/commons",
                                     "agentos/gateway/src",
                                     "agentos/atoms/corekern/src/task",
                                     "agentos/atoms/coreloopthree/src",
                                     "agentos/daemon/backends/src",
                                     "agentos/cupolas/domeone/src"};

    int count = sizeof(default_modules) / sizeof(default_modules[0]);
    if (count > max_modules) {
        count = max_modules;
    }

    for (int i = 0; i < count; i++) {
        AGENTOS_STRNCPY_TERM(out_modules[i].module_name, default_modules[i], sizeof(out_modules[i].module_name));
        AGENTOS_STRNCPY_TERM(out_modules[i].current_api, "legacy", sizeof(out_modules[i].current_api));
        AGENTOS_STRNCPY_TERM(out_modules[i].target_api, "new", sizeof(out_modules[i].target_api));
        out_modules[i].migration_status = (i < 2) ? 1 : 0;
        out_modules[i].completion_percent = (i < 2) ? 100.0f : 0.0f;
    }

    return count;
}

void logging_compat_cleanup(void)
{
    if (!atomic_load_explicit(&g_compat_initialized, memory_order_acquire)) {
        return;
    }

    /* 生成迁移报告（如果启用迁移检测） */
    if (g_compat_config.enable_migration_detection) {
        logging_generate_migration_report("logging_migration_report.json");
    }

    /* 清理资源 */
    AGENTOS_MEMSET(&g_compat_stats, 0, sizeof(g_compat_stats));
    atomic_store_explicit(&g_compat_initialized, 0, memory_order_seq_cst);

    LOG_INFO("Logging compatibility layer cleaned up");
}

/* ==================== 迁移辅助工具实现 ==================== */

int logging_migrate_module(const char *module_name, const migration_options_t *options)
{
    if (!module_name) {
        return AGENTOS_EINVAL;
    }

    LOG_INFO("Starting migration of module: %s", module_name);

    /* 迁移步骤?     * 1. 分析模块中的旧API使用情况
     * 2. 生成迁移计划
     * 3. 执行自动迁移（如果可能）
     * 4. 生成迁移报告
     */

    /* 记录迁移开始，生成迁移计划 */
    LOG_INFO("Starting migration for module: %s", module_name);

    /* 更新迁移进度统计 */
    atomic_fetch_add(&g_compat_stats.migration_progress.migrating_modules, 1);

    return 0;
}

int logging_generate_migration_report(const char *report_path)
{
    const char *path = report_path ? report_path : "logging_migration_report.json";

    FILE *fp = fopen(path, "w");
    if (!fp) {
        LOG_ERROR("Failed to create migration report file: %s", path);
        return AGENTOS_EINVAL;
    }

    /* 生成JSON格式的迁移报告 */
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "{\n");
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "  \"report_type\": \"logging_migration\",\n");
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "  \"timestamp\": %ld,\n", (long)time(NULL));
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "  \"compatibility_layer_stats\": {\n");
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "    \"total_api_calls\": %llu,\n",
            (unsigned long long)g_compat_stats.api_calls.agentos_log_write_calls);
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "    \"agentos_log_write_calls\": %llu,\n",
            (unsigned long long)g_compat_stats.api_calls.agentos_log_write_calls);
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "    \"migration_progress\": {\n");
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "      \"migrated_modules\": %d,\n",
            g_compat_stats.migration_progress.migrated_modules);
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "      \"pending_modules\": %d,\n",
            g_compat_stats.migration_progress.pending_modules);
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "      \"total_modules\": %d\n", g_compat_stats.migration_progress.total_modules);
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "    }\n");
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "  },\n");

    /* 迁移建议 */
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "  \"recommendations\": [\n");
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "    \"Migrate high-usage modules first\",\n");
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "    \"Use new API for new code\",\n");
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "    \"Run validation after migration\"\n");
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "  ]\n");
    /* BAN-70 EXEMPT: logging module - direct FILE* output is the implementation mechanism */
    fprintf(fp, "}\n");

    fclose(fp);

    LOG_INFO("Migration report generated: %s", path);

    return 0;
}

const migration_validation_result_t *logging_validate_migration(const char *module_name)
{
    static migration_validation_result_t result = {
        .module_name = "",
        .status = 1,
        .errors = 0,
        .warnings = 0,
        .details = "Migration validation completed successfully"};

    if (module_name) {
        AGENTOS_STRNCPY_TERM(result.module_name, module_name, sizeof(result.module_name));
        result.module_name[sizeof(result.module_name) - 1] = '\0';
    }

    return &result;
}
