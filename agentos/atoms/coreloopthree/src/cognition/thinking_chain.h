/**
 * @file thinking_chain.h
 * @brief 思考链路模块 - DS-001: Context Window管理+推理依赖链+Working Memory
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * 设计依据: AgentOS双思考系统全方位设计01.md
 * - Phase 0: 指令拆解(S1) → Context Window初始化
 * - Phase 2: 执行-验证循环(流式批判) → Thinking Step链式管理
 * - Working Memory: 短期上下文缓存，支持跨步骤信息传递
 */

#ifndef AGENTOS_THINKING_CHAIN_H
#define AGENTOS_THINKING_CHAIN_H

#include "agentos.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_thinking_chain agentos_thinking_chain_t;
typedef struct agentos_thinking_step agentos_thinking_step_t;
typedef struct agentos_working_memory agentos_working_memory_t;
typedef struct agentos_context_window agentos_context_window_t;
typedef struct agentos_memory_engine agentos_memory_engine_t;

/* ==================== 常量定义 ==================== */

#define TC_MAX_TOKENS_DEFAULT       8192    /**< 默认Context Window最大token数 */
#define TC_CHUNK_SIZE_DEFAULT        15      /**< 默认流式分块大小(token) */
#define TC_MAX_CORRECTIONS_DEFAULT   3       /**< 默认每块最大修正次数 */
#define TC_WORKING_MEM_CAPACITY      64      /**< Working Memory默认容量(条目) */
#define TC_MAX_THINKING_STEPS        256     /**< 最大思考步骤数 */
#define TC_SENTENCE_BOUNDARY_CHARS   256     /**< 句子边界检测最大回溯字符数 */

/* ==================== 枚举类型 ==================== */

/**
 * @brief 思考步骤类型（对应双思考系统Phase 0-4）
 */
typedef enum {
    TC_STEP_DECOMPOSITION = 0,    /**< Phase 0: 指令拆解 (S1) */
    TC_STEP_PLANNING = 1,         /**< Phase 1: 规划 (S2+S1) */
    TC_STEP_GENERATION = 2,       /**< Phase 2: 内容生成 (S2) */
    TC_STEP_VERIFICATION = 3,     /**< Phase 2: 验证检查 (S1) */
    TC_STEP_CORRECTION = 4,        /**< Phase 2: 修正 (S2) */
    TC_STEP_AUDIT = 5,             /**< Phase 3: 子任务审计 (S1+专家S1) */
    TC_STEP_ALIGNMENT = 6          /**< Phase 4: 目标对齐检查 */
} tc_step_type_t;

/**
 * @brief 思考步骤状态
 */
typedef enum {
    TC_STATUS_PENDING = 0,         /**< 待执行 */
    TC_STATUS_EXECUTING = 1,       /**< 执行中 */
    TC_STATUS_COMPLETED = 2,       /**< 已完成 */
    TC_STATUS_CORRECTED = 3,       /**< 已修正 */
    TC_STATUS_FAILED = 4,          /**< 失败 */
    TC_STATUS_SKIPPED = 5          /**< 跳过（如修正失败） */
} tc_step_status_t;

/* ==================== DS-005/006: 注意力分配类型（前向声明） ==================== */

typedef struct {
    float decomposition_weight;
    float planning_weight;
    float generation_weight;
    float verification_weight;
    float audit_weight;
    float alignment_weight;
} tc_attention_weights_t;

#define TC_ATTENTION_DEFAULTS { \
    .base_tokens = 8192, \
    .weights = { .decomposition_weight = 0.15f, .planning_weight = 0.20f, \
                 .generation_weight = 0.30f, .verification_weight = 0.15f, \
                 .audit_weight = 0.12f, .alignment_weight = 0.08f }, \
    .pressure_threshold = 0.80f, .enable_dynamic_adjustment = 1, \
    .min_step_tokens = 256, .max_step_tokens = 4096 }

/**
 * @brief 验证结果
 */
typedef enum {
    TC_VERIFY_ACCEPT = 0,           /**< 通过，无需修正 */
    TC_VERIFY_MINOR_FIX = 1,       /**< 小问题，自动修正 */
    TC_VERIFY_MAJOR_FIX = 2,       /**< 大问题，需S2重写 */
    TC_VERIFY_REJECT = 3            /**< 拒绝，无法修正 */
} tc_verify_result_t;

/* ==================== 数据结构 ==================== */

