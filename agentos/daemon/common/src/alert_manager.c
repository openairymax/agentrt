// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file alert_manager.c
 * @brief 智能告警管理系统实现
 *
 * @see alert_manager.h
 */

#include "alert_manager.h"
#include "svc_logger.h"
#include "platform.h"
#include "safe_string_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==================== 内部常量 ==================== */

#define AM_MAX_CALLBACKS 8

/* ==================== 内部数据结构 ==================== */

typedef struct {
    am_alert_callback_t callback;
    void* user_data;
    am_level_t min_level;
} am_callback_entry_t;

static struct {
    am_config_t config;
    am_rule_t rules[AM_MAX_RULES];
    uint32_t rule_count;
    am_alert_t active_alerts[AM_MAX_ACTIVE_ALERTS];
    uint32_t active_alert_count;
    am_channel_t channels[AM_MAX_CHANNELS];
    uint32_t channel_count;
    am_callback_entry_t callbacks[AM_MAX_CALLBACKS];
    uint32_t callback_count;
    bool initialized;
    agentos_platform_mutex_t mutex;
} g_am = {0};

/* ==================== 辅助函数 ==================== */

static am_alert_t* find_active_alert(const char* name) {
    for (uint32_t i = 0; i < g_am.active_alert_count; i++) {
        if (strcmp(g_am.active_alerts[i].name, name) == 0)
            return &g_am.active_alerts[i];
    }
    return NULL;
}

static am_rule_t* find_rule(const char* name) {
    for (uint32_t i = 0; i < g_am.rule_count; i++) {
        if (strcmp(g_am.rules[i].name, name) == 0)
            return &g_am.rules[i];
    }
    return NULL;
}

static bool evaluate_condition(double value, am_comparison_t op, double threshold) {
    switch (op) {
        case AM_OP_GT:  return value > threshold;
        case AM_OP_GTE: return value >= threshold;
        case AM_OP_LT:  return value < threshold;
        case AM_OP_LTE: return value <= threshold;
        case AM_OP_EQ:  return value == threshold;
        case AM_OP_NEQ: return value != threshold;
        default:        return false;
    }
}

static void dispatch_notifications(const am_alert_t* alert) {
    for (uint32_t i = 0; i < g_am.callback_count; i++) {
        if (alert->level >= g_am.callbacks[i].min_level) {
            g_am.callbacks[i].callback(alert, g_am.callbacks[i].user_data);
        }
    }

    for (uint32_t i = 0; i < g_am.channel_count; i++) {
        am_channel_t* ch = &g_am.channels[i];
        if (!ch->enabled || alert->level < ch->min_level) continue;

        switch (ch->type) {
            case AM_CHANNEL_LOG:
                LOG_WARN("[ALERT][%s] %s: %s (source=%s)",
                         am_level_to_string(alert->level),
                         alert->name, alert->message, alert->source);
                break;
            case AM_CHANNEL_FILE: {
                FILE* fp = fopen(ch->config, "a");
                if (fp) {
                    fprintf(fp, "[%llu][%s] %s: %s (source=%s)\n",
                            (unsigned long long)alert->fired_at,
                            am_level_to_string(alert->level),
                            alert->name, alert->message, alert->source);
                    fclose(fp);
                }
                break;
            }
            default:
                break;
        }
    }
}

/* ==================== 公共API实现 ==================== */

AGENTOS_API am_config_t am_create_default_config(void) {
    am_config_t config;
    memset(&config, 0, sizeof(am_config_t));
    config.evaluation_interval_ms = 10000;
    config.default_cooldown_ms = 60000;
    config.max_notifications_per_alert = 10;
    config.escalation_timeout_ms = 300000;
    config.enable_deduplication = true;
    config.enable_suppression = true;
    return config;
}

