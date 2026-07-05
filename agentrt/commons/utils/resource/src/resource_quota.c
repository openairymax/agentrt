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

#include "../../../../atoms/corekern/include/agentrt.h"
#include "../../utils/observability/include/logger.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RQ_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", \
         agentrt_error_str(c)); return (c); } while(0)

#define RESOURCE_FLAG_MEMORY_EXCEEDED 0x01
#define RESOURCE_FLAG_CPU_EXCEEDED 0x02
#define RESOURCE_FLAG_IO_EXCEEDED 0x04
#define RESOURCE_FLAG_NETWORK_EXCEEDED 0x08

agentrt_error_t agentrt_resource_manager_create(const agentrt_resource_quota_t *quota,
                                                const char *resource_id,
                                                agentrt_resource_manager_t **out_manager)
{

    if (!quota || !resource_id || !out_manager) {
        RQ_RET_ERR(AGENTRT_EINVAL);
    }

    agentrt_resource_manager_t *manager =
        (agentrt_resource_manager_t *)AGENTRT_CALLOC(1, sizeof(agentrt_resource_manager_t));
    if (!manager) {
        RQ_RET_ERR(AGENTRT_ENOMEM);
    }

    __builtin_memcpy(&manager->quota, quota, sizeof(agentrt_resource_quota_t));
    AGENTRT_MEMSET(&manager->usage, 0, sizeof(agentrt_resource_usage_t));

    manager->resource_id = AGENTRT_STRDUP(resource_id);
    if (!manager->resource_id) {
        AGENTRT_FREE(manager);
        RQ_RET_ERR(AGENTRT_ENOMEM);
    }

    manager->lock = NULL;
    manager->enabled = 1;
    manager->exceeded_flags = 0;
    manager->usage.start_time = time(NULL);
    manager->usage.last_update = manager->usage.start_time;

    *out_manager = manager;
    return AGENTRT_SUCCESS;
}

void agentrt_resource_manager_destroy(agentrt_resource_manager_t *manager)
{
    if (!manager)
        return;

    if (manager->resource_id) {
        AGENTRT_FREE(manager->resource_id);
    }

    AGENTRT_FREE(manager);
}

agentrt_error_t agentrt_resource_check_memory(agentrt_resource_manager_t *manager,
                                              size_t requested_bytes)
{

    if (!manager || !manager->enabled) {
        return AGENTRT_SUCCESS;
    }

    if (requested_bytes == 0) {
        RQ_RET_ERR(AGENTRT_EINVAL);
    }

    if (manager->quota.max_memory_bytes > 0) {
        size_t projected = manager->usage.current_memory_bytes + requested_bytes;
        if (projected > manager->quota.max_memory_bytes) {
            manager->exceeded_flags |= RESOURCE_FLAG_MEMORY_EXCEEDED;
            AGENTRT_LOG_WARN("Resource %s: Memory quota exceeded (current: %zu, "
                             "requested: %zu, limit: %zu)",
                             manager->resource_id, manager->usage.current_memory_bytes,
                             requested_bytes, manager->quota.max_memory_bytes);
            RQ_RET_ERR(AGENTRT_ENOMEM);
        }
    }

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_resource_record_allocation(agentrt_resource_manager_t *manager,
                                                   size_t bytes)
{

    if (!manager || !manager->enabled) {
        return AGENTRT_SUCCESS;
    }

    if (bytes == 0) {
        RQ_RET_ERR(AGENTRT_EINVAL);
    }

    manager->usage.current_memory_bytes += bytes;
    manager->usage.operation_count++;
    manager->usage.last_update = time(NULL);

    if (manager->usage.current_memory_bytes > manager->usage.peak_usage) {
        manager->usage.peak_usage = manager->usage.current_memory_bytes;
    }

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_resource_record_free(agentrt_resource_manager_t *manager, size_t bytes)
{

    if (!manager || !manager->enabled) {
        return AGENTRT_SUCCESS;
    }

    if (bytes == 0) {
        RQ_RET_ERR(AGENTRT_EINVAL);
    }

    if (bytes <= manager->usage.current_memory_bytes) {
        manager->usage.current_memory_bytes -= bytes;
    } else {
        AGENTRT_LOG_WARN("Resource %s: Free amount (%zu) exceeds allocated (%zu)",
                         manager->resource_id, bytes, manager->usage.current_memory_bytes);
        manager->usage.current_memory_bytes = 0;
    }

    manager->usage.last_update = time(NULL);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_resource_record_io(agentrt_resource_manager_t *manager)
{

    if (!manager || !manager->enabled) {
        return AGENTRT_SUCCESS;
    }

    manager->usage.total_io_ops++;
    manager->usage.operation_count++;
    manager->usage.last_update = time(NULL);

    if (manager->quota.max_io_ops > 0 && manager->usage.total_io_ops >= manager->quota.max_io_ops) {
        manager->exceeded_flags |= RESOURCE_FLAG_IO_EXCEEDED;
        AGENTRT_LOG_WARN("Resource %s: I/O quota exceeded (total: %zu, limit: %zu)",
                         manager->resource_id, manager->usage.total_io_ops,
                         manager->quota.max_io_ops);
        RQ_RET_ERR(AGENTRT_EBUSY);
    }

    return AGENTRT_SUCCESS;
}

int agentrt_resource_is_exceeded(agentrt_resource_manager_t *manager)
{
    if (!manager || !manager->enabled) {
        return 0;
    }
    return (manager->exceeded_flags != 0) ? 1 : 0;
}

void agentrt_resource_get_usage(agentrt_resource_manager_t *manager,
                                agentrt_resource_usage_t *out_usage)
{

    if (!manager || !out_usage) {
        return;
    }

    __builtin_memcpy(out_usage, &manager->usage, sizeof(agentrt_resource_usage_t));
}

const char *agentrt_resource_get_exceeded_info(agentrt_resource_manager_t *manager)
{

    static char info_buffer[512];

    if (!manager || manager->exceeded_flags == 0) {
        return "No resource exceeded";
    }

    int offset = 0;
    offset += snprintf(info_buffer + offset, sizeof(info_buffer) - offset,
                       "Resource '%s' exceeded: ", manager->resource_id);

    if (manager->exceeded_flags & RESOURCE_FLAG_MEMORY_EXCEEDED) {
        offset += snprintf(info_buffer + offset, sizeof(info_buffer) - offset, "[Memory] ");
    }
    if (manager->exceeded_flags & RESOURCE_FLAG_CPU_EXCEEDED) {
        offset += snprintf(info_buffer + offset, sizeof(info_buffer) - offset, "[CPU] ");
    }
    if (manager->exceeded_flags & RESOURCE_FLAG_IO_EXCEEDED) {
        offset += snprintf(info_buffer + offset, sizeof(info_buffer) - offset, "[I/O] ");
    }
    if (manager->exceeded_flags & RESOURCE_FLAG_NETWORK_EXCEEDED) {
        offset += snprintf(info_buffer + offset, sizeof(info_buffer) - offset, "[Network] ");
    }

    return info_buffer;
}