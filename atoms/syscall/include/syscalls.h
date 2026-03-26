/**
 * @file syscalls.h
 * @brief 内核系统调用接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_SYSCALL_H
#define AGENTOS_SYSCALL_H

// API 版本声明 (MAJOR.MINOR.PATCH)
#define SYSCALL_API_VERSION_MAJOR 1
#define SYSCALL_API_VERSION_MINOR 0
#define SYSCALL_API_VERSION_PATCH 0

// ABI 兼容性声明
// 在相同 MAJOR 版本内保证 ABI 兼容
// 破坏性更改需递增 MAJOR 并发布迁移说明

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
// From data intelligence emerges. by spharx

/* ==================== 系统初始化 ==================== */

/**
 * @brief 初始化系统调用模块，设置底层引擎句柄
 * @param cognition 认知引擎句柄
 * @param execution 执行引擎句柄
 * @param memory 记忆引擎句柄
 */
AGENTOS_API void agentos_sys_init(void* cognition, void* execution, void* memory);

/* ==================== 任务管理 ==================== */

/**
 * @brief 提交一个自然语言任务，同步等待执行完成
 * @param input 输入文本
 * @param input_len 文本长度
 * @param timeout_ms 超时（毫秒，0表示无限）
 * @param out_result 输出结果（JSON字符串，需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_submit(const char* input, size_t input_len,
                                        uint32_t timeout_ms, char** out_result);

/**
 * @brief 查询任务状态（通过任务ID）
 * @param task_id 任务ID
 * @param out_status 输出状态（0 pending, 1 running, 2 succeeded, 3 failed, 4 cancelled）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_query(const char* task_id, int* out_status);

/**
 * @brief 等待指定任务完成并获取结果
 * @param task_id 任务ID
 * @param timeout_ms 超时（毫秒，0无限）
 * @param out_result 输出结果（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_wait(const char* task_id, uint32_t timeout_ms, char** out_result);

/**
 * @brief 取消任务
 * @param task_id 任务ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_task_cancel(const char* task_id);

/* ==================== 记忆管理 ==================== */

/**
 * @brief 写入原始记忆
 * @param data 数据
 * @param len 数据长度
 * @param metadata JSON元数据（可为NULL）
 * @param out_record_id 输出记录ID（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_write(const void* data, size_t len,
                                         const char* metadata, char** out_record_id);

/**
 * @brief 查询记忆（语义搜索）
 * @param query 查询文本
 * @param limit 最大结果数
 * @param out_record_ids 输出记录ID数组（需调用者释放）
 * @param out_scores 输出得分数组（需调用者释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_search(const char* query, uint32_t limit,
                                          char*** out_record_ids, float** out_scores,
                                          size_t* out_count);

/**
 * @brief 获取记忆原始数据
 * @param record_id 记录ID
 * @param out_data 输出数据（需调用者释放）
 * @param out_len 输出长度
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_get(const char* record_id, void** out_data, size_t* out_len);

/**
 * @brief 删除记忆（永久移除）
 * @param record_id 记录ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_memory_delete(const char* record_id);

/* ==================== 会话管理 ==================== */

/**
 * @brief 创建新会话
 * @param metadata JSON元数据（可为NULL）
 * @param out_session_id 输出会话ID（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_create(const char* metadata, char** out_session_id);

/**
 * @brief 获取会话信息
 * @param session_id 会话ID
 * @param out_info 输出JSON信息（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_get(const char* session_id, char** out_info);

/**
 * @brief 关闭会话
 * @param session_id 会话ID
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_close(const char* session_id);

/**
 * @brief 列出所有活跃会话
 * @param out_sessions 输出会话ID数组（需调用者释放）
 * @param out_count 输出数量
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_session_list(char*** out_sessions, size_t* out_count);

/* ==================== 可观测性 ==================== */

/**
 * @brief 获取系统指标
 * @param out_metrics 输出JSON指标（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_telemetry_metrics(char** out_metrics);

/**
 * @brief 获取追踪数据
 * @param out_traces 输出JSON追踪（需调用者释放）
 * @return agentos_error_t
 */
AGENTOS_API agentos_error_t agentos_sys_telemetry_traces(char** out_traces);

/* ==================== 内部系统调用入口 ==================== */

