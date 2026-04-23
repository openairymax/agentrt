/* Stub implementations for LTO-unresolvable symbols */
#include <string.h>
#include <stdlib.h>
#include "id_utils.h"
#include "execution.h"
#include "compensation.h"

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

/* memoryrov FFI stubs */
agentos_error_t agentos_memoryrov_create(const char* path, void** handle) {
    (void)path;
    *handle = NULL;
    return 0;
}

void agentos_memoryrov_destroy(void* handle) { (void)handle; }

agentos_error_t agentos_memoryrov_write_raw(void* handle, const void* data,
                                          size_t len, const char* meta,
                                          char** out_id) {
    (void)handle; (void)data; (void)len; (void)meta;
    *out_id = NULL;
    return 0;
}

agentos_error_t agentos_memoryrov_query(void* handle, const char* text,
                                        size_t limit, char*** results,
                                        size_t* count) {
    (void)handle; (void)text; (void)limit;
    *results = NULL;
    *count = 0;
    return 0;
}

agentos_error_t agentos_memoryrov_get_raw(void* handle, const char* id,
                                         void** data, size_t* len) {
    (void)handle; (void)id;
    *data = NULL;
    *len = 0;
    return 0;
}

agentos_error_t agentos_memoryrov_mount(void* handle, const char* id,
                                        const char* ctx) {
    (void)handle; (void)id; (void)ctx;
    return 0;
}
