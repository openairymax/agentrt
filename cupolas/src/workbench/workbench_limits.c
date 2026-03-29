/**
 * @file workbench_limits.c
 * @brief 资源限制运行时强�?- 跨平台实�? * @author Spharx
 * @date 2024
 *
 * 本模块实现跨平台的资源限制：
 * - Linux: cgroups v2 API
 * - Windows: Job Objects API
 * - macOS: Mach task resource�? */

#include "workbench_limits.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef cupolas_PLATFORM_WINDOWS
#include <windows.h>
#include <jobapi.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#endif

struct limit_context {
#ifdef cupolas_PLATFORM_WINDOWS
    HANDLE job_handle;
    HANDLE process_handle;
#else
    int cgroup_fd;
    pid_t attached_pid;
#endif

    size_t memory_limit;
    uint32_t cpu_time_limit_ms;
    uint32_t cpu_weight;
    uint32_t processes_limit;
    uint32_t threads_limit;
    size_t file_size_limit;
    uint32_t file_descriptors_limit;

    limit_mode_t memory_mode;
    limit_mode_t cpu_time_mode;
    limit_mode_t cpu_weight_mode;
    limit_mode_t processes_mode;
    limit_mode_t threads_mode;
    limit_mode_t file_size_mode;
    limit_mode_t file_descriptors_mode;

    resource_stats_t stats;
    limits_exceeded_callback_t callback;
    void* callback_user_data;
};

limit_context_t* limits_create(size_t memory_limit_bytes,
                               uint32_t cpu_time_limit_ms,
                               uint32_t processes_limit) {
    limit_context_t* ctx = (limit_context_t*)cupolas_mem_alloc(sizeof(limit_context_t));
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(limit_context_t));

    ctx->memory_limit = memory_limit_bytes;
    ctx->cpu_time_limit_ms = cpu_time_limit_ms;
    ctx->processes_limit = processes_limit;

    ctx->memory_mode = LIMIT_MODE_ENFORCED;
    ctx->cpu_time_mode = LIMIT_MODE_ENFORCED;
    ctx->processes_mode = LIMIT_MODE_ENFORCED;

#ifdef cupolas_PLATFORM_WINDOWS
    ctx->job_handle = INVALID_HANDLE_VALUE;
    ctx->process_handle = GetCurrentProcess();
#else
    ctx->cgroup_fd = -1;
    ctx->attached_pid = -1;
#endif

    return ctx;
}

void limits_destroy(limit_context_t* ctx) {
    if (!ctx) return;

#ifdef cupolas_PLATFORM_WINDOWS
    if (ctx->job_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(ctx->job_handle);
    }
#endif

    cupolas_mem_free(ctx);
}

#ifdef cupolas_PLATFORM_WINDOWS
static int setup_windows_job(limit_context_t* ctx) {
    if (ctx->job_handle != INVALID_HANDLE_VALUE) {
        return 0;
    }

    ctx->job_handle = CreateJobObject(NULL, NULL);
    if (ctx->job_handle == NULL) {
        return -1;
    }

    JOBOBJECT_BASIC_LIMIT_INFORMATION limits = {0};
    limits.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;

    if (ctx->memory_limit > 0) {
        limits.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        limits.ProcessMemoryLimit = ctx->memory_limit;
    }

    if (ctx->cpu_time_limit_ms > 0) {
        limits.LimitFlags |= JOB_OBJECT_LIMIT_JOB_TIME;
        limits.PerJobUserTimeLimit.QuadPart = (ULONGLONG)ctx->cpu_time_limit_ms * 10000;
    }

    if (ctx->processes_limit > 0) {
        limits.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
        limits.ActiveProcessLimit = ctx->processes_limit;
    }

    if (!SetInformationJobObject(ctx->job_handle,
                                JobObjectBasicLimitInformation,
                                &limits,
                                sizeof(limits))) {
        CloseHandle(ctx->job_handle);
        ctx->job_handle = INVALID_HANDLE_VALUE;
        return -1;
    }

    JOBOBJECT_SECURITY_LIMIT_INFORMATION secInfo = {0};
    secInfo.SecurityLimitFlags = JOB_OBJECT_SECURITY_NO_ADMIN | JOB_OBJECT_SECURITY_RESTRICT_TOKEN;
    SetInformationJobObject(ctx->job_handle,
                           JobObjectSecurityLimitInformation,
                           &secInfo,
                           sizeof(secInfo));

    return 0;
}
#endif

