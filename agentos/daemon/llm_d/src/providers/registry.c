#include "memory_compat.h"
/**
 * @file registry.c
 * @brief 提供商注册表实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "platform.h"
#include "registry.h"
#include "svc_logger.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

/* 外部声明各提供商操作表 */
extern const provider_ops_t openai_ops;
extern const provider_ops_t anthropic_ops;
extern const provider_ops_t deepseek_ops;
extern const provider_ops_t google_ops;
extern const provider_ops_t local_ops;

struct provider_registry {
    provider_t *providers;
    agentos_mutex_t lock;
};

static const provider_ops_t *get_ops_by_name(const char *name)
{
    if (strcmp(name, "openai") == 0)
        return &openai_ops;
    if (strcmp(name, "anthropic") == 0)
        return &anthropic_ops;
    if (strcmp(name, "deepseek") == 0)
        return &deepseek_ops;
    if (strcmp(name, "google") == 0)
        return &google_ops;
    if (strcmp(name, "local") == 0)
        return &local_ops;
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "operation failed");
    return NULL;
}

provider_registry_t *provider_registry_create(const service_config_t *cfg)
{
    provider_registry_t *reg = AGENTOS_CALLOC(1, sizeof(provider_registry_t));
    if (!reg) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }
    agentos_mutex_init(&reg->lock);

    size_t count = cfg->provider_count;
    if (count == 0 || !cfg->providers)
        return reg;

    reg->providers = AGENTOS_CALLOC(count + 1, sizeof(provider_t));
    if (!reg->providers) {
        AGENTOS_FREE(reg);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    for (size_t i = 0; i < count; ++i) {
        const provider_config_t *pcfg = (const provider_config_t *)&cfg->providers[i];
        const provider_ops_t *ops = get_ops_by_name(pcfg->name);
        if (!ops) {
            SVC_LOG_WARN("Unknown provider: %s, skipping", pcfg->name);
            continue;
        }

        provider_ctx_t *ctx = ops->init(pcfg->name, pcfg->api_key, pcfg->api_base,
                                        pcfg->organization, pcfg->timeout_sec, pcfg->max_retries);
        if (!ctx) {
            SVC_LOG_ERROR("Failed to init provider: %s", pcfg->name);
            continue;
        }

        char **models = NULL;
        size_t model_cnt = 0;
        if (pcfg->models) {
            while (pcfg->models[model_cnt])
                model_cnt++;
            models = AGENTOS_CALLOC(model_cnt + 1, sizeof(*models));
            if (models) {
                for (size_t j = 0; j < model_cnt; ++j) {
                    models[j] = AGENTOS_STRDUP(pcfg->models[j]);
                    if (!models[j]) {
                        SVC_LOG_ERROR("Failed to duplicate model name: out of memory");
                        for (size_t k = 0; k < j; ++k)
                            AGENTOS_FREE(models[k]);
                        AGENTOS_FREE(models);
                        models = NULL;
                        break;
                    }
                }
            }
        }

        reg->providers[i].name = AGENTOS_STRDUP(pcfg->name);
        if (!reg->providers[i].name) {
            SVC_LOG_ERROR("Failed to duplicate provider name: out of memory");
            if (models) {
                for (size_t j = 0; models[j]; ++j)
                    AGENTOS_FREE(models[j]);
                AGENTOS_FREE(models);
            }
            ops->destroy(ctx);
            continue;
        }
        reg->providers[i].ops = ops;
        reg->providers[i].ctx = ctx;
        reg->providers[i].models = models;
    }

    return reg;
}