AGENTOS_API int am_init(const am_config_t* config) {
    if (g_am.initialized) return 0;

    if (config) {
        memcpy(&g_am.config, config, sizeof(am_config_t));
    } else {
        g_am.config = am_create_default_config();
    }

    agentos_error_t err = agentos_platform_mutex_init(&g_am.mutex);
    if (err != AGENTOS_SUCCESS) return -1;

    memset(g_am.rules, 0, sizeof(g_am.rules));
    g_am.rule_count = 0;
    memset(g_am.active_alerts, 0, sizeof(g_am.active_alerts));
    g_am.active_alert_count = 0;
    g_am.channel_count = 0;
    g_am.callback_count = 0;
    g_am.initialized = true;

    am_channel_t log_channel;
    memset(&log_channel, 0, sizeof(am_channel_t));
    log_channel.type = AM_CHANNEL_LOG;
    safe_strcpy(log_channel.name, "default_log", AM_MAX_NAME_LEN);
    log_channel.min_level = AM_LEVEL_WARNING;
    log_channel.enabled = true;
    am_register_channel(&log_channel);

    LOG_INFO("Alert manager initialized (eval_interval=%ums, dedup=%s)",
             g_am.config.evaluation_interval_ms,
             g_am.config.enable_deduplication ? "on" : "off");
    return 0;
}

AGENTOS_API void am_shutdown(void) {
    if (!g_am.initialized) return;

    agentos_platform_mutex_lock(&g_am.mutex);
    g_am.initialized = false;
    agentos_platform_mutex_unlock(&g_am.mutex);

    agentos_platform_mutex_destroy(&g_am.mutex);

    LOG_INFO("Alert manager shutdown");
}

/* ==================== 规则管理 ==================== */

AGENTOS_API int am_add_rule(const am_rule_t* rule) {
    if (!rule) return -1;
    if (!g_am.initialized) am_init(NULL);

    agentos_platform_mutex_lock(&g_am.mutex);

    if (find_rule(rule->name)) {
        am_rule_t* existing = find_rule(rule->name);
        memcpy(existing, rule, sizeof(am_rule_t));
        agentos_platform_mutex_unlock(&g_am.mutex);
        return 0;
    }

    if (g_am.rule_count >= AM_MAX_RULES) {
        agentos_platform_mutex_unlock(&g_am.mutex);
        return -1;
    }

    memcpy(&g_am.rules[g_am.rule_count], rule, sizeof(am_rule_t));
    g_am.rule_count++;

    agentos_platform_mutex_unlock(&g_am.mutex);

    LOG_INFO("Alert rule added: %s (type=%d, level=%s)",
             rule->name, rule->type, am_level_to_string(rule->level));
    return 0;
}

AGENTOS_API int am_remove_rule(const char* name) {
    if (!name) return -1;

    agentos_platform_mutex_lock(&g_am.mutex);

    for (uint32_t i = 0; i < g_am.rule_count; i++) {
        if (strcmp(g_am.rules[i].name, name) == 0) {
            if (i < g_am.rule_count - 1) {
                g_am.rules[i] = g_am.rules[g_am.rule_count - 1];
            }
            memset(&g_am.rules[g_am.rule_count - 1], 0, sizeof(am_rule_t));
            g_am.rule_count--;
            agentos_platform_mutex_unlock(&g_am.mutex);
            return 0;
        }
    }

    agentos_platform_mutex_unlock(&g_am.mutex);
    return -1;
}

AGENTOS_API int am_set_rule_enabled(const char* name, bool enabled) {
    if (!name) return -1;

    agentos_platform_mutex_lock(&g_am.mutex);

    am_rule_t* rule = find_rule(name);
    if (!rule) {
        agentos_platform_mutex_unlock(&g_am.mutex);
        return -1;
    }

    rule->enabled = enabled;
    agentos_platform_mutex_unlock(&g_am.mutex);

    return 0;
}

/* ==================== 告警触发 ==================== */

