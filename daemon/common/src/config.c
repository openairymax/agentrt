/**
 * @file config.c
 * @brief 配置加载实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "svc_common.h"
#include <yaml.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief 安全复制字符串，检查返回值
 * @param src 源字符串
 * @param dest 目标指针地址
 * @return 0 成功，非0 失败
 */
static int safe_strdup(const char* src, char** dest) {
    if (!src || !dest) {
        return SVC_ERR_INVALID_PARAM;
    }
    char* tmp = strdup(src);
    if (!tmp) {
        return SVC_ERR_OUT_OF_MEMORY;
    }
    *dest = tmp;
    return SVC_OK;
}

/**
 * @brief 清理配置资源
 * @param cfg 配置指针
 */
static void cleanup_config(svc_config_t* cfg) {
    if (!cfg) return;
    if (cfg->service_name) {
        free(cfg->service_name);
        cfg->service_name = NULL;
    }
    if (cfg->listen_addr) {
        free(cfg->listen_addr);
        cfg->listen_addr = NULL;
    }
}

int svc_config_load(const char* path, svc_config_t** out_config) {
    if (!path || !out_config) {
        return SVC_ERR_INVALID_PARAM;
    }

    *out_config = NULL;

    FILE* f = fopen(path, "rb");
    if (!f) {
        return SVC_ERR_IO;
    }

    yaml_parser_t parser;
    yaml_event_t event;
    
    if (!yaml_parser_initialize(&parser)) {
        fclose(f);
        return SVC_ERR_OUT_OF_MEMORY;
    }
    yaml_parser_set_input_file(&parser, f);

    svc_config_t* cfg = (svc_config_t*)calloc(1, sizeof(svc_config_t));
    if (!cfg) {
        yaml_parser_delete(&parser);
        fclose(f);
        return SVC_ERR_OUT_OF_MEMORY;
    }

    int done = 0;
    int in_service = 0;
    char* key = NULL;
    int result = SVC_OK;

    while (!done && result == SVC_OK) {
        if (!yaml_parser_parse(&parser, &event)) {
            result = SVC_ERR_PARSE_ERROR;
            break;
        }
        switch (event.type) {
            case YAML_SCALAR_EVENT:
                if (!in_service) {
                    if (strcmp((char*)event.data.scalar.value, "service") == 0) {
                        in_service = 1;
                    }
                } else {
                    if (!key) {
                        result = safe_strdup((char*)event.data.scalar.value, &key);
                    } else {
                        const char* val = (char*)event.data.scalar.value;
                        if (strcmp(key, "name") == 0) {
                            result = safe_strdup(val, &cfg->service_name);
                        } else if (strcmp(key, "listen") == 0) {
                            result = safe_strdup(val, &cfg->listen_addr);
                        } else if (strcmp(key, "log_level") == 0) {
                            cfg->log_level = atoi(val);
                        }
                        free(key);
                        key = NULL;
                    }
                }
                break;
            case YAML_MAPPING_END_EVENT:
                if (in_service) {
                    in_service = 0;
                }
                break;
            case YAML_STREAM_END_EVENT:
                done = 1;
                break;
            default:
                break;
        }
        yaml_event_delete(&event);
    }

    if (key) {
        free(key);
        key = NULL;
    }

    yaml_parser_delete(&parser);
    fclose(f);

    if (result != SVC_OK) {
        cleanup_config(cfg);
        free(cfg);
        return result;
    }

    if (!cfg->service_name) {
        result = safe_strdup("unknown", &cfg->service_name);
        if (result != SVC_OK) {
            cleanup_config(cfg);
            free(cfg);
            return result;
        }
    }
    if (!cfg->listen_addr) {
        result = safe_strdup(":0", &cfg->listen_addr);
        if (result != SVC_OK) {
            cleanup_config(cfg);
            free(cfg);
            return result;
        }
    }
    if (cfg->log_level == 0) {
        cfg->log_level = 3;
    }

    *out_config = cfg;
    return SVC_OK;
}

void svc_config_free(svc_config_t* config) {
    if (!config) return;
    cleanup_config(config);
    free(config);
}
