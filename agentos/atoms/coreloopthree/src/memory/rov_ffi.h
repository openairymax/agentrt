/**
 * @file rov_ffi.h
 * @brief MemoryRovol 对外 C 接口（由 memoryrovol 模块实现）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_ROV_FFI_H
#define AGENTOS_ROV_FFI_H

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_memoryrov_handle agentos_memoryrov_handle_t;

agentos_memoryrov_handle_t* agentos_memoryrov_create(void);

void agentos_memoryrov_destroy(agentos_memoryrov_handle_t* handle);

agentos_error_t agentos_memoryrov_write_raw(
    agentos_memoryrov_handle_t* handle,
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id);

agentos_error_t agentos_memoryrov_query(
    agentos_memoryrov_handle_t* handle,
    const char* query,
    uint32_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count);

agentos_error_t agentos_memoryrov_get_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id,
    void** out_data,
    size_t* out_len);

agentos_error_t agentos_memoryrov_mount(
    agentos_memoryrov_handle_t* handle,
    const char* record_id,
    const char* context);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ROV_FFI_H */