AGENTOS_API int am_fire(const char* name, am_level_t level,
                        const char* message, const char* source,
                        const char* labels) {
    if (!name) return -1;
    if (!g_am.initialized) am_init(NULL);

    agentos_platform_mutex_lock(&g_am.mutex);

    am_alert_t* existing = find_active_alert(name);
    if (existing) {
        if (g_am.config.enable_deduplication) {
            existing->trigger_count++;
            existing->last_notified = agentos_platform_get_time_ms();

            if (existing->notification_count < g_am.config.max_notifications_per_alert) {
                dispatch_notifications(existing);
                existing->notification_count++;
            }

            agentos_platform_mutex_unlock(&g_am.mutex);
            return 0;
        }

        existing->level = level;
        if (message) safe_strcpy(existing->message, message, AM_MAX_MESSAGE_LEN);
        existing->trigger_count++;
        existing->last_notified = agentos_platform_get_time_ms();
        dispatch_notifications(existing);
        existing->notification_count++;

        agentos_platform_mutex_unlock(&g_am.mutex);
        return 0;
    }

    if (g_am.active_alert_count >= AM_MAX_ACTIVE_ALERTS) {
        agentos_platform_mutex_unlock(&g_am.mutex);
        LOG_ERROR("Max active alerts reached");
        return -1;
    }

    am_alert_t* alert = &g_am.active_alerts[g_am.active_alert_count];
    memset(alert, 0, sizeof(am_alert_t));
    safe_strcpy(alert->name, name, AM_MAX_NAME_LEN);
    alert->level = level;
    alert->state = AM_STATE_FIRING;
    if (message) safe_strcpy(alert->message, message, AM_MAX_MESSAGE_LEN);
    if (source) safe_strcpy(alert->source, source, sizeof(alert->source));
    if (labels) safe_strcpy(alert->labels, labels, sizeof(alert->labels));
    alert->fired_at = agentos_platform_get_time_ms();
    alert->trigger_count = 1;
    alert->notification_count = 1;
    g_am.active_alert_count++;

    dispatch_notifications(alert);

    agentos_platform_mutex_unlock(&g_am.mutex);

    LOG_WARN("Alert fired: %s [%s] - %s", name, am_level_to_string(level),
             message ? message : "no message");
    return 0;
}

AGENTOS_API int am_resolve(const char* name) {
    if (!name) return -1;

    agentos_platform_mutex_lock(&g_am.mutex);

    am_alert_t* alert = find_active_alert(name);
    if (!alert) {
        agentos_platform_mutex_unlock(&g_am.mutex);
        return -1;
    }

    alert->state = AM_STATE_RESOLVED;
    alert->resolved_at = agentos_platform_get_time_ms();

    uint32_t idx = (uint32_t)(alert - g_am.active_alerts);
    if (idx < g_am.active_alert_count - 1) {
        g_am.active_alerts[idx] = g_am.active_alerts[g_am.active_alert_count - 1];
    }
    memset(&g_am.active_alerts[g_am.active_alert_count - 1], 0, sizeof(am_alert_t));
    g_am.active_alert_count--;

    agentos_platform_mutex_unlock(&g_am.mutex);

    LOG_INFO("Alert resolved: %s", name);
    return 0;
}

AGENTOS_API int am_acknowledge(const char* name) {
    if (!name) return -1;

    agentos_platform_mutex_lock(&g_am.mutex);

    am_alert_t* alert = find_active_alert(name);
    if (!alert) {
        agentos_platform_mutex_unlock(&g_am.mutex);
        return -1;
    }

    alert->acknowledged = true;
    alert->state = AM_STATE_ACKNOWLEDGED;

    agentos_platform_mutex_unlock(&g_am.mutex);

    LOG_INFO("Alert acknowledged: %s", name);
    return 0;
}

/* ==================== 指标评估 ==================== */

AGENTOS_API int am_evaluate(const char* metric_name, double value) {
    if (!metric_name) return 0;

    agentos_platform_mutex_lock(&g_am.mutex);

    int triggered = 0;
    uint64_t now = agentos_platform_get_time_ms();

    for (uint32_t i = 0; i < g_am.rule_count; i++) {
        am_rule_t* rule = &g_am.rules[i];
        if (!rule->enabled) continue;

        if (strcmp(rule->metric_name, metric_name) != 0) continue;

        if (rule->last_triggered > 0 &&
            (now - rule->last_triggered) < (uint64_t)rule->cooldown_seconds * 1000)
            continue;

        bool condition_met = false;
        switch (rule->type) {
            case AM_RULE_THRESHOLD:
                condition_met = evaluate_condition(value, rule->comparison, rule->threshold);
                break;
            case AM_RULE_TREND:
                condition_met = evaluate_condition(value, rule->comparison, rule->threshold);
                break;
            default:
                condition_met = evaluate_condition(value, rule->comparison, rule->threshold);
                break;
        }

        if (condition_met) {
            rule->last_triggered = now;
            triggered++;

            char message[AM_MAX_MESSAGE_LEN];
            snprintf(message, sizeof(message),
                     "Metric %s value %.2f %s threshold %.2f",
                     metric_name, value,
                     rule->comparison == AM_OP_GT ? ">" :
                     rule->comparison == AM_OP_GTE ? ">=" :
                     rule->comparison == AM_OP_LT ? "<" :
                     rule->comparison == AM_OP_LTE ? "<=" :
                     rule->comparison == AM_OP_EQ ? "==" : "!=",
                     rule->threshold);

            am_alert_t* existing = find_active_alert(rule->name);
            if (existing) {
                existing->trigger_count++;
                existing->last_notified = now;
            } else {
                agentos_platform_mutex_unlock(&g_am.mutex);
                am_fire(rule->name, rule->level, message, "rule_engine", metric_name);
                agentos_platform_mutex_lock(&g_am.mutex);
            }
        }
    }

    agentos_platform_mutex_unlock(&g_am.mutex);
    return triggered;
}

