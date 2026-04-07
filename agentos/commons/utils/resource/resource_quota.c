/**
 * @file resource_quota.c
 * @brief 资源配额管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 基于E-3资源确定性原则实现的资源配额管理系统。
 * 确保每个资源的生命周期可预测、可追踪、可验证。
 */

#include "resource_quota.h"
#include "../../atoms/corekern/include/agentos.h"
#include "../../utils/observability/include/logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RESOURCE_FLAG_MEMORY_EXCEEDED 0x01
#define RESOURCE_FLAG_CPU_EXCEEDED     0x02
#define RESOURCE_FLAG_IO_EXCEEDED      0x04
#define RESOURCE_FLAG_NETWORK_EXCEEDED 0x08

agentos_error_t agentos_resource_manager_create(
    const agentos_resource_quota_t* quota,
    const char* resource_id,
    agentos_resource_manager_t** out_manager) {

    if (!quota || !resource_id || !out_manager) {
        return AGENTOS_EINVAL;
    }

    agentos_resource_manager_t* manager = (agentos_resource_manager_t*)
        AGENTOS_CALLOC(1, sizeof(agentos_resource_manager_t));
    if (!manager) {
        return AGENTOS_ENOMEM;
    }

    memcpy(&manager->quota, quota, sizeof(agentos_resource_quota_t));
    memset(&manager->usage, 0, sizeof(agentos_resource_usage_t));

    manager->resource_id = AGENTOS_STRDUP(resource_id);
    if (!manager->resource_id) {
        AGENTOS_FREE(manager);
        return AGENTOS_ENOMEM;
    }

    manager->lock = NULL;
    manager->enabled = 1;
    manager->exceeded_flags = 0;
    manager->usage.start_time = time(NULL);
    manager->usage.last_update = manager->usage.start_time;

    *out_manager = manager;
    return AGENTOS_SUCCESS;
}

void agentos_resource_manager_destroy(agentos_resource_manager_t* manager) {
    if (!manager) return;

    if (manager->resource_id) {
        AGENTOS_FREE(manager->resource_id);
    }

    AGENTOS_FREE(manager);
}

agentos_error_t agentos_resource_check_memory(
    agentos_resource_manager_t* manager,
    size_t requested_bytes) {

    if (!manager || !manager->enabled) {
        return AGENTOS_SUCCESS;
    }

    if (requested_bytes == 0) {
        return AGENTOS_EINVAL;
    }

    if (manager->quota.max_memory_bytes > 0) {
        size_t projected = manager->usage.current_memory_bytes + requested_bytes;
        if (projected > manager->quota.max_memory_bytes) {
            manager->exceeded_flags |= RESOURCE_FLAG_MEMORY_EXCEEDED;
            AGENTOS_LOG_WARN("Resource %s: Memory quota exceeded (current: %zu, "
                           "requested: %zu, limit: %zu)",
                           manager->resource_id,
                           manager->usage.current_memory_bytes,
                           requested_bytes,
                           manager->quota.max_memory_bytes);
            return AGENTOS_ENOMEM;
        }
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_resource_record_allocation(
    agentos_resource_manager_t* manager,
    size_t bytes) {

    if (!manager || !manager->enabled) {
        return AGENTOS_SUCCESS;
    }

    if (bytes == 0) {
        return AGENTOS_EINVAL;
    }

    manager->usage.current_memory_bytes += bytes;
    manager->usage.operation_count++;
    manager->usage.last_update = time(NULL);

    if (manager->usage.current_memory_bytes > manager->usage.peak_usage) {
        manager->usage.peak_usage = manager->usage.current_memory_bytes;
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_resource_record_free(
    agentos_resource_manager_t* manager,
    size_t bytes) {

    if (!manager || !manager->enabled) {
        return AGENTOS_SUCCESS;
    }

    if (bytes == 0) {
        return AGENTOS_EINVAL;
    }

    if (bytes <= manager->usage.current_memory_bytes) {
        manager->usage.current_memory_bytes -= bytes;
    } else {
        AGENTOS_LOG_WARN("Resource %s: Free amount (%zu) exceeds allocated (%zu)",
                        manager->resource_id, bytes, manager->usage.current_memory_bytes);
        manager->usage.current_memory_bytes = 0;
    }

    manager->usage.last_update = time(NULL);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_resource_record_io(
    agentos_resource_manager_t* manager) {

    if (!manager || !manager->enabled) {
        return AGENTOS_SUCCESS;
    }

    manager->usage.total_io_ops++;
    manager->usage.operation_count++;
    manager->usage.last_update = time(NULL);

    if (manager->quota.max_io_ops > 0 &&
        manager->usage.total_io_ops >= manager->quota.max_io_ops) {
        manager->exceeded_flags |= RESOURCE_FLAG_IO_EXCEEDED;
        AGENTOS_LOG_WARN("Resource %s: I/O quota exceeded (total: %zu, limit: %zu)",
                        manager->resource_id,
                        manager->usage.total_io_ops,
                        manager->quota.max_io_ops);
        return AGENTOS_EBUSY;
    }

    return AGENTOS_SUCCESS;
}

int agentos_resource_is_exceeded(agentos_resource_manager_t* manager) {
    if (!manager || !manager->enabled) {
        return 0;
    }
    return (manager->exceeded_flags != 0) ? 1 : 0;
}

void agentos_resource_get_usage(
    agentos_resource_manager_t* manager,
    agentos_resource_usage_t* out_usage) {

    if (!manager || !out_usage) {
        return;
    }

    memcpy(out_usage, &manager->usage, sizeof(agentos_resource_usage_t));
}

const char* agentos_resource_get_exceeded_info(
    agentos_resource_manager_t* manager) {

    static char info_buffer[512];

    if (!manager || manager->exceeded_flags == 0) {
        return "No resource exceeded";
    }

    int offset = 0;
    offset += snprintf(info_buffer + offset, sizeof(info_buffer) - offset,
                       "Resource '%s' exceeded: ", manager->resource_id);

    if (manager->exceeded_flags & RESOURCE_FLAG_MEMORY_EXCEEDED) {
        offset += snprintf(info_buffer + offset, sizeof(info_buffer) - offset,
                          "[Memory] ");
    }
    if (manager->exceeded_flags & RESOURCE_FLAG_CPU_EXCEEDED) {
        offset += snprintf(info_buffer + offset, sizeof(info_buffer) - offset,
                          "[CPU] ");
    }
    if (manager->exceeded_flags & RESOURCE_FLAG_IO_EXCEEDED) {
        offset += snprintf(info_buffer + offset, sizeof(info_buffer) - offset,
                          "[I/O] ");
    }
    if (manager->exceeded_flags & RESOURCE_FLAG_NETWORK_EXCEEDED) {
        offset += snprintf(info_buffer + offset, sizeof(info_buffer) - offset,
                          "[Network] ");
    }

    return info_buffer;
}