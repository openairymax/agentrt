/**
 * @file observability.c
 * @brief AgentOS 微内核可观测性子系统实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */
#include "observability.h"
#include "mem.h"
#include "task.h"
#include "time.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#define MAX_METRICS 1024
#define MAX_HEALTH_CHECKS 64
#define MAX_TRACE_SPANS 256

/* 参数检查宏定义 */
#define AGENTOS_CHECK_NULL(ptr, name) \
    do { \
        if (!(ptr)) { \
            return AGENTOS_EINVAL; \
        } \
    } while(0)

#define AGENTOS_CHECK_ALLOC(ptr) \
    do { \
        if (!(ptr)) { \
            return AGENTOS_ENOMEM; \
        } \
    } while(0)

/* 错误推送函数（简化实现） */
static void agentos_error_push_ex(int err, const char* file, int line, const char* func, const char* msg) {
    (void)err;
    (void)file;
    (void)line;
    (void)func;
    (void)msg;
}

typedef struct metric_entry {
    char name[128];
    char labels[256];
    agentos_metric_type_t type;
    double value;
    uint64_t last_update_ns;
    uint64_t create_time_ns;
    size_t update_count;
    struct metric_entry* next;
} metric_entry_t;

typedef struct health_check_entry {
    char name[64];
    agentos_health_check_cb callback;
    void* user_data;
    uint64_t last_check_ns;
    agentos_health_status_t last_status;
    int check_count;
    struct health_check_entry* next;
} health_check_entry_t;

typedef struct histogram_bucket {
    double upper_bound;
    uint64_t count;
    struct histogram_bucket* next;
} histogram_bucket_t;

typedef struct histogram_entry {
    char name[128];
    char labels[256];
    histogram_bucket_t* buckets;
    uint64_t count;
    double sum;
    struct histogram_entry* next;
} histogram_entry_t;

typedef struct {
    int initialized;
    agentos_mutex_t* lock;
    agentos_observability_config_t manager;
    metric_entry_t* metrics_head;
    size_t metrics_count;
    health_check_entry_t* health_checks_head;
    size_t health_checks_count;
    histogram_entry_t* histograms_head;
    size_t histograms_count;
    agentos_thread_t metrics_thread;  /* 线程句柄（Windows: HANDLE, POSIX: pthread_t） */
    agentos_thread_t health_check_thread;  /* 线程句柄 */
    int shutdown_requested;
    uint64_t start_time_ns;
    uint64_t total_metrics_collected;
    uint64_t total_health_checks_run;
} observability_state_t;

static observability_state_t* g_state = NULL;
static agentos_mutex_t* g_init_lock = NULL;

static void generate_trace_id(char* buffer, size_t size) {
    static const char hex_chars[] = "0123456789abcdef";
    uint64_t rand_values[2];
    uint64_t timestamp_ns = agentos_time_monotonic_ns();
    rand_values[0] = timestamp_ns * 0x9E3779B97F4A7C15ULL;
    rand_values[1] = (timestamp_ns >> 32) * 0xBF58476D1CE4E5B9ULL;
    for (size_t i = 0; i < 16 && i * 2 + 1 < size; i++) {
        uint8_t byte = ((uint8_t*)rand_values)[i % 16];
        buffer[i * 2] = hex_chars[(byte >> 4) & 0xF];
        buffer[i * 2 + 1] = hex_chars[byte & 0xF];
    }
    if (size > 0) buffer[(size - 1 < 31) ? size - 1 : 31] = '\0';
}

static void generate_span_id(char* buffer, size_t size) {
    static const char hex_chars[] = "0123456789abcdef";
    uint64_t timestamp_ns = agentos_time_monotonic_ns();
    uint64_t rand_val = timestamp_ns * 0xBF58476D1CE4E5B9ULL;
    for (size_t i = 0; i < 8 && i * 2 + 1 < size; i++) {
        uint8_t byte = ((uint8_t*)&rand_val)[i % 8];
        buffer[i * 2] = hex_chars[(byte >> 4) & 0xF];
        buffer[i * 2 + 1] = hex_chars[byte & 0xF];
    }
    if (size > 0) buffer[(size - 1 < 15) ? size - 1 : 15] = '\0';
}

