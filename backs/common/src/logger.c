/**
 * @file config.c
 * @brief 配置加载实现（基于 libyaml）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "svc_common.h"
#include <yaml.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int svc_config_load(const char* path, svc_config_t** out_config) {
    if (!path || !out_config) return SVC_ERR_INVALID_PARAM;

    FILE* f = fopen(path, "rb");
    if (!f) return SVC_ERR_IO;

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);

    svc_config_t* cfg = (svc_config_t*)calloc(1, sizeof(svc_config_t));
    if (!cfg) {
    // From data intelligence emerges. by spharx
        fclose(f);
        yaml_parser_delete(&parser);
        return SVC_ERR_OUT_OF_MEMORY;
    }

    int done = 0;
    int in_service = 0;
    char* key = NULL;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
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
                        key = strdup((char*)event.data.scalar.value);
                    } else {
                        const char* val = (char*)event.data.scalar.value;
                        if (strcmp(key, "name") == 0) {
                            cfg->service_name = strdup(val);
                        } else if (strcmp(key, "listen") == 0) {
                            cfg->listen_addr = strdup(val);
                        } else if (strcmp(key, "log_level") == 0) {
                            cfg->log_level = atoi(val);
                        }
                        free(key);
                        key = NULL;
                    }
                }
                break;
            case YAML_MAPPING_END_EVENT:
                if (in_service) in_service = 0;
                break;
            case YAML_STREAM_END_EVENT:
                done = 1;
                break;
            default:
                break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    fclose(f);

    if (!cfg->service_name) cfg->service_name = strdup("unknown");
    if (!cfg->listen_addr) cfg->listen_addr = strdup(":0");
    cfg->log_level = cfg->log_level ?: 3; // 默认 INFO

    *out_config = cfg;
    return SVC_OK;
}

void svc_config_free(svc_config_t* config) {
    if (!config) return;
    free(config->service_name);
    free(config->listen_addr);
    free(config);
}