/**
 * @brief 单个思考步骤（链表节点）
 *
 * 每个步骤代表双思考系统中的一次推理操作，
 * 步骤之间通过显式依赖关系链接形成有向无环图(DAG)。
 */
struct agentos_thinking_step {
    uint32_t step_id;                       /**< 全局唯一步骤ID */
    tc_step_type_t type;                     /**< 步骤类型 */
    tc_step_status_t status;                /**< 当前状态 */

    /* 内容区 */
    char* content;                           /**< 步骤输出内容（token文本） */
    size_t content_len;                      /**< 内容长度 */
    char* raw_input;                         /**< 原始输入提示 */
    size_t raw_input_len;                    /**< 输入长度 */

    /* 依赖关系 */
    uint32_t* depends_on;                    /**< 依赖的步骤ID数组 */
    size_t depends_count;                   /**< 依赖数量 */
    uint32_t* dependents;                    /**< 被哪些步骤依赖 */
    size_t dependents_count;                 /**< 被依赖数量 */

    /* 验证与修正（Phase 2 流式批判） */
    tc_verify_result_t verify_result;        /**< S1验证结果 */
    char* critique;                          /**< S1批判意见 */
    size_t critique_len;                     /**< 批判长度 */
    int correction_count;                    /**< 已执行修正次数 */
    char** correction_history;               /**< 修正历史记录 */
    size_t correction_history_count;        /**< 修正历史条数 */
    float confidence;                        /**< 该步骤置信度 (0.0-1.0) */

    /* 元数据 */
    uint64_t start_time_ns;                  /**< 开始时间戳 */
    uint64_t end_time_ns;                    /**< 结束时间戳 */
    uint32_t token_count;                    /**< 产出token数 */
    char* role;                              /**< 执行角色 (S1/S2/专家S1/审计) */

    /* 内部引用（用于回调通知） */
    struct agentos_thinking_chain* chain_ref;  /**< 所属思考链路（内部使用） */
};

/**
 * @brief Context Window（上下文窗口）
 *
 * 管理当前推理会话的token预算，
 * 实现滑动窗口机制以保持最近N个token在视野内。
 */
struct agentos_context_window {
    /* Token预算 */
    size_t max_tokens;                       /**< 最大token容量 */
    size_t used_tokens;                      /**< 已使用token数 */
    size_t chunk_size;                       /**< 分块大小（用于流式批判） */

    /* 滑动窗口内容 */
    char* buffer;                            /**< 环形缓冲区 */
    size_t buffer_capacity;                  /**< 缓冲区字节容量 */
    size_t buffer_head;                      /**< 写入位置 */
    size_t buffer_tail;                      /**< 读取位置 */
    size_t buffer_used;                      /**< 当前已用字节数 */

    /* 统计 */
    uint64_t total_tokens_generated;         /**< 累计生成token数 */
    uint64_t total_corrections;              /**< 累计修正次数 */
    uint32_t total_steps;                    /**< 总步骤数 */
    uint32_t completed_steps;                /**< 已完成步骤数 */

    /* 配置 */
    int enable_dynamic_chunk;                /**< 是否启用动态分块 */
    float low_confidence_threshold;          /**< 低置信度阈值(减小分块) */
    float high_confidence_threshold;         /**< 高置信度阈值(增大分块) */
    int max_corrections_per_chunk;           /**< 每块最大修正次数 */
};

/**
 * @brief Working Memory（工作记忆）
 *
 * 短期记忆缓存，存储当前推理过程中的中间结果。
 * 支持按key存取，用于跨步骤信息传递。
 * 对应双思考设计文档中的"Working Memory"组件。
 */
struct agentos_working_memory {
    /* 键值存储 */
    struct wm_entry {
        char* key;                            /**< 键名 */
        void* value;                          /**< 值（任意类型） */
        size_t value_size;                    /**< 值大小 */
        char* type;                           /**< 类型标识字符串 */
        uint64_t created_ns;                  /**< 创建时间 */
        uint64_t last_accessed_ns;            /**< 最后访问时间 */
        uint32_t access_count;                /**< 访问次数 */
        int pinned;                           /**< 是否锁定不被淘汰 */
    }* entries;                               /**< 条目数组 */
    size_t capacity;                          /**< 最大容量 */
    size_t count;                             /**< 当前条目数 */

    /* LRU索引（简化实现：按访问时间排序） */
    uint32_t* lru_order;                      /**< LRU顺序（索引到entries） */
    size_t lru_index;                         /**< 当前LRU写入位置 */