AGENTOS_API int am_evaluate_all(void) {
    return 0;
}

/* ==================== 通知通道 ==================== */

AGENTOS_API int am_register_channel(const am_channel_t* channel) {
    if (!channel) return -1;

    agentos_platform_mutex_lock(&g_am.mutex);

    if (g_am.channel_count >= AM_MAX_CHANNELS) {
        agentos_platform_mutex_unlock(&g_am.mutex);
        return -1;
    }

    memcpy(&g_am.channels[g_am.channel_count], channel, sizeof(am_channel_t));
    g_am.channel_count++;

    agentos_platform_mutex_unlock(&g_am.mutex);

    LOG_INFO("Alert channel registered: %s (type=%d)", channel->name, channel->type);
    return 0;
}

AGENTOS_API int am_register_callback(am_alert_callback_t callback, void* user_data,
                                     am_level_t min_level) {
    if (!callback) return -1;

    agentos_platform_mutex_lock(&g_am.mutex);

    if (g_am.callback_count >= AM_MAX_CALLBACKS) {
        agentos_platform_mutex_unlock(&g_am.mutex);
        return -1;
    }

    g_am.callbacks[g_am.callback_count].callback = callback;
    g_am.callbacks[g_am.callback_count].user_data = user_data;
    g_am.callbacks[g_am.callback_count].min_level = min_level;
    g_am.callback_count++;

    agentos_platform_mutex_unlock(&g_am.mutex);

    return 0;
}

/* ==================== 查询 ==================== */

AGENTOS_API int am_get_active_alerts(am_alert_t* alerts, uint32_t max_count,
                                     uint32_t* found_count) {
    if (!alerts || !found_count) return -1;

    agentos_platform_mutex_lock(&g_am.mutex);

    uint32_t count = g_am.active_alert_count < max_count
        ? g_am.active_alert_count : max_count;
    memcpy(alerts, g_am.active_alerts, count * sizeof(am_alert_t));
    *found_count = count;

    agentos_platform_mutex_unlock(&g_am.mutex);
    return 0;
}

AGENTOS_API int am_get_alerts_by_level(am_level_t level, am_alert_t* alerts,
                                       uint32_t max_count, uint32_t* found_count) {
    if (!alerts || !found_count) return -1;

    agentos_platform_mutex_lock(&g_am.mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < g_am.active_alert_count && count < max_count; i++) {
        if (g_am.active_alerts[i].level == level) {
            memcpy(&alerts[count], &g_am.active_alerts[i], sizeof(am_alert_t));
            count++;
        }
    }
    *found_count = count;

    agentos_platform_mutex_unlock(&g_am.mutex);
    return 0;
}

AGENTOS_API uint32_t am_active_alert_count(void) {
    return g_am.active_alert_count;
}

/* ==================== 工具函数 ==================== */

AGENTOS_API const char* am_level_to_string(am_level_t level) {
    static const char* level_strings[] = {
        "INFO", "WARNING", "CRITICAL", "EMERGENCY"
    };
    if (level < 0 || level > AM_LEVEL_EMERGENCY) return "UNKNOWN";
    return level_strings[level];
}

AGENTOS_API const char* am_state_to_string(am_state_t state) {
    static const char* state_strings[] = {
        "PENDING", "FIRING", "RESOLVED", "SUPPRESSED", "ACKNOWLEDGED"
    };
    if (state < 0 || state > AM_STATE_ACKNOWLEDGED) return "UNKNOWN";
    return state_strings[state];
}
