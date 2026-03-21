/**
 * @file sanitizer_rules.c
 * @brief 净化规则管理器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "sanitizer_rules.h"
#include "logger.h"
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* 从 JSON 对象解析一条规则 */
static rule_t* parse_rule(const cJSON* obj) {
    const cJSON* type_item = cJSON_GetObjectItem(obj, "type");
    const cJSON* pattern_item = cJSON_GetObjectItem(obj, "pattern");
    const cJSON* replace_item = cJSON_GetObjectItem(obj, "replace");

    if (!cJSON_IsString(type_item) || !cJSON_IsString(pattern_item)) {
        AGENTOS_LOG_ERROR("sanitizer: rule missing type or pattern");
        return NULL;
    }
    // From data intelligence emerges. by spharx

    const char* type_str = type_item->valuestring;
    const char* pattern_str = pattern_item->valuestring;

    rule_t* r = (rule_t*)calloc(1, sizeof(rule_t));
    if (!r) return NULL;

    if (strcmp(type_str, "block") == 0) {
        r->type = RULE_TYPE_BLOCK;
    } else if (strcmp(type_str, "warn") == 0) {
        r->type = RULE_TYPE_WARN;
    } else {
        AGENTOS_LOG_ERROR("sanitizer: unknown rule type: %s", type_str);
        free(r);
        return NULL;
    }

    // 编译正则表达式
    int regflags = REG_EXTENDED | REG_ICASE;
    int ret = regcomp(&r->pattern, pattern_str, regflags);
    if (ret != 0) {
        char errbuf[128];
        regerror(ret, &r->pattern, errbuf, sizeof(errbuf));
        AGENTOS_LOG_ERROR("sanitizer: regex compilation failed: %s", errbuf);
        free(r);
        return NULL;
    }
    r->compiled = 1;

    if (cJSON_IsString(replace_item)) {
        r->replacement = strdup(replace_item->valuestring);
        if (!r->replacement) {
            regfree(&r->pattern);
            free(r);
            return NULL;
        }
    }

    return r;
}

/* 加载规则文件 */
static rule_t* load_rules_from_file(const char* path, time_t* out_mtime) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        AGENTOS_LOG_ERROR("sanitizer: failed to open rules file %s: %s",
                          path, strerror(errno));
        return NULL;
    }

    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        fclose(f);
        return NULL;
    }
    if (out_mtime) *out_mtime = st.st_mtime;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return NULL;
    }

    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    fread(content, 1, size, f);
    fclose(f);
    content[size] = '\0';

    cJSON* root = cJSON_Parse(content);
    free(content);
    if (!root) {
        AGENTOS_LOG_ERROR("sanitizer: invalid JSON in rules file");
        return NULL;
    }

    const cJSON* rules_arr = cJSON_GetObjectItem(root, "rules");
    if (!cJSON_IsArray(rules_arr)) {
        cJSON_Delete(root);
        AGENTOS_LOG_ERROR("sanitizer: rules file missing 'rules' array");
        return NULL;
    }

    rule_t* head = NULL;
    rule_t* tail = NULL;
    int arr_size = cJSON_GetArraySize(rules_arr);
    for (int i = 0; i < arr_size; i++) {
        const cJSON* item = cJSON_GetArrayItem(rules_arr, i);
        rule_t* r = parse_rule(item);
        if (!r) continue;
        if (!head) {
            head = r;
            tail = r;
        } else {
            tail->next = r;
            tail = r;
        }
    }

    cJSON_Delete(root);
    return head;
}

/* 释放规则链表 */
static void free_rules(rule_t* rules) {
    while (rules) {
        rule_t* next = rules->next;
        if (rules->compiled) regfree(&rules->pattern);
        free(rules->replacement);
        free(rules);
        rules = next;
    }
}

rule_set_t* rule_set_load(const char* path) {
    if (!path) return NULL;
    rule_set_t* rs = (rule_set_t*)calloc(1, sizeof(rule_set_t));
    if (!rs) return NULL;

    pthread_rwlock_init(&rs->rwlock, NULL);
    rs->path = strdup(path);
    if (!rs->path) {
        free(rs);
        return NULL;
    }

    time_t mtime = 0;
    rs->rules = load_rules_from_file(path, &mtime);
    rs->last_load_time = mtime;
    if (!rs->rules) {
        AGENTOS_LOG_WARN("sanitizer: no rules loaded from %s", path);
        // 仍然返回一个空的规则集，允许后续重载
    }
    return rs;
}

