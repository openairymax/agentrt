/**
 * @file stream_critic.h
 * @brief 流式批判核心接口 — Phase0~4 全管线
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 设计依据: G-67 技术融合差距填补 + AgentOS双思维模型详细落地方案
 *
 * Phase0: intent_classifier    — 输入意图识别（关键词+规则混合）
 * Phase1: stream_validator     — 处理过程实时校验（语义单元粒度）
 * Phase2: (由 triple_coordinator 已有 t2/t1-f/t1-p 批判循环覆盖)
 * Phase3: output_corrector     — 结果验证+纠错（最终输出质量把关）
 * Phase4: memory_confirmer     — 记忆确认+持久化（输出写入记忆引擎）
 */

#ifndef AGENTOS_STREAM_CRITIC_H
#define AGENTOS_STREAM_CRITIC_H

#include "agentos.h"
#include "cognition.h"
#include "intent_classifier.h"
#include "confidence_calibrator.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置常量 ==================== */

#define SC_MAX_VALIDATION_HINTS  8
#define SC_MIN_QUALITY_SCORE     0.35f
#define SC_ACCEPT_QUALITY        0.65f
#define SC_HIGH_QUALITY          0.85f
#define SC_MAX_MEMORY_TOKENS     4096
#define SC_MEMORY_COMPRESS_MIN   512

/* ==================== Phase0: 意图分类 ==================== */

typedef enum sc_intent_category {
    SC_INTENT_TASK     = 0,  /* 执行型：run / build / deploy */
    SC_INTENT_QUERY    = 1,  /* 查询型：what / how / show */
    SC_INTENT_ANALYSIS = 2,  /* 分析型：analyze / compare / review */
    SC_INTENT_CREATE   = 3,  /* 创建型：create / generate / write */
    SC_INTENT_MODIFY   = 4,  /* 修改型：fix / update / change */
    SC_INTENT_META     = 5,  /* 元认知型：think / reflect / plan */
    SC_INTENT_AMBI     = 6   /* 歧义型：无法明确分类 */
} sc_intent_category_t;

typedef struct sc_intent_result {
    sc_intent_category_t category;
    agentos_intent_type_t original_type;
    float confidence;
    const char* category_name;
    const char** extracted_keywords;
    size_t keyword_count;
    int is_urgent;
    int requires_multi_step;
    char* reasoning_hint;
    size_t hint_len;
} sc_intent_result_t;

/* ==================== Phase1: 流式验证 ==================== */

typedef enum sc_validation_aspect {
    SC_VALIDATE_ACCURACY   = 0x01,  /* 事实准确性 */
    SC_VALIDATE_COHERENCE  = 0x02,  /* 逻辑连贯性 */
    SC_VALIDATE_COMPLETENESS = 0x04,/* 内容完整性 */
    SC_VALIDATE_RELEVANCE  = 0x08,  /* 与原始意图的相关性 */
    SC_VALIDATE_SAFETY     = 0x10   /* 安全性检查 */
} sc_validation_aspect_t;

typedef struct sc_validation_hint {
    const char* aspect_name;
    size_t aspect_name_len;
    float score;
    const char* suggestion;
    size_t suggestion_len;
} sc_validation_hint_t;

typedef struct sc_validation_result {
    float overall_score;
    int is_acceptable;              /* overall_score >= SC_ACCEPT_QUALITY */
    int is_high_quality;            /* overall_score >= SC_HIGH_QUALITY */
    sc_validation_hint_t hints[SC_MAX_VALIDATION_HINTS];
    size_t hint_count;
    const char* rejection_reason;
    size_t rejection_reason_len;
} sc_validation_result_t;

/* ==================== Phase3: 输出纠错 ==================== */

typedef struct sc_correction_entry {
    size_t offset_start;            /* 原文中出现问题的起始字节 */
    size_t offset_end;              /* 结束字节 */
    const char* original_text;      /* 原文片段 */
    size_t original_len;
    const char* corrected_text;     /* 纠正后文本 */
    size_t corrected_len;
    const char* reason;             /* 纠正原因 */
    size_t reason_len;
    float correction_confidence;
} sc_correction_entry_t;

typedef struct sc_correction_result {
    int corrections_applied;
    char* final_output;             /* 纠错后完整输出（由调用方释放） */
    size_t final_output_len;
    float final_quality_score;
    size_t entry_count;
    sc_correction_entry_t* entries;
    size_t entries_capacity;
} sc_correction_result_t;

/* ==================== Phase4: 记忆确认 ==================== */

typedef struct sc_memory_entry {
    const char* key;                /* 记忆键 */
    size_t key_len;
    const char* value;              /* 记忆值 */
    size_t value_len;
    const char* content_type;       /* MIME类型 */
    int is_important;               /* 标记为重要 */
    float relevance_score;          /* 相关性评分 */
    int persisted;                  /* 是否成功持久化 */
} sc_memory_entry_t;