/**
 * @brief 系统调用入口（供底层调用）
 * @param syscall_num 系统调用号
 * @param args 参数数组
 * @param argc 参数数量
 * @return 结果指针（通常为整数错误码或数据指针）
 */
void* agentos_syscall_invoke(int syscall_num, void** args, int argc);

/* ==================== 安全沙箱接口 ==================== */

/**
 * @brief 沙箱句柄
 */
typedef struct agentos_sandbox agentos_sandbox_t;

/**
 * @brief 沙箱配置结构
 */
typedef struct sandbox_config {
    char* sandbox_name;           /**< 沙箱名称 */
    char* owner_id;               /**< 所有者ID */
    uint32_t priority;            /**< 优先级 */
    uint32_t timeout_ms;          /**< 超时时间 */
    uint32_t flags;               /**< 标志位 */
    struct {
        uint64_t max_memory_bytes;    /**< 最大内存 */
        uint64_t current_memory;      /**< 当前内存使用 */
        uint64_t max_cpu_time_ms;     /**< 最大CPU时间 */
        uint64_t current_cpu_time_ms; /**< 当前CPU时间 */
        uint64_t max_io_ops;          /**< 最大I/O操作 */
        uint64_t current_io_ops;      /**< 当前I/O操作 */
        uint32_t max_file_size;       /**< 最大文件大小（MB） */
        uint32_t max_network_bytes;   /**< 最大网络传输（MB） */
    } quota;
} sandbox_config_t;

/**
 * @brief 权限类型枚举
 */
typedef enum {
    PERM_ALLOW = 0,    /**< 允许 */
    PERM_DENY,         /**< 拒绝 */
    PERM_ASK           /**< 需确认 */
} permission_type_t;

/**
 * @brief 初始化沙箱管理器
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_sandbox_manager_init(void);

/**
 * @brief 销毁沙箱管理器
 */
AGENTOS_API void agentos_sandbox_manager_destroy(void);

/**
 * @brief 创建沙箱
 * @param config 沙箱配置
 * @param out_sandbox 输出沙箱句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_sandbox_create(const sandbox_config_t* config,
                                                   agentos_sandbox_t** out_sandbox);

/**
 * @brief 销毁沙箱
 * @param sandbox 沙箱句柄
 */
AGENTOS_API void agentos_sandbox_destroy(agentos_sandbox_t* sandbox);

/**
 * @brief 在沙箱中执行系统调用
 * @param sandbox 沙箱句柄
 * @param syscall_num 系统调用号
 * @param args 参数数组
 * @param argc 参数数量
 * @param out_result 输出结果
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_sandbox_invoke(agentos_sandbox_t* sandbox,
                                                   int syscall_num,
                                                   void** args,
                                                   int argc,
                                                   void** out_result);

/**
 * @brief 添加权限规则
 * @param sandbox 沙箱句柄
 * @param syscall_num 系统调用号（-1表示所有）
 * @param perm_type 权限类型
 * @param condition 条件表达式
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_sandbox_add_rule(agentos_sandbox_t* sandbox,
                                                     int syscall_num,
                                                     permission_type_t perm_type,
                                                     const char* condition);

/**
 * @brief 获取沙箱统计信息
 * @param sandbox 沙箱句柄
 * @param out_stats 输出统计JSON
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_sandbox_get_stats(agentos_sandbox_t* sandbox, char** out_stats);

/**
 * @brief 获取管理器统计信息
 * @param out_stats 输出统计JSON
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_sandbox_manager_get_stats(char** out_stats);

/**
 * @brief 重置沙箱资源配额
 * @param sandbox 沙箱句柄
 */
AGENTOS_API void agentos_sandbox_reset_quota(agentos_sandbox_t* sandbox);

/**
 * @brief 暂停沙箱
 * @param sandbox 沙箱句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_sandbox_suspend(agentos_sandbox_t* sandbox);

/**
 * @brief 恢复沙箱
 * @param sandbox 沙箱句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_sandbox_resume(agentos_sandbox_t* sandbox);

/**
 * @brief 终止沙箱
 * @param sandbox 沙箱句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_sandbox_terminate(agentos_sandbox_t* sandbox);

/**
 * @brief 健康检查
 * @param sandbox 沙箱句柄
 * @param out_json 输出健康状态JSON
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_sandbox_health_check(agentos_sandbox_t* sandbox, char** out_json);

/* ==================== 熔断器接口 ==================== */