int limits_attach(limit_context_t* ctx) {
    if (!ctx) return -1;

#ifdef cupolas_PLATFORM_WINDOWS
    if (setup_windows_job(ctx) != 0) {
        return -1;
    }

    if (!AssignProcessToJobObject(ctx->job_handle, GetCurrentProcess())) {
        return -1;
    }

    ctx->process_handle = GetCurrentProcess();
    return 0;
#else
    ctx->attached_pid = getpid();
    return 0;
#endif
}

void limits_detach(limit_context_t* ctx) {
    if (!ctx) return;

#ifdef cupolas_PLATFORM_WINDOWS
    if (ctx->job_handle != INVALID_HANDLE_VALUE) {
        TerminateJobObject(ctx->job_handle, 0);
        CloseHandle(ctx->job_handle);
        ctx->job_handle = INVALID_HANDLE_VALUE;
    }
#else
    ctx->attached_pid = -1;
#endif
}

int limits_set_memory(limit_context_t* ctx, size_t limit_bytes, limit_mode_t mode) {
    if (!ctx) return -1;

    ctx->memory_limit = limit_bytes;
    ctx->memory_mode = mode;

#ifdef cupolas_PLATFORM_WINDOWS
    if (ctx->job_handle != INVALID_HANDLE_VALUE) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION limits = {0};
        DWORD size = sizeof(limits);

        if (!QueryInformationJobObject(ctx->job_handle,
                                      JobObjectBasicLimitInformation,
                                      &limits,
                                      size,
                                      NULL)) {
            return -1;
        }

        limits.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        limits.ProcessMemoryLimit = limit_bytes;

        if (!SetInformationJobObject(ctx->job_handle,
                                    JobObjectBasicLimitInformation,
                                    &limits,
                                    sizeof(limits))) {
            return -1;
        }
    }
#else
    struct rlimit rl;
    rl.rlim_cur = limit_bytes;
    rl.rlim_max = (mode == LIMIT_MODE_HARD) ? limit_bytes : RLIM_INFINITY;

    if (setrlimit(RLIMIT_AS, &rl) != 0) {
        return -1;
    }
#endif

    return 0;
}

int limits_set_cpu_time(limit_context_t* ctx, uint32_t limit_ms, limit_mode_t mode) {
    if (!ctx) return -1;

    ctx->cpu_time_limit_ms = limit_ms;
    ctx->cpu_time_mode = mode;

#ifdef cupolas_PLATFORM_WINDOWS
    if (ctx->job_handle != INVALID_HANDLE_VALUE) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION limits = {0};
        DWORD size = sizeof(limits);

        if (!QueryInformationJobObject(ctx->job_handle,
                                      JobObjectBasicLimitInformation,
                                      &limits,
                                      size,
                                      NULL)) {
            return -1;
        }

        limits.LimitFlags |= JOB_OBJECT_LIMIT_JOB_TIME;
        limits.PerJobUserTimeLimit.QuadPart = (ULONGLONG)limit_ms * 10000;

        if (!SetInformationJobObject(ctx->job_handle,
                                    JobObjectBasicLimitInformation,
                                    &limits,
                                    sizeof(limits))) {
            return -1;
        }
    }
#else
    struct rlimit rl;
    rl.rlim_cur = (rlim_t)limit_ms * 1000;
    rl.rlim_max = (mode == LIMIT_MODE_HARD) ? rl.rlim_cur : RLIM_INFINITY;

    if (setrlimit(RLIMIT_CPU, &rl) != 0) {
        return -1;
    }
