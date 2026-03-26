/**
 * @file resource_guard.h
 * @brief 资源作用域守卫 - RAII模式实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块提供资源自动释放机制，确保资源在作用域结束时正确释放。
 * 支持自定义释放函数，适用于文件句柄、内存、锁、网络连接等资源。
 * 
 * 使用示例：
 * @code
 * FILE* file = fopen("test.txt", "r");
 * AGENTOS_SCOPE_EXIT(fclose(file));  // 作用域结束时自动关闭
 * 
 * void* buffer = malloc(1024);
 * AGENTOS_SCOPE_EXIT(free(buffer));  // 作用域结束时自动释放
 * @endcode
 */

#ifndef AGENTOS_RESOURCE_GUARD_H
#define AGENTOS_RESOURCE_GUARD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 类型定义 ==================== */

/**
 * @brief 资源释放函数类型
 * @param resource 资源指针
 */
typedef void (*agentos_resource_cleanup_t)(void* resource);

/**
 * @brief 资源守卫结构
 */
typedef struct agentos_resource_guard {
    void* resource;                       /**< 资源指针 */
    agentos_resource_cleanup_t cleanup;   /**< 清理函数 */
    const char* file;                     /**< 分配文件名 */
    int line;                             /**< 分配行号 */
    const char* name;                     /**< 资源名称 */
    int active;                           /**< 是否激活（用于取消释放） */
} agentos_resource_guard_t;

/* ==================== 核心接口 ==================== */

/**
 * @brief 初始化资源守卫
 * @param guard [out] 守卫结构指针
 * @param resource [in] 资源指针
 * @param cleanup [in] 清理函数
 * @param file [in] 文件名
 * @param line [in] 行号
 * @param name [in] 资源名称
 */
void agentos_resource_guard_init(
    agentos_resource_guard_t* guard,
    void* resource,
    agentos_resource_cleanup_t cleanup,
    const char* file,
    int line,
    const char* name);

/**
 * @brief 执行资源清理
 * @param guard [in] 守卫结构指针
 */
void agentos_resource_guard_cleanup(agentos_resource_guard_t* guard);

/**
 * @brief 取消资源清理（转移所有权）
 * @param guard [in] 守卫结构指针
 */
void agentos_resource_guard_dismiss(agentos_resource_guard_t* guard);

/* ==================== 资源追踪接口 ==================== */

#ifdef AGENTOS_RESOURCE_TRACKING

/**
 * @brief 资源追踪记录
 */
typedef struct agentos_resource_record {
    void* resource;                       /**< 资源指针 */
    const char* type;                     /**< 资源类型 */
    const char* file;                     /**< 分配文件名 */
    int line;                             /**< 分配行号 */
    uint64_t timestamp_ns;                /**< 分配时间戳 */
    struct agentos_resource_record* next; /**< 下一条记录 */
} agentos_resource_record_t;

/**
 * @brief 注册资源分配
 * @param resource 资源指针
 * @param type 资源类型
 * @param file 文件名
 * @param line 行号
 */
void agentos_resource_track_alloc(void* resource, const char* type, const char* file, int line);

/**
 * @brief 注销资源分配
 * @param resource 资源指针
 */
void agentos_resource_track_free(void* resource);

/**
 * @brief 获取资源追踪报告
 * @param out_report [out] 输出报告字符串（调用者负责释放）
 * @return 未释放资源数量
 */
int agentos_resource_track_report(char** out_report);

/**
 * @brief 清空资源追踪记录
 */
void agentos_resource_track_clear(void);

#endif /* AGENTOS_RESOURCE_TRACKING */

/* ==================== 便捷宏定义 ==================== */

/**
 * @brief 创建作用域守卫（自动生成变量名）
 * @param resource 资源指针
 * @param cleanup 清理函数
 */
#define AGENTOS_SCOPE_GUARD(resource, cleanup) \
    agentos_resource_guard_t AGENTOS_UNIQUE_NAME(_guard) __attribute__((cleanup(agentos_resource_guard_cleanup))) = { \
        .resource = (resource), \
        .cleanup = (cleanup), \
        .file = __FILE__, \
        .line = __LINE__, \
        .name = #resource, \
        .active = 1 \
    }

/**
 * @brief 创建作用域守卫（带自定义清理）
 * @param resource 资源指针
 * @param cleanup 清理函数
 */
#define AGENTOS_SCOPE_EXIT(resource, cleanup) \
    agentos_resource_guard_t AGENTOS_UNIQUE_NAME(_scope_exit) __attribute__((cleanup(agentos_resource_guard_cleanup))) = { \
        .resource = (resource), \
        .cleanup = (cleanup), \
        .file = __FILE__, \
        .line = __LINE__, \
        .name = #resource, \
        .active = 1 \
    }

/**
 * @brief 取消作用域守卫（转移所有权）
 * @param resource 资源指针
 */
#define AGENTOS_SCOPE_DISMISS(resource) \
    do { \
        agentos_resource_guard_dismiss(&AGENTOS_UNIQUE_NAME(_scope_exit)); \
    } while(0)

/**
 * @brief 生成唯一变量名
 */
#define AGENTOS_UNIQUE_NAME(prefix) \
    AGENTOS_CONCAT(prefix, __LINE__)

#define AGENTOS_CONCAT(a, b) \
    AGENTOS_CONCAT_IMPL(a, b)

#define AGENTOS_CONCAT_IMPL(a, b) \
    a##b

/* ==================== 资源追踪宏 ==================== */

#ifdef AGENTOS_RESOURCE_TRACKING

/**
 * @brief 追踪内存分配
 */
#define AGENTOS_TRACK_ALLOC(ptr, type) \
    agentos_resource_track_alloc(ptr, type, __FILE__, __LINE__)

/**
 * @brief 追踪内存释放
 */
#define AGENTOS_TRACK_FREE(ptr) \
    agentos_resource_track_free(ptr)

/**
 * @brief 追踪的内存分配
 */
#define AGENTOS_TRACKED_MALLOC(size) \
    ({ \
        void* _ptr = malloc(size); \
        if (_ptr) AGENTOS_TRACK_ALLOC(_ptr, "memory"); \
        _ptr; \
    })

/**
 * @brief 追踪的内存释放
 */
#define AGENTOS_TRACKED_FREE(ptr) \
    do { \
        if (ptr) { \
            AGENTOS_TRACK_FREE(ptr); \
            free(ptr); \
            ptr = NULL; \
        } \
    } while(0)

#else

#define AGENTOS_TRACK_ALLOC(ptr, type) ((void)0)
#define AGENTOS_TRACK_FREE(ptr) ((void)0)
#define AGENTOS_TRACKED_MALLOC(size) malloc(size)
#define AGENTOS_TRACKED_FREE(ptr) free(ptr)

#endif /* AGENTOS_RESOURCE_TRACKING */

/* ==================== 跨平台属性支持 ==================== */

#ifndef __attribute__
#define __attribute__(x)
#endif

#ifdef _MSC_VER
#define __attribute__(x)
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_RESOURCE_GUARD_H */
