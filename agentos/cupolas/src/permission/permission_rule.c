/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * permission_rule.c - Permission Rule Manager Implementation
 */

/**
 * @file permission_rule.c
 * @brief Permission Rule Manager Implementation
 * @author Spharx
 * @date 2024
 */

#include "permission_rule.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_LINE_LENGTH 4096
#define DEFAULT_PRIORITY 100

static void cupolas_permission_free_rule(permission_rule_t* rule) {
    if (!rule) return;
    cupolas_mem_free(rule->agent_id);
    cupolas_mem_free(rule->action);
    cupolas_mem_free(rule->resource);
    cupolas_mem_free(rule->resource_pattern);
    cupolas_mem_free(rule);
}

static void cupolas_permission_free_rules(permission_rule_t* rules) {
    while (rules) {
        permission_rule_t* next = rules->next;
        cupolas_permission_free_rule(rules);
        rules = next;
    }
}

static permission_rule_t* cupolas_permission_create_rule(const char* agent_id, const char* action,
                                       const char* resource, int allow, int priority) {
    permission_rule_t* rule = (permission_rule_t*)cupolas_mem_alloc(sizeof(permission_rule_t));
    if (!rule) return NULL;
    
    memset(rule, 0, sizeof(permission_rule_t));
    
    if (agent_id) {
        rule->agent_id = cupolas_strdup(agent_id);
        if (!rule->agent_id) goto error;
    }
    if (action) {
        rule->action = cupolas_strdup(action);
        if (!rule->action) goto error;
    }
    if (resource) {
        rule->resource = cupolas_strdup(resource);
        if (!rule->resource) goto error;
    }
    
    rule->allow = allow;
    rule->priority = priority;
    rule->next = NULL;
    
    return rule;
    
error:
    cupolas_permission_free_rule(rule);
    return NULL;
}

static int cupolas_permission_match_pattern(const char* pattern, const char* str) {
    if (!pattern || !str) return 0;
    if (strcmp(pattern, "*") == 0) return 1;
    
    const char* p = pattern;
    const char* s = str;
    const char* star = NULL;
    const char* ss = s;
    
    while (*s) {
        if (*p == '*') {
            star = p++;
            ss = s;
        } else if (*p == *s || *p == '?') {
            p++;
            s++;
        } else if (star) {
            p = star + 1;
            s = ++ss;
        } else {
            return 0;
        }
    }
    
    while (*p == '*') {
        p++;
    }
    
    return *p == '\0';
}

static int cupolas_permission_parse_yaml_line(const char* line, char** agent_id, char** action,
                           char** resource, int* allow, int* priority) {
    *agent_id = NULL;
    *action = NULL;
    *resource = NULL;
    *allow = 1;
    *priority = DEFAULT_PRIORITY;
    
    char buf[MAX_LINE_LENGTH];
    strncpy(buf, line, MAX_LINE_LENGTH - 1);
    buf[MAX_LINE_LENGTH - 1] = '\0';
    
    char* p = buf;
    while (*p == ' ' || *p == '\t') p++;
    
    if (*p == '\0' || *p == '#' || *p == '\n' || *p == '\r') {
        return -1;
    }
    
    char* token;
    char* saveptr;
    
    token = strtok_r(p, ":\n\r", &saveptr);
    if (!token) return -1;
    
    while (token) {
        char* key = token;
        while (*key == ' ' || *key == '\t') key++;
        
        char* value = strchr(key, ' ');
        if (value) {
            *value = '\0';
            value++;
            while (*value == ' ' || *value == '\t') value++;
        }
        
        if (strcmp(key, "agent") == 0 && value) {
            *agent_id = cupolas_strdup(value);
        } else if (strcmp(key, "action") == 0 && value) {
            *action = cupolas_strdup(value);
        } else if (strcmp(key, "resource") == 0 && value) {
            *resource = cupolas_strdup(value);
        } else if (strcmp(key, "allow") == 0 && value) {
            *allow = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) ? 1 : 0;
        } else if (strcmp(key, "priority") == 0 && value) {
            *priority = atoi(value);
        }
        
        token = strtok_r(NULL, ":\n\r", &saveptr);
    }
    
    return 0;
}

rule_manager_t* rule_manager_create(const char* path) {
    rule_manager_t* mgr = (rule_manager_t*)cupolas_mem_alloc(sizeof(rule_manager_t));
    if (!mgr) return NULL;
    
    memset(mgr, 0, sizeof(rule_manager_t));
    
    if (cupolas_rwlock_init(&mgr->rwlock) != cupolas_OK) {
        cupolas_mem_free(mgr);
        return NULL;
    }
    
    if (path) {
        mgr->path = cupolas_strdup(path);
        if (!mgr->path) {
            cupolas_rwlock_destroy(&mgr->rwlock);
            cupolas_mem_free(mgr);
            return NULL;
        }
        
        if (rule_manager_reload(mgr) != 0) {
            cupolas_mem_free(mgr->path);
            cupolas_rwlock_destroy(&mgr->rwlock);
            cupolas_mem_free(mgr);
            return NULL;
        }
    }
    
    return mgr;
}