    /* 统计 */
    uint64_t hits;                            /**< 缓存命中数 */
    uint64_t misses;                          /**< 缓存未命中数 */
    uint64_t evictions;                       /**< 淘汰数 */
};

typedef struct {
    size_t base_tokens;
    tc_attention_weights_t weights;
    float pressure_threshold;
    int enable_dynamic_adjustment;
    uint32_t min_step_tokens;
    uint32_t max_step_tokens;
} tc_attention_config_t;

/**
 * @brief 思考链路（完整结构）
 *
 * 将Context Window + Thinking Steps + Working Memory
 * 整合为统一的双思考推理管线。
 */
struct agentos_thinking_chain {
    uint64_t session_id;                      /**< 会话唯一ID */
    char* session_goal;                       /**< 会话目标描述 */
    agentos_context_window_t* ctx_window;     /**< Context Window */
    agentos_working_memory_t* working_mem;    /**< Working Memory */
    agentos_memory_engine_t* memory;          /**< MemoryRovol 引用 (P2-B03) */

    /* 思考步骤DAG */
    agentos_thinking_step_t* steps;           /**< 步骤数组 */
    size_t step_capacity;                     /**< 数组容量 */
    size_t step_count;                        /**< 当前步骤数 */
    uint32_t next_step_id;                    /**< 下一步ID分配器 */

    /* 链路状态 */
    int active;                               /**< 是否活跃 */
    uint64_t created_ns;                      /**< 创建时间 */
    uint64_t last_activity_ns;                /**< 最后活动时间 */

    /* 回调函数 */
    void (*on_step_completed)(agentos_thinking_step_t* step, void* user_data);
    void (*on_correction)(agentos_thinking_step_t* step, const char* critique, void* user_data);
    void* callback_user_data;

    /* DS-005: 注意力分配配置 */
    tc_attention_config_t attention_config;
    int attention_configured;
};

/* ==================== Context Window API ==================== */

/**
 * @brief 创建Context Window
 * @param max_tokens 最大token容量（0使用默认值8192）
 * @param out_window 输出句柄
 * @return AGENTOS_SUCCESS 或错误码
 */
AGENTOS_API agentos_error_t agentos_tc_context_window_create(
    size_t max_tokens,
    agentos_context_window_t** out_window);

/**
 * @brief 销毁Context Window
 */
AGENTOS_API void agentos_tc_context_window_destroy(agentos_context_window_t* window);

/**
 * @brief 向Context Window追加内容
 * @return 追加后的总token数估算，或错误码(<0时)
 */
AGENTOS_API ssize_t agentos_tc_context_window_append(
    agentos_context_window_t* window,
    const char* data,
    size_t len);

/**
 * @brief 获取Context Window中最近的N个token
 * @param token_count 请求的token数（0=全部）
 * @param out_data 输出数据（调用者释放）
 * @param out_len 输出长度
 */
AGENTOS_API agentos_error_t agentos_tc_context_window_get_recent(
    agentos_context_window_t* window,
    size_t token_count,
    char** out_data,
    size_t* out_len);

/**
 * @brief 检查Context Window是否有足够空间
 * @param needed_tokens 需要的token数
 * @return 1=有空间, 0=空间不足
 */
AGENTOS_API int agentos_tc_context_window_has_space(
    agentos_context_window_t* window,
    size_t needed_tokens);

/**
 * @brief 获取Context Window统计信息
 */
AGENTOS_API agentos_error_t agentos_tc_context_window_stats(
    agentos_context_window_t* window,
    char** out_json);

/* ==================== Working Memory API ==================== */

/**
 * @brief 创建Working Memory
 * @param capacity 最大条目数（0使用默认值64）
 */
AGENTOS_API agentos_error_t agentos_tc_working_memory_create(
    size_t capacity,
    agentos_working_memory_t** out_mem);

/**
 * @brief 销毁Working Memory
 */
AGENTOS_API void agentos_tc_working_memory_destroy(agentos_working_memory_t* mem);

/**
 * @brief 存储键值对到Working Memory
 * @param key 键名（不能为NULL）
 * @param value 值数据（会被拷贝）
 * @param value_size 值大小
 * @param type 类型标识（可为NULL）
 * @param pin 是否锁定不被LRU淘汰
 */
AGENTOS_API agentos_error_t agentos_tc_working_memory_store(
    agentos_working_memory_t* mem,
    const char* key,
    const void* value,
    size_t value_size,
    const char* type,
    int pin);