static metric_entry_t* find_metric_locked(const char* name, const char* labels) {
    metric_entry_t* current = g_state->metrics_head;
    while (current) {
        if (strcmp(current->name, name) == 0 && strcmp(current->labels, labels) == 0) return current;
        current = current->next;
    }
    return NULL;
}

static metric_entry_t* create_metric_locked(const char* name, const char* labels,
                                           agentos_metric_type_t type, double initial_value) {
    if (g_state->metrics_count >= MAX_METRICS) return NULL;
    metric_entry_t* entry = (metric_entry_t*)agentos_mem_alloc(sizeof(metric_entry_t));
    if (!entry) return NULL;
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    strncpy(entry->labels, labels, sizeof(entry->labels) - 1);
    entry->labels[sizeof(entry->labels) - 1] = '\0';
    entry->type = type;
    entry->value = initial_value;
    entry->last_update_ns = agentos_time_monotonic_ns();
    entry->create_time_ns = entry->last_update_ns;
    entry->update_count = 0;
    entry->next = g_state->metrics_head;
    g_state->metrics_head = entry;
    g_state->metrics_count++;
    return entry;
}

static histogram_entry_t* find_histogram_locked(const char* name, const char* labels) {
    histogram_entry_t* current = g_state->histograms_head;
    while (current) {
        if (strcmp(current->name, name) == 0 && strcmp(current->labels, labels) == 0) return current;
        current = current->next;
    }
    return NULL;
}

static histogram_entry_t* create_histogram_locked(const char* name, const char* labels) {
    if (g_state->histograms_count >= MAX_METRICS) return NULL;
    histogram_entry_t* entry = (histogram_entry_t*)agentos_mem_alloc(sizeof(histogram_entry_t));
    if (!entry) return NULL;
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    strncpy(entry->labels, labels, sizeof(entry->labels) - 1);
    entry->labels[sizeof(entry->labels) - 1] = '\0';
    entry->buckets = NULL;
    entry->count = 0;
    entry->sum = 0.0;
    entry->next = g_state->histograms_head;
    g_state->histograms_head = entry;
    g_state->histograms_count++;
    double default_bounds[] = {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};
    for (int i = 0; i < 11; i++) {
        histogram_bucket_t* bucket = (histogram_bucket_t*)agentos_mem_alloc(sizeof(histogram_bucket_t));
        if (bucket) {
            bucket->upper_bound = default_bounds[i];
            bucket->count = 0;
            bucket->next = entry->buckets;
            entry->buckets = bucket;
        }
    }
    return entry;
}

static void metrics_collection_thread(void* arg) {
    (void)arg;
    while (!g_state->shutdown_requested) {
        agentos_mutex_lock(g_state->lock);
        g_state->total_metrics_collected++;
        agentos_mutex_unlock(g_state->lock);
        agentos_task_sleep(g_state->manager.metrics_interval_ms);
    }
}

static void health_check_thread(void* arg) {
    (void)arg;
    while (!g_state->shutdown_requested) {
        agentos_mutex_lock(g_state->lock);
        health_check_entry_t* current = g_state->health_checks_head;
        while (current && !g_state->shutdown_requested) {
            agentos_health_status_t status = current->callback(current->user_data);
            current->last_status = status;
            current->last_check_ns = agentos_time_monotonic_ns();
            current->check_count++;
            current = current->next;
        }
        g_state->total_health_checks_run++;
        agentos_mutex_unlock(g_state->lock);
        agentos_task_sleep(g_state->manager.health_check_interval_ms);
    }
}