int rule_set_reload(rule_set_t* rs) {
    if (!rs || !rs->path) return -1;
    struct stat st;
    if (stat(rs->path, &st) != 0) {
        AGENTOS_LOG_ERROR("sanitizer: cannot stat rules file %s", rs->path);
        return -1;
    }
    if (st.st_mtime <= rs->last_load_time) {
        return 0; // 未更新
    }
    time_t new_mtime = 0;
    rule_t* new_rules = load_rules_from_file(rs->path, &new_mtime);
    if (!new_rules) {
        AGENTOS_LOG_ERROR("sanitizer: failed to reload rules");
        return -1;
    }
    pthread_rwlock_wrlock(&rs->rwlock);
    free_rules(rs->rules);
    rs->rules = new_rules;
    rs->last_load_time = new_mtime;
    pthread_rwlock_unlock(&rs->rwlock);
    AGENTOS_LOG_INFO("sanitizer: rules reloaded from %s", rs->path);
    return 0;
}

/* 在字符串中替换所有匹配项 */
static char* replace_all(const char* input, const regex_t* regex, const char* replacement) {
    regmatch_t matches[10];
    const char* p = input;
    char* result = NULL;
    size_t result_len = 0;
    size_t result_cap = 0;

    while (regexec(regex, p, 10, matches, 0) == 0) {
        int match_start = matches[0].rm_so;
        int match_end = matches[0].rm_eo;
        size_t prefix_len = match_start;

        // 计算新字符串大小
        size_t need = result_len + prefix_len;
        if (replacement) need += strlen(replacement);
        // 剩余部分
        const char* rest = p + match_end;
        need += strlen(rest) + 1;

        if (need > result_cap) {
            result_cap = need + 1024;
            char* new_result = realloc(result, result_cap);
            if (!new_result) {
                free(result);
                return NULL;
            }
            result = new_result;
        }

        // 复制前缀
        memcpy(result + result_len, p, prefix_len);
        result_len += prefix_len;

        // 复制替换
        if (replacement) {
            size_t repl_len = strlen(replacement);
            memcpy(result + result_len, replacement, repl_len);
            result_len += repl_len;
        }

        // 更新 p 到剩余部分
        p = rest;
    }

    // 复制剩余部分
    size_t rest_len = strlen(p);
    if (result_len + rest_len + 1 > result_cap) {
        result_cap = result_len + rest_len + 1;
        char* new_result = realloc(result, result_cap);
        if (!new_result) {
            free(result);
            return NULL;
        }
        result = new_result;
    }
    memcpy(result + result_len, p, rest_len + 1);
    return result;
}

int rule_set_apply(rule_set_t* rs, char* str, char** out_new) {
    if (!rs || !str || !out_new) return 0;
    int max_risk = 0;
    char* current = strdup(str);
    if (!current) return 0;

    pthread_rwlock_rdlock(&rs->rwlock);
    rule_t* r = rs->rules;
    while (r) {
        int risk_increment = (r->type == RULE_TYPE_BLOCK) ? 3 : 2;
        char* new_str = replace_all(current, &r->pattern, r->replacement);
        if (new_str) {
            free(current);
            current = new_str;
            if (risk_increment > max_risk) max_risk = risk_increment;
        } else {
            // 替换失败，但可能仍有匹配
            if (regexec(&r->pattern, current, 0, NULL, 0) == 0) {
                if (risk_increment > max_risk) max_risk = risk_increment;
            }
        }
        r = r->next;
    }
    pthread_rwlock_unlock(&rs->rwlock);

    *out_new = current;
    return max_risk;
}

void rule_set_destroy(rule_set_t* rs) {
    if (!rs) return;
    pthread_rwlock_destroy(&rs->rwlock);
    free_rules(rs->rules);
    free(rs->path);
    free(rs);
}