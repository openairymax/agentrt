// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file svc_config.h
 * @brief 配置服务兼容层
 */

#ifndef SVC_CONFIG_H
#define SVC_CONFIG_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fallback definitions when config_unified is unavailable */
#ifndef CONFIG_SUCCESS
#define CONFIG_SUCCESS              0
#endif
#ifndef CONFIG_ERROR_INVALID_PARAM
#define CONFIG_ERROR_INVALID_PARAM  (-1)
#endif
#ifndef CONFIG_ERROR_IO
#define CONFIG_ERROR_IO            (-2)
#endif
#ifndef CONFIG_ERROR_OUT_OF_MEMORY
#define CONFIG_ERROR_OUT_OF_MEMORY (-3)
#endif
#ifndef CONFIG_ERROR_PARSE
#define CONFIG_ERROR_PARSE         (-4)
#endif

/* ==================== 错误码兼容 ==================== */

#define SVC_OK                  CONFIG_SUCCESS
#define SVC_ERR_INVALID_PARAM   CONFIG_ERROR_INVALID_PARAM
#define SVC_ERR_IO              CONFIG_ERROR_IO
#define SVC_ERR_OUT_OF_MEMORY   CONFIG_ERROR_OUT_OF_MEMORY
#define SVC_ERR_PARSE_ERROR     CONFIG_ERROR_PARSE
#define SVC_ERR_RPC             (-5001)

/* ==================== 类型兼容 ==================== */

typedef struct {
    char* service_name;
    char* listen_addr;
    int log_level;
} svc_config_t;

/* ==================== 兼容性函数包装 ==================== */

static inline int svc_config_load(const char* path, svc_config_t** out_config) {
    (void)path;
    if (!out_config) return SVC_ERR_INVALID_PARAM;

    *out_config = NULL;
    svc_config_t* cfg = (svc_config_t*)calloc(1, sizeof(svc_config_t));
    if (!cfg) return SVC_ERR_OUT_OF_MEMORY;

    cfg->service_name = strdup("agentos-service");
    cfg->listen_addr = strdup(":8080");
    cfg->log_level = 3;

    if (!cfg->service_name || !cfg->listen_addr) {
        free(cfg->service_name);
        free(cfg->listen_addr);
        free(cfg);
        return SVC_ERR_OUT_OF_MEMORY;
    }

    *out_config = cfg;
    return SVC_OK;
}

static inline void svc_config_free(svc_config_t* config) {
    if (!config) return;
    free(config->service_name);
    free(config->listen_addr);
    free(config);
}

static inline const char* svc_config_get_name(const svc_config_t* config) {
    return config ? config->service_name : NULL;
}

static inline const char* svc_config_get_listen(const svc_config_t* config) {
    return config ? config->listen_addr : NULL;
}

static inline int svc_config_get_log_level(const svc_config_t* config) {
    return config ? config->log_level : 3;
}

#ifdef __cplusplus
}
#endif

#endif /* SVC_CONFIG_H */
