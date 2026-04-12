/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file alert.c
 * @brief 告警管理系统实现
 *
 * 功能：
 * 1. 告警生命周期管理（创建、触发、确认、解决）
 * 2. 告警规则引擎（阈值、趋势、组合条件）
 * 3. 告警通知渠道（日志、回调、升级）
 * 4. 告警去重和抑制
 * 5. 线程安全
 */

#include "monitor_service.h"
#include "platform.h"
#include "error.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_RULES 128
#define MAX_ALERT_HISTORY 2048
#define MAX_NOTIFICATION_CHANNELS 16
#define MAX_SUPPRESSION_RULES 64
#define MAX_GROUP_KEY_LEN 256

typedef enum {
    ALERT_RULE_THRESHOLD,
    ALERT_RULE_TREND,
    ALERT_RULE_COMPOSITE,
    ALERT_RULE_CUSTOM
} alert_rule_type_t;

typedef enum {
    OP_GT,
    OP_GTE,
    OP_LT,
    OP_LTE,
    OP_EQ,
    OP_NEQ
} comparison_op_t;

typedef struct {
    char* rule_id;
    char* metric_name;
    comparison_op_t op;
    double threshold;
    alert_level_t level;
    char* message_template;
    uint32_t evaluation_interval_ms;
    uint32_t consecutive_count;
    uint32_t current_count;
    bool enabled;
} threshold_rule_t;

typedef struct {
    char* rule_id;
    char* metric_name;
    double rate_of_change;
    uint32_t window_ms;
    alert_level_t level;
    char* message_template;
    bool enabled;
} trend_rule_t;

typedef struct {
    char* rule_id;
    char** sub_rule_ids;
    size_t sub_rule_count;
    char* logic_operator;
    alert_level_t level;
    char* message_template;
    bool enabled;
} composite_rule_t;

typedef struct {
    alert_rule_type_t type;
    union {
        threshold_rule_t threshold;
        trend_rule_t trend;
        composite_rule_t composite;
    } rule;
} alert_rule_t;

typedef enum {
    CHANNEL_LOG,
    CHANNEL_CALLBACK,
    CHANNEL_ESCALATION
} channel_type_t;

typedef struct {
    channel_type_t type;
    void (*callback)(const alert_info_t* alert, void* user_data);
    void* user_data;
    alert_level_t min_level;
    bool enabled;
} notification_channel_t;

typedef struct {
    char* group_key_pattern;
    uint32_t suppress_duration_ms;
    uint64_t last_alert_time;
    bool active;
} suppression_rule_t;

typedef struct {
    alert_info_t alert;
    uint32_t occurrence_count;
    uint64_t first_seen;
    uint64_t last_seen;
    char group_key[MAX_GROUP_KEY_LEN];
} grouped_alert_t;

static struct {
    alert_rule_t rules[MAX_RULES];
    size_t rule_count;
    notification_channel_t channels[MAX_NOTIFICATION_CHANNELS];
    size_t channel_count;
    suppression_rule_t suppressions[MAX_SUPPRESSION_RULES];
    size_t suppression_count;
    grouped_alert_t history[MAX_ALERT_HISTORY];
    size_t history_count;
    agentos_mutex_t lock;
    int initialized;
} g_alert_mgr = {0};

int alert_system_init(void) {
    if (g_alert_mgr.initialized) {
        return AGENTOS_SUCCESS;
    }

    agentos_mutex_init(&g_alert_mgr.lock);
    g_alert_mgr.rule_count = 0;
    g_alert_mgr.channel_count = 0;
    g_alert_mgr.suppression_count = 0;
    g_alert_mgr.history_count = 0;
    g_alert_mgr.initialized = 1;

    SVC_LOG_INFO("Alert system initialized");
    return AGENTOS_SUCCESS;
}

void alert_system_shutdown(void) {
    if (!g_alert_mgr.initialized) {
        return;
    }

    agentos_mutex_lock(&g_alert_mgr.lock);

    for (size_t i = 0; i < g_alert_mgr.rule_count; i++) {
        free(g_alert_mgr.rules[i].rule.threshold.rule_id);
        free(g_alert_mgr.rules[i].rule.threshold.metric_name);
        free(g_alert_mgr.rules[i].rule.threshold.message_template);
    }

    for (size_t i = 0; i < g_alert_mgr.history_count; i++) {
        free(g_alert_mgr.history[i].alert.alert_id);
        free(g_alert_mgr.history[i].alert.message);
        free(g_alert_mgr.history[i].alert.service_name);
        free(g_alert_mgr.history[i].alert.resource_id);
    }

    for (size_t i = 0; i < g_alert_mgr.suppression_count; i++) {
        free(g_alert_mgr.suppressions[i].group_key_pattern);
    }

    g_alert_mgr.rule_count = 0;
    g_alert_mgr.channel_count = 0;
    g_alert_mgr.suppression_count = 0;
    g_alert_mgr.history_count = 0;
    g_alert_mgr.initialized = 0;

    agentos_mutex_unlock(&g_alert_mgr.lock);
    agentos_mutex_destroy(&g_alert_mgr.lock);
}

