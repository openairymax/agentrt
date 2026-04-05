/**
 * @file optimizer_utils.h
 * @brief 内存优化器内部工具函数接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef OPTIMIZER_UTILS_H
#define OPTIMIZER_UTILS_H

#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 内存对齐大小 */
#define MEMORY_ALIGNMENT 64

/** @brief 最小分配单元 */
#define MIN_ALLOCATION_SIZE 64

uint64_t optimizer_get_timestamp_ns(void);
size_t align_size_up(size_t size, size_t alignment);
double calculate_fragmentation_ratio(uint64_t total_size, uint64_t free_size);

#ifdef __cplusplus
}
#endif

#endif /* OPTIMIZER_UTILS_H */