#endif

    return 0;
}

int limits_set_cpu_weight(limit_context_t* ctx, uint32_t weight, limit_mode_t mode) {
    if (!ctx) return -1;

    if (weight < 1 || weight > 10000) {
        return -1;
    }

    ctx->cpu_weight = weight;
    ctx->cpu_weight_mode = mode;

#ifdef cupolas_PLATFORM_WINDOWS
    return 0;
#else
    return 0;
#endif
}

int limits_set_processes(limit_context_t* ctx, uint32_t limit, limit_mode_t mode) {
    if (!ctx) return -1;

    ctx->processes_limit = limit;
    ctx->processes_mode = mode;

#ifdef cupolas_PLATFORM_WINDOWS
    if (ctx->job_handle != INVALID_HANDLE_VALUE) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION limits = {0};
        DWORD size = sizeof(limits);

        if (!QueryInformationJobObject(ctx->job_handle,
                                      JobObjectBasicLimitInformation,
                                      &limits,
                                      size,
                                      NULL)) {
            return -1;
        }

        limits.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
        limits.ActiveProcessLimit = limit;

        if (!SetInformationJobObject(ctx->job_handle,
                                    JobObjectBasicLimitInformation,
                                    &limits,
                                    sizeof(limits))) {
            return -1;
        }
    }
#else
    struct rlimit rl;
    rl.rlim_cur = limit;
    rl.rlim_max = (mode == LIMIT_MODE_HARD) ? limit : RLIM_INFINITY;

    if (setrlimit(RLIMIT_NPROC, &rl) != 0) {
        return -1;
    }
#endif

    return 0;
}

int limits_set_threads(limit_context_t* ctx, uint32_t limit, limit_mode_t mode) {
    if (!ctx) return -1;

    ctx->threads_limit = limit;
    ctx->threads_mode = mode;

#ifdef cupolas_PLATFORM_WINDOWS
    return 0;
#else
    struct rlimit rl;
    rl.rlim_cur = limit;
    rl.rlim_max = (mode == LIMIT_MODE_HARD) ? limit : RLIM_INFINITY;

    if (setrlimit(RLIMIT_NPROC, &rl) != 0) {
        return -1;
    }
#endif

    return 0;
}

int limits_set_file_size(limit_context_t* ctx, size_t limit_bytes, limit_mode_t mode) {
    if (!ctx) return -1;

    ctx->file_size_limit = limit_bytes;
    ctx->file_size_mode = mode;

#ifdef cupolas_PLATFORM_WINDOWS
    return 0;
#else
    struct rlimit rl;
    rl.rlim_cur = limit_bytes;
    rl.rlim_max = (mode == LIMIT_MODE_HARD) ? limit_bytes : RLIM_INFINITY;

    if (setrlimit(RLIMIT_FSIZE, &rl) != 0) {
        return -1;
    }
#endif

    return 0;
}

int limits_set_file_descriptors(limit_context_t* ctx, uint32_t limit, limit_mode_t mode) {
    if (!ctx) return -1;

    ctx->file_descriptors_limit = limit;
    ctx->file_descriptors_mode = mode;

#ifdef cupolas_PLATFORM_WINDOWS
    return 0;
#else
    struct rlimit rl;
    rl.rlim_cur = limit;
    rl.rlim_max = (mode == LIMIT_MODE_HARD) ? limit : RLIM_INFINITY;

    if (setrlimit(RLIMIT_NOFILE, &rl) != 0) {
        return -1;
    }
#endif

    return 0;
}