static double get_cpu_usage(void) {
    double usage = 0.0;
#ifdef _WIN32
    static ULARGE_INTEGER last_cpu_time = {0}, last_sys_time = {0}, last_user_time = {0};
    FILETIME idle_time, kernel_time, user_time;
    ULARGE_INTEGER sys_time, user_time_uli, cpu_time;
    if (GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        sys_time.LowPart = kernel_time.dwLowDateTime;
        sys_time.HighPart = kernel_time.dwHighDateTime;
        user_time_uli.LowPart = user_time.dwLowDateTime;
        user_time_uli.HighPart = user_time.dwHighDateTime;
        cpu_time.QuadPart = sys_time.QuadPart + user_time_uli.QuadPart;
        if (last_cpu_time.QuadPart != 0) {
            ULONGLONG cpu_diff = cpu_time.QuadPart - last_cpu_time.QuadPart;
            ULONGLONG sys_diff = sys_time.QuadPart - last_sys_time.QuadPart;
            ULONGLONG user_diff = user_time_uli.QuadPart - last_user_time.QuadPart;
            if (cpu_diff > 0) usage = 100.0 * (1.0 - ((double)(sys_diff + user_diff) / (double)cpu_diff));
        }
        last_cpu_time = cpu_time;
        last_sys_time = sys_time;
        last_user_time = user_time_uli;
    }
#else
    static struct rusage last_usage = {0};
    struct rusage current_usage;
    if (getrusage(RUSAGE_SELF, &current_usage) == 0) {
        long total_time = (current_usage.ru_utime.tv_sec + current_usage.ru_stime.tv_sec) * 1000000L +
                         (current_usage.ru_utime.tv_usec + current_usage.ru_stime.tv_usec);
        long last_total_time = (last_usage.ru_utime.tv_sec + last_usage.ru_stime.tv_sec) * 1000000L +
                              (last_usage.ru_utime.tv_usec + last_usage.ru_stime.tv_usec);
        if (last_total_time != 0) {
            usage = (double)(total_time - last_total_time) / 10000.0;
            if (usage > 100.0) usage = 100.0;
            if (usage < 0.0) usage = 0.0;
        }
        last_usage = current_usage;
    }
#endif
    return usage;
}

static double get_memory_usage(void) {
    double usage = 0.0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        MEMORYSTATUSEX mem_status;
        mem_status.dwLength = sizeof(mem_status);
        if (GlobalMemoryStatusEx(&mem_status)) {
            if (mem_status.ullTotalPhys > 0) usage = 100.0 * (double)pmc.WorkingSetSize / (double)mem_status.ullTotalPhys;
        }
    }
#else
    long page_size = sysconf(_SC_PAGESIZE);
    long total_pages = sysconf(_SC_PHYS_PAGES);
    if (page_size > 0 && total_pages > 0) {
        FILE* statm = fopen("/proc/self/statm", "r");
        if (statm) {
            unsigned long size, resident, share, text, lib, data, dt;
            if (fscanf(statm, "%lu %lu %lu %lu %lu %lu %lu", &size, &resident, &share, &text, &lib, &data, &dt) == 7) {
                unsigned long resident_bytes = resident * page_size;
                unsigned long total_bytes = total_pages * page_size;
                usage = 100.0 * (double)resident_bytes / (double)total_bytes;
            }
            fclose(statm);
        }
    }
#endif
    return usage;
}

static int get_thread_count(void) {
    int count = 0;
#ifdef _WIN32
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        DWORD pid = GetCurrentProcessId();
        if (Thread32First(hSnapshot, &te)) {
            do {
                if (te.th32OwnerProcessID == pid) count++;
            } while (Thread32Next(hSnapshot, &te));
        }
        CloseHandle(hSnapshot);
    }
#else
    FILE* stat = fopen("/proc/self/status", "r");
    if (stat) {
        char line[256];
        while (fgets(line, sizeof(line), stat)) {
            if (strncmp(line, "Threads:", 8) == 0) {
                count = atoi(line + 8);
                break;
            }
        }
        fclose(stat);
    }
#endif
    return count;
}

