/**
 * @file resource_quota.h
 * @brief 资源配额管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 基于E-3资源确定性原则实现的资源配额管理系统。
 * 提供内存、CPU、I/O、网络等资源的配额限制和统计功能。
 */

#ifndef RESOURCE_QUOTA_H
#define RESOURCE_QUOTA_H

#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_resource_quota {
    size_t max_memory_bytes;
    uint64_t max_cpu_time_ms;
    size_t max_io_ops;
    size_t max_network_bytes;
    uint64_t timeout_ms;
} agentos_resource_quota_t;

typedef struct agentos_resource_usage {
    size_t current_memory_bytes;
    uint64_t total_cpu_time_ms;
    size_t total_io_ops;
    size_t total_network_bytes;
    time_t start_time;
    time_t last_update;
    uint64_t operation_count;
} agentos_resource_usage_t;

typedef struct agentos_resource_manager {
    agentos_resource_quota_t quota;
    agentos_resource_usage_t usage;
    char* resource_id;
    void* lock;
    int enabled;
    uint8_t exceeded_flags;
} agentos_resource_manager_t;

agentos_error_t agentos_resource_manager_create(
    const agentos_resource_quota_t* quota,
    const char* resource_id,
    agentos_resource_manager_t** out_manager);

void agentos_resource_manager_destroy(agentos_resource_manager_t* manager);

agentos_error_t agentos_resource_check_memory(
    agentos_resource_manager_t* manager,
    size_t requested_bytes);

agentos_error_t agentos_resource_record_allocation(
    agentos_resource_manager_t* manager,
    size_t bytes);

agentos_error_t agentos_resource_record_free(
    agentos_resource_manager_t* manager,
    size_t bytes);

agentos_error_t agentos_resource_record_io(
    agentos_resource_manager_t* manager);

int agentos_resource_is_exceeded(agentos_resource_manager_t* manager);

void agentos_resource_get_usage(
    agentos_resource_manager_t* manager,
    agentos_resource_usage_t* out_usage);

const char* agentos_resource_get_exceeded_info(
    agentos_resource_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_QUOTA_H */
