/**
 * @file sanitizer_rules.c
 * @brief 净化规则管理器实现
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
    domes_mutex_t lock;
};

sanitizer_rules_t* sanitizer_rules_create(const char* rules_path) {
    sanitizer_rules_t* rules = (sanitizer_rules_t*)domes_mem_alloc(sizeof(sanitizer_rules_t));
    if (!rules) return NULL;
    
    memset(rules, 0, sizeof(sanitizer_rules_t));
    
    if (domes_mutex_init(&rules->lock) != DOMES_OK) {
        domes_mem_free(rules);
        return NULL;
    }
    
    return rules;
}

void sanitizer_rules_destroy(sanitizer_rules_t* rules) {
    if (!rules) return;
    
    domes_mutex_lock(&rules->lock);
    
    struct sanitize_rule* rule = rules->head;
    while (rule) {
        struct sanitize_rule* next = rule->next;
        domes_mem_free(rule->pattern);
        domes_mem_free(rule->replacement);
        domes_mem_free(rule);
        rule = next;
    }
    
    domes_mutex_unlock(&rules->lock);
    domes_mutex_destroy(&rules->lock);
    domes_mem_free(rules);
}

int sanitizer_rules_add(sanitizer_rules_t* rules, const char* pattern, const char* replacement) {
    if (!rules || !pattern) return DOMES_ERROR_INVALID_ARG;
    
    struct sanitize_rule* rule = (struct sanitize_rule*)domes_mem_alloc(sizeof(struct sanitize_rule));
    if (!rule) return DOMES_ERROR_NO_MEMORY;
    
    memset(rule, 0, sizeof(struct sanitize_rule));
    
    rule->pattern = domes_strdup(pattern);
    if (!rule->pattern) {
        domes_mem_free(rule);
        return DOMES_ERROR_NO_MEMORY;
    }
    
    if (replacement) {
        rule->replacement = domes_strdup(replacement);
        if (!rule->replacement) {
            domes_mem_free(rule->pattern);
            domes_mem_free(rule);
            return DOMES_ERROR_NO_MEMORY;
        }
    }
    
    domes_mutex_lock(&rules->lock);
    
    rule->next = rules->head;
    rules->head = rule;
    rules->count++;
    
    domes_mutex_unlock(&rules->lock);
    
    return DOMES_OK;
}

int sanitizer_rules_apply(sanitizer_rules_t* rules, const char* input, char* output, size_t output_size) {
    if (!rules || !input || !output || output_size == 0) {
        return DOMES_ERROR_INVALID_ARG;
    }
    
    domes_mutex_lock(&rules->lock);
    
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
                domes_mutex_unlock(&rules->lock);
                return DOMES_ERROR_UNKNOWN;
            }
        }
        rule = rule->next;
    }
    
    domes_mutex_unlock(&rules->lock);
    
    return DOMES_OK;
}

void sanitizer_rules_clear(sanitizer_rules_t* rules) {
    if (!rules) return;
    
    domes_mutex_lock(&rules->lock);
    
    struct sanitize_rule* rule = rules->head;
    while (rule) {
        struct sanitize_rule* next = rule->next;
        domes_mem_free(rule->pattern);
        domes_mem_free(rule->replacement);
        domes_mem_free(rule);
        rule = next;
    }
    
    rules->head = NULL;
    rules->count = 0;
    
    domes_mutex_unlock(&rules->lock);
}
