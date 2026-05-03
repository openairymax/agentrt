/**
 * @file memoryrovol_provider.c
 * @brief MemoryRovol 商业提供商适配器
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 将 MemoryRovol 的 C FFI 接口适配为 agentos_memory_provider_t。
 * 通过 __attribute__((constructor)) 自动注册。
 * 链接 MemoryRovol .a 后 AgentOS 启动日志显示
 * "using MemoryRovol (commercial)"。
 */

#include "memory_provider.h"
#include "memoryrovol.h"
#include "license.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct memoryrovol_provider_impl {
    agentos_memoryrov_handle_t* handle;
    agentos_memory_stats_t stats;
} memoryrovol_provider_impl_t;

static agentos_error_t mr_provider_init(
    agentos_memory_provider_t* provider, const char* config_path) {
    (void)config_path;
    if (!provider || !provider->impl) return AGENTOS_EINVAL;

    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;
    impl->handle = agentos_memoryrov_create();
    if (!impl->handle) return AGENTOS_ENOMEM;

    printf("[AgentOS] using MemoryRovol (commercial) - full L1+L2+L3+L4\n");
    return AGENTOS_SUCCESS;
}

static void mr_provider_destroy(agentos_memory_provider_t* provider) {
    if (!provider || !provider->impl) return;
    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;
    if (impl->handle) {
        agentos_memoryrov_destroy(impl->handle);
        impl->handle = NULL;
    }
    free(impl);
    provider->impl = NULL;
}

static agentos_error_t mr_write_raw(
    agentos_memory_provider_t* provider,
    const void* data, size_t len,
    const char* metadata_json,
    char** out_record_id) {
    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;
    if (!impl->handle) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_write_raw(impl->handle, data, len, metadata_json, out_record_id);
}

static agentos_error_t mr_get_raw(
    agentos_memory_provider_t* provider,
    const char* record_id,
    void** out_data, size_t* out_len) {
    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;
    if (!impl->handle) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_get_raw(impl->handle, record_id, out_data, out_len);
}

static agentos_error_t mr_delete_raw(
    agentos_memory_provider_t* provider,
    const char* record_id) {
    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;
    if (!impl->handle) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_delete_raw(impl->handle, record_id);
}

static agentos_error_t mr_query(
    agentos_memory_provider_t* provider,
    const char* query_text,
    uint32_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count) {
    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;
    if (!impl->handle) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_query(impl->handle, query_text, limit, out_record_ids, out_scores, out_count);
}

static agentos_error_t mr_retrieve(
    agentos_memory_provider_t* provider,
    const char* query_text,
    uint32_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count) {
    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;
    if (!impl->handle) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_query(impl->handle, query_text, limit, out_record_ids, out_scores, out_count);
}

static agentos_error_t mr_evolve(
    agentos_memory_provider_t* provider, int force) {
    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;
    if (!impl->handle) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_evolve(impl->handle, force);
}

static agentos_error_t mr_forget(
    agentos_memory_provider_t* provider) {
    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;
    if (!impl->handle) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_forget(impl->handle);
}

static agentos_error_t mr_stats(
    agentos_memory_provider_t* provider,
    agentos_memory_stats_t* out_stats) {
    if (!provider || !provider->impl || !out_stats) return AGENTOS_EINVAL;
    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;

    char* rov_stats = NULL;
    if (impl->handle) {
        agentos_memoryrov_stats(impl->handle, &rov_stats);
    }

    memset(out_stats, 0, sizeof(*out_stats));
    snprintf(out_stats->provider_name, sizeof(out_stats->provider_name), "memoryrovol");
    snprintf(out_stats->provider_version, sizeof(out_stats->provider_version), "2.0.0");

    if (rov_stats) {
        AGENTOS_FREE(rov_stats);
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t mr_mount(
    agentos_memory_provider_t* provider,
    const char* record_id,
    const char* context) {
    if (!provider || !provider->impl) return AGENTOS_EINVAL;
    memoryrovol_provider_impl_t* impl = (memoryrovol_provider_impl_t*)provider->impl;
    if (!impl->handle) return AGENTOS_ENOTINIT;
    return agentos_memoryrov_mount(impl->handle, record_id, context);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor(200)))
static void memoryrovol_provider_auto_register(void) {
    const agentos_license_status_t* status = agentos_license_get_status();
    if (!status) return;

    static agentos_memory_provider_t provider;
    static memoryrovol_provider_impl_t impl;

    memset(&provider, 0, sizeof(provider));
    memset(&impl, 0, sizeof(impl));

    provider.name = "memoryrovol";
    provider.version = "2.0.0";
    provider.impl = &impl;

    provider.capabilities.l1_raw = 1;
    provider.capabilities.l2_feature = 1;

#ifndef MEMORYROVOL_OSS
    if (agentos_license_check_feature(AGENTOS_FEATURE_L3_STRUCTURE)) {
        provider.capabilities.l3_structure = 1;
    }
    if (agentos_license_check_feature(AGENTOS_FEATURE_L4_PATTERN)) {
        provider.capabilities.l4_pattern = 1;
    }
    if (agentos_license_check_feature(AGENTOS_FEATURE_FORGETTING)) {
        provider.capabilities.forgetting = 1;
    }
    if (agentos_license_check_feature(AGENTOS_FEATURE_ATTRACTOR)) {
        provider.capabilities.attractor = 1;
    }
    if (agentos_license_check_feature(AGENTOS_FEATURE_PERSISTENCE)) {
        provider.capabilities.persistence = 1;
    }
    if (agentos_license_check_feature(AGENTOS_FEATURE_FAISS)) {
        provider.capabilities.faiss = 1;
    }
    if (agentos_license_check_feature(AGENTOS_FEATURE_ASYNC)) {
        provider.capabilities.async_ops = 1;
    }
#endif

    provider.init = mr_provider_init;
    provider.destroy = mr_provider_destroy;
    provider.write_raw = mr_write_raw;
    provider.get_raw = mr_get_raw;
    provider.delete_raw = mr_delete_raw;
    provider.query = mr_query;
    provider.retrieve = mr_retrieve;
    provider.evolve = mr_evolve;
    provider.forget = mr_forget;
    provider.stats = mr_stats;
    provider.mount = mr_mount;

    if (provider.init(&provider, NULL) == AGENTOS_SUCCESS) {
        agentos_memory_provider_register(&provider);
    }
}
#endif
