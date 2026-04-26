/**
 * @file registry.c
 * @brief 提供商注册表实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "registry.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>

/* 外部声明各提供商操作表 */
extern const provider_ops_t openai_ops;
extern const provider_ops_t anthropic_ops;
extern const provider_ops_t deepseek_ops;
extern const provider_ops_t google_ops;
extern const provider_ops_t local_ops;

struct provider_registry {
    provider_t* providers;
};

static const provider_ops_t* get_ops_by_name(const char* name) {
    if (strcmp(name, "openai") == 0) return &openai_ops;
    if (strcmp(name, "anthropic") == 0) return &anthropic_ops;
    if (strcmp(name, "deepseek") == 0) return &deepseek_ops;
    if (strcmp(name, "google") == 0) return &google_ops;
    if (strcmp(name, "local") == 0) return &local_ops;
    return NULL;
}

provider_registry_t* provider_registry_create(const service_config_t* cfg) {
    provider_registry_t* reg = calloc(1, sizeof(provider_registry_t));
    if (!reg) return NULL;

    size_t count = 0;
    while (cfg->providers && cfg->providers[count].name) count++;
    if (count == 0) return reg;

    reg->providers = calloc(count + 1, sizeof(provider_t));
    if (!reg->providers) {
        free(reg);
        return NULL;
    }

    for (size_t i = 0; i < count; ++i) {
        const provider_config_t* pcfg = (const provider_config_t*)&cfg->providers[i];
        const provider_ops_t* ops = get_ops_by_name(pcfg->name);
        if (!ops) {
            SVC_LOG_WARN("Unknown provider: %s, skipping", pcfg->name);
            continue;
        }

        provider_ctx_t* ctx = ops->init(pcfg->name, pcfg->api_key,
                                         pcfg->api_base, pcfg->organization,
                                         pcfg->timeout_sec, pcfg->max_retries);
        if (!ctx) {
            SVC_LOG_ERROR("Failed to init provider: %s", pcfg->name);
            continue;
        }

        char** models = NULL;
        size_t model_cnt = 0;
        if (pcfg->models) {
            while (pcfg->models[model_cnt]) model_cnt++;
            models = calloc(model_cnt + 1, sizeof(*models));
            if (models) {
                for (size_t j = 0; j < model_cnt; ++j)
                    models[j] = strdup(pcfg->models[j]);
            }
        }

        reg->providers[i].name = strdup(pcfg->name);
        reg->providers[i].ops = ops;
        reg->providers[i].ctx = ctx;
        reg->providers[i].models = models;
    }

    return reg;
}

void provider_registry_destroy(provider_registry_t* reg) {
    if (!reg) return;
    if (reg->providers) {
        for (provider_t* p = reg->providers; p->name; ++p) {
            p->ops->destroy(p->ctx);
            free((void*)p->name);
            if (p->models) {
                for (char** m = p->models; *m; ++m) free(*m);
                free(p->models);
            }
        }
        free(reg->providers);
    }
    free(reg);
}

const provider_t* provider_registry_find(provider_registry_t* reg, const char* model) {
    if (!reg || !reg->providers) return NULL;
    for (provider_t* p = reg->providers; p->name; ++p) {
        if (!p->models) continue;
        for (char** m = p->models; *m; ++m) {
            if (strcmp(*m, model) == 0) return p;
        }
    }
    return NULL;
}