/**
 * @brief 从Working Memory检索值
 * @param key 键名
 * @param out_value 输出值（调用者不负责释放，下次store/destroy失效）
 * @param out_size 输出大小
 * @return AGENTOS_SUCCESS 或 AGENTOS_ENOTFOUND
 */
AGENTOS_API agentos_error_t agentos_tc_working_memory_retrieve(
    agentos_working_memory_t* mem,
    const char* key,
    void** out_value,
    size_t* out_size);

/**
 * @brief 从Working Memory移除条目
 */
AGENTOS_API agentos_error_t agentos_tc_working_memory_remove(
    agentos_working_memory_t* mem,
    const char* key);

/**
 * @brief 清空Working Memory所有非锁定条目
 */
AGENTOS_API void agentos_tc_working_memory_clear_unpinned(agentos_working_memory_t* mem);

/* ==================== Thinking Step API ==================== */

/**
 * @brief 创建新的思考步骤
 * @param chain 所属思考链
 * @param type 步骤类型
 * @param input 输入提示
 * @param input_len 输入长度
 * @param depends_on 依赖的步骤ID数组（可为NULL）
 * @param depends_count 依赖数量
 * @param out_step 输出步骤指针（属于chain内部管理）
 */
AGENTOS_API agentos_error_t agentos_tc_step_create(
    agentos_thinking_chain_t* chain,
    tc_step_type_t type,
    const char* input,
    size_t input_len,
    const uint32_t* depends_on,
    size_t depends_count,
    agentos_thinking_step_t** out_step);

/**
 * @brief 标记步骤完成并设置输出内容
 * @param step 步骤指针
 * @param content 输出内容（会被拷贝）
 * @param content_len 内容长度
 * @param confidence 置信度 (0.0-1.0)
 * @param role 执行角色标识
 */
AGENTOS_API agentos_error_t agentos_tc_step_complete(
    agentos_thinking_step_t* step,
    const char* content,
    size_t content_len,
    float confidence,
    const char* role);

/**
 * @brief 对步骤执行S1验证（流式批判）
 * @param step 待验证步骤
 * @param is_valid 验证结果 (true=通过)
 * @param critique 批评意见（可为NULL表示无批评）
 * @param critique_len 批评长度
 */
AGENTOS_API agentos_error_t agentos_tc_step_verify(
    agentos_thinking_step_t* step,
    int* is_valid,
    const char* critique,
    size_t critique_len);

/**
 * @brief 对步骤应用修正
 * @param step 待修正步骤
 * @param corrected_content 修正后内容
 * @param corrected_len 内容长度
 */
AGENTOS_API agentos_error_t agentos_tc_step_correct(
    agentos_thinking_step_t* step,
    const char* corrected_content,
    size_t corrected_len);

/**
 * @brief 获取步骤的可执行性（所有依赖是否已完成）
 * @return 1=可执行, 0=有未完成依赖, -1=错误
 */
AGENTOS_API int agentos_tc_step_is_ready(const agentos_thinking_step_t* step,
                                         const agentos_thinking_chain_t* chain);

/* ==================== Thinking Chain API ==================== */

/**
 * @brief 创建思考链路实例
 * @param goal 会话目标描述
 * @param max_tokens Context Window最大token数（0=默认）
 * @param wm_capacity Working Memory容量（0=默认）
 * @param out_chain 输出句柄
 */
AGENTOS_API agentos_error_t agentos_tc_chain_create(
    const char* goal,
    size_t max_tokens,
    size_t wm_capacity,
    agentos_thinking_chain_t** out_chain);

/**
 * @brief 销毁思考链路及其所有子组件
 */
AGENTOS_API void agentos_tc_chain_destroy(agentos_thinking_chain_t* chain);

/**
 * @brief 启动思考链路（标记活跃，初始化统计）
 */
AGENTOS_API agentos_error_t agentos_tc_chain_start(agentos_thinking_chain_t* chain);

/**
 * @brief 停止思考链路
 */
AGENTOS_API void agentos_tc_chain_stop(agentos_thinking_chain_t* chain);

/**
 * @brief 获取下一个可执行的思考步骤
 * @return AGENTOS_SUCCESS且有步骤, AGENTOS_ENOENT(无可执行步骤), 或其他错误
 */
AGENTOS_API agentos_error_t agentos_tc_chain_next_ready_step(
    agentos_thinking_chain_t* chain,
    agentos_thinking_step_t** out_step);

