/**
 * @file metacognition.h
 * @brief 元认知模块 - DS-002: 自我评估+纠错+置信度校准
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * 元认知(Metacognition)是Agent对自身思考过程的监控与调节能力。
 * 本模块实现双思考系统中的S1验证角色核心逻辑：
 * - 推理质量自评估（多维度评分）
 * - 错误检测与自动纠错
 * - 置信度校准（防止过度自信/不自信）
 * - 思考过程审计日志
 */

#ifndef AGENTOS_METACOGNITION_H
#define AGENTOS_METACOGNITION_H

#include "../../../atoms/corekern/include/agentos.h"
#include "thinking_chain.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct agentos_metacognition agentos_metacognition_t;
typedef struct mc_evaluation_record mc_evaluation_record_t;

/* ==================== 常量 ==================== */

#define MC_MAX_HISTORY_RECORDS    256     /**< 评估历史最大记录数 */
#define MC_DEFAULT_CONFIDENCE     0.5f    /**< 默认置信度 */
#define MC_CALIBRATION_WINDOW     32      /**< 置信度校准滑动窗口大小 */
#define MC_MAX_CRITIQUE_LEN       4096    /**< 最大批判意见长度 */

/* ==================== 枚举 ==================== */

/**
 * @brief 评估维度
 */
typedef enum {
    MC_DIM_RELEVANCE = 0,        /**< 相关性：输出是否回应了输入问题 */
    MC_DIM_ACCURACY = 1,         /**< 准确性：事实和推理是否正确 */
    MC_DIM_COMPLETENESS = 2,     /**< 完整性：是否覆盖所有必要方面 */
    MC_DIM_CONSISTENCY = 3,      /**< 一致性：是否与之前的结论一致 */
    MC_DIM_CLARITY = 4,          /**< 清晰度：表达是否清晰无歧义 */
    MC_DIM_COUNT = 5             /**< 维度总数 */
} mc_dimension_t;

/**
 * @brief 纠错策略
 */
typedef enum {
    MC_CORRECT_NONE = 0,         /**< 无需修正 */
    MC_CORRECT_AUTO = 1,         /**< 自动修正（小错误） */
    MC_CORRECT_RERUN = 2,        /**< 重新生成（大错误） */
    MC_CORRECT_ESCALATE = 3      /**< 上报人工介入 */
} mc_correction_strategy_t;

/**
 * @brief 严重程度
 */
typedef enum {
    MC_SEV_INFO = 0,
    MC_SEV_WARNING = 1,
    MC_SEV_ERROR = 2,
    MC_SEV_CRITICAL = 3
} mc_severity_t;

/* ==================== 数据结构 ==================== */

/**
 * @brief 单维度评估结果
 */
typedef struct {
    mc_dimension_t dimension;     /**< 评估维度 */
    float score;                  /**< 评分 (0.0-1.0) */
    const char* reason;           /**< 评分理由 */
} mc_dimension_score_t;

/**
 * @brief 综合评估结果
 */
typedef struct {
    float overall_score;          /**< 综合评分 (0.0-1.0) */
    mc_dimension_score_t dimensions[MC_DIM_COUNT]; /**< 各维度评分 */
    int is_acceptable;            /**< 是否可接受 (综合>=阈值) */
    char* critique_text;          /**< 批判意见文本 */
    size_t critique_len;          /**< 批判长度 */
    mc_correction_strategy_t strategy; /**< 建议的纠正策略 */
    mc_severity_t severity;       /**< 问题严重程度 */
    float calibrated_confidence;  /**< 校准后的置信度 */
} mc_evaluation_result_t;

/**
 * @brief 评估历史记录
 */
struct mc_evaluation_record {
    uint32_t step_id;             /**< 关联的思考步骤ID */
    uint64_t timestamp_ns;        /**< 评估时间戳 */
    mc_evaluation_result_t result;/**< 评估结果 */
    const char* original_content; /**< 被评估的原始内容 */
    const char* corrected_content;/**< 修正后内容（如有） */
};

/**
 * @brief 置信度校准器状态
 */
typedef struct {
    float calibration_sum;        /**< (预测概率-实际结果)累计偏差 */
    size_t calibration_count;     /**< 校准样本数 */
    float last_calibration_error; /**< 最近一次校准误差 */
    float overconfidence_rate;    /**< 过度自信率 */
    float underconfidence_rate;   /**< 自信不足率 */
    struct { float predicted; float actual; } history[MC_CALIBRATION_WINDOW];
    size_t history_index;         /**< 历史写入位置 */
} mc_calibrator_t;

/* ==================== DS-005: 持久学习类型（需在struct之前声明） ==================== */

/**
 * @brief 错误模式条目（用于持久学习）
 */