void rule_manager_destroy(rule_manager_t* mgr) {
    if (!mgr) return;
    
    cupolas_rwlock_wrlock(&mgr->rwlock);
    cupolas_permission_free_rules(mgr->rules);
    mgr->rules = NULL;
    cupolas_rwlock_unlock(&mgr->rwlock);
    
    cupolas_rwlock_destroy(&mgr->rwlock);
    cupolas_mem_free(mgr->path);
    cupolas_mem_free(mgr);
}

int rule_manager_reload(rule_manager_t* mgr) {
    if (!mgr || !mgr->path) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_file_stat_t st;
    if (cupolas_file_stat(mgr->path, &st) != cupolas_OK) {
        return cupolas_ERROR_NOT_FOUND;
    }
    
    uint64_t mtime = (uint64_t)st.mtime.sec * 1000 + st.mtime.nsec / 1000000;
    if (mtime == mgr->last_mtime) {
        return cupolas_OK;
    }
    
    FILE* fp = fopen(mgr->path, "r");
    if (!fp) {
        return cupolas_ERROR_IO;
    }
    
    permission_rule_t* new_rules = NULL;
    permission_rule_t** tail = &new_rules;
    
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), fp)) {
        char* agent_id = NULL;
        char* action = NULL;
        char* resource = NULL;
        int allow = 1;
        int priority = DEFAULT_PRIORITY;
        
        if (cupolas_permission_parse_yaml_line(line, &agent_id, &action, &resource, &allow, &priority) != 0) {
            continue;
        }
        
        permission_rule_t* rule = cupolas_permission_create_rule(agent_id, action, resource, allow, priority);
        cupolas_mem_free(agent_id);
        cupolas_mem_free(action);
        cupolas_mem_free(resource);
        
        if (!rule) {
            fclose(fp);
            cupolas_permission_free_rules(new_rules);
            return cupolas_ERROR_NO_MEMORY;
        }
        
        *tail = rule;
        tail = &rule->next;
    }
    
    fclose(fp);
    
    cupolas_rwlock_wrlock(&mgr->rwlock);
    permission_rule_t* old_rules = mgr->rules;
    mgr->rules = new_rules;
    mgr->last_mtime = mtime;
    cupolas_atomic_inc32(&mgr->version);
    cupolas_rwlock_unlock(&mgr->rwlock);
    
    cupolas_permission_free_rules(old_rules);
    
    return cupolas_OK;
}

int rule_manager_match(rule_manager_t* mgr,
                       const char* agent_id,
                       const char* action,
                       const char* resource,
                       const char* context) {
    (void)context;
    
    if (!mgr) return 0;
    
    int best_priority = -1;
    int result = 0;
    
    cupolas_rwlock_rdlock(&mgr->rwlock);
    
    permission_rule_t* rule = mgr->rules;
    while (rule) {
        if (rule->priority <= best_priority) {
            rule = rule->next;
            continue;
        }
        
        int match = 1;
        
        if (rule->agent_id && agent_id) {
            if (strcmp(rule->agent_id, "*") != 0 && strcmp(rule->agent_id, agent_id) != 0) {
                match = 0;
            }
        }
        
        if (match && rule->action && action) {
            if (strcmp(rule->action, "*") != 0 && strcmp(rule->action, action) != 0) {
                match = 0;
            }
        }
        
        if (match && rule->resource && resource) {
            if (!cupolas_permission_match_pattern(rule->resource, resource)) {
                match = 0;
            }
        }
        
        if (match) {
            best_priority = rule->priority;
            result = rule->allow;
        }
        
        rule = rule->next;
    }
    
    cupolas_rwlock_unlock(&mgr->rwlock);
    
    return result;
}

int rule_manager_add(rule_manager_t* mgr,
                     const char* agent_id,
                     const char* action,
                     const char* resource,
                     int allow,
                     int priority) {
    if (!mgr) return cupolas_ERROR_INVALID_ARG;
    
    permission_rule_t* rule = cupolas_permission_create_rule(agent_id, action, resource, allow, priority);
    if (!rule) return cupolas_ERROR_NO_MEMORY;
    
    cupolas_rwlock_wrlock(&mgr->rwlock);
    
    permission_rule_t** pp = &mgr->rules;
    while (*pp && (*pp)->priority >= priority) {
        pp = &(*pp)->next;
    }
    
    rule->next = *pp;
    *pp = rule;
    
    cupolas_atomic_inc32(&mgr->version);
    
    cupolas_rwlock_unlock(&mgr->rwlock);
    
    return cupolas_OK;
}

void rule_manager_clear(rule_manager_t* mgr) {
    if (!mgr) return;
    
    cupolas_rwlock_wrlock(&mgr->rwlock);
    cupolas_permission_free_rules(mgr->rules);
    mgr->rules = NULL;
    cupolas_atomic_inc32(&mgr->version);
    cupolas_rwlock_unlock(&mgr->rwlock);
}

size_t rule_manager_count(rule_manager_t* mgr) {
    if (!mgr) return 0;
    
    cupolas_rwlock_rdlock(&mgr->rwlock);
    
    size_t count = 0;
    permission_rule_t* rule = mgr->rules;
    while (rule) {
        count++;
        rule = rule->next;
    }
    
    cupolas_rwlock_unlock(&mgr->rwlock);
    
    return count;
}
