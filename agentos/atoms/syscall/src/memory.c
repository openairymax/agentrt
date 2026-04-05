/**
 * @file memory.c
 * @brief 记忆管理系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include "memoryrovol/memoryrovol.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

static agentos_memoryrov_handle_t* g_memory = NULL;

void agentos_sys_set_memory(agentos_memoryrov_handle_t* mem) {
    g_memory = mem;
}

agentos_error_t agentos_sys_memory_write(const void* data, size_t len,
                                         const char* metadata, char** out_record_id) {
    if (!data || len == 0 || !out_record_id) return AGENTOS_EINVAL;
    if (!g_memory) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_write_raw(g_memory, data, len, metadata, out_record_id);
}

agentos_error_t agentos_sys_memory_search(const char* query, uint32_t limit,
                                          char*** out_record_ids, float** out_scores,
                                          size_t* out_count) {
    if (!query || !out_record_ids || !out_scores || !out_count) return AGENTOS_EINVAL;
    if (!g_memory) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_query(g_memory, query, limit, out_record_ids, out_scores, out_count);
}

agentos_error_t agentos_sys_memory_get(const char* record_id, void** out_data, size_t* out_len) {
    if (!record_id || !out_data || !out_len) return AGENTOS_EINVAL;
    if (!g_memory) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_get_raw(g_memory, record_id, out_data, out_len);
}

agentos_error_t agentos_sys_memory_delete(const char* record_id) {
    if (!record_id) return AGENTOS_EINVAL;
    if (!g_memory) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_delete_raw(g_memory, record_id);
}