typedef struct mc_error_pattern {
    char pattern_key[128];           /**< 模式特征键 (如"math_query", "long_context") */
    uint64_t occurrence_count;       /**< 出现次数 */
    uint64_t failure_count;          /**< 失败次数 */
    float failure_rate;              /**< 失败率 (0.0-1.0) */
    mc_correction_strategy_t best_strategy; /**< 最有效的纠正策略 */
    float strategy_success_rate;     /**< 策略成功率 */
    uint64_t last_seen_ns;           /**< 最后出现时间 */
    int is_active;                   /**< 是否活跃 (近期有出现) */
} mc_error_pattern_t;

#define MC_MAX_PATTERNS  32          /**< 最大错误模式数 */

/**
 * @brief 元认知引擎（含DS-005持久学习扩展）
 */
struct agentos_metacognition {
    /* 配置参数 */
    float acceptance_threshold;   /**< 可接受性阈值 (默认0.7) */
    float auto_correct_threshold;/**< 自动修正阈值 (默认0.5) */
    int enable_confidence_calibration; /**< 启用置信度校准 */
    int enable_learning;         /**< 启用自适应学习 */

    /* 评估历史 */
    mc_evaluation_record_t* records; /**< 评估记录环形缓冲区 */
    size_t record_capacity;      /**< 记录容量 */
    size_t record_count;         /**< 当前记录数 */
    size_t record_head;          /**< 写入位置 */

    /* 置信度校准器 */
    mc_calibrator_t calibrator;

    /* 统计 */
    uint64_t total_evaluations;   /**< 总评估次数 */
    uint64_t total_corrections;  /**< 总修正次数 */
    uint64_t total_rejections;   /**< 总拒绝次数 */
    uint64_t total_auto_fixes;   /**< 总自动修正次数 */
    uint64_t total_rerun_successes; /**< RERUN策略成功次数 */

    /* 关联的思考链路 */
    agentos_thinking_chain_t* chain; /**< 可选关联（用于回调通知） */

    /* DS-005: 持久学习扩展 */
    mc_error_pattern_t patterns[MC_MAX_PATTERNS];
    size_t pattern_count;
    float adaptive_acceptance_threshold;
    size_t consecutive_accepts;
    size_t consecutive_rejects;
    uint64_t patterns_detected;
    uint64_t preemptive_corrections;
    float learning_effectiveness;
};

/* ==================== 核心API ==================== */

/**
 * @brief 创建元认知引擎实例
 * @param out_mc 输出句柄
 * @return AGENTOS_SUCCESS 或错误码
 */
AGENTOS_API agentos_error_t agentos_mc_create(agentos_metacognition_t** out_mc);

/**
 * @brief 销毁元认知引擎
 */
AGENTOS_API void agentos_mc_destroy(agentos_metacognition_t* mc);

/**
 * @brief 关联思考链路（用于回调通知）
 */
AGENTOS_API void agentos_mc_set_chain(
    agentos_metacognition_t* mc,
    agentos_thinking_chain_t* chain);

/* ==================== 评估API ==================== */

/**
 * @brief 对思考步骤执行完整的多维度评估
 *
 * 这是元认知的核心函数。模拟S1验证角色的判断过程，
 * 从相关性、准确性、完整性、一致性、清晰度五个维度评估。
 *
 * @param step 待评估的思考步骤
 * @param context 当前上下文窗口内容（用于一致性检查）
 * @param context_len 上下文长度
 * @param out_result 输出评估结果（调用者负责释放critique_text）
 * @return AGENTOS_SUCCESS 或错误码
 */
AGENTOS_API agentos_error_t agentos_mc_evaluate_step(
    agentos_metacognition_t* mc,
    agentos_thinking_step_t* step,
    const char* context,
    size_t context_len,
    mc_evaluation_result_t* out_result);

/**
 * @brief 快速评估（仅检查关键维度）
 *
 * 用于流式批判场景，比完整评估更快。
 */
AGENTOS_API agentos_error_t agentos_mc_evaluate_quick(
    agentos_metacognition_t* mc,
    agentos_thinking_step_t* step,
    float* out_score,
    int* out_acceptable);

/* ==================== 纠错API ==================== */

/**
 * @brief 根据评估结果决定并执行纠正动作
 *
 * @param step 待纠正的思考步骤
 * @param eval 评估结果
 * @param corrector_fn 纠正回调函数（重新生成内容的实际执行者）
 * @param corrector_user_data 回调用户数据
 * @return AGENTOS_SUCCESS 或错误码
 */
AGENTOS_API agentos_error_t agentos_mc_apply_correction(
    agentos_metacognition_t* mc,
    agentos_thinking_step_t* step,
    const mc_evaluation_result_t* eval,
    agentos_error_t (*corrector_fn)(const char* input, size_t input_len,
                                     char** output, size_t* output_len,
                                     void* user_data),
    void* corrector_user_data);