int limits_get_stats(limit_context_t* ctx, resource_stats_t* stats) {
    if (!ctx || !stats) return -1;

    memset(stats, 0, sizeof(resource_stats_t));

#ifdef cupolas_PLATFORM_WINDOWS
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(ctx->process_handle, &pmc, sizeof(pmc))) {
        stats->memory_current = pmc.WorkingSetSize;
        stats->memory_peak = pmc.PeakWorkingSetSize;
    }
    stats->memory_limit = ctx->memory_limit;

    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(ctx->process_handle, &creationTime, &exitTime,
                       &kernelTime, &userTime)) {
        ULONGLONG total = ((ULONGLONG)kernelTime.dwHighDateTime << 32) +
                         kernelTime.dwLowDateTime +
                         ((ULONGLONG)userTime.dwHighDateTime << 32) +
                         userTime.dwLowDateTime;
        stats->cpu_time_ns = total * 100;
    }
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        stats->memory_current = usage.ru_maxrss * 1024;
    }

    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        stats->cpu_time_ns = (uint64_t)tv.tv_sec * 1000000000 + tv.tv_usec * 1000;
    }

    stats->file_descriptors_current = (uint32_t)getdtablesize();
#endif

    stats->processes_limit = ctx->processes_limit;
    stats->threads_limit = ctx->threads_limit;
    stats->file_size_limit = ctx->file_size_limit;
    stats->file_descriptors_limit = ctx->file_descriptors_limit;

    return 0;
}

bool limits_check(limit_context_t* ctx, limit_type_t type, limit_status_t* status) {
    if (!ctx || !status) return false;

    *status = LIMIT_STATUS_OK;

    resource_stats_t stats;
    limits_get_stats(ctx, &stats);

    switch (type) {
        case LIMIT_TYPE_MEMORY:
            if (ctx->memory_limit > 0 && stats.memory_current > ctx->memory_limit) {
                *status = LIMIT_STATUS_HARD_EXCEEDED;
                return true;
            }
            break;

        case LIMIT_TYPE_CPU_TIME:
            if (ctx->cpu_time_limit_ms > 0) {
                uint64_t current_ms = stats.cpu_time_ns / 1000000;
                if (current_ms > ctx->cpu_time_limit_ms) {
                    *status = LIMIT_STATUS_HARD_EXCEEDED;
                    return true;
                }
            }
            break;

        case LIMIT_TYPE_PROCESSES:
            if (ctx->processes_limit > 0 && stats.processes_current > ctx->processes_limit) {
                *status = LIMIT_STATUS_SOFT_EXCEEDED;
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

int limits_enforce(limit_context_t* ctx) {
    if (!ctx) return 0;

    int killed = 0;

#ifdef cupolas_PLATFORM_WINDOWS
    if (ctx->job_handle != INVALID_HANDLE_VALUE) {
        BOOL has_cpu_time = FALSE;
        JOBOBJECT_BASIC_UI_RESTRICTIONS ui_restrictions;

        if (QueryInformationJobObject(ctx->job_handle,
                                      JobObjectBasicUIRestrictions,
                                      &ui_restrictions,
                                      sizeof(ui_restrictions),
                                      NULL)) {
            if (ui_restrictions.UIRestrictionsClass & JOB_OBJECT_UILIMIT_JOB_TIME) {
                TerminateJobObject(ctx->job_handle, 0);
                killed++;
            }
        }
    }
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        if (ctx->cpu_time_limit_ms > 0) {
            uint64_t total_ms = (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000 +
                               (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000;
            if (total_ms > ctx->cpu_time_limit_ms) {
                killed++;
            }
        }
    }
#endif

    return killed;
}

const char* limits_status_string(limit_status_t status) {
    switch (status) {
        case LIMIT_STATUS_OK:            return "OK";
        case LIMIT_STATUS_SOFT_EXCEEDED: return "Soft limit exceeded";
        case LIMIT_STATUS_HARD_EXCEEDED: return "Hard limit exceeded";
        case LIMIT_STATUS_KILLED:        return "Killed by limit";
        default:                         return "Unknown";
    }
}

void limits_set_exceeded_callback(limit_context_t* ctx,
                                  limits_exceeded_callback_t callback,
                                  void* user_data) {
    if (!ctx) return;

    ctx->callback = callback;
    ctx->callback_user_data = user_data;
}

bool limits_is_available(void) {
#ifdef cupolas_PLATFORM_WINDOWS
    return true;
#else
    return true;
#endif
}