/**
 * @brief 熔断器句柄
 */
typedef struct agentos_circuit_breaker agentos_circuit_breaker_t;

/**
 * @brief 熔断器状态枚举
 */
typedef enum {
    CB_STATE_CLOSED = 0,    /**< 关闭状态（正常） */
    CB_STATE_OPEN,          /**< 打开状态（熔断） */
    CB_STATE_HALF_OPEN      /**< 半开状态（探测） */
} circuit_breaker_state_t;

/**
 * @brief 熔断器配置
 */
typedef struct cb_config {
    char* name;                       /**< 熔断器名称 */
    uint32_t failure_threshold;       /**< 失败阈值 */
    uint32_t success_threshold;       /**< 成功阈值 */
    uint32_t timeout_ms;              /**< 超时时间 */
    uint32_t window_size;             /**< 滑动窗口大小 */
    uint32_t half_open_requests;      /**< 半开探测数量 */
    uint32_t flags;                   /**< 标志位 */
} cb_config_t;

/**
 * @brief 初始化熔断器管理器
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_circuit_breaker_manager_init(void);

/**
 * @brief 销毁熔断器管理器
 */
AGENTOS_API void agentos_circuit_breaker_manager_destroy(void);

/**
 * @brief 创建熔断器
 * @param name 熔断器名称
 * @param config 配置（可为NULL使用默认值）
 * @param out_cb 输出熔断器句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_circuit_breaker_create(const char* name,
                                                           const cb_config_t* config,
                                                           agentos_circuit_breaker_t** out_cb);

/**
 * @brief 销毁熔断器
 * @param cb 熔断器句柄
 */
AGENTOS_API void agentos_circuit_breaker_destroy(agentos_circuit_breaker_t* cb);

/**
 * @brief 检查熔断器是否允许调用
 * @param cb 熔断器句柄
 * @return 1允许，0拒绝
 */
AGENTOS_API int agentos_circuit_breaker_allow(agentos_circuit_breaker_t* cb);

/**
 * @brief 记录成功
 * @param cb 熔断器句柄
 */
AGENTOS_API void agentos_circuit_breaker_success(agentos_circuit_breaker_t* cb);

/**
 * @brief 记录失败
 * @param cb 熔断器句柄
 */
AGENTOS_API void agentos_circuit_breaker_failure(agentos_circuit_breaker_t* cb);

/**
 * @brief 强制打开熔断器
 * @param cb 熔断器句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_circuit_breaker_force_open(agentos_circuit_breaker_t* cb);

/**
 * @brief 强制关闭熔断器
 * @param cb 熔断器句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_circuit_breaker_force_close(agentos_circuit_breaker_t* cb);

/**
 * @brief 获取熔断器状态
 * @param cb 熔断器句柄
 * @return 熔断器状态
 */
AGENTOS_API circuit_breaker_state_t agentos_circuit_breaker_state(agentos_circuit_breaker_t* cb);

/**
 * @brief 获取熔断器统计信息
 * @param cb 熔断器句柄
 * @param out_stats 输出统计JSON
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_circuit_breaker_stats(agentos_circuit_breaker_t* cb, char** out_stats);

/**
 * @brief 设置降级函数
 * @param cb 熔断器句柄
 * @param fallback_fn 降级函数
 * @param data 降级数据
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_circuit_breaker_set_fallback(agentos_circuit_breaker_t* cb,
                                                                 agentos_error_t (*fallback_fn)(void*),
                                                                 void* data);

/**
 * @brief 执行降级
 * @param cb 熔断器句柄
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_circuit_breaker_fallback(agentos_circuit_breaker_t* cb);

/**
 * @brief 获取管理器统计信息
 * @param out_stats 输出统计JSON
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_circuit_breaker_manager_stats(char** out_stats);

/**
 * @brief 重置熔断器统计
 * @param cb 熔断器句柄
 */
AGENTOS_API void agentos_circuit_breaker_reset_stats(agentos_circuit_breaker_t* cb);

/**
 * @brief 健康检查
 * @param cb 熔断器句柄
 * @param out_json 输出健康状态JSON
 * @return AGENTOS_SUCCESS成功，其他为错误码
 */
AGENTOS_API agentos_error_t agentos_circuit_breaker_health_check(agentos_circuit_breaker_t* cb, char** out_json);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SYSCALL_H */