int agentos_observability_init(const agentos_observability_config_t* manager) {
    int ret = AGENTOS_SUCCESS;
    observability_state_t* state = NULL;

    /* 前置检查（无需清理） */
    AGENTOS_CHECK_NULL(manager, "manager");

    if (!g_init_lock) {
        g_init_lock = agentos_mutex_create();
        AGENTOS_CHECK_ALLOC(g_init_lock);
    }

    agentos_mutex_lock(g_init_lock);

    /* 已经初始化 */
    if (g_state) {
        agentos_mutex_unlock(g_init_lock);
        return AGENTOS_SUCCESS;
    }

    /* 分配状态结构体 */
    state = (observability_state_t*)agentos_mem_alloc(sizeof(observability_state_t));
    if (!state) {
        ret = AGENTOS_ENOMEM;
        agentos_error_push_ex(ret, __FILE__, __LINE__, __func__,
                            "Failed to allocate observability state");
        goto error;
    }
    memset(state, 0, sizeof(observability_state_t));
    memcpy(&state->manager, manager, sizeof(agentos_observability_config_t));

    /* 创建互斥锁 */
    state->lock = agentos_mutex_create();
    if (!state->lock) {
        ret = AGENTOS_ENOMEM;
        agentos_error_push_ex(ret, __FILE__, __LINE__, __func__,
                            "Failed to create observability mutex");
        goto error;
    }

    state->start_time_ns = agentos_time_monotonic_ns();

    /* 创建指标收集线程（如启用） */
    if (manager->enable_metrics && manager->metrics_interval_ms > 0) {
        agentos_error_t err = agentos_thread_create(&state->metrics_thread, NULL, metrics_collection_thread, NULL);
        if (err != AGENTOS_SUCCESS) {
            ret = err;  // 使用实际的错误码而非硬编码 ENOMEM
            goto error;
        }
    }

    /* 创建健康检查线程（如启用） */
    if (manager->enable_health_check && manager->health_check_interval_ms > 0) {
        agentos_error_t err = agentos_thread_create(&state->health_check_thread, NULL, health_check_thread, NULL);
        if (err != AGENTOS_SUCCESS) {
            ret = err;  // 使用实际的错误码而非硬编码 ENOMEM
            goto error;
        }
    }

    /* 成功完成初始化 */
    state->initialized = 1;
    g_state = state;
    agentos_mutex_unlock(g_init_lock);
    return AGENTOS_SUCCESS;

error:
    /* 清理已分配的资源 */
    if (state) {
        if (state->metrics_thread) {
            agentos_thread_join(state->metrics_thread, NULL);
        }
        if (state->health_check_thread) {
            agentos_thread_join(state->health_check_thread, NULL);
        }
        if (state->lock) {
            agentos_mutex_destroy(state->lock);
        }
        agentos_mem_free(state);
    }
    agentos_mutex_unlock(g_init_lock);
    return ret;
}

void agentos_observability_shutdown(void) {
    if (!g_state) return;
    agentos_mutex_lock(g_state->lock);
    g_state->shutdown_requested = 1;
    agentos_mutex_unlock(g_state->lock);
#ifdef _WIN32
    if (g_state->metrics_thread != NULL) {
        agentos_thread_join(g_state->metrics_thread, NULL);
        g_state->metrics_thread = NULL;
    }
    if (g_state->health_check_thread != NULL) {
        agentos_thread_join(g_state->health_check_thread, NULL);
        g_state->health_check_thread = NULL;
    }
#else
    if (g_state->metrics_thread) {
        agentos_thread_join(g_state->metrics_thread, NULL);
    }
    if (g_state->health_check_thread) {
        agentos_thread_join(g_state->health_check_thread, NULL);
    }
#endif
    agentos_mutex_lock(g_state->lock);
    metric_entry_t* m = g_state->metrics_head;
    while (m) {
        metric_entry_t* next = m->next;
        agentos_mem_free(m);
        m = next;
    }
    health_check_entry_t* h = g_state->health_checks_head;
    while (h) {
        health_check_entry_t* next = h->next;
        agentos_mem_free(h);
        h = next;
    }
    histogram_entry_t* hg = g_state->histograms_head;
    while (hg) {
        histogram_bucket_t* b = hg->buckets;
        while (b) {
            histogram_bucket_t* bn = b->next;
            agentos_mem_free(b);
            b = bn;
        }
        histogram_entry_t* next = hg->next;
        agentos_mem_free(hg);
        hg = next;
    }
    agentos_mutex_unlock(g_state->lock);
    agentos_mutex_destroy(g_state->lock);
    agentos_mem_free(g_state);
    g_state = NULL;
}

