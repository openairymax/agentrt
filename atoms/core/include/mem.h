/**
 * @file mem.h
 * @brief 内核内存管理接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_MEM_H
#define AGENTOS_MEM_H

#include <stddef.h>
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 内存分配信息结构
 */
typedef struct agentos_mem_alloc_info {
    void* ptr;             /**< 内存指针 */
    size_t size;           /**< 分配大小 */
    const char* file;      /**< 分配文件 */
    int line;              /**< 分配行号 */
    struct agentos_mem_alloc_info* next;  /**< 下一个分配信息 */
} agentos_mem_alloc_info_t;

/**
 * @brief 初始化内存子系统
 * @param heap_size 堆大小（字节）
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_mem_init(size_t heap_size);

/**
 * @brief 分配内存
 * @param size 请求大小
 * @return 分配的内存指针，失败返回 NULL
 */
void* agentos_mem_alloc(size_t size);

/**
 * @brief 分配内存（带文件和行号信息）
 * @param size 请求大小
 * @param file 调用文件
 * @param line 调用行号
 * @return 分配的内存指针，失败返回 NULL
 */
void* agentos_mem_alloc_ex(size_t size, const char* file, int line);

/**
 * @brief 分配对齐内存
 * @param size 请求大小
 * @param alignment 对齐边界（必须是2的幂）
 * @return 对齐的内存指针，失败返回 NULL
 */
void* agentos_mem_aligned_alloc(size_t size, size_t alignment);

/**
 * @brief 分配对齐内存（带文件和行号信息）
 * @param size 请求大小
 * @param alignment 对齐边界（必须是2的幂）
 * @param file 调用文件
 * @param line 调用行号
 * @return 对齐的内存指针，失败返回 NULL
 */
void* agentos_mem_aligned_alloc_ex(size_t size, size_t alignment, const char* file, int line);

/**
 * @brief 释放内存
 * @param ptr 之前分配的内存指针
 */
void agentos_mem_free(void* ptr);

/**
 * @brief 重新分配内存
 * @param ptr 原指针
 * @param new_size 新大小
 * @return 新指针，失败返回 NULL（原内存保持不变）
 */
void* agentos_mem_realloc(void* ptr, size_t new_size);

/**
 * @brief 重新分配内存（带文件和行号信息）
 * @param ptr 原指针
 * @param new_size 新大小
 * @param file 调用文件
 * @param line 调用行号
 * @return 新指针，失败返回 NULL（原内存保持不变）
 */
void* agentos_mem_realloc_ex(void* ptr, size_t new_size, const char* file, int line);

/**
 * @brief 创建内存池
 * @param block_size 每个块的大小
 * @param block_count 初始块数量
 * @return 内存池句柄，失败返回 NULL
 */
void* agentos_mem_pool_create(size_t block_size, uint32_t block_count);

/**
 * @brief 从内存池分配块
 * @param pool 内存池句柄
 * @return 块指针，失败返回 NULL
 */
void* agentos_mem_pool_alloc(void* pool);

/**
 * @brief 释放块回内存池
 * @param pool 内存池句柄
 * @param ptr 要释放的块
 */
void agentos_mem_pool_free(void* pool, void* ptr);

/**
 * @brief 销毁内存池
 * @param pool 内存池句柄
 */
void agentos_mem_pool_destroy(void* pool);

/**
 * @brief 获取内存统计信息
 * @param out_total 输出总分配大小
 * @param out_used 输出已使用大小
 * @param out_peak 输出峰值使用
 */
void agentos_mem_stats(size_t* out_total, size_t* out_used, size_t* out_peak);

/**
 * @brief 检查内存泄露
 * @return 泄露的内存分配数量
 */
size_t agentos_mem_check_leaks(void);

/**
 * @brief 清理内存子系统
 */
void agentos_mem_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEM_H */