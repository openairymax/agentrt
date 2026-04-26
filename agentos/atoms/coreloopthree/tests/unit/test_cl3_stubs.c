/* Stub implementations for LTO-unresolvable symbols */
#include <string.h>
#include <stdlib.h>
#include "id_utils.h"
#include "execution.h"
#include "compensation.h"
#include "memoryrovol.h"

/* id_utils stubs */
agentos_error_t agentos_generate_uuid(char* buf) {
    if (!buf) return -1;
    memset(buf, '0', 36);
    buf[8] = '-';
    buf[13] = '-';
    buf[18] = '-';
    buf[23] = '-';
    buf[36] = '\0';
    return 0;
}

void agentos_generate_plan_id(char* buf, size_t len) {
    if (!buf || len < 2) return;
    memset(buf, 'P', len - 1);
    buf[0] = 'p';
    buf[len - 1] = '\0';
}

/* execution engine stubs */
agentos_error_t agentos_execution_register_unit(
    agentos_execution_engine_t* engine,
    const char* name,
    agentos_execution_unit_t unit) {
    (void)engine; (void)name; (void)unit;
    return AGENTOS_SUCCESS;
}

void agentos_execution_unregister_unit(
    agentos_execution_engine_t* engine,
    const char* name) {
    (void)engine; (void)name;
}

void agentos_execution_set_feedback_callback(
    agentos_execution_engine_t* engine,
    agentos_feedback_callback_t callback,
    void* user_data) {
    (void)engine; (void)callback; (void)user_data;
}

/* compensation stubs */
agentos_error_t agentos_compensation_compensate(
    agentos_compensation_t* mgr,
    const char* action_id) {
    (void)mgr; (void)action_id;
    return AGENTOS_SUCCESS;
}

/* memoryrov FFI stubs - signatures must exactly match memoryrovol.h */
static int g_memoryrov_stub_initialized = 0;

agentos_memoryrov_handle_t* agentos_memoryrov_create(void) {
    g_memoryrov_stub_initialized = 1;
    return (agentos_memoryrov_handle_t*)&g_memoryrov_stub_initialized;
}

void agentos_memoryrov_destroy(agentos_memoryrov_handle_t* handle) {
    (void)handle;
}

agentos_error_t agentos_memoryrov_init(
    const agentos_memoryrov_config_t* manager,
    agentos_memoryrov_handle_t** out_handle) {
    (void)manager;
    if (out_handle) *out_handle = agentos_memoryrov_create();
    return 0;
}

void agentos_memoryrov_cleanup(agentos_memoryrov_handle_t* handle) {
    agentos_memoryrov_destroy(handle);
}

agentos_error_t agentos_memoryrov_evolve(
    agentos_memoryrov_handle_t* handle,
    int force) {
    (void)handle; (void)force;
    return 0;
}

agentos_error_t agentos_memoryrov_stats(
    agentos_memoryrov_handle_t* handle,
    char** out_stats) {
    (void)handle;
    if (out_stats) *out_stats = NULL;
    return 0;
}

agentos_error_t agentos_memoryrov_write_raw(
    agentos_memoryrov_handle_t* handle,
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id) {
    (void)handle; (void)data; (void)len; (void)metadata;
    if (out_record_id) *out_record_id = NULL;
    return 0;
}

agentos_error_t agentos_memoryrov_get_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id,
    void** out_data,
    size_t* out_len) {
    (void)handle; (void)record_id;
    if (out_data) *out_data = NULL;
    if (out_len) *out_len = 0;
    return 0;
}

agentos_error_t agentos_memoryrov_delete_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id) {
    (void)handle; (void)record_id;
    return 0;
}

agentos_error_t agentos_memoryrov_query(
    agentos_memoryrov_handle_t* handle,
    const char* query,
    uint32_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count) {
    (void)handle; (void)query; (void)limit;
    if (out_record_ids) *out_record_ids = NULL;
    if (out_scores) *out_scores = NULL;
    if (out_count) *out_count = 0;
    return 0;
}

agentos_error_t agentos_memoryrov_add_memory(
    agentos_memoryrov_handle_t* handle,
    const char* content,
    size_t content_len) {
    (void)handle; (void)content; (void)content_len;
    return 0;
}

agentos_error_t agentos_memoryrov_retrieve(
    agentos_memoryrov_handle_t* handle,
    const char* query,
    size_t limit,
    agentos_memory_t** out_results,
    size_t* out_count) {
    (void)handle; (void)query; (void)limit;
    if (out_results) *out_results = NULL;
    if (out_count) *out_count = 0;
    return 0;
}

agentos_error_t agentos_memoryrov_forget(
    agentos_memoryrov_handle_t* handle) {
    (void)handle;
    return 0;
}
