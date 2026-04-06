/**
 * @file trace_manager.h
 * @brief 追踪管理器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_TRACE_MANAGER_H
#define AGENTOS_TRACE_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_trace_context {
    char trace_id[33];
    char span_id[17];
    char parent_span_id[17];
    char service_name[64];
    char operation_name[128];
    uint64_t start_time_ns;
    uint64_t end_time_ns;
    int error_code;
    int is_active;
} agentos_trace_context_t;

int agentos_trace_manager_init(void);
void agentos_trace_manager_cleanup(void);

int agentos_trace_span_start(agentos_trace_context_t* context, 
                             const char* service_name, 
                             const char* operation_name);
int agentos_trace_span_start_with_parent(agentos_trace_context_t* context,
                                         const char* service_name,
                                         const char* operation_name,
                                         const char* parent_trace_id);
int agentos_trace_span_end(agentos_trace_context_t* context, int error_code);
int agentos_trace_set_tag(agentos_trace_context_t* context, 
                         const char* key, 
                         const char* value);
int agentos_trace_log(agentos_trace_context_t* context, const char* message);
int agentos_trace_is_error(agentos_trace_context_t* context);

void generate_trace_id(char* buffer, size_t size);
void generate_span_id(char* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TRACE_MANAGER_H */
