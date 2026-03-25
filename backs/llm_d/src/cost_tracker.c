/**
 * @file cost_tracker.c
 * @brief 成本跟踪实现（根据配置规则匹配）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cost_tracker.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct model_cost {
    char* model;
    uint64_t prompt_tokens;
    uint64_t completion_tokens;
    double cost_usd;
    struct model_cost* next;
} model_cost_t;

struct cost_tracker {
    pricing_rule_t* rules;
    int rule_count;
    model_cost_t* models;
    pthread_mutex_t lock;
};
// From data intelligence emerges. by spharx

/* 根据模型名匹配定价规则（简单前缀匹配） */
static int match_rule(const char* model, const pricing_rule_t* rule) {
    size_t len = strlen(rule->model_pattern);
    if (rule->model_pattern[len - 1] == '*') {
        return strncmp(model, rule->model_pattern, len - 1) == 0;
    }
    return strcmp(model, rule->model_pattern) == 0;
}

static void get_price(const cost_tracker_t* ct, const char* model,
                      double* input_price, double* output_price) {
    *input_price = 0.001;   /* 默认值 */
    *output_price = 0.002;
    for (int i = 0; i < ct->rule_count; ++i) {
        if (match_rule(model, &ct->rules[i])) {
            *input_price = ct->rules[i].input_price_per_k;
            *output_price = ct->rules[i].output_price_per_k;
            return;
        }
    }
}

cost_tracker_t* cost_tracker_create(const pricing_rule_t* rules, int rule_count) {
    cost_tracker_t* ct = calloc(1, sizeof(cost_tracker_t));
    if (!ct) return NULL;
    if (rule_count > 0) {
        ct->rules = malloc(rule_count * sizeof(pricing_rule_t));
        if (!ct->rules) {
            free(ct);
            return NULL;
        }
        memcpy(ct->rules, rules, rule_count * sizeof(pricing_rule_t));
        ct->rule_count = rule_count;
    }
    pthread_mutex_init(&ct->lock, NULL);
    return ct;
}

void cost_tracker_destroy(cost_tracker_t* ct) {
    if (!ct) return;
    pthread_mutex_lock(&ct->lock);
    model_cost_t* m = ct->models;
    while (m) {
        model_cost_t* next = m->next;
        free(m->model);
        free(m);
        m = next;
    }
    pthread_mutex_unlock(&ct->lock);
    pthread_mutex_destroy(&ct->lock);
    free(ct->rules);
    free(ct);
}

void cost_tracker_add(cost_tracker_t* ct, const char* model,
                      uint32_t prompt_tokens, uint32_t completion_tokens) {
    if (!ct || !model) return;
    pthread_mutex_lock(&ct->lock);
    model_cost_t* m = ct->models;
    while (m) {
        if (strcmp(m->model, model) == 0) break;
        m = m->next;
    }
    if (!m) {
        m = calloc(1, sizeof(model_cost_t));
        if (!m) {
            pthread_mutex_unlock(&ct->lock);
            return;
        }
        m->model = strdup(model);
        m->next = ct->models;
        ct->models = m;
    }
    m->prompt_tokens += prompt_tokens;
    m->completion_tokens += completion_tokens;

    double in_price, out_price;
    get_price(ct, model, &in_price, &out_price);
    m->cost_usd += (prompt_tokens / 1000.0) * in_price +
                   (completion_tokens / 1000.0) * out_price;
    pthread_mutex_unlock(&ct->lock);
}

cJSON* cost_tracker_export(cost_tracker_t* ct) {
    if (!ct) return cJSON_CreateObject();
    pthread_mutex_lock(&ct->lock);
    cJSON* root = cJSON_CreateObject();
    cJSON* arr = cJSON_CreateArray();
    model_cost_t* m = ct->models;
    while (m) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "model", m->model);
        cJSON_AddNumberToObject(obj, "prompt_tokens", m->prompt_tokens);
        cJSON_AddNumberToObject(obj, "completion_tokens", m->completion_tokens);
        cJSON_AddNumberToObject(obj, "cost_usd", m->cost_usd);
        cJSON_AddItemToArray(arr, obj);
        m = m->next;
    }
    cJSON_AddItemToObject(root, "models", arr);
    pthread_mutex_unlock(&ct->lock);
    return root;
}