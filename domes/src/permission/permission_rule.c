/**
 * @file permission_rule.c
 * @brief 权限规则管理器实现（基于 YAML）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "permission_rule.h"
#include "logger.h"
#include <yaml.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <unistd.h>

/* 解析规则文件 */
static permission_rule_t* parse_rules(const char* path, time_t* out_mtime) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        AGENTOS_LOG_ERROR("permission_rule: cannot open %s", path);
        return NULL;
    }
    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        fclose(f);
        return NULL;
    }
    if (out_mtime) *out_mtime = st.st_mtime;

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);

    permission_rule_t* head = NULL;
    permission_rule_t* tail = NULL;
    int in_rules = 0;
    int in_rule = 0;
    char* cur_agent = NULL;
    char* cur_action = NULL;
    char* cur_resource = NULL;
    int cur_allow = 1;

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            const char* val = (const char*)event.data.scalar.value;
            if (!in_rules && strcmp(val, "rules") == 0) {
                in_rules = 1;
            } else if (in_rules && !in_rule) {
                in_rule = 1;
            } else if (in_rule) {
                if (strcmp(val, "agent") == 0) {
                    yaml_parser_parse(&parser, &event);
                    cur_agent = strdup((const char*)event.data.scalar.value);
                } else if (strcmp(val, "action") == 0) {
                    yaml_parser_parse(&parser, &event);
                    cur_action = strdup((const char*)event.data.scalar.value);
                } else if (strcmp(val, "resource") == 0) {
                    yaml_parser_parse(&parser, &event);
                    cur_resource = strdup((const char*)event.data.scalar.value);
                } else if (strcmp(val, "effect") == 0) {
                    yaml_parser_parse(&parser, &event);
                    cur_allow = (strcmp((const char*)event.data.scalar.value, "allow") == 0);
                }
            }
        } else if (event.type == YAML_MAPPING_END_EVENT) {
            if (in_rule) {
                permission_rule_t* r = (permission_rule_t*)malloc(sizeof(permission_rule_t));
                if (r) {
                    r->agent_id = cur_agent;
                    r->action = cur_action;
                    r->resource = cur_resource;
                    r->allow = cur_allow;
                    r->next = NULL;
                    if (tail) tail->next = r;
                    else head = r;
                    tail = r;
                } else {
                    free(cur_agent);
                    free(cur_action);
                    free(cur_resource);
                }
                cur_agent = NULL;
                cur_action = NULL;
                cur_resource = NULL;
                in_rule = 0;
            }
        }
        yaml_event_delete(&event);
        if (event.type == YAML_STREAM_END_EVENT) break;
    }
    yaml_parser_delete(&parser);
    fclose(f);
    return head;
}

static void free_rules(permission_rule_t* rules) {
    while (rules) {
        permission_rule_t* next = rules->next;
        free(rules->agent_id);
        free(rules->action);
        free(rules->resource);
        free(rules);
        rules = next;
    }
}

rule_manager_t* rule_manager_create(const char* path) {
    rule_manager_t* mgr = (rule_manager_t*)calloc(1, sizeof(rule_manager_t));
    if (!mgr) return NULL;
    pthread_rwlock_init(&mgr->rwlock, NULL);
    mgr->path = strdup(path);
    if (!mgr->path) {
        free(mgr);
        return NULL;
    }
    time_t mtime = 0;
    mgr->rules = parse_rules(path, &mtime);
    mgr->last_mtime = mtime;
    return mgr;
}

void rule_manager_destroy(rule_manager_t* mgr) {
    if (!mgr) return;
    pthread_rwlock_destroy(&mgr->rwlock);
    free_rules(mgr->rules);
    free(mgr->path);
    free(mgr);
}

/* 简单的 glob 匹配（支持 * 通配符） */
static int match_glob(const char* pattern, const char* str) {
    if (!pattern) return 1; // 通配
    return fnmatch(pattern, str, 0) == 0;
}

int rule_manager_match(rule_manager_t* mgr,
                       const char* agent_id,
                       const char* action,
                       const char* resource,
                       const char* context) {
    if (!mgr) return 0;
    pthread_rwlock_rdlock(&mgr->rwlock);
    permission_rule_t* r = mgr->rules;
    while (r) {
        if ((!r->agent_id || strcmp(r->agent_id, agent_id) == 0) &&
            (!r->action || strcmp(r->action, action) == 0) &&
            match_glob(r->resource, resource)) {
            int result = r->allow ? 1 : 0;
            pthread_rwlock_unlock(&mgr->rwlock);
            return result;
        }
        r = r->next;
    }
    pthread_rwlock_unlock(&mgr->rwlock);
    return 0; // 无匹配规则，默认拒绝
}