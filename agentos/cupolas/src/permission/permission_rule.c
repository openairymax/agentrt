/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * permission_rule.c - Permission Rule Manager Implementation
 */

/**
 * @file permission_rule.c
 * @brief Permission Rule Manager Implementation
 * @author Spharx AgentRT Team
 * @date 2024
 */

#include "permission_rule.h"

#include "../yaml_minimal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 4096
#define DEFAULT_PRIORITY 100

static void cupolas_permission_free_rule(permission_rule_t *rule)
{
    if (!rule)
        return;
    cupolas_mem_free(rule->agent_id);
    cupolas_mem_free(rule->action);
    cupolas_mem_free(rule->resource);
    cupolas_mem_free(rule->resource_pattern);
    cupolas_mem_free(rule);
}

static void cupolas_permission_free_rules(permission_rule_t *rules)
{
    while (rules) {
        permission_rule_t *next = rules->next;
        cupolas_permission_free_rule(rules);
        rules = next;
    }
}

static permission_rule_t *cupolas_permission_create_rule(const char *agent_id, const char *action,
                                                         const char *resource, int allow,
                                                         int priority)
{
    permission_rule_t *rule = (permission_rule_t *)cupolas_mem_alloc(sizeof(permission_rule_t));
    if (!rule)
        return NULL;

    __builtin_memset(rule, 0, sizeof(permission_rule_t));

    if (agent_id) {
        rule->agent_id = cupolas_strdup(agent_id);
        if (!rule->agent_id)
            goto error;
    }
    if (action) {
        rule->action = cupolas_strdup(action);
        if (!rule->action)
            goto error;
    }
    if (resource) {
        rule->resource = cupolas_strdup(resource);
        if (!rule->resource)
            goto error;
    }

    rule->allow = allow;
    rule->priority = priority;
    rule->next = NULL;

    return rule;

error:
    cupolas_permission_free_rule(rule);
    return NULL;
}

static int cupolas_permission_match_pattern(const char *pattern, const char *str)
{
    if (!pattern || !str)
        return 0;
    if (strcmp(pattern, "*") == 0)
        return 1;

    const char *p = pattern;
    const char *s = str;
    const char *star = NULL;
    const char *ss = s;

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

rule_manager_t *rule_manager_create(const char *path)
{
    rule_manager_t *mgr = (rule_manager_t *)cupolas_mem_alloc(sizeof(rule_manager_t));
    if (!mgr)
        return NULL;

    __builtin_memset(mgr, 0, sizeof(rule_manager_t));

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
            mgr->path = NULL;
        }
    }

    return mgr;
}

void rule_manager_destroy(rule_manager_t *mgr)
{
    if (!mgr)
        return;

    cupolas_rwlock_wrlock(&mgr->rwlock);
    cupolas_permission_free_rules(mgr->rules);
    mgr->rules = NULL;
    cupolas_rwlock_unlock(&mgr->rwlock);

    cupolas_rwlock_destroy(&mgr->rwlock);
    cupolas_mem_free(mgr->path);
    cupolas_mem_free(mgr);
}

