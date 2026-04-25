/**
 * @file semantic_unit.c
 * @brief 流式语义单元检测器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "semantic_unit.h"
#include "agentos.h"
#include "memory_compat.h"
#include "string_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void free_pending_units(su_stream_detector_t* d) {
    for (size_t i = 0; i < d->pending_count; i++) {
        if (d->pending_units[i].text) {
            AGENTOS_FREE(d->pending_units[i].text);
            d->pending_units[i].text = NULL;
        }
    }
    d->pending_count = 0;
}

static agentos_error_t emit_unit(su_stream_detector_t* d,
                                  const char* text, size_t len,
                                  su_boundary_type_t boundary) {
    if (d->pending_count >= SU_MAX_PENDING_UNITS) {
        return AGENTOS_ENOMEM;
    }

    char* copy = (char*)AGENTOS_MALLOC(len + 1);
    if (!copy) return AGENTOS_ENOMEM;
    memcpy(copy, text, len);
    copy[len] = '\0';

    su_semantic_unit_t* unit = &d->pending_units[d->pending_count];
    unit->text = copy;
    unit->text_len = len;
    unit->boundary = boundary;
    unit->token_estimate = su_estimate_tokens(text, len);
    unit->unit_index = d->total_units_emitted;
    unit->confidence = d->last_confidence;
    d->pending_count++;
    d->total_units_emitted++;
    d->total_boundary_detections++;

    if (d->config.on_unit_ready) {
        d->config.on_unit_ready(d, unit, d->config.callback_user_data);
    }

    return AGENTOS_SUCCESS;
}

su_boundary_type_t su_detect_boundary(const char* text, size_t len, size_t pos) {
    if (!text || pos >= len) return SU_BOUNDARY_NONE;

    char c = text[pos];
    if (c == '\n' && pos + 1 < len && text[pos + 1] == '\n') {
        return SU_BOUNDARY_PARAGRAPH;
    }
    if (c == '\n' && pos > 0 && text[pos - 1] == '\n') {
        return SU_BOUNDARY_PARAGRAPH;
    }

    if (c == '.' || c == '!' || c == '?') {
        if (pos + 1 < len) {
            char next = text[pos + 1];
            if (next == ' ' || next == '\n' || next == '\t' || next == '\r') {
                if (pos >= 2 && text[pos - 1] == '.' && text[pos - 2] == '.') {
                    return SU_BOUNDARY_NONE;
                }
                return SU_BOUNDARY_SENTENCE;
            }
        } else if (pos + 1 == len) {
            return SU_BOUNDARY_SENTENCE;
        }
    }

    if (c == ':' && pos + 1 < len && text[pos + 1] == '\n') {
        return SU_BOUNDARY_SECTION;
    }

    return SU_BOUNDARY_NONE;
}

uint32_t su_estimate_tokens(const char* text, size_t len) {
    if (!text || len == 0) return 0;
    uint32_t words = 1;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == ' ' || text[i] == '\t' || text[i] == '\n') {
            words++;
        }
    }
    return (words * 4 + 2) / 3;
}

agentos_error_t su_stream_detector_create(
    const su_config_t* config,
    su_stream_detector_t** out_detector) {
    if (!out_detector) return AGENTOS_EINVAL;

    su_stream_detector_t* d = (su_stream_detector_t*)AGENTOS_CALLOC(1, sizeof(su_stream_detector_t));
    if (!d) return AGENTOS_ENOMEM;

    if (config) {
        d->config = *config;
    } else {
        su_config_t defaults = SU_CONFIG_DEFAULTS;
        d->config = defaults;
    }

    d->buffer = (char*)AGENTOS_MALLOC(SU_BUFFER_CAPACITY);
    if (!d->buffer) {
        AGENTOS_FREE(d);
        return AGENTOS_ENOMEM;
    }
    d->buffer_capacity = SU_BUFFER_CAPACITY;
    d->buffer_used = 0;
    d->dynamic_chunk_target = d->config.chunk_token_target;

    *out_detector = d;
    return AGENTOS_SUCCESS;
}

void su_stream_detector_destroy(su_stream_detector_t* detector) {
    if (!detector) return;
    free_pending_units(detector);
    if (detector->buffer) AGENTOS_FREE(detector->buffer);
    AGENTOS_FREE(detector);
}

agentos_error_t su_stream_detector_feed(
    su_stream_detector_t* detector,
    const char* tokens,
    size_t len,
    float confidence) {
    if (!detector || !tokens) return AGENTOS_EINVAL;
    if (len == 0) return AGENTOS_SUCCESS;

    detector->last_confidence = confidence;
    detector->total_bytes_received += len;

    if (detector->config.enable_dynamic_chunk) {
        su_stream_detector_adjust_chunk(detector, confidence);
    }

    size_t space = detector->buffer_capacity - detector->buffer_used;
    size_t to_copy = (len < space) ? len : space;
    if (to_copy == 0) {
        agentos_error_t flush_err = su_stream_detector_flush(detector);
        if (flush_err != AGENTOS_SUCCESS) return flush_err;
        to_copy = (len < detector->buffer_capacity) ? len : detector->buffer_capacity;
    }

    memcpy(detector->buffer + detector->buffer_used, tokens, to_copy);
    detector->buffer_used += to_copy;
    detector->current_token_estimate = su_estimate_tokens(
        detector->buffer, detector->buffer_used);

    size_t last_boundary = 0;
    for (size_t i = 0; i < detector->buffer_used; i++) {
        su_boundary_type_t bt = su_detect_boundary(detector->buffer, detector->buffer_used, i);
        if (bt != SU_BOUNDARY_NONE) {
            uint32_t unit_tokens = su_estimate_tokens(
                detector->buffer + last_boundary, i + 1 - last_boundary);

            if (unit_tokens >= detector->dynamic_chunk_target ||
                bt >= SU_BOUNDARY_PARAGRAPH) {
                size_t unit_len = i + 1 - last_boundary;
                agentos_error_t err = emit_unit(
                    detector, detector->buffer + last_boundary, unit_len, bt);
                if (err != AGENTOS_SUCCESS) {
                    last_boundary = i + 1;
                    continue;
                }
                detector->total_tokens_processed += unit_tokens;
                last_boundary = i + 1;
            }
        }
    }

    if (last_boundary > 0) {
        size_t remaining = detector->buffer_used - last_boundary;
        if (remaining > 0) {
            memmove(detector->buffer, detector->buffer + last_boundary, remaining);
        }
        detector->buffer_used = remaining;
        detector->current_token_estimate = su_estimate_tokens(
            detector->buffer, detector->buffer_used);
    }

    if (detector->current_token_estimate >= detector->dynamic_chunk_target * 2) {
        agentos_error_t err = emit_unit(
            detector, detector->buffer, detector->buffer_used, SU_BOUNDARY_FORCED);
        if (err == AGENTOS_SUCCESS) {
            detector->total_tokens_processed += detector->current_token_estimate;
            detector->buffer_used = 0;
            detector->current_token_estimate = 0;
        }
    }

    if (to_copy < len) {
        return su_stream_detector_feed(detector, tokens + to_copy, len - to_copy, confidence);
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t su_stream_detector_flush(su_stream_detector_t* detector) {
    if (!detector) return AGENTOS_EINVAL;
    if (detector->buffer_used == 0) return AGENTOS_SUCCESS;

    su_boundary_type_t bt = (detector->buffer_used > 0) ?
        SU_BOUNDARY_FORCED : SU_BOUNDARY_NONE;

    agentos_error_t err = emit_unit(
        detector, detector->buffer, detector->buffer_used, bt);
    if (err == AGENTOS_SUCCESS) {
        detector->total_tokens_processed += detector->current_token_estimate;
        detector->buffer_used = 0;
        detector->current_token_estimate = 0;
    }
    return err;
}

agentos_error_t su_stream_detector_reset(su_stream_detector_t* detector) {
    if (!detector) return AGENTOS_EINVAL;
    free_pending_units(detector);
    detector->buffer_used = 0;
    detector->current_token_estimate = 0;
    detector->total_units_emitted = 0;
    detector->total_tokens_processed = 0;
    detector->total_bytes_received = 0;
    detector->total_boundary_detections = 0;
    detector->last_confidence = 0.5f;
    detector->dynamic_chunk_target = detector->config.chunk_token_target;
    return AGENTOS_SUCCESS;
}

void su_stream_detector_adjust_chunk(
    su_stream_detector_t* detector,
    float confidence) {
    if (!detector || !detector->config.enable_dynamic_chunk) return;

    uint32_t target = detector->config.chunk_token_target;

    if (confidence < detector->config.low_confidence_threshold) {
        target = detector->config.min_chunk_tokens;
    } else if (confidence > detector->config.high_confidence_threshold) {
        target = detector->config.max_chunk_tokens;
    } else {
        float range = detector->config.high_confidence_threshold -
                      detector->config.low_confidence_threshold;
        float ratio = (range > 0.0f) ?
            (confidence - detector->config.low_confidence_threshold) / range : 0.5f;
        target = detector->config.min_chunk_tokens +
            (uint32_t)(ratio * (detector->config.max_chunk_tokens -
                                detector->config.min_chunk_tokens));
    }

    detector->dynamic_chunk_target = target;
}

size_t su_stream_detector_pending_count(const su_stream_detector_t* detector) {
    return detector ? detector->pending_count : 0;
}

agentos_error_t su_stream_detector_pop_pending(
    su_stream_detector_t* detector,
    su_semantic_unit_t* out_unit) {
    if (!detector || !out_unit) return AGENTOS_EINVAL;
    if (detector->pending_count == 0) return AGENTOS_ENOENT;

    *out_unit = detector->pending_units[0];
    for (size_t i = 1; i < detector->pending_count; i++) {
        detector->pending_units[i - 1] = detector->pending_units[i];
    }
    detector->pending_count--;
    return AGENTOS_SUCCESS;
}

agentos_error_t su_stream_detector_stats(
    const su_stream_detector_t* detector,
    char** out_json) {
    if (!detector || !out_json) return AGENTOS_EINVAL;

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"units_emitted\":%u,\"tokens_processed\":%u,"
        "\"bytes_received\":%llu,\"boundaries\":%llu,"
        "\"pending\":%zu,\"dynamic_chunk\":%u}",
        detector->total_units_emitted,
        detector->total_tokens_processed,
        (unsigned long long)detector->total_bytes_received,
        (unsigned long long)detector->total_boundary_detections,
        detector->pending_count,
        detector->dynamic_chunk_target);

    char* result = (char*)AGENTOS_MALLOC(len + 1);
    if (!result) return AGENTOS_ENOMEM;
    memcpy(result, buf, len + 1);
    *out_json = result;
    return AGENTOS_SUCCESS;
}
