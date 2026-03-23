/**
 * @file agentos.h
 * @brief AgentOS Dynamic 模块依赖的公共接口
 * 
 * 此头文件提供 dynamic 模块所需的核心类型和函数声明。
 * 完整实现请参考 atoms/corekern/include/agentos.h
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_DYNAMIC_AGENTOS_H
#define AGENTOS_DYNAMIC_AGENTOS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 符号导出宏 ========== */

#ifdef AGENTOS_BUILDING_SHARED
    #if defined(_WIN32) || defined(_WIN64)
        #define AGENTOS_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define AGENTOS_API __attribute__((visibility("default")))
    #else
        #define AGENTOS_API
    #endif
#else
    #if defined(_WIN32) || defined(_WIN64)
        #define AGENTOS_API __declspec(dllimport)
    #else
        #define AGENTOS_API
    #endif
#endif

#ifndef AGENTOS_API
    #define AGENTOS_API
#endif

/* ========== 错误码定义 ========== */

typedef int32_t agentos_error_t;

#define AGENTOS_SUCCESS                0
#define AGENTOS_EINVAL                 -1
#define AGENTOS_ENOMEM                 -2
#define AGENTOS_EBUSY                  -3
#define AGENTOS_ENOENT                 -4
#define AGENTOS_EPERM                  -5
#define AGENTOS_ETIMEDOUT              -6
#define AGENTOS_EEXIST                 -7
#define AGENTOS_ECANCELED              -8
#define AGENTOS_ENOTSUP                -9
#define AGENTOS_EIO                    -10
#define AGENTOS_EINTR                  -11
#define AGENTOS_EOVERFLOW              -12
#define AGENTOS_EBADF                  -13
#define AGENTOS_ENOTINIT               -14
#define AGENTOS_ERESOURCE              -15
#define AGENTOS_ERROR                  -100

/**
 * @brief 获取错误码对应的错误字符串
 * @param err 错误码
 * @return 错误描述字符串
 */
AGENTOS_API const char* agentos_strerror(agentos_error_t err);

/* ========== 时间服务 ========== */

/**
 * @brief 获取单调递增时间（纳秒）
 * @return 纳秒级时间戳
 */
AGENTOS_API uint64_t agentos_time_monotonic_ns(void);

/**
 * @brief 获取单调递增时间（毫秒）
 * @return 毫秒级时间戳
 */
AGENTOS_API uint64_t agentos_time_monotonic_ms(void);

/**
 * @brief 获取当前时间（纳秒）
 * @return 纳秒级时间戳
 */
AGENTOS_API uint64_t agentos_time_current_ns(void);

/**
 * @brief 纳秒级睡眠
 * @param ns 睡眠时间（纳秒）
 * @return AGENTOS_SUCCESS 成功
 */
AGENTOS_API agentos_error_t agentos_time_nanosleep(uint64_t ns);

/* ========== 日志服务 ========== */

#define AGENTOS_LOG_LEVEL_ERROR 1
#define AGENTOS_LOG_LEVEL_WARN  2
#define AGENTOS_LOG_LEVEL_INFO  3
#define AGENTOS_LOG_LEVEL_DEBUG 4

#ifndef AGENTOS_LOG_LEVEL
#define AGENTOS_LOG_LEVEL AGENTOS_LOG_LEVEL_INFO
#endif

/**
 * @brief 初始化日志系统
 * @param config_path 配置文件路径（可为 NULL 使用默认）
 * @return AGENTOS_SUCCESS 成功
 */
AGENTOS_API agentos_error_t agentos_logger_init(const char* config_path);

/**
 * @brief 关闭日志系统
 */
AGENTOS_API void agentos_logger_shutdown(void);

/**
 * @brief 设置当前线程的追踪ID
 * @param trace_id 追踪ID，若为NULL则自动生成
 * @return 实际设置的追踪ID（静态内存，无需释放）
 */
AGENTOS_API const char* agentos_log_set_trace_id(const char* trace_id);

/**
 * @brief 获取当前线程的追踪ID
 * @return 追踪ID，可能为NULL
 */
AGENTOS_API const char* agentos_log_get_trace_id(void);

/**
 * @brief 记录日志
 * @param level 日志级别
 * @param file 文件名
 * @param line 行号
 * @param fmt 格式字符串
 * @param ... 参数
 */
AGENTOS_API void agentos_log_write(int level, const char* file, int line, const char* fmt, ...);

#define AGENTOS_LOG_ERROR(fmt, ...) agentos_log_write(AGENTOS_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_WARN(fmt, ...)  agentos_log_write(AGENTOS_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_INFO(fmt, ...)  agentos_log_write(AGENTOS_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_DEBUG(fmt, ...) agentos_log_write(AGENTOS_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DYNAMIC_AGENTOS_H */
