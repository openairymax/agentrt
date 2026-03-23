/**
 * @file permission_rule.c
 * @brief 权限规则管理器实现
 * @author Spharx
 * @date 2024
 */

#include "permission_rule.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_LINE_LENGTH 4096
#define DEFAULT_PRIORITY 100

static void free_rule(permission_rule_t* rule) {
    if (!rule) return;
    domes_mem_free(rule->agent_id);
    domes_mem_free(rule->action);
    domes_mem_free(rule->resource);
    domes_mem_free(rule->resource_pattern);
    domes_mem_free(rule);
}

static void free_rules(permission_rule_t* rules) {
    while (rules) {
        permission_rule_t* next = rules->next;
        free_rule(rules);
        rules = next;
    }
}

static permission_rule_t* create_rule(const char* agent_id, const char* action,
                                       const char* resource, int allow, int priority) {
    permission_rule_t* rule = (permission_rule_t*)domes_mem_alloc(sizeof(permission_rule_t));
    if (!rule) return NULL;
    
    memset(rule, 0, sizeof(permission_rule_t));
    
    if (agent_id) {
        rule->agent_id = domes_strdup(agent_id);
        if (!rule->agent_id) goto error;
    }
    if (action) {
        rule->action = domes_strdup(action);
        if (!rule->action) goto error;
    }
    if (resource) {
        rule->resource = domes_strdup(resource);
        if (!rule->resource) goto error;
    }
    
    rule->allow = allow;
    rule->priority = priority;
    rule->next = NULL;
    
    return rule;
    
error:
    free_rule(rule);
    return NULL;
}

static int match_pattern(const char* pattern, const char* str) {
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

static int parse_yaml_line(const char* line, char** agent_id, char** action,
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
            *agent_id = domes_strdup(value);
        } else if (strcmp(key, "action") == 0 && value) {
            *action = domes_strdup(value);
        } else if (strcmp(key, "resource") == 0 && value) {
            *resource = domes_strdup(value);
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
    rule_manager_t* mgr = (rule_manager_t*)domes_mem_alloc(sizeof(rule_manager_t));
    if (!mgr) return NULL;
    
    memset(mgr, 0, sizeof(rule_manager_t));
    
    if (domes_rwlock_init(&mgr->rwlock) != DOMES_OK) {
        domes_mem_free(mgr);
        return NULL;
    }
    
    if (path) {
        mgr->path = domes_strdup(path);
        if (!mgr->path) {
            domes_rwlock_destroy(&mgr->rwlock);
            domes_mem_free(mgr);
            return NULL;
        }
        
        if (rule_manager_reload(mgr) != 0) {
            domes_mem_free(mgr->path);
            domes_rwlock_destroy(&mgr->rwlock);
            domes_mem_free(mgr);
            return NULL;
        }
    }
    
    return mgr;
}

void rule_manager_destroy(rule_manager_t* mgr) {
    if (!mgr) return;
    
    domes_rwlock_wrlock(&mgr->rwlock);
    free_rules(mgr->rules);
    mgr->rules = NULL;
    domes_rwlock_unlock(&mgr->rwlock);
    
    domes_rwlock_destroy(&mgr->rwlock);
    domes_mem_free(mgr->path);
    domes_mem_free(mgr);
}

int rule_manager_reload(rule_manager_t* mgr) {
    if (!mgr || !mgr->path) return DOMES_ERROR_INVALID_ARG;
    
    domes_file_stat_t st;
    if (domes_file_stat(mgr->path, &st) != DOMES_OK) {
        return DOMES_ERROR_NOT_FOUND;
    }
    
    uint64_t mtime = (uint64_t)st.mtime.sec * 1000 + st.mtime.nsec / 1000000;
    if (mtime == mgr->last_mtime) {
        return DOMES_OK;
    }
    
    FILE* fp = fopen(mgr->path, "r");
    if (!fp) {
        return DOMES_ERROR_IO;
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
        
        if (parse_yaml_line(line, &agent_id, &action, &resource, &allow, &priority) != 0) {
            continue;
        }
        
        permission_rule_t* rule = create_rule(agent_id, action, resource, allow, priority);
        domes_mem_free(agent_id);
        domes_mem_free(action);
        domes_mem_free(resource);
        
        if (!rule) {
            fclose(fp);
            free_rules(new_rules);
            return DOMES_ERROR_NO_MEMORY;
        }
        
        *tail = rule;
        tail = &rule->next;
    }
    
    fclose(fp);
    
    domes_rwlock_wrlock(&mgr->rwlock);
    permission_rule_t* old_rules = mgr->rules;
    mgr->rules = new_rules;
    mgr->last_mtime = mtime;
    domes_atomic_inc32(&mgr->version);
    domes_rwlock_unlock(&mgr->rwlock);
    
    free_rules(old_rules);
    
    return DOMES_OK;
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
    
    domes_rwlock_rdlock(&mgr->rwlock);
    
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
            if (!match_pattern(rule->resource, resource)) {
                match = 0;
            }
        }
        
        if (match) {
            best_priority = rule->priority;
            result = rule->allow;
        }
        
        rule = rule->next;
    }
    
    domes_rwlock_unlock(&mgr->rwlock);
    
    return result;
}

int rule_manager_add(rule_manager_t* mgr,
                     const char* agent_id,
                     const char* action,
                     const char* resource,
                     int allow,
                     int priority) {
    if (!mgr) return DOMES_ERROR_INVALID_ARG;
    
    permission_rule_t* rule = create_rule(agent_id, action, resource, allow, priority);
    if (!rule) return DOMES_ERROR_NO_MEMORY;
    
    domes_rwlock_wrlock(&mgr->rwlock);
    
    permission_rule_t** pp = &mgr->rules;
    while (*pp && (*pp)->priority >= priority) {
        pp = &(*pp)->next;
    }
    
    rule->next = *pp;
    *pp = rule;
    
    domes_atomic_inc32(&mgr->version);
    
    domes_rwlock_unlock(&mgr->rwlock);
    
    return DOMES_OK;
}

void rule_manager_clear(rule_manager_t* mgr) {
    if (!mgr) return;
    
    domes_rwlock_wrlock(&mgr->rwlock);
    free_rules(mgr->rules);
    mgr->rules = NULL;
    domes_atomic_inc32(&mgr->version);
    domes_rwlock_unlock(&mgr->rwlock);
}

size_t rule_manager_count(rule_manager_t* mgr) {
    if (!mgr) return 0;
    
    domes_rwlock_rdlock(&mgr->rwlock);
    
    size_t count = 0;
    permission_rule_t* rule = mgr->rules;
    while (rule) {
        count++;
        rule = rule->next;
    }
    
    domes_rwlock_unlock(&mgr->rwlock);
    
    return count;
}