typedef struct sc_memory_result {
    int memory_updated;
    size_t entries_stored;
    size_t entries_pruned;
    sc_memory_entry_t* entries;
    size_t entries_count;
    size_t entries_capacity;
} sc_memory_result_t;

/* ==================== 流式批评配置 ==================== */

typedef struct {
    double decay_factor;
} cc_config_t;

typedef struct sc_config {
    float accept_threshold;         /* 接受阈值 Phase1 */
    float high_quality_threshold;   /* 高质量阈值 Phase3 */
    uint32_t max_corrections;       /* Phase3 最大纠错条目 */
    uint32_t max_memory_entries;    /* Phase4 最大记忆条目 */
    int enable_intent_classify;     /* 启用 Phase0 */
    int enable_stream_validate;     /* 启用 Phase1 */
    int enable_output_correct;      /* 启用 Phase3 */
    int enable_memory_confirm;      /* 启用 Phase4 */
    cc_config_t calibrator_config;  /* 置信度校准器配置 */
} sc_config_t;

#define SC_CONFIG_DEFAULTS { \
    .accept_threshold       = SC_ACCEPT_QUALITY, \
    .high_quality_threshold = SC_HIGH_QUALITY, \
    .max_corrections        = 32, \
    .max_memory_entries     = 64, \
    .enable_intent_classify = 1, \
    .enable_stream_validate = 1, \
    .enable_output_correct  = 0, \
    .enable_memory_confirm  = 0, \
    .calibrator_config      = { .decay_factor = 0.85 } \
}

/* ==================== Opaque Handle ==================== */

typedef struct sc_stream_critic sc_stream_critic_t;

/* ==================== 生命周期 ==================== */

AGENTOS_API agentos_error_t sc_stream_critic_create(
    const sc_config_t* config,
    sc_stream_critic_t** out_critic);

AGENTOS_API void sc_stream_critic_destroy(sc_stream_critic_t* critic);

AGENTOS_API agentos_error_t sc_stream_critic_reset(sc_stream_critic_t* critic);

/* ==================== Phase 0: 意图分类 ==================== */

AGENTOS_API agentos_error_t sc_intent_classifier(
    sc_stream_critic_t* critic,
    const char* input,
    size_t input_len,
    sc_intent_result_t* out_result);

AGENTOS_API void sc_intent_result_free(sc_intent_result_t* result);

/* ==================== Phase 1: 流式验证 ==================== */

AGENTOS_API agentos_error_t sc_stream_validator(
    sc_stream_critic_t* critic,
    const char* content,
    size_t content_len,
    const sc_intent_result_t* intent_context,
    uint32_t validate_aspects,
    sc_validation_result_t* out_result);

AGENTOS_API void sc_validation_result_free(sc_validation_result_t* result);

/* ==================== Phase 3: 输出纠错 ==================== */

AGENTOS_API agentos_error_t sc_output_corrector(
    sc_stream_critic_t* critic,
    const char* raw_output,
    size_t raw_output_len,
    const sc_intent_result_t* intent_context,
    sc_correction_result_t* out_result);

AGENTOS_API void sc_correction_result_free(sc_correction_result_t* result);

/* ==================== Phase 4: 记忆确认 ==================== */

AGENTOS_API agentos_error_t sc_memory_confirmer(
    sc_stream_critic_t* critic,
    const char* output,
    size_t output_len,
    const sc_intent_result_t* intent_context,
    agentos_memory_engine_t* memory_engine,
    sc_memory_result_t* out_result);

AGENTOS_API void sc_memory_result_free(sc_memory_result_t* result);

/* ==================== 便捷全管线接口 ==================== */

AGENTOS_API agentos_error_t sc_stream_critic_pipeline(
    sc_stream_critic_t* critic,
    const char* input,
    size_t input_len,
    const char* raw_output,
    size_t raw_output_len,
    agentos_memory_engine_t* memory_engine,
    char** out_final_output,
    size_t* out_final_len,
    float* out_final_quality);

/* ==================== 统计信息 ==================== */

typedef struct sc_critic_stats {
    uint64_t intent_classifications;
    uint64_t stream_validations;
    uint64_t stream_accepted;
    uint64_t stream_rejected;
    uint64_t corrections_applied;
    uint64_t memory_confirmations;
    uint64_t memory_entries_stored;
    float avg_validation_score;
    float avg_correction_confidence;
    uint64_t total_time_ns;
} sc_critic_stats_t;

AGENTOS_API agentos_error_t sc_stream_critic_get_stats(
    const sc_stream_critic_t* critic,
    sc_critic_stats_t* out_stats);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_STREAM_CRITIC_H */
