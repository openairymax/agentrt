// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file svc_config.h
 * @brief 配置服务兼容层
 */

#ifndef SVC_CONFIG_H
#define SVC_CONFIG_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 错误码兼容 ==================== */

#define SVC_OK AGENTOS_OK
#define SVC_ERR_INVALID_PARAM AGENTOS_ERR_INVALID_PARAM
#define SVC_ERR_IO AGENTOS_ERR_IO
#define SVC_ERR_OUT_OF_MEMORY AGENTOS_ERR_OUT_OF_MEMORY
#define SVC_ERR_PARSE_ERROR AGENTOS_ERR_PARSE_ERROR
#define SVC_ERR_RPC (-5001)

/* ==================== 类型兼容 ==================== */

typedef struct {
    char *service_name;
    char *listen_addr;
    int log_level;
} svc_config_t;

/* ==================== 兼容性函数包装 ==================== */

static inline int svc_config_load(const char *path __attribute__((unused)),
                                  svc_config_t **out_config)
{
    if (!out_config)
        return SVC_ERR_INVALID_PARAM;

    *out_config = NULL;
    svc_config_t *cfg = (svc_config_t *)AGENTOS_CALLOC(1, sizeof(svc_config_t));
    if (!cfg)
        return SVC_ERR_OUT_OF_MEMORY;

    cfg->service_name = AGENTOS_STRDUP("agentos-service");
    cfg->listen_addr = AGENTOS_STRDUP(":8080");
    cfg->log_level = 3;

    if (!cfg->service_name || !cfg->listen_addr) {
        AGENTOS_FREE(cfg->service_name);
        AGENTOS_FREE(cfg->listen_addr);
        AGENTOS_FREE(cfg);
        return SVC_ERR_OUT_OF_MEMORY;
    }

    *out_config = cfg;
    return SVC_OK;
}

static inline void svc_config_free(svc_config_t *config)
{
    if (!config)
        return;
    AGENTOS_FREE(config->service_name);
    AGENTOS_FREE(config->listen_addr);
    AGENTOS_FREE(config);
}

static inline const char *svc_config_get_name(const svc_config_t *config)
{
    return config ? config->service_name : NULL;
}

static inline const char *svc_config_get_listen(const svc_config_t *config)
{
    return config ? config->listen_addr : NULL;
}

static inline int svc_config_get_log_level(const svc_config_t *config)
{
    return config ? config->log_level : 3;
}

static inline const char *svc_config_get_string(const svc_config_t *config, const char *key)
{
    if (!config)
        return NULL;
    if (!key)
        return NULL;
    if (strcmp(key, "service_name") == 0)
        return config->service_name;
    if (strcmp(key, "listen_addr") == 0)
        return config->listen_addr;
    return NULL;
}

static inline int svc_config_get_int(const svc_config_t *config, const char *key)
{
    if (!config || !key)
        return 0;
    if (strcmp(key, "log_level") == 0)
        return config->log_level;
    return 0;
}

static inline int svc_config_get_bool(const svc_config_t *config, const char *key)
{
    if (!config || !key)
        return 0;
    if (strcmp(key, "log_level") == 0)
        return config->log_level > 0;
    return 0;
}

static inline void svc_config_destroy(svc_config_t *config)
{
    svc_config_free(config);
}

#ifdef __cplusplus
}
#endif

#endif /* SVC_CONFIG_H */
