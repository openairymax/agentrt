/**
 * @file sanitizer_core.c
 * @brief 输入净化器核心实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "sanitizer.h"
#include "sanitizer_rules.h"
#include "sanitizer_cache.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct sanitizer {
    uint32_t            max_len;
    rule_set_t*         rules;
    sanitizer_cache_t*  cache;
    pthread_mutex_t     lock;   // 保护规则重载等
};

sanitizer_t* sanitizer_create(uint32_t max_input_len,
                               const char* rules_path,
                               size_t cache_capacity,
                               uint32_t cache_ttl_ms) {
    sanitizer_t* s = (sanitizer_t*)calloc(1, sizeof(sanitizer_t));
    if (!s) return NULL;

    s->max_len = max_input_len;

    if (rules_path) {
        s->rules = rule_set_load(rules_path);
        if (!s->rules) {
            AGENTOS_LOG_WARN("sanitizer: failed to load rules from %s, continuing without rules", rules_path);
        }
    }

    if (cache_capacity > 0) {
        s->cache = sanitizer_cache_create(cache_capacity, cache_ttl_ms);
        if (!s->cache) {
            AGENTOS_LOG_ERROR("sanitizer: failed to create cache");
            rule_set_destroy(s->rules);
            free(s);
            return NULL;
        }
    }

    pthread_mutex_init(&s->lock, NULL);
    return s;
}

void sanitizer_destroy(sanitizer_t* s) {
    if (!s) return;
    pthread_mutex_destroy(&s->lock);
    if (s->rules) rule_set_destroy(s->rules);
    if (s->cache) sanitizer_cache_destroy(s->cache);
    free(s);
}

int sanitizer_clean(sanitizer_t* s, const char* input,
                    char** out_cleaned, int* out_risk_level) {
    if (!s || !input || !out_cleaned || !out_risk_level) return -1;

    // 尝试从缓存获取
    if (s->cache) {
        int hit = sanitizer_cache_get(s->cache, input, out_cleaned, out_risk_level);
        if (hit == 1) {
            return 0;
        }
    }

    // 复制输入并截断
    size_t len = strlen(input);
    if (len > s->max_len) len = s->max_len;
    char* cleaned = (char*)malloc(len + 1);
    if (!cleaned) return -1;
    strncpy(cleaned, input, len);
    cleaned[len] = '\0';
    int risk = 0;

    // 应用规则
    if (s->rules) {
        // 尝试重载规则（可选）
        pthread_mutex_lock(&s->lock);
        rule_set_reload(s->rules);  // 检查文件是否更新
        pthread_mutex_unlock(&s->lock);

        char* after_rules = NULL;
        int rule_risk = rule_set_apply(s->rules, cleaned, &after_rules);
        if (after_rules) {
            free(cleaned);
            cleaned = after_rules;
            risk = rule_risk;
        } else {
            // 规则应用失败，但可能仍有风险标记
            risk = rule_risk;
        }
    }

    *out_cleaned = cleaned;
    *out_risk_level = risk;

    // 存入缓存
    if (s->cache) {
        sanitizer_cache_put(s->cache, input, cleaned, risk);
    }

    return 0;
}