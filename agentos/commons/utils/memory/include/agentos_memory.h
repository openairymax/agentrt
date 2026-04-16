/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file agentos_memory.h
 * @brief 统一内存管理模块 - 核心层API
 * 
 * 提供安全、高效、统一的内存管理接口，支持内存分配、释放、调试和统计功能。
 * 本模块旨在消除项目中分散的内存管理代码，提供一致的内存管理策略。
 * 
 * @author Spharx AgentOS Team
 * @date 2026-03-30
 * @version 2.0
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-3 资源确定性原则
 */

#ifndef AGENTOS_MEMORY_H
#define AGENTOS_MEMORY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup memory_api 内存管理API
 * @{
 */

/**
 * @brief 内存分配失败处理策略
 */
typedef enum {
    MEMORY_FAIL_STRATEGY_RETURN_NULL,    /**< 返回NULL指针 */
    MEMORY_FAIL_STRATEGY_ABORT,          /**< 终止程序 */
    MEMORY_FAIL_STRATEGY_CALLBACK,       /**< 调用用户回调函数 */
    MEMORY_FAIL_STRATEGY_RETRY           /**< 重试分配（有限次数） */
} memory_fail_strategy_t;

/**
 * @brief 内存分配选项
 */
typedef struct {
    size_t alignment;                    /**< 内存对齐要求（0表示默认） */
    bool zero_memory;                    /**< 是否将分配的内存清零 */
    const char* tag;                     /**< 内存分配标签（用于调试） */
    memory_fail_strategy_t fail_strategy;/**< 分配失败处理策略 */
    void (*fail_callback)(size_t size, const char* tag, void* user_data); /**< 失败回调 */
    void* fail_callback_user_data;       /**< 失败回调用户数据 */
} memory_options_t;

/**
 * @brief 内存统计信息
 */
#define AGENTOS_MEMORY_STATS_T_DEFINED
typedef struct {
    size_t total_allocated;              /**< 总分配内存（字节） */
    size_t total_freed;                  /**< 总释放内存（字节） */
    size_t current_allocated;            /**< 当前分配内存（字节） */
    size_t peak_allocated;               /**< 峰值分配内存（字节） */
    size_t allocation_count;             /**< 分配次数 */
    size_t free_count;                   /**< 释放次数 */
    size_t leak_count;                   /**< 泄漏次数（如果启用调试） */
} memory_stats_t;

/**
 * @brief 内存调试信息
 */
typedef struct memory_debug_info {
    void* address;                       /**< 内存地址 */
    size_t size;                         /**< 分配大小 */
    const char* tag;                     /**< 分配标签 */
    const char* file;                    /**< 分配位置文件 */
    int line;                            /**< 分配位置行号 */
    const char* function;                /**< 分配位置函数 */
    uint64_t timestamp;                  /**< 分配时间戳 */
    struct memory_debug_info* next;      /**< 下一个调试信息节点 */
} memory_debug_info_t;

/**
 * @brief 初始化内存管理模块
 * 
 * @param[in] options 初始化选项（可为NULL，使用默认选项）
 * @return 成功返回true，失败返回false
 * 
 * @note 如果模块已初始化，再次调用将返回true
 */
bool memory_init(const memory_options_t* options);

/**
 * @brief 清理内存管理模块
 * 
 * 释放模块内部资源，如果启用调试模式，会检查内存泄漏。
 * 
 * @note 调用此函数后，模块将不可用，除非重新初始化
 */
void memory_cleanup(void);

/**
 * @brief 分配内存
 * 
 * @param[in] size 要分配的字节数
 * @param[in] tag 内存分配标签（用于调试，可为NULL）
 * @return 成功返回分配的内存指针，失败返回NULL
 * 
 * @note 分配的内存未初始化，除非设置了zero_memory选项
 */
void* memory_alloc(size_t size, const char* tag);

/**
 * @brief 分配并清零内存
 * 
 * @param[in] size 要分配的字节数
 * @param[in] tag 内存分配标签（用于调试，可为NULL）
 * @return 成功返回分配的内存指针，失败返回NULL
 * 
 * @note 分配的内存会被清零
 */
void* memory_calloc(size_t size, const char* tag);

/**
 * @brief 分配对齐内存
 * 
 * @param[in] alignment 对齐要求（必须是2的幂）
 * @param[in] size 要分配的字节数
 * @param[in] tag 内存分配标签（用于调试，可为NULL）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void* memory_aligned_alloc(size_t alignment, size_t size, const char* tag);

/**
 * @brief 重新分配内存
 * 
 * @param[in] ptr 原始内存指针
 * @param[in] new_size 新的字节数
 * @param[in] tag 内存分配标签（用于调试，可为NULL）
 * @return 成功返回重新分配的内存指针，失败返回NULL
 * 
 * @note 如果ptr为NULL，等同于memory_alloc
 * @note 如果new_size为0，等同于memory_free
 * @note 原始内存内容会被复制到新内存（如果新内存更大）
 */
void* memory_realloc(void* ptr, size_t new_size, const char* tag);

/**
 * @brief 释放内存
 * 
 * @param[in] ptr 要释放的内存指针
 * 
 * @note 如果ptr为NULL，函数无操作
 * @note 释放后建议将指针置为NULL，防止重复释放
 */
void memory_free(void* ptr);

/**
 * @brief 安全释放内存并将指针置为NULL
 * 
 * @param[inout] ptr_ptr 指向内存指针的指针
 * 
 * @note 释放后会自动将指针置为NULL
 */
#define MEMORY_FREE_SAFE(ptr_ptr) \
    do { \
        if ((ptr_ptr) != NULL && *(ptr_ptr) != NULL) { \
            memory_free(*(ptr_ptr)); \
            *(ptr_ptr) = NULL; \
        } \
    } while (0)

/**
 * @brief 获取内存统计信息
 * 
 * @param[out] stats 统计信息输出缓冲区
 * @return 成功返回true，失败返回false
 */
bool memory_get_stats(memory_stats_t* stats);

/**
 * @brief 重置内存统计信息
 */
void memory_reset_stats(void);

/**
 * @brief 启用或禁用内存调试
 * 
 * @param[in] enable 是否启用调试
 * @return 成功返回true，失败返回false
 * 
 * @note 启用调试会增加内存开销和性能开销
 */
bool memory_debug_enable(bool enable);

/**
 * @brief 检查内存泄漏
 * 
 * @param[in] dump_to_stderr 是否将泄漏信息输出到stderr
 * @return 泄漏的字节数，0表示无泄漏
 * 
 * @note 需要启用内存调试功能
 */
size_t memory_check_leaks(bool dump_to_stderr);

/**
 * @brief 转储内存调试信息
 * 
 * @param[in] file 输出文件名（NULL表示stderr）
 * 
 * @note 需要启用内存调试功能
 */
void memory_dump_debug_info(const char* file);

/**
 * @brief 验证内存块完整性
 * 
 * @param[in] ptr 内存指针
 * @return 内存块完整返回true，损坏返回false
 * 
 * @note 需要启用内存调试功能
 */
bool memory_validate(void* ptr);

/**
 * @brief 设置内存分配失败回调
 * 
 * @param[in] callback 回调函数
 * @param[in] user_data 用户数据
 */
void memory_set_fail_callback(
    void (*callback)(size_t size, const char* tag, void* user_data),
    void* user_data);

/**
 * @brief 获取当前分配的内存大小
 * 
 * @return 当前分配的总字节数
 */
size_t memory_get_current_usage(void);

/**
 * @brief 获取内存分配峰值
 * 
 * @return 内存分配峰值（字节）
 */
size_t memory_get_peak_usage(void);

/** @} */ // end of memory_api

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORY_H */