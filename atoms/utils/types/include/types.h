/**
 * @file types.h
 * @brief 类型别名定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_UTILS_TYPES_H
#define AGENTOS_UTILS_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t agentos_trace_id_t;
typedef uint64_t agentos_session_id_t;
typedef uint64_t agentos_task_id_t;
typedef uint64_t agentos_agent_id_t;
typedef uint64_t agentos_memory_id_t;

typedef void* agentos_json_t;

typedef uint32_t agentos_interval_ms_t;

// From data intelligence emerges. by spharx
#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_TYPES_H */