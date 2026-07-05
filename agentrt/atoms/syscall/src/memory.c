/**
 * @file memory.c
 * @brief 记忆管理系统调用实现（基于 provider 接口）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt.h"
#include "memory_compat.h"
#include "memory_provider.h"
#include "string_compat.h"
#include "syscalls.h"

#include <stdlib.h>
#include <string.h>

static agentrt_memory_provider_t *g_memory_provider = NULL;

void agentrt_sys_set_memory_provider(agentrt_memory_provider_t *provider)
{
    g_memory_provider = provider;
}

agentrt_memory_provider_t *agentrt_sys_get_memory_provider(void)
{
    return g_memory_provider ? g_memory_provider : agentrt_memory_provider_get_active();
}

agentrt_error_t agentrt_sys_memory_write(const void *data, size_t len, const char *metadata,
                                         char **out_record_id)
{
    if (!data || len == 0 || !out_record_id)
        return AGENTRT_EINVAL;
    agentrt_memory_provider_t *p = agentrt_sys_get_memory_provider();
    if (!p)
        return AGENTRT_ENOTINIT;
    return p->write_raw(p, data, len, metadata, out_record_id);
}

agentrt_error_t agentrt_sys_memory_search(const char *query, uint32_t limit, char ***out_record_ids,
                                          float **out_scores, size_t *out_count)
{
    if (!query || !out_record_ids || !out_scores || !out_count)
        return AGENTRT_EINVAL;
    agentrt_memory_provider_t *p = agentrt_sys_get_memory_provider();
    if (!p)
        return AGENTRT_ENOTINIT;
    return p->query(p, query, limit, out_record_ids, out_scores, out_count);
}

agentrt_error_t agentrt_sys_memory_get(const char *record_id, void **out_data, size_t *out_len)
{
    if (!record_id || !out_data || !out_len)
        return AGENTRT_EINVAL;
    agentrt_memory_provider_t *p = agentrt_sys_get_memory_provider();
    if (!p)
        return AGENTRT_ENOTINIT;
    return p->get_raw(p, record_id, out_data, out_len);
}

agentrt_error_t agentrt_sys_memory_delete(const char *record_id)
{
    if (!record_id)
        return AGENTRT_EINVAL;
    agentrt_memory_provider_t *p = agentrt_sys_get_memory_provider();
    if (!p)
        return AGENTRT_ENOTINIT;
    return p->delete_raw(p, record_id);
}
