/**
 * @file config_loader.c
 * @brief 配置加载器实现（使用libyaml）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "config_loader.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

agentos_error_t agentos_config_load(const char* path, char** out_json) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        AGENTOS_LOG_ERROR("Failed to open config file: %s", path);
        return AGENTOS_ENOENT;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        return AGENTOS_EIO;
    }

    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return AGENTOS_ENOMEM;
    }
    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);

    *out_json = buffer;
    return AGENTOS_SUCCESS;
}

/**
 * 解析YAML并提取调度权重（假设YAML格式如：
 * scheduler:
 *   weights:
 *     cost: 0.3
 *     performance: 0.4
 *     trust: 0.3
 */
agentos_error_t agentos_config_parse_weights(
    const char* config_json,
    float* out_cost_weight,
    float* out_perf_weight,
    float* out_trust_weight) {

    yaml_parser_t parser;
    yaml_event_t event;

    if (!config_json) return AGENTOS_EINVAL;

    // 初始化解析器
    if (!yaml_parser_initialize(&parser)) {
        return AGENTOS_ENOMEM;
    }

    yaml_parser_set_input_string(&parser, (const unsigned char*)config_json, strlen(config_json));

    int in_scheduler = 0;
    int in_weights = 0;

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            yaml_parser_delete(&parser);
            return AGENTOS_EIO;
        }

        if (event.type == YAML_SCALAR_EVENT) {
            const char* value = (const char*)event.data.scalar.value;
            if (in_scheduler && in_weights) {
                if (strcmp(value, "cost") == 0) {
                    // 下一个事件应该是标量值
                    yaml_parser_parse(&parser, &event);
                    if (event.type == YAML_SCALAR_EVENT) {
                        *out_cost_weight = (float)atof((const char*)event.data.scalar.value);
                    }
                } else if (strcmp(value, "performance") == 0) {
                    yaml_parser_parse(&parser, &event);
                    if (event.type == YAML_SCALAR_EVENT) {
                        *out_perf_weight = (float)atof((const char*)event.data.scalar.value);
                    }
                } else if (strcmp(value, "trust") == 0) {
                    yaml_parser_parse(&parser, &event);
                    if (event.type == YAML_SCALAR_EVENT) {
                        *out_trust_weight = (float)atof((const char*)event.data.scalar.value);
                    }
                }
            } else if (strcmp(value, "scheduler") == 0) {
                in_scheduler = 1;
            } else if (in_scheduler && strcmp(value, "weights") == 0) {
                in_weights = 1;
            }
        } else if (event.type == YAML_MAPPING_END_EVENT) {
            if (in_weights) in_weights = 0;
            else if (in_scheduler) in_scheduler = 0;
        }

        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    return AGENTOS_SUCCESS;
}