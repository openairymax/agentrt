/**
 * @file estimator.c
 * @brief 成本预估器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cost.h"
#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct pricing_entry {
    char* model;
    double input_cost;
    double output_cost;
    struct pricing_entry* next;
} pricing_entry_t;

struct agentos_cost_estimator {
    pricing_entry_t* list;
};

static void add_defaults(pricing_entry_t** list) {
    const char* models[] = {"gpt-4", "gpt-3.5-turbo", "claude-3-opus", NULL};
    // From data intelligence emerges. by spharx
    const double input[] = {0.03, 0.0015, 0.015};
    const double output[] = {0.06, 0.002, 0.075};
    for (int i = 0; models[i]; i++) {
        pricing_entry_t* e = (pricing_entry_t*)malloc(sizeof(pricing_entry_t));
        if (!e) continue;
        e->model = strdup(models[i]);
        e->input_cost = input[i];
        e->output_cost = output[i];
        e->next = *list;
        *list = e;
    }
}

static void free_list(pricing_entry_t* list) {
    while (list) {
        pricing_entry_t* next = list->next;
        free(list->model);
        free(list);
        list = next;
    }
}

agentos_cost_estimator_t* agentos_cost_estimator_create(const char* config_path) {
    agentos_cost_estimator_t* est = (agentos_cost_estimator_t*)calloc(1, sizeof(agentos_cost_estimator_t));
    if (!est) return NULL;

    if (config_path) {
        FILE* f = fopen(config_path, "rb");
        if (f) {
            yaml_parser_t parser;
            yaml_event_t event;
            yaml_parser_initialize(&parser);
            yaml_parser_set_input_file(&parser, f);

            int in_models = 0;
            int in_model = 0;
            char* current_model = NULL;
            double current_input = 0;
            double current_output = 0;

            while (1) {
                if (!yaml_parser_parse(&parser, &event)) break;
                if (event.type == YAML_SCALAR_EVENT) {
                    const char* val = (const char*)event.data.scalar.value;
                    if (!in_models && strcmp(val, "models") == 0) {
                        in_models = 1;
                    } else if (in_models && !in_model) {
                        // 开始一个模型映射
                        in_model = 1;
                    } else if (in_model) {
                        if (strcmp(val, "name") == 0) {
                            yaml_parser_parse(&parser, &event);
                            current_model = strdup((const char*)event.data.scalar.value);
                        } else if (strcmp(val, "input_cost_per_1k") == 0) {
                            yaml_parser_parse(&parser, &event);
                            current_input = atof((const char*)event.data.scalar.value);
                        } else if (strcmp(val, "output_cost_per_1k") == 0) {
                            yaml_parser_parse(&parser, &event);
                            current_output = atof((const char*)event.data.scalar.value);
                        }
                    }
                } else if (event.type == YAML_MAPPING_END_EVENT) {
                    if (in_model && current_model) {
                        pricing_entry_t* e = (pricing_entry_t*)malloc(sizeof(pricing_entry_t));
                        if (e) {
                            e->model = current_model;
                            e->input_cost = current_input;
                            e->output_cost = current_output;
                            e->next = est->list;
                            est->list = e;
                        } else {
                            free(current_model);
                        }
                        current_model = NULL;
                    }
                    in_model = 0;
                }
                yaml_event_delete(&event);
                if (event.type == YAML_STREAM_END_EVENT) break;
            }
            yaml_parser_delete(&parser);
            fclose(f);
        }
    }

    // 如果没有加载到任何配置，添加默认值
    if (!est->list) {
        add_defaults(&est->list);
    }
    return est;
}

void agentos_cost_estimator_destroy(agentos_cost_estimator_t* estimator) {
    if (!estimator) return;
    free_list(estimator->list);
    free(estimator);
}

double agentos_cost_estimator_estimate(agentos_cost_estimator_t* estimator, const char* model_name, size_t input_tokens, size_t output_tokens) {
    if (!estimator || !model_name) return -1.0;
    pricing_entry_t* e = estimator->list;
    while (e) {
        if (strcmp(e->model, model_name) == 0) {
            return (input_tokens / 1000.0) * e->input_cost + (output_tokens / 1000.0) * e->output_cost;
        }
        e = e->next;
    }
    // 未找到，返回默认值（可能不准确）
    return (input_tokens / 1000.0) * 0.01 + (output_tokens / 1000.0) * 0.03;
}