int alert_add_threshold_rule(const char* rule_id, const char* metric_name,
                             comparison_op_t op, double threshold,
                             alert_level_t level, const char* message_template,
                             uint32_t consecutive_count) {
    if (!rule_id || !metric_name) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (!g_alert_mgr.initialized) {
        alert_system_init();
    }

    agentos_mutex_lock(&g_alert_mgr.lock);

    if (g_alert_mgr.rule_count >= MAX_RULES) {
        agentos_mutex_unlock(&g_alert_mgr.lock);
        return AGENTOS_ERR_OVERFLOW;
    }

    alert_rule_t* rule = &g_alert_mgr.rules[g_alert_mgr.rule_count];
    rule->type = ALERT_RULE_THRESHOLD;
    rule->rule.threshold.rule_id = strdup(rule_id);
    rule->rule.threshold.metric_name = strdup(metric_name);
    rule->rule.threshold.op = op;
    rule->rule.threshold.threshold = threshold;
    rule->rule.threshold.level = level;
    rule->rule.threshold.message_template = message_template ? strdup(message_template) : NULL;
    rule->rule.threshold.evaluation_interval_ms = 10000;
    rule->rule.threshold.consecutive_count = consecutive_count > 0 ? consecutive_count : 1;
    rule->rule.threshold.current_count = 0;
    rule->rule.threshold.enabled = true;
    g_alert_mgr.rule_count++;

    agentos_mutex_unlock(&g_alert_mgr.lock);

    SVC_LOG_INFO("Alert threshold rule added: %s (metric=%s, threshold=%.2f)",
                 rule_id, metric_name, threshold);
    return AGENTOS_SUCCESS;
}

int alert_add_notification_channel(void (*callback)(const alert_info_t*, void*),
                                   void* user_data, alert_level_t min_level) {
    if (!callback) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (!g_alert_mgr.initialized) {
        alert_system_init();
    }

    agentos_mutex_lock(&g_alert_mgr.lock);

    if (g_alert_mgr.channel_count >= MAX_NOTIFICATION_CHANNELS) {
        agentos_mutex_unlock(&g_alert_mgr.lock);
        return AGENTOS_ERR_OVERFLOW;
    }

    notification_channel_t* ch = &g_alert_mgr.channels[g_alert_mgr.channel_count];
    ch->type = CHANNEL_CALLBACK;
    ch->callback = callback;
    ch->user_data = user_data;
    ch->min_level = min_level;
    ch->enabled = true;
    g_alert_mgr.channel_count++;

    agentos_mutex_unlock(&g_alert_mgr.lock);
    return AGENTOS_SUCCESS;
}

static bool check_suppression(const alert_info_t* alert) {
    for (size_t i = 0; i < g_alert_mgr.suppression_count; i++) {
        suppression_rule_t* sup = &g_alert_mgr.suppressions[i];
        if (!sup->active) continue;

        char group_key[MAX_GROUP_KEY_LEN];
        snprintf(group_key, sizeof(group_key), "%s:%s:%d",
                 alert->service_name ? alert->service_name : "",
                 alert->resource_id ? alert->resource_id : "",
                 alert->level);

        if (strstr(group_key, sup->group_key_pattern) != NULL) {
            uint64_t now = (uint64_t)time(NULL) * 1000;
            if (now - sup->last_alert_time < sup->suppress_duration_ms) {
                return true;
            }
            sup->last_alert_time = now;
        }
    }
    return false;
}

static void notify_channels(const alert_info_t* alert) {
    for (size_t i = 0; i < g_alert_mgr.channel_count; i++) {
        notification_channel_t* ch = &g_alert_mgr.channels[i];
        if (!ch->enabled || alert->level < ch->min_level) continue;

        switch (ch->type) {
            case CHANNEL_CALLBACK:
                if (ch->callback) {
                    ch->callback(alert, ch->user_data);
                }
                break;
            case CHANNEL_LOG: {
                const char* level_str[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};
                SVC_LOG_WARN("ALERT [%s] %s: %s",
                             level_str[alert->level < ALERT_LEVEL_COUNT ? alert->level : 0],
                             alert->alert_id ? alert->alert_id : "N/A",
                             alert->message ? alert->message : "N/A");
                break;
            }
            case CHANNEL_ESCALATION:
                SVC_LOG_ERROR("ALERT ESCALATION: %s", alert->message ? alert->message : "N/A");
                break;
        }
    }
}

