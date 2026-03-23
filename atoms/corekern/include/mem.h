/**
 * @file mem.h
 * @brief 内核内存管理接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MEM_H
#define AGENTOS_MEM_H

#include <stddef.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_mem_alloc_info {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    struct agentos_mem_alloc_info* next;
} agentos_mem_alloc_info_t;

AGENTOS_API agentos_error_t agentos_mem_init(size_t heap_size);

AGENTOS_API void* agentos_mem_alloc(size_t size);

AGENTOS_API void* agentos_mem_alloc_ex(size_t size, const char* file, int line);

AGENTOS_API void* agentos_mem_aligned_alloc(size_t size, size_t alignment);

AGENTOS_API void* agentos_mem_aligned_alloc_ex(size_t size, size_t alignment, const char* file, int line);

AGENTOS_API void agentos_mem_free(void* ptr);

AGENTOS_API void agentos_mem_aligned_free(void* ptr);

AGENTOS_API void* agentos_mem_realloc(void* ptr, size_t new_size);

AGENTOS_API void* agentos_mem_realloc_ex(void* ptr, size_t new_size, const char* file, int line);

AGENTOS_API void* agentos_mem_pool_create(size_t block_size, uint32_t block_count);

AGENTOS_API void* agentos_mem_pool_alloc(void* pool);

AGENTOS_API void agentos_mem_pool_free(void* pool, void* ptr);

AGENTOS_API void agentos_mem_pool_destroy(void* pool);

AGENTOS_API void agentos_mem_stats(size_t* out_total, size_t* out_used, size_t* out_peak);

AGENTOS_API size_t agentos_mem_check_leaks(void);

AGENTOS_API void agentos_mem_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEM_H */
