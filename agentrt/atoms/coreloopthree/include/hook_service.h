/**
 * @file hook_service.h
 * @brief Hook 守护进程服务接口
 *
 * Hook daemon 管理全局 Hook 生命周期，提供 Hook 注册、
 * 触发、卸载和审计功能。支持 8 种 Hook 类型：
 *   - PRE_EXEC / POST_EXEC    执行前后
 *   - PRE_LLM / POST_LLM      LLM 调用前后
 *   - PRE_TOOL / POST_TOOL    工具调用前后
 *   - ON_ERROR                 错误发生时
 *   - ON_MEMORY_EVOLVE         记忆进化时
 *
 * @owner team-A
 * @see contracts/contract_A_B.h 第6节
 */

#ifndef AGENTRT_DAEMON_HOOK_D_HOOK_SERVICE_H
#define AGENTRT_DAEMON_HOOK_D_HOOK_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Hook 类型 ── */

typedef enum {
    HOOK_TYPE_PRE_EXEC       = 0,  /**< 执行前 */
    HOOK_TYPE_POST_EXEC      = 1,  /**< 执行后 */
    HOOK_TYPE_PRE_LLM        = 2,  /**< LLM 调用前 */
    HOOK_TYPE_POST_LLM       = 3,  /**< LLM 调用后 */
    HOOK_TYPE_PRE_TOOL       = 4,  /**< 工具调用前 */
    HOOK_TYPE_POST_TOOL      = 5,  /**< 工具调用后 */
    HOOK_TYPE_ON_ERROR       = 6,  /**< 错误发生时 */
    HOOK_TYPE_ON_MEMORY_EVOLVE = 7, /**< 记忆进化时 */
    HOOK_TYPE_COUNT          = 8   /**< Hook 类型总数 */
} hook_type_t;

/* ── Hook 决策 ── */

typedef enum {
    HOOK_DECISION_CONTINUE = 0,  /**< 继续（无干预） */
    HOOK_DECISION_SKIP     = 1,  /**< 跳过当前操作 */
    HOOK_DECISION_RETRY    = 2,  /**< 重试当前操作 */
    HOOK_DECISION_ABORT    = 3,  /**< 中止当前操作 */
    HOOK_DECISION_MODIFY   = 4   /**< 修改输入/输出 */
} hook_decision_t;

/* ── Hook 上下文 ── */

typedef struct {
    hook_type_t type;             /**< Hook 类型 */
    const char *hook_name;        /**< Hook 名称 */
    const char *source_daemon;    /**< 来源 daemon */
    const char *operation;        /**< 操作名称 */
    const void *input_data;       /**< 输入数据（只读） */
    size_t input_data_len;        /**< 输入数据长度 */
    void *output_data;            /**< 输出数据（HOOK_DECISION_MODIFY 时可写） */
    size_t output_data_len;       /**< 输出数据长度 */
    uint64_t timestamp_ns;        /**< 时间戳（纳秒） */
    char trace_id[64];            /**< 追踪 ID */
    char session_id[64];          /**< 会话 ID */
    void *user_data;              /**< 用户数据 */
} hook_context_t;

/* ── Hook 回调类型 ── */

/**
 * @brief Hook 回调函数
 * @param ctx Hook 上下文
 * @return Hook 决策
 *
 * @contract 回调内不执行阻塞操作（> 10ms）
 */
typedef hook_decision_t (*hook_callback_t)(hook_context_t *ctx);

/* ── Hook 注册信息 ── */

typedef struct {
    const char *name;             /**< Hook 名称（全局唯一） */
    hook_type_t type;             /**< Hook 类型 */
    hook_callback_t callback;     /**< 回调函数 */
    void *user_data;              /**< 用户数据 */
    int priority;                 /**< 优先级（数值越大越先执行） */
    bool enabled;                 /**< 是否启用 */
} hook_registration_t;

/* ── Hook 统计 ── */

typedef struct {
    uint64_t invoke_count;        /**< 调用次数 */
    uint64_t skip_count;          /**< 跳过次数 */
    uint64_t abort_count;         /**< 中止次数 */
    uint64_t retry_count;         /**< 重试次数 */
    uint64_t modify_count;        /**< 修改次数 */
    uint64_t total_duration_ns;   /**< 总耗时（纳秒） */
    uint64_t max_duration_ns;     /**< 最大耗时（纳秒） */
} hook_stats_t;

/* ── 服务 API ── */

/**
 * @brief 注册 Hook
 * @param reg Hook 注册信息
 * @return 0 成功，非0失败
 */
int hook_service_register(const hook_registration_t *reg);

/**
 * @brief 注销 Hook
 * @param name Hook 名称
 * @return 0 成功，非0失败
 */
int hook_service_unregister(const char *name);

/**
 * @brief 触发 Hook 链
 * @param ctx Hook 上下文
 * @return 聚合决策（最严格的决策优先级：ABORT > RETRY > MODIFY > SKIP > CONTINUE）
 */
hook_decision_t hook_service_fire(hook_context_t *ctx);

/**
 * @brief 获取 Hook 统计
 * @param name Hook 名称
 * @param stats 输出统计
 * @return 0 成功，非0失败
 */
int hook_service_get_stats(const char *name, hook_stats_t *stats);

/**
 * @brief 启用/禁用 Hook
 * @param name    Hook 名称
 * @param enabled 是否启用
 * @return 0 成功，非0失败
 */
int hook_service_set_enabled(const char *name, bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_DAEMON_HOOK_D_HOOK_SERVICE_H */