/**
 * @brief 获取思考链路的完整执行统计（JSON格式）
 */
AGENTOS_API agentos_error_t agentos_tc_chain_stats(
    agentos_thinking_chain_t* chain,
    char** out_json,
    size_t* out_len);

/**
 * @brief 设置步骤完成回调
 */
AGENTOS_API void agentos_tc_chain_set_step_callback(
    agentos_thinking_chain_t* chain,
    void (*on_step_completed)(agentos_thinking_step_t*, void*),
    void (*on_correction)(agentos_thinking_step_t*, const char*, void*),
    void* user_data);

/* ==================== MemoryRovol 集成API (P2-B03) ==================== */

AGENTOS_API void agentos_tc_chain_set_memory(
    agentos_thinking_chain_t* chain,
    agentos_memory_engine_t* memory);

AGENTOS_API agentos_error_t agentos_tc_context_window_prepopulate(
    agentos_thinking_chain_t* chain,
    const char* query_text,
    size_t query_len,
    uint32_t limit);

AGENTOS_API agentos_error_t agentos_tc_working_memory_sync_to_persistent(
    agentos_thinking_chain_t* chain,
    float min_importance);

AGENTOS_API agentos_error_t agentos_tc_step_write_to_memory(
    agentos_thinking_chain_t* chain,
    agentos_thinking_step_t* step);

AGENTOS_API agentos_error_t agentos_tc_metacognition_inform_memory(
    agentos_thinking_chain_t* chain,
    const void* eval,
    agentos_thinking_step_t* step);

/* ==================== DS-007: 执行监控API ==================== */

/**
 * @brief 异常类型枚举
 */
typedef enum {
    TC_ANOMALY_NONE = 0,            /**< 正常 */
    TC_ANOMALY_TIMEOUT,             /**< 超时 */
    TC_ANOMALY_EMPTY_OUTPUT,        /**< 空输出 */
    TC_ANOMALY_TRUNCATED_OUTPUT,    /**< 截断输出（过短） */
    TC_ANOMALY_EXCESSIVE_OUTPUT,    /**< 过长输出 */
    TC_ANOMALY_REPETITIVE_CONTENT,  /**< 重复内容 */
    TC_ANOMALY_CONFIDENCE_DROP,     /**< 置信度骤降 */
    TC_ANOMALY_DEPENDENCY_CYCLE     /**< 依赖循环 */
} tc_anomaly_type_t;

/**
 * @brief 步骤执行监控结果
 */
typedef struct {
    tc_anomaly_type_t anomaly;      /**< 检测到的异常类型 */
    int is_critical;                 /**< 是否严重异常(需立即处理) */
    char* description;               /**< 异常描述文本 */
    size_t description_len;
    float severity_score;            /**< 严重程度评分 (0.0-1.0) */
} tc_monitor_result_t;

/**
 * @brief 执行监控器配置
 */
typedef struct {
    uint32_t default_timeout_ms;     /**< 默认超时时间(毫秒) */
    size_t min_output_chars;         /**< 最小输出字符数 */
    size_t max_output_chars;         /**< 最大输出字符数 */
    float repetition_threshold;      /**< 重复检测阈值 (0.0-1.0) */
    float confidence_drop_threshold;/**< 置信度骤降阈值 */
    int enable_quality_gate;         /**< 启用质量门禁 */
    float quality_gate_threshold;    /**< 质量门禁最低分数 */
} tc_monitor_config_t;

#define TC_MONITOR_DEFAULTS { .default_timeout_ms = 30000, .min_output_chars = 10, \
    .max_output_chars = 100000, .repetition_threshold = 0.7f, \
    .confidence_drop_threshold = 0.3f, .enable_quality_gate = 1, \
    .quality_gate_threshold = 0.5f }

/**
 * @brief 监控单个思考步骤的执行状态
 *
 * DS-007核心函数：检查步骤是否出现超时、空输出、重复、
 * 置信度骤降等异常行为。
 *
 * @param step 待监控的步骤
 * @param config 监控配置（NULL使用默认值）
 * @param out_result 输出监控结果
 * @return AGENTOS_SUCCESS 或错误码
 */
AGENTOS_API agentos_error_t agentos_tc_step_monitor(
    const agentos_thinking_step_t* step,
    const tc_monitor_config_t* config,
    tc_monitor_result_t* out_result);

