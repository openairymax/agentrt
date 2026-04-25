/**
 * @file semantic_unit.h
 * @brief 流式语义单元检测器 - S2流式生成时实时检测语义单元边界
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 设计依据: AgentOS双思维模型详细落地方案
 * - t2持续输出token流，t1-f以完整语义单元为检查块
 * - 语义单元边界: 句号/问号/感叹号 + 空格/换行
 * - 动态分块: 低置信度时减小分块(更频繁检查)，高置信度时增大分块
 */

#ifndef AGENTOS_SEMANTIC_UNIT_H
#define AGENTOS_SEMANTIC_UNIT_H

#include "agentos.h"
#include "thinking_chain.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SU_DEFAULT_CHUNK_TOKENS      15
#define SU_MIN_CHUNK_TOKENS          5
#define SU_MAX_CHUNK_TOKENS          50
#define SU_BUFFER_CAPACITY           8192
#define SU_MAX_PENDING_UNITS         64

typedef enum {
    SU_BOUNDARY_NONE = 0,
    SU_BOUNDARY_SENTENCE = 1,
    SU_BOUNDARY_PARAGRAPH = 2,
    SU_BOUNDARY_SECTION = 3,
    SU_BOUNDARY_FORCED = 4
} su_boundary_type_t;

typedef struct {
    char* text;
    size_t text_len;
    su_boundary_type_t boundary;
    uint32_t token_estimate;
    uint32_t unit_index;
    float confidence;
} su_semantic_unit_t;

typedef struct su_stream_detector su_stream_detector_t;

typedef void (*su_unit_ready_cb_t)(
    su_stream_detector_t* detector,
    const su_semantic_unit_t* unit,
    void* user_data);

typedef struct {
    uint32_t chunk_token_target;
    uint32_t min_chunk_tokens;
    uint32_t max_chunk_tokens;
    int enable_dynamic_chunk;
    float low_confidence_threshold;
    float high_confidence_threshold;
    su_unit_ready_cb_t on_unit_ready;
    void* callback_user_data;
} su_config_t;

#define SU_CONFIG_DEFAULTS { \
    .chunk_token_target = SU_DEFAULT_CHUNK_TOKENS, \
    .min_chunk_tokens = SU_MIN_CHUNK_TOKENS, \
    .max_chunk_tokens = SU_MAX_CHUNK_TOKENS, \
    .enable_dynamic_chunk = 1, \
    .low_confidence_threshold = 0.4f, \
    .high_confidence_threshold = 0.8f, \
    .on_unit_ready = NULL, \
    .callback_user_data = NULL }

struct su_stream_detector {
    su_config_t config;

    char* buffer;
    size_t buffer_capacity;
    size_t buffer_used;

    uint32_t current_token_estimate;
    uint32_t total_units_emitted;
    uint32_t total_tokens_processed;

    su_semantic_unit_t pending_units[SU_MAX_PENDING_UNITS];
    size_t pending_count;

    float last_confidence;
    uint32_t dynamic_chunk_target;

    uint64_t total_bytes_received;
    uint64_t total_boundary_detections;
};

AGENTOS_API agentos_error_t su_stream_detector_create(
    const su_config_t* config,
    su_stream_detector_t** out_detector);

AGENTOS_API void su_stream_detector_destroy(su_stream_detector_t* detector);

AGENTOS_API agentos_error_t su_stream_detector_feed(
    su_stream_detector_t* detector,
    const char* tokens,
    size_t len,
    float confidence);

AGENTOS_API agentos_error_t su_stream_detector_flush(
    su_stream_detector_t* detector);

AGENTOS_API agentos_error_t su_stream_detector_reset(
    su_stream_detector_t* detector);

AGENTOS_API void su_stream_detector_adjust_chunk(
    su_stream_detector_t* detector,
    float confidence);

AGENTOS_API size_t su_stream_detector_pending_count(
    const su_stream_detector_t* detector);

AGENTOS_API agentos_error_t su_stream_detector_pop_pending(
    su_stream_detector_t* detector,
    su_semantic_unit_t* out_unit);

AGENTOS_API agentos_error_t su_stream_detector_stats(
    const su_stream_detector_t* detector,
    char** out_json);

su_boundary_type_t su_detect_boundary(const char* text, size_t len, size_t pos);

uint32_t su_estimate_tokens(const char* text, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SEMANTIC_UNIT_H */
