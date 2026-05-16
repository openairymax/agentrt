#include "sync_internal.h"
#include "memory_compat.h"
#include <string.h>
#include <errno.h>

char* sync_internal_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* dup = (char*)AGENTOS_MALLOC(len);
    if (dup) memcpy(dup, str, len);
    return dup;
}

sync_result_t sync_internal_posix_error_to_result(int error_code) {
    switch (error_code) {
        case EINVAL: return SYNC_ERROR_INVALID;
        case ENOMEM: return SYNC_ERROR_MEMORY;
        case EPERM:  return SYNC_ERROR_PERMISSION;
        case EBUSY:  return SYNC_ERROR_BUSY;
        case ETIMEDOUT: return SYNC_ERROR_TIMEOUT;
        case EDEADLK: return SYNC_ERROR_DEADLOCK;
        default:     return SYNC_ERROR_UNKNOWN;
    }
}

void sync_internal_update_stats_lock(sync_stats_t* stats, int64_t elapsed_ms) {
    if (stats) {
        stats->lock_count++;
        stats->total_wait_time_ms += (uint64_t)elapsed_ms;
        if ((uint64_t)elapsed_ms > stats->max_wait_time_ms)
            stats->max_wait_time_ms = (uint64_t)elapsed_ms;
    }
}

void sync_internal_update_stats_timeout(sync_stats_t* stats) {
    if (stats) {
        stats->timeout_count++;
    }
}

void sync_internal_update_stats_wait(sync_stats_t* stats, int64_t elapsed_ms) {
    if (stats) {
        stats->wait_count++;
        stats->total_wait_time_ms += (uint64_t)elapsed_ms;
        if ((uint64_t)elapsed_ms > stats->max_wait_time_ms)
            stats->max_wait_time_ms = (uint64_t)elapsed_ms;
    }
}