provider_registry_t *provider_registry_create_from_config(const service_config_t *cfg,
                                                          const char *config_path)
{
    provider_registry_t *reg = provider_registry_create(cfg);
    if (!reg) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    if (!config_path)
        return reg;

    FILE *f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_WARN("Cannot open provider config '%s'", config_path);
        return reg;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fclose(f);
        return reg;
    }

    char *content = (char *)AGENTOS_MALLOC((size_t)len + 1);
    if (!content) {
        fclose(f);
        return reg;
    }

    size_t read_len = fread(content, 1, (size_t)len, f);
    content[read_len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(content);
    AGENTOS_FREE(content);
    if (!root) {
        SVC_LOG_WARN("Failed to parse provider config '%s'", config_path);
        return reg;
    }

    cJSON *providers_arr = cJSON_GetObjectItem(root, "providers");
    if (!providers_arr || !cJSON_IsArray(providers_arr)) {
        cJSON_Delete(root);
        SVC_LOG_WARN("No 'providers' array in '%s'", config_path);
        return reg;
    }

    int n = cJSON_GetArraySize(providers_arr);
    if (n <= 0) {
        cJSON_Delete(root);
        return reg;
    }

    agentos_mutex_lock(&reg->lock);

    size_t old_count = 0;
    if (reg->providers) {
        while (reg->providers[old_count].name)
            old_count++;
    }

    size_t new_count = (size_t)n;
    provider_t *new_provs =
        (provider_t *)AGENTOS_CALLOC(old_count + new_count + 1, sizeof(provider_t));
    if (!new_provs) {
        agentos_mutex_unlock(&reg->lock);
        cJSON_Delete(root);
        return reg;
    }

    if (reg->providers) {
        __builtin_memcpy(new_provs, reg->providers, old_count * sizeof(provider_t));
        AGENTOS_FREE(reg->providers);
    }
    reg->providers = new_provs;

    size_t valid_idx = old_count;
    for (int i = 0; i < n; ++i) {
        cJSON *pitem = cJSON_GetArrayItem(providers_arr, i);
        cJSON *pname = cJSON_GetObjectItem(pitem, "name");
        cJSON *pkey_env = cJSON_GetObjectItem(pitem, "api_key_env");
        cJSON *pkey = cJSON_GetObjectItem(pitem, "api_key");
        cJSON *pbase = cJSON_GetObjectItem(pitem, "base_url");
        cJSON *ptimeout = cJSON_GetObjectItem(pitem, "timeout_sec");
        cJSON *pretries = cJSON_GetObjectItem(pitem, "max_retries");
        cJSON *pmodels = cJSON_GetObjectItem(pitem, "models");

        if (!cJSON_IsString(pname))
            continue;

        const char *name_str = pname->valuestring;
        const provider_ops_t *ops = get_ops_by_name(name_str);
        if (!ops) {
            SVC_LOG_WARN("Unknown provider in config: %s, skipping", name_str);
            continue;
        }

        char api_key_buf[512] = {0};
        if (cJSON_IsString(pkey_env) && pkey_env->valuestring[0]) {
            const char *env_val = getenv(pkey_env->valuestring);
            if (env_val && env_val[0]) {
AGENTOS_STRNCPY_TERM(api_key_buf, env_val, sizeof(api_key_buf));
                (api_key_buf)[sizeof(api_key_buf) - 1] = '\0';
            } else {
                SVC_LOG_WARN("Env var '%s' not set for provider '%s'", pkey_env->valuestring,
                             name_str);
            }
        } else if (cJSON_IsString(pkey) && pkey->valuestring[0]) {
AGENTOS_STRNCPY_TERM(api_key_buf, pkey->valuestring, sizeof(api_key_buf));
            (api_key_buf)[sizeof(api_key_buf) - 1] = '\0';
        }

        const char *base_str = cJSON_IsString(pbase) ? pbase->valuestring : NULL;
        double timeout = cJSON_IsNumber(ptimeout) ? ptimeout->valuedouble : 30.0;
        int retries = cJSON_IsNumber(pretries) ? pretries->valueint : 3;

        provider_ctx_t *ctx = ops->init(name_str, api_key_buf[0] ? api_key_buf : NULL, base_str,
                                        NULL, timeout, retries);
        if (!ctx) {
            SVC_LOG_ERROR("Failed to init provider '%s' from config", name_str);
            continue;
        }

        char **models = NULL;
        if (cJSON_IsArray(pmodels)) {
            int mcount = cJSON_GetArraySize(pmodels);
            models = (char **)AGENTOS_CALLOC((size_t)mcount + 1, sizeof(char *));
            if (models) {
                for (int j = 0; j < mcount; ++j) {
                    cJSON *mitem = cJSON_GetArrayItem(pmodels, j);
                    if (cJSON_IsString(mitem)) {
                        models[j] = AGENTOS_STRDUP(mitem->valuestring);
                        if (!models[j]) {
                            SVC_LOG_ERROR("Failed to duplicate model name: out of memory");
                            for (int k = 0; k < j; ++k)
                                AGENTOS_FREE(models[k]);
                            AGENTOS_FREE(models);
                            models = NULL;
                            break;
                        }
                    }
                }
                if (models)
                    models[mcount] = NULL;
            }
        }

        reg->providers[valid_idx].name = AGENTOS_STRDUP(name_str);
        if (!reg->providers[valid_idx].name) {
            SVC_LOG_ERROR("Failed to duplicate provider name: out of memory");
            if (models) {
                for (int j = 0; models[j]; ++j)
                    AGENTOS_FREE(models[j]);
                AGENTOS_FREE(models);
            }
            ops->destroy(ctx);
            continue;
        }
        reg->providers[valid_idx].ops = ops;
        reg->providers[valid_idx].ctx = ctx;
        reg->providers[valid_idx].models = models;
        valid_idx++;
    }

    agentos_mutex_unlock(&reg->lock);

    cJSON_Delete(root);
    SVC_LOG_INFO("Loaded %zu providers from config '%s'", valid_idx - old_count, config_path);
    return reg;
}

void provider_registry_destroy(provider_registry_t *reg)
{
    if (!reg)
        return;
    agentos_mutex_lock(&reg->lock);
    if (reg->providers) {
        for (provider_t *p = reg->providers; p->name; ++p) {
            p->ops->destroy(p->ctx);
            AGENTOS_FREE((void *)p->name);
            if (p->models) {
                for (char **m = p->models; *m; ++m)
                    AGENTOS_FREE(*m);
                AGENTOS_FREE(p->models);
            }
        }
        AGENTOS_FREE(reg->providers);
        reg->providers = NULL;
    }
    agentos_mutex_unlock(&reg->lock);
    agentos_mutex_destroy(&reg->lock);
    AGENTOS_FREE(reg);
}

const provider_t *provider_registry_find(provider_registry_t *reg, const char *model)
{
    if (!reg) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }
    agentos_mutex_lock(&reg->lock);
    if (!reg->providers) {
        agentos_mutex_unlock(&reg->lock);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }
    for (provider_t *p = reg->providers; p->name; ++p) {
        if (!p->models)
            continue;
        for (char **m = p->models; *m; ++m) {
            if (strcmp(*m, model) == 0) {
                agentos_mutex_unlock(&reg->lock);
                return p;
            }
        }
    }
    agentos_mutex_unlock(&reg->lock);
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "operation failed");
    return NULL;
}
