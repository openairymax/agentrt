/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * sanitizer_rules.c - Sanitizer Rules Manager Implementation
 */

/**
 * @file sanitizer_rules.c
 * @brief Sanitizer Rules Manager Implementation
 * @author Spharx
 * @date 2024
 */

#include "sanitizer_rules.h"
#include <stdlib.h>
#include <string.h>

struct sanitize_rule {
    char* pattern;
    char* replacement;
    struct sanitize_rule* next;
};

struct sanitizer_rules {
    struct sanitize_rule* head;
    size_t count;
    cupolas_mutex_t lock;
};

sanitizer_rules_t* sanitizer_rules_create(const char* rules_path) {
    sanitizer_rules_t* rules = (sanitizer_rules_t*)cupolas_mem_alloc(sizeof(sanitizer_rules_t));
    if (!rules) return NULL;
    
    memset(rules, 0, sizeof(sanitizer_rules_t));
    
    if (cupolas_mutex_init(&rules->lock) != cupolas_OK) {
        cupolas_mem_free(rules);
        return NULL;
    }
    
    return rules;
}

void sanitizer_rules_destroy(sanitizer_rules_t* rules) {
    if (!rules) return;
    
    cupolas_mutex_lock(&rules->lock);
    
    struct sanitize_rule* rule = rules->head;
    while (rule) {
        struct sanitize_rule* next = rule->next;
        cupolas_mem_free(rule->pattern);
        cupolas_mem_free(rule->replacement);
        cupolas_mem_free(rule);
        rule = next;
    }
    
    cupolas_mutex_unlock(&rules->lock);
    cupolas_mutex_destroy(&rules->lock);
    cupolas_mem_free(rules);
}

int sanitizer_rules_add(sanitizer_rules_t* rules, const char* pattern, const char* replacement) {
    if (!rules || !pattern) return cupolas_ERROR_INVALID_ARG;
    
    struct sanitize_rule* rule = (struct sanitize_rule*)cupolas_mem_alloc(sizeof(struct sanitize_rule));
    if (!rule) return cupolas_ERROR_NO_MEMORY;
    
    memset(rule, 0, sizeof(struct sanitize_rule));
    
    rule->pattern = cupolas_strdup(pattern);
    if (!rule->pattern) {
        cupolas_mem_free(rule);
        return cupolas_ERROR_NO_MEMORY;
    }
    
    if (replacement) {
        rule->replacement = cupolas_strdup(replacement);
        if (!rule->replacement) {
            cupolas_mem_free(rule->pattern);
            cupolas_mem_free(rule);
            return cupolas_ERROR_NO_MEMORY;
        }
    }
    
    cupolas_mutex_lock(&rules->lock);
    
    rule->next = rules->head;
    rules->head = rule;
    rules->count++;
    
    cupolas_mutex_unlock(&rules->lock);
    
    return cupolas_OK;
}

int sanitizer_rules_apply(sanitizer_rules_t* rules, const char* input, char* output, size_t output_size) {
    if (!rules || !input || !output || output_size == 0) {
        return cupolas_ERROR_INVALID_ARG;
    }
    
    cupolas_mutex_lock(&rules->lock);
    
    strncpy(output, input, output_size - 1);
    output[output_size - 1] = '\0';
    
    struct sanitize_rule* rule = rules->head;
    while (rule) {
        if (strstr(output, rule->pattern) != NULL) {
            if (rule->replacement) {
                char* found = strstr(output, rule->pattern);
                if (found) {
                    size_t pat_len = strlen(rule->pattern);
                    size_t rep_len = strlen(rule->replacement);
                    size_t out_len = strlen(output);
                    
                    if (out_len - pat_len + rep_len < output_size) {
                        memmove(found + rep_len, found + pat_len, out_len - (found - output) - pat_len);
                        memcpy(found, rule->replacement, rep_len);
                    }
                }
            } else {
                cupolas_mutex_unlock(&rules->lock);
                return cupolas_ERROR_UNKNOWN;
            }
        }
        rule = rule->next;
    }
    
    cupolas_mutex_unlock(&rules->lock);
    
    return cupolas_OK;
}

void sanitizer_rules_clear(sanitizer_rules_t* rules) {
    if (!rules) return;
    
    cupolas_mutex_lock(&rules->lock);
    
    struct sanitize_rule* rule = rules->head;
    while (rule) {
        struct sanitize_rule* next = rule->next;
        cupolas_mem_free(rule->pattern);
        cupolas_mem_free(rule->replacement);
        cupolas_mem_free(rule);
        rule = next;
    }
    
    rules->head = NULL;
    rules->count = 0;
    
    cupolas_mutex_unlock(&rules->lock);
}