/**
 * @brief 检查是否需要自我纠错（基于历史模式）
 *
 * @return 1=需要纠错, 0=不需要, -1=错误
 */
AGENTOS_API int agentos_mc_should_self_correct(
    agentos_metacognition_t* mc,
    tc_step_type_t step_type);

/* ==================== 置信度校准API ==================== */

/**
 * @brief 校准置信度（基于历史准确率）
 *
 * 如果Agent历史上在某个置信度水平下经常出错，
 * 则降低其未来报告的该置信度值。
 */
AGENTOS_API float agentos_mc_calibrate_confidence(
    agentos_metacognition_t* mc,
    float raw_confidence);

/**
 * @brief 反馈实际结果（用于校准学习）
 *
 * 在步骤执行完成后调用，告知系统实际的正确与否。
 */
AGENTOS_API agentos_error_t agentos_mc_feedback(
    agentos_metacognition_t* mc,
    float predicted_confidence,
    int was_correct);

/* ==================== 统计与诊断API ==================== */

/**
 * @brief 获取元认知统计信息（JSON格式）
 */
AGENTOS_API agentos_error_t agentos_mc_stats(
    agentos_metacognition_t* mc,
    char** out_json);

/**
 * @brief 获取最近的N条评估记录
 */
AGENTOS_API agentos_error_t agentos_mc_get_history(
    agentos_metacognition_t* mc,
    size_t count,
    mc_evaluation_record_t** out_records,
    size_t* out_count);

/**
 * @brief 重置校准器和历史记录
 */
AGENTOS_API void agentos_mc_reset(agentos_metacognition_t* mc);

/* ==================== 持久学习API (DS-005) ==================== */

/**
 * @brief 从评估结果中提取错误模式特征
 *
 * 分析最近N次评估结果，识别重复出现的失败模式。
 * 模式基于：步骤类型、低分维度、输入关键词等。
 *
 * @param mc 元认知引擎
 * @param out_patterns 输出检测到的模式数组
 * @param out_count 输出模式数量
 * @return AGENTOS_SUCCESS 或错误码
 */
AGENTOS_API agentos_error_t agentos_mc_detect_patterns(
    agentos_metacognition_t* mc,
    mc_error_pattern_t** out_patterns,
    size_t* out_count);

/**
 * @brief 学习最优纠正策略
 *
 * 基于历史数据，为特定错误模式选择最有效的纠正策略。
 * 通过分析每种策略在类似场景下的成功率来决策。
 *
 * @param mc 元认知引擎
 * @param pattern_key 模式键名
 * @param out_strategy 输出推荐策略
 * @return AGENTOS_SUCCESS 或错误码
 */
AGENTOS_API agentos_error_t agentos_mc_learn_best_strategy(
    agentos_metacognition_t* mc,
    const char* pattern_key,
    mc_correction_strategy_t* out_strategy);

/**
 * @brief 预检测与预纠正
 *
 * 在执行S2生成之前，检查当前输入是否匹配已知失败模式。
 * 如果匹配，返回建议的系统提示前缀以预防性避免该类错误。
 *
 * @param mc 元认知引擎
 * @param step_type 即将执行的步骤类型
 * @param input 输入内容
 * @param input_len 输入长度
 * @param out_preemptive_hint 输出预防性提示 (调用者释放)
 * @param out_hint_len 提示长度
 * @return 1=检测到模式并给出提示, 0=无匹配模式, -1=错误
 */
AGENTOS_API int agentos_mc_preemptive_check(
    agentos_metacognition_t* mc,
    tc_step_type_t step_type,
    const char* input,
    size_t input_len,
    char** out_preemptive_hint,
    size_t* out_hint_len);

/**
 * @brief 记录策略执行结果（用于学习）
 *
 * 在每次纠正操作完成后调用，记录哪种策略对哪种模式有效。
 *
 * @param mc 元认知引擎
 * @param pattern_key 模式键名
 * @param strategy 使用的策略
 * @param success 是否成功 (1=成功)
 * @return AGENTOS_SUCCESS 或错误码
 */
AGENTOS_API agentos_error_t agentos_mc_record_strategy_result(
    agentos_metacognition_t* mc,
    const char* pattern_key,
    mc_correction_strategy_t strategy,
    int success);

/**
 * @brief 自适应调整接受阈值
 *
 * 基于连续通过/拒绝的历史记录，动态调整acceptance_threshold。
 * 连续多次通过 → 放宽阈值(提高效率)
 * 连续多次拒绝 → 收紧阈值(提高质量)
 *
 * @param mc 元认知引擎
 * @return 调整后的当前阈值
 */
AGENTOS_API float agentos_mc_adapt_threshold(agentos_metacognition_t* mc);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_METACOGNITION_H */