/**
 * @brief 对整个链路进行健康检查
 *
 * 扫描所有已完成/失败的步骤，汇总异常情况。
 *
 * @param chain 思考链路
 * @param out_anomaly_count 输出异常数量
 * @param out_has_critical 是否存在严重异常
 * @return AGENTOS_SUCCESS 或错误码
 */
AGENTOS_API agentos_error_t agentos_tc_chain_health_check(
    const agentos_thinking_chain_t* chain,
    size_t* out_anomaly_count,
    int* out_has_critical);

/* ==================== DS-008: 异常恢复API ==================== */

/**
 * @brief 恢复策略枚举
 */
typedef enum {
    TC_RECOVER_RETRY = 0,           /**< 重试（相同参数） */
    TC_RECOVER_RETRY_WITH_HINT,     /**< 带提示重试 */
    TC_RECOVER_DEGRADE,             /**< 降级（简化输出） */
    TC_RECOVER_SKIP_AND_CONTINUE,   /**< 跳过并继续 */
    TC_RECOVER_ROLLBACK,            /**< 回滚到上一步 */
    TC_RECOVER_ABORT                /**< 中止整个链路 */
} tc_recovery_strategy_t;

/**
 * @brief 恢复操作结果
 */
typedef struct {
    tc_recovery_strategy_t strategy_used;  /**< 实际使用的恢复策略 */
    int success;                           /**< 恢复是否成功 */
    char* recovery_log;                    /**< 恢复日志 */
    size_t recovery_log_len;              /**< 恢复日志长度 */
    uint32_t attempts_made;                /**< 尝试次数 */
} tc_recovery_result_t;

/**
 * @brief 自动恢复失败的思考步骤
 *
 * DS-008核心函数：当步骤失败或被标记为异常时，
 * 按优先级尝试多种恢复策略：
 * 1. 重试（最多3次，指数退避）
 * 2. 带提示重试（注入上下文提示）
 * 3. 降级（接受较低质量输出）
 * 4. 回滚到上一个已知良好状态
 *
 * @param chain 思考链路
 * @param failed_step 失败的步骤
 * @param monitor_result 监控结果（可为NULL）
 * @param corrector_fn 纠错回调函数（用于重试时重新生成）
 * @param user_data 用户数据传给corrector_fn
 * @param out_result 输出恢复结果
 * @return AGENTOS_SUCCESS 或错误码
 */
AGENTOS_API agentos_error_t agentos_tc_step_recover(
    agentos_thinking_chain_t* chain,
    agentos_thinking_step_t* failed_step,
    const tc_monitor_result_t* monitor_result,
    agentos_error_t (*corrector_fn)(const char*, size_t, char**, size_t*, void*),
    void* user_data,
    tc_recovery_result_t* out_result);

/**
 * @brief 创建恢复检查点（保存当前状态快照）
 *
 * 在关键步骤完成后调用，用于后续回滚。
 *
 * @param chain 思考链路
 * @return 检查点ID (>0), 或0表示失败
 */
AGENTOS_API uint32_t agentos_tc_chain_checkpoint(agentos_thinking_chain_t* chain);

/**
 * @brief 回滚到指定检查点
 *
 * @param chain 思考链路
 * @param checkpoint_id 检查点ID
 * @return 成功删除的步骤数
 */
AGENTOS_API size_t agentos_tc_chain_rollback(
    agentos_thinking_chain_t* chain,
    uint32_t checkpoint_id);

/* ==================== DS-005/006: 注意力分配与动态预算API ==================== */

typedef struct {
    size_t allocated_tokens;
    float priority_score;
    float urgency_score;
    int is_elevated;
} tc_allocation_result_t;

AGENTOS_API agentos_error_t agentos_tc_set_attention_config(
    agentos_thinking_chain_t* chain,
    const tc_attention_config_t* config);

AGENTOS_API agentos_error_t agentos_tc_allocate_attention(
    agentos_thinking_chain_t* chain,
    agentos_thinking_step_t* step,
    tc_allocation_result_t* out_alloc);

AGENTOS_API agentos_error_t agentos_tc_adjust_dynamic_budget(
    agentos_thinking_chain_t* chain,
    tc_step_type_t step_type,
    float performance_score);

AGENTOS_API float agentos_tc_compute_priority(
    const agentos_thinking_step_t* step,
    const agentos_thinking_chain_t* chain);

AGENTOS_API agentos_error_t agentos_tc_wm_set_priority(
    agentos_working_memory_t* wm,
    const char* key,
    float priority);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_THINKING_CHAIN_H */
