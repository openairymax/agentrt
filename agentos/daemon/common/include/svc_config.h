// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file svc_config.h
 * @brief 配置服务兼容层
 * 
 * 本文件是 agentos/commons/utils/config_unified 的兼容层，提供向后兼容的 API。
 * 新代码应直接使用 #include "agentos/utils/config_unified/config_unified.h"
 * 
 * @see agentos/commons/utils/config_unified/include/config_unified.h
 */

#ifndef SVC_CONFIG_H
#define SVC_CONFIG_H

/* 包含 commons 的统一配置库 */
#include "agentos/utils/config_unified/config_unified.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 错误码兼容 ==================== */

#define SVC_OK                  CONFIG_SUCCESS
#define SVC_ERR_INVALID_PARAM   CONFIG_ERROR_INVALID_PARAM
#define SVC_ERR_IO              CONFIG_ERROR_IO
#define SVC_ERR_OUT_OF_MEMORY   CONFIG_ERROR_OUT_OF_MEMORY
#define SVC_ERR_PARSE_ERROR     CONFIG_ERROR_PARSE

/* ==================== 类型兼容 ==================== */

/**
 * @brief 兼容旧配置结构
 * @deprecated 请使用 config_context_t
 */
typedef struct {
    char* service_name;
    char* listen_addr;
    int log_level;
} svc_config_t;

/* ==================== 兼容性函数包装 ==================== */

/**
 * @brief 加载配置文件（兼容层）
 * @param path 配置文件路径
 * @param out_config 输出配置指针
 * @return 0 表示成功，非 0 表示错误
 * @deprecated 请使用 config_source_create_file() + config_source_load()
 */
static inline int svc_config_load(const char* path, svc_config_t** out_config) {
    if (!path || !out_config) {
        return SVC_ERR_INVALID_PARAM;
    }
    
    *out_config = NULL;
    
    /* 使用 commons 的配置加载 */
    config_file_source_options_t file_opts = {
        .file_path = path,
        .format = "yaml"
    };
    
    config_source_t* source = config_source_create_file(&file_opts);
    if (!source) {
        return SVC_ERR_OUT_OF_MEMORY;
    }
    
    config_context_t* ctx = config_context_create("service");
    if (!ctx) {
        config_source_destroy(source);
        return SVC_ERR_OUT_OF_MEMORY;
    }
    
    config_error_t err = config_source_load(source, ctx);
    if (err != CONFIG_SUCCESS) {
        config_context_destroy(ctx);
        config_source_destroy(source);
        return SVC_ERR_PARSE_ERROR;
    }
    
    /* 创建兼容配置结构 */
    svc_config_t* cfg = (svc_config_t*)calloc(1, sizeof(svc_config_t));
    if (!cfg) {
        config_context_destroy(ctx);
        config_source_destroy(source);
        return SVC_ERR_OUT_OF_MEMORY;
    }
    
    /* 从配置上下文读取值 */
    const char* name = CONFIG_GET_STRING_SAFE(ctx, "service.name", "unknown");
    const char* listen = CONFIG_GET_STRING_SAFE(ctx, "service.listen", ":0");
    int log_level = CONFIG_GET_INT_SAFE(ctx, "service.log_level", 3);
    
    cfg->service_name = strdup(name);
    cfg->listen_addr = strdup(listen);
    cfg->log_level = log_level;
    
    if (!cfg->service_name || !cfg->listen_addr) {
        free(cfg->service_name);
        free(cfg->listen_addr);
        free(cfg);
        config_context_destroy(ctx);
        config_source_destroy(source);
        return SVC_ERR_OUT_OF_MEMORY;
    }
    
    config_context_destroy(ctx);
    config_source_destroy(source);
    
    *out_config = cfg;
    return SVC_OK;
}

/**
 * @brief 释放配置（兼容层）
 * @param config 配置指针
 * @deprecated 请使用 config_context_destroy()
 */
static inline void svc_config_free(svc_config_t* config) {
    if (!config) return;
    
    free(config->service_name);
    free(config->listen_addr);
    free(config);
}

/* ==================== 额外的便捷函数 ==================== */

/**
 * @brief 获取服务名称
 * @param config 配置指针
 * @return 服务名称
 */
static inline const char* svc_config_get_name(const svc_config_t* config) {
    return config ? config->service_name : NULL;
}

/**
 * @brief 获取监听地址
 * @param config 配置指针
 * @return 监听地址
 */
static inline const char* svc_config_get_listen(const svc_config_t* config) {
    return config ? config->listen_addr : NULL;
}

/**
 * @brief 获取日志级别
 * @param config 配置指针
 * @return 日志级别
 */
static inline int svc_config_get_log_level(const svc_config_t* config) {
    return config ? config->log_level : 3;
}

#ifdef __cplusplus
}
#endif

#endif /* SVC_CONFIG_H */