int agentos_health_check_register(const char* name, agentos_health_check_cb callback, void* user_data) {
    if (!g_state || !name || !callback) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    if (g_state->health_checks_count >= MAX_HEALTH_CHECKS) {
        agentos_mutex_unlock(g_state->lock);
        return AGENTOS_ERESOURCE;
    }
    health_check_entry_t* entry = (health_check_entry_t*)agentos_mem_alloc(sizeof(health_check_entry_t));
    if (!entry) {
        agentos_mutex_unlock(g_state->lock);
        return AGENTOS_ENOMEM;
    }
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->callback = callback;
    entry->user_data = user_data;
    entry->last_check_ns = 0;
    entry->last_status = AGENTOS_HEALTH_PASS;
    entry->check_count = 0;
    entry->next = g_state->health_checks_head;
    g_state->health_checks_head = entry;
    g_state->health_checks_count++;
    agentos_mutex_unlock(g_state->lock);
    return AGENTOS_SUCCESS;
}

agentos_health_status_t agentos_health_check_run(int timeout_ms) {
    (void)timeout_ms;
    if (!g_state || !g_state->initialized) return AGENTOS_HEALTH_FAIL;
    double cpu = get_cpu_usage();
    double mem = get_memory_usage();
    if (cpu > 90.0 || mem > 90.0) return AGENTOS_HEALTH_FAIL;
    if (cpu > 70.0 || mem > 70.0) return AGENTOS_HEALTH_WARN;
    return AGENTOS_HEALTH_PASS;
}