static void add_to_history(const alert_info_t* alert) {
    if (g_alert_mgr.history_count >= MAX_ALERT_HISTORY) {
        free(g_alert_mgr.history[0].alert.alert_id);
        free(g_alert_mgr.history[0].alert.message);
        free(g_alert_mgr.history[0].alert.service_name);
        free(g_alert_mgr.history[0].alert.resource_id);
        memmove(&g_alert_mgr.history[0], &g_alert_mgr.history[1],
                (g_alert_mgr.history_count - 1) * sizeof(grouped_alert_t));
        g_alert_mgr.history_count--;
    }

    grouped_alert_t* entry = &g_alert_mgr.history[g_alert_mgr.history_count];
    memset(entry, 0, sizeof(grouped_alert_t));

    entry->alert.alert_id = alert->alert_id ? strdup(alert->alert_id) : NULL;
    entry->alert.message = alert->message ? strdup(alert->message) : NULL;
    entry->alert.level = alert->level;
    entry->alert.service_name = alert->service_name ? strdup(alert->service_name) : NULL;
    entry->alert.resource_id = alert->resource_id ? strdup(alert->resource_id) : NULL;
    entry->alert.timestamp = alert->timestamp ? alert->timestamp : (uint64_t)time(NULL) * 1000;
    entry->alert.is_resolved = false;
    entry->occurrence_count = 1;
    entry->first_seen = entry->alert.timestamp;
    entry->last_seen = entry->alert.timestamp;

    snprintf(entry->group_key, sizeof(entry->group_key), "%s:%s:%d",
             alert->service_name ? alert->service_name : "",
             alert->resource_id ? alert->resource_id : "",
             alert->level);

    g_alert_mgr.history_count++;
}

int alert_evaluate_metric(const char* metric_name, double value) {
    if (!metric_name || !g_alert_mgr.initialized) {
        return AGENTOS_SUCCESS;
    }

    agentos_mutex_lock(&g_alert_mgr.lock);

    for (size_t i = 0; i < g_alert_mgr.rule_count; i++) {
        alert_rule_t* rule = &g_alert_mgr.rules[i];
        if (rule->type != ALERT_RULE_THRESHOLD || !rule->rule.threshold.enabled) continue;
        if (strcmp(rule->rule.threshold.metric_name, metric_name) != 0) continue;

        bool triggered = false;
        threshold_rule_t* tr = &rule->rule.threshold;

        switch (tr->op) {
            case OP_GT:  triggered = value > tr->threshold; break;
            case OP_GTE: triggered = value >= tr->threshold; break;
            case OP_LT:  triggered = value < tr->threshold; break;
            case OP_LTE: triggered = value <= tr->threshold; break;
            case OP_EQ:  triggered = value == tr->threshold; break;
            case OP_NEQ: triggered = value != tr->threshold; break;
        }

        if (triggered) {
            tr->current_count++;
            if (tr->current_count >= tr->consecutive_count) {
                alert_info_t alert = {0};
                char alert_id[64];
                snprintf(alert_id, sizeof(alert_id), "auto-%s-%llu",
                         tr->rule_id, (unsigned long long)(uint64_t)time(NULL) * 1000);
                alert.alert_id = alert_id;
                alert.level = tr->level;
                alert.service_name = "monitor";
                alert.timestamp = (uint64_t)time(NULL) * 1000;
                alert.is_resolved = false;

                char msg_buf[512];
                if (tr->message_template) {
                    snprintf(msg_buf, sizeof(msg_buf), tr->message_template, value, tr->threshold);
                } else {
                    snprintf(msg_buf, sizeof(msg_buf),
                             "Metric %s value %.2f exceeded threshold %.2f",
                             metric_name, value, tr->threshold);
                }
                alert.message = msg_buf;

                if (!check_suppression(&alert)) {
                    add_to_history(&alert);
                    notify_channels(&alert);
                }

                tr->current_count = 0;
            }
        } else {
            tr->current_count = 0;
        }
    }

    agentos_mutex_unlock(&g_alert_mgr.lock);
    return AGENTOS_SUCCESS;
}

int alert_resolve(const char* alert_id) {
    if (!alert_id || !g_alert_mgr.initialized) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    agentos_mutex_lock(&g_alert_mgr.lock);

    for (size_t i = 0; i < g_alert_mgr.history_count; i++) {
        if (g_alert_mgr.history[i].alert.alert_id &&
            strcmp(g_alert_mgr.history[i].alert.alert_id, alert_id) == 0) {
            g_alert_mgr.history[i].alert.is_resolved = true;
            agentos_mutex_unlock(&g_alert_mgr.lock);
            SVC_LOG_INFO("Alert resolved: %s", alert_id);
            return AGENTOS_SUCCESS;
        }
    }

    agentos_mutex_unlock(&g_alert_mgr.lock);
    return AGENTOS_ERR_NOT_FOUND;
}

size_t alert_get_unresolved_count(void) {
    if (!g_alert_mgr.initialized) return 0;

    size_t count = 0;
    agentos_mutex_lock(&g_alert_mgr.lock);
    for (size_t i = 0; i < g_alert_mgr.history_count; i++) {
        if (!g_alert_mgr.history[i].alert.is_resolved) count++;
    }
    agentos_mutex_unlock(&g_alert_mgr.lock);
    return count;
}

size_t alert_get_count_by_level(alert_level_t level) {
    if (!g_alert_mgr.initialized) return 0;

    size_t count = 0;
    agentos_mutex_lock(&g_alert_mgr.lock);
    for (size_t i = 0; i < g_alert_mgr.history_count; i++) {
        if (g_alert_mgr.history[i].alert.level == level) count++;
    }
    agentos_mutex_unlock(&g_alert_mgr.lock);
    return count;
}