int rule_manager_reload(rule_manager_t *mgr)
{
    if (!mgr || !mgr->path)
        return cupolas_ERROR_INVALID_ARG;

    cupolas_file_stat_t st;
    if (cupolas_file_stat(mgr->path, &st) != cupolas_OK) {
        return cupolas_ERROR_NOT_FOUND;
    }

    uint64_t mtime = (uint64_t)st.mtime.sec * 1000 + st.mtime.nsec / 1000000;
    if (mtime == mgr->last_mtime) {
        return cupolas_OK;
    }

    yaml_document_t *doc = yaml_create();
    if (!doc)
        return cupolas_ERROR_NO_MEMORY;

    if (yaml_parse_file(doc, mgr->path) != 0) {
        yaml_destroy(doc);
        return cupolas_ERROR_IO;
    }

    permission_rule_t *new_rules = NULL;
    permission_rule_t **tail = &new_rules;

    struct yaml_node *root = yaml_root(doc);
    if (root) {
        if (root->type == YAML_NODE_SEQUENCE) {
            size_t count = yaml_size(root);
            for (size_t i = 0; i < count; i++) {
                struct yaml_node *entry = yaml_get_index(root, i);
                if (!entry || entry->type != YAML_NODE_MAPPING)
                    continue;

                const char *agent_id = yaml_as_string(yaml_get(entry, "agent"), "*");
                const char *action = yaml_as_string(yaml_get(entry, "action"), "*");
                const char *resource = yaml_as_string(yaml_get(entry, "resource"), "*");
                int allow = (int)yaml_as_bool(yaml_get(entry, "allow"), true);
                int priority = (int)yaml_as_int64(yaml_get(entry, "priority"), DEFAULT_PRIORITY);

                permission_rule_t *rule =
                    cupolas_permission_create_rule(agent_id, action, resource, allow, priority);
                if (!rule) {
                    yaml_destroy(doc);
                    cupolas_permission_free_rules(new_rules);
                    return cupolas_ERROR_NO_MEMORY;
                }
                *tail = rule;
                tail = &rule->next;
            }
        } else if (root->type == YAML_NODE_MAPPING) {
            struct yaml_node *rules_node = yaml_get(root, "rules");
            if (rules_node && rules_node->type == YAML_NODE_SEQUENCE) {
                size_t count = yaml_size(rules_node);
                for (size_t i = 0; i < count; i++) {
                    struct yaml_node *entry = yaml_get_index(rules_node, i);
                    if (!entry || entry->type != YAML_NODE_MAPPING)
                        continue;

                    const char *agent_id = yaml_as_string(yaml_get(entry, "agent"), "*");
                    const char *action = yaml_as_string(yaml_get(entry, "action"), "*");
                    const char *resource = yaml_as_string(yaml_get(entry, "resource"), "*");
                    int allow = (int)yaml_as_bool(yaml_get(entry, "allow"), true);
                    int priority =
                        (int)yaml_as_int64(yaml_get(entry, "priority"), DEFAULT_PRIORITY);

                    permission_rule_t *rule =
                        cupolas_permission_create_rule(agent_id, action, resource, allow, priority);
                    if (!rule) {
                        yaml_destroy(doc);
                        cupolas_permission_free_rules(new_rules);
                        return cupolas_ERROR_NO_MEMORY;
                    }
                    *tail = rule;
                    tail = &rule->next;
                }
            }
        }
    }

    yaml_destroy(doc);

    cupolas_rwlock_wrlock(&mgr->rwlock);
    permission_rule_t *old_rules = mgr->rules;
    mgr->rules = new_rules;
    mgr->last_mtime = mtime;
    cupolas_atomic_inc32(&mgr->version);
    cupolas_rwlock_unlock(&mgr->rwlock);

    cupolas_permission_free_rules(old_rules);

    return cupolas_OK;
}

int rule_manager_match(rule_manager_t *mgr, const char *agent_id, const char *action,
                       const char *resource, const char *context)
{
    (void)context;

    if (!mgr)
        return 0;

    int best_priority = -1;
    int result = 0;

    cupolas_rwlock_rdlock(&mgr->rwlock);

    permission_rule_t *rule = mgr->rules;
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

int rule_manager_add(rule_manager_t *mgr, const char *agent_id, const char *action,
                     const char *resource, int allow, int priority)
{
    if (!mgr)
        return cupolas_ERROR_INVALID_ARG;

    permission_rule_t *rule =
        cupolas_permission_create_rule(agent_id, action, resource, allow, priority);
    if (!rule)
        return cupolas_ERROR_NO_MEMORY;

    cupolas_rwlock_wrlock(&mgr->rwlock);

    permission_rule_t **pp = &mgr->rules;
    while (*pp && (*pp)->priority >= priority) {
        pp = &(*pp)->next;
    }

    rule->next = *pp;
    *pp = rule;

    cupolas_atomic_inc32(&mgr->version);

    cupolas_rwlock_unlock(&mgr->rwlock);

    return cupolas_OK;
}

void rule_manager_clear(rule_manager_t *mgr)
{
    if (!mgr)
        return;

    cupolas_rwlock_wrlock(&mgr->rwlock);
    cupolas_permission_free_rules(mgr->rules);
    mgr->rules = NULL;
    cupolas_atomic_inc32(&mgr->version);
    cupolas_rwlock_unlock(&mgr->rwlock);
}

size_t rule_manager_count(rule_manager_t *mgr)
{
    if (!mgr)
        return 0;

    cupolas_rwlock_rdlock(&mgr->rwlock);

    size_t count = 0;
    permission_rule_t *rule = mgr->rules;
    while (rule) {
        count++;
        rule = rule->next;
    }

    cupolas_rwlock_unlock(&mgr->rwlock);

    return count;
}