int agentos_metric_record(const agentos_metric_sample_t* sample) {
    if (!g_state || !sample) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    metric_entry_t* entry = find_metric_locked(sample->name, sample->labels);
    if (!entry) entry = create_metric_locked(sample->name, sample->labels, sample->type, sample->value);
    if (entry) {
        entry->value = sample->value;
        entry->last_update_ns = sample->timestamp_ns;
        entry->update_count++;
    }
    agentos_mutex_unlock(g_state->lock);
    return entry ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

int agentos_metric_counter_create(const char* name, const char* labels) {
    if (!g_state || !name) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    if (find_metric_locked(name, labels ? labels : "")) {
        agentos_mutex_unlock(g_state->lock);
        return AGENTOS_SUCCESS;
    }
    metric_entry_t* entry = create_metric_locked(name, labels ? labels : "", AGENTOS_METRIC_COUNTER, 0.0);
    agentos_mutex_unlock(g_state->lock);
    return entry ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

int agentos_metric_counter_inc(const char* name, const char* labels, double value) {
    if (!g_state || !name) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    metric_entry_t* entry = find_metric_locked(name, labels ? labels : "");
    if (!entry) entry = create_metric_locked(name, labels ? labels : "", AGENTOS_METRIC_COUNTER, 0.0);
    if (entry) {
        entry->value += value;
        entry->last_update_ns = agentos_time_monotonic_ns();
        entry->update_count++;
    }
    agentos_mutex_unlock(g_state->lock);
    return entry ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

int agentos_metric_gauge_create(const char* name, const char* labels, double initial_value) {
    if (!g_state || !name) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    if (find_metric_locked(name, labels ? labels : "")) {
        agentos_mutex_unlock(g_state->lock);
        return AGENTOS_SUCCESS;
    }
    metric_entry_t* entry = create_metric_locked(name, labels ? labels : "", AGENTOS_METRIC_GAUGE, initial_value);
    agentos_mutex_unlock(g_state->lock);
    return entry ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

int agentos_metric_gauge_set(const char* name, const char* labels, double value) {
    if (!g_state || !name) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    metric_entry_t* entry = find_metric_locked(name, labels ? labels : "");
    if (!entry) entry = create_metric_locked(name, labels ? labels : "", AGENTOS_METRIC_GAUGE, value);
    if (entry) {
        entry->value = value;
        entry->last_update_ns = agentos_time_monotonic_ns();
        entry->update_count++;
    }
    agentos_mutex_unlock(g_state->lock);
    return entry ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

int agentos_metric_histogram_observe(const char* name, const char* labels, double value) {
    if (!g_state || !name) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    histogram_entry_t* entry = find_histogram_locked(name, labels ? labels : "");
    if (!entry) entry = create_histogram_locked(name, labels ? labels : "");
    if (entry) {
        entry->count++;
        entry->sum += value;
        histogram_bucket_t* bucket = entry->buckets;
        while (bucket) {
            if (value <= bucket->upper_bound) bucket->count++;
            bucket = bucket->next;
        }
    }
    agentos_mutex_unlock(g_state->lock);
    return entry ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

int agentos_trace_span_start(agentos_trace_context_t* context, const char* service_name, const char* operation_name) {
    if (!context || !service_name || !operation_name) return AGENTOS_EINVAL;
    memset(context, 0, sizeof(agentos_trace_context_t));
    generate_trace_id(context->trace_id, sizeof(context->trace_id));
    generate_span_id(context->span_id, sizeof(context->span_id));
    strncpy(context->service_name, service_name, sizeof(context->service_name) - 1);
    strncpy(context->operation_name, operation_name, sizeof(context->operation_name) - 1);
    context->start_ns = agentos_time_monotonic_ns();
    return AGENTOS_SUCCESS;
}

int agentos_trace_span_end(agentos_trace_context_t* context, int error_code) {
    if (!context) return AGENTOS_EINVAL;
    context->end_ns = agentos_time_monotonic_ns();
    context->error_code = error_code;
    return AGENTOS_SUCCESS;
}

int agentos_trace_set_tag(agentos_trace_context_t* context, const char* key, const char* value) {
    (void)context; (void)key; (void)value;
    return AGENTOS_SUCCESS;
}

int agentos_trace_log(agentos_trace_context_t* context, const char* message) {
    (void)context; (void)message;
    return AGENTOS_SUCCESS;
}

int agentos_performance_get_metrics(double* out_cpu_usage, double* out_memory_usage, int* out_thread_count) {
    if (out_cpu_usage) *out_cpu_usage = get_cpu_usage();
    if (out_memory_usage) *out_memory_usage = get_memory_usage();
    if (out_thread_count) *out_thread_count = get_thread_count();
    return AGENTOS_SUCCESS;
}

int agentos_metrics_export_prometheus(char* buffer, size_t buffer_size) {
    if (!g_state || !buffer || buffer_size == 0) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset, "# HELP agentos_metrics_total Total metrics\n");
    offset += snprintf(buffer + offset, buffer_size - offset, "# TYPE agentos_metrics_total gauge\n");
    offset += snprintf(buffer + offset, buffer_size - offset, "agentos_metrics_total %zu\n", g_state->metrics_count);
    offset += snprintf(buffer + offset, buffer_size - offset, "# HELP agentos_uptime_seconds System uptime\n");
    offset += snprintf(buffer + offset, buffer_size - offset, "# TYPE agentos_uptime_seconds gauge\n");
    uint64_t uptime_ns = agentos_time_monotonic_ns() - g_state->start_time_ns;
    offset += snprintf(buffer + offset, buffer_size - offset, "agentos_uptime_seconds %llu\n", (unsigned long long)(uptime_ns / 1000000000ULL));
    metric_entry_t* current = g_state->metrics_head;
    while (current && offset < buffer_size - 1) {
        const char* type_str = "untyped";
        switch (current->type) {
            case AGENTOS_METRIC_COUNTER: type_str = "counter"; break;
            case AGENTOS_METRIC_GAUGE: type_str = "gauge"; break;
            case AGENTOS_METRIC_HISTOGRAM: type_str = "histogram"; break;
            case AGENTOS_METRIC_SUMMARY: type_str = "summary"; break;
        }
        offset += snprintf(buffer + offset, buffer_size - offset, "# TYPE %s %s\n", current->name, type_str);
        offset += snprintf(buffer + offset, buffer_size - offset, "%s{%s} %.6f\n", current->name, current->labels, current->value);
        current = current->next;
    }
    histogram_entry_t* hg = g_state->histograms_head;
    while (hg && offset < buffer_size - 1) {
        offset += snprintf(buffer + offset, buffer_size - offset, "# TYPE %s histogram\n", hg->name);
        histogram_bucket_t* bucket = hg->buckets;
        while (bucket && offset < buffer_size - 1) {
            offset += snprintf(buffer + offset, buffer_size - offset, "%s_bucket{le=\"%.3f\"} %llu\n",
                             hg->name, bucket->upper_bound, (unsigned long long)bucket->count);
            bucket = bucket->next;
        }
        offset += snprintf(buffer + offset, buffer_size - offset, "%s_sum %.6f\n", hg->name, hg->sum);
        offset += snprintf(buffer + offset, buffer_size - offset, "%s_count %llu\n", hg->name, (unsigned long long)hg->count);
        hg = hg->next;
    }
    agentos_mutex_unlock(g_state->lock);
    return (int)offset;
}

int agentos_health_export_status(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return AGENTOS_EINVAL;
    agentos_health_status_t status = agentos_health_check_run(0);
    const char* status_str = "unknown";
    switch (status) {
        case AGENTOS_HEALTH_PASS: status_str = "pass"; break;
        case AGENTOS_HEALTH_WARN: status_str = "warn"; break;
        case AGENTOS_HEALTH_FAIL: status_str = "fail"; break;
    }
    double cpu = 0.0, mem = 0.0;
    int threads = 0;
    agentos_performance_get_metrics(&cpu, &mem, &threads);
    return snprintf(buffer, buffer_size,
        "{\"status\":\"%s\",\"cpu_usage\":%.2f,\"memory_usage\":%.2f,\"thread_count\":%d,\"timestamp_ns\":%llu}",
        status_str, cpu, mem, threads, (unsigned long long)agentos_time_monotonic_ns());
}

size_t agentos_observability_get_metric_count(void) {
    if (!g_state) return 0;
    agentos_mutex_lock(g_state->lock);
    size_t count = g_state->metrics_count;
    agentos_mutex_unlock(g_state->lock);
    return count;
}

size_t agentos_observability_get_health_check_count(void) {
    if (!g_state) return 0;
    agentos_mutex_lock(g_state->lock);
    size_t count = g_state->health_checks_count;
    agentos_mutex_unlock(g_state->lock);
    return count;
}

int agentos_observability_reset_metrics(void) {
    if (!g_state) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    metric_entry_t* current = g_state->metrics_head;
    while (current) {
        current->value = 0.0;
        current->update_count = 0;
        current->last_update_ns = agentos_time_monotonic_ns();
        current = current->next;
    }
    agentos_mutex_unlock(g_state->lock);
    return AGENTOS_SUCCESS;
}

uint64_t agentos_observability_get_uptime_ns(void) {
    return g_state ? agentos_time_monotonic_ns() - g_state->start_time_ns : 0;
}
int agentos_metric_histogram_create(const char* name, const char* labels) {
    if (!g_state || !name) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    if (find_histogram_locked(name, labels ? labels : "")) { agentos_mutex_unlock(g_state->lock); return AGENTOS_SUCCESS; }
    histogram_entry_t* entry = create_histogram_locked(name, labels ? labels : "");
    agentos_mutex_unlock(g_state->lock);
    return entry ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}
int agentos_metric_summary_create(const char* name, const char* labels) {
    if (!g_state || !name) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    if (find_metric_locked(name, labels ? labels : "")) { agentos_mutex_unlock(g_state->lock); return AGENTOS_SUCCESS; }
    metric_entry_t* entry = create_metric_locked(name, labels ? labels : "", AGENTOS_METRIC_SUMMARY, 0.0);
    agentos_mutex_unlock(g_state->lock);
    return entry ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}
int agentos_metric_summary_observe(const char* name, const char* labels, double value) {
    if (!g_state || !name) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    metric_entry_t* entry = find_metric_locked(name, labels ? labels : "");
    if (!entry) entry = create_metric_locked(name, labels ? labels : "", AGENTOS_METRIC_SUMMARY, value);
    if (entry) { entry->value = value; entry->last_update_ns = agentos_time_monotonic_ns(); entry->update_count++; }
    agentos_mutex_unlock(g_state->lock);
    return entry ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

int agentos_health_check_unregister(const char* name) {
    if (!g_state || !name) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    health_check_entry_t** pp = &g_state->health_checks_head;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            health_check_entry_t* to_remove = *pp;
            *pp = (*pp)->next;
            agentos_mem_free(to_remove);
            g_state->health_checks_count--;
            agentos_mutex_unlock(g_state->lock);
            return AGENTOS_SUCCESS;
        }
        pp = &(*pp)->next;
    }
    agentos_mutex_unlock(g_state->lock);
    return AGENTOS_ENOENT;
}

int agentos_metric_unregister(const char* name, const char* labels) {
    if (!g_state || !name) return AGENTOS_EINVAL;
    agentos_mutex_lock(g_state->lock);
    metric_entry_t** pp = &g_state->metrics_head;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0 && strcmp((*pp)->labels, labels ? labels : "") == 0) {
            metric_entry_t* to_remove = *pp;
            *pp = (*pp)->next;
            agentos_mem_free(to_remove);
            g_state->metrics_count--;
            agentos_mutex_unlock(g_state->lock);
            return AGENTOS_SUCCESS;
        }
        pp = &(*pp)->next;
    }
    agentos_mutex_unlock(g_state->lock);
    return AGENTOS_ENOENT;
}

double agentos_metric_get_value(const char* name, const char* labels) {
    if (!g_state || !name) return 0.0;
    agentos_mutex_lock(g_state->lock);
    metric_entry_t* entry = find_metric_locked(name, labels ? labels : "");
    double value = entry ? entry->value : 0.0;
    agentos_mutex_unlock(g_state->lock);
    return value;
}

int agentos_metric_get_update_count(const char* name, const char* labels) {
    if (!g_state || !name) return 0;
    agentos_mutex_lock(g_state->lock);
    metric_entry_t* entry = find_metric_locked(name, labels ? labels : "");
    int count = entry ? (int)entry->update_count : 0;
    agentos_mutex_unlock(g_state->lock);
    return count;
}

uint64_t agentos_metric_get_last_update_ns(const char* name, const char* labels) {
    if (!g_state || !name) return 0;
    agentos_mutex_lock(g_state->lock);
    metric_entry_t* entry = find_metric_locked(name, labels ? labels : "");
    uint64_t ts = entry ? entry->last_update_ns : 0;
    agentos_mutex_unlock(g_state->lock);
    return ts;
}

int agentos_trace_span_start_with_parent(agentos_trace_context_t* context, const char* service_name, const char* operation_name, const char* parent_trace_id) {
    int ret = agentos_trace_span_start(context, service_name, operation_name);
    if (ret == AGENTOS_SUCCESS && parent_trace_id) {
        strncpy(context->parent_span_id, context->span_id, sizeof(context->parent_span_id) - 1);
        strncpy(context->trace_id, parent_trace_id, sizeof(context->trace_id) - 1);
    }
    return ret;
}
uint64_t agentos_trace_get_duration_ns(agentos_trace_context_t* context) {
    if (!context || context->end_ns == 0) return 0;
    return context->end_ns - context->start_ns;
}
int agentos_trace_is_error(agentos_trace_context_t* context) {
    return context ? context->error_code != 0 : 0;
}
uint64_t agentos_observability_get_total_metrics_collected(void) {
    if (!g_state) return 0;
    agentos_mutex_lock(g_state->lock);
    uint64_t count = g_state->total_metrics_collected;
    agentos_mutex_unlock(g_state->lock);
    return count;
}
uint64_t agentos_observability_get_total_health_checks_run(void) {
    if (!g_state) return 0;
    agentos_mutex_lock(g_state->lock);
    uint64_t count = g_state->total_health_checks_run;
    agentos_mutex_unlock(g_state->lock);
    return count;
}
int agentos_observability_is_initialized(void) { return g_state != NULL && g_state->initialized; }
size_t agentos_observability_get_histogram_count(void) {
    if (!g_state) return 0;
    agentos_mutex_lock(g_state->lock);
    size_t count = g_state->histograms_count;
    agentos_mutex_unlock(g_state->lock);
    return count;
}

int agentos_observability_get_config(agentos_observability_config_t* out_config) {
    if (!out_config) return AGENTOS_EINVAL;
    if (!g_state) return AGENTOS_ENOTINIT;
    memcpy(out_config, &g_state->manager, sizeof(agentos_observability_config_t));
    return AGENTOS_SUCCESS;
}
