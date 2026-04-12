/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_rate_limiter.c
 * @brief Gateway 内置速率限制器实现
 *
 * 实现基于令牌桶算法的速率限制功能。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "gateway_rate_limiter.h"
#include "gateway_utils.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 跨平台原子操作支持 */
#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#include "../../../commons/utils/include/atomic_compat.h"
#else
#include <stdatomic.h>
#endif

/* ========== 客户端状态结构 ========== */

/**
 * @brief 单个客户端的速率限制状态
 */
typedef struct client_state {
    char* client_key;               /**< 客户端标识 */
    uint64_t tokens;                /**< 当前令牌数 */
    uint64_t last_update_ns;        /**< 上次更新时间 */
    uint32_t request_count_minute;  /**< 分钟计数器 */
    uint32_t request_count_hour;    /**< 小时计数器 */
    uint64_t minute_start_ns;       /**< 分钟窗口起始时间 */
    uint64_t hour_start_ns;         /**< 小时窗口起始时间 */
    time_t last_access_time;        /**< 最后访问时间（用于清理） */
    struct client_state* next;      /**< 哈希表链表指针 */
} client_state_t;

/* ========== 速率限制器结构 ========== */

struct gateway_rate_limiter {
    gateway_rate_limit_config_t config;     /**< 配置 */
    
    client_state_t** clients_table;         /**< 客户端哈希表 */
    size_t table_size;                      /**< 哈希表大小 */
    
    atomic_uint_fast64_t total_allowed;     /**< 总允许请求数 */
    atomic_uint_fast64_t total_rejected;    /**< 总拒绝请求数 */
    atomic_uint_fast32_t active_clients;    /**< 活跃客户端数 */
    
    atomic_bool running;                    /**< 运行标志 */
    time_t last_cleanup_time;               /**< 上次清理时间 */
};

/* ========== 哈希函数 ========== */

/**
 * @brief 简单的字符串哈希函数（djb2算法）
 */
static uint32_t hash_string(const char* str, size_t table_size) {
    uint32_t hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  /* hash * 33 + c */
    }
    
    return hash % table_size;
}

/* ========== 客户端状态管理 ========== */

/**
 * @brief 创建客户端状态
 */
static client_state_t* client_state_create(const char* client_key, uint64_t now_ns) {
    client_state_t* state = calloc(1, sizeof(client_state_t));
    if (!state) return NULL;
    
    state->client_key = strdup(client_key);
    if (!state->client_key) {
        free(state);
        return NULL;
    }
    
    state->tokens = 0;
    state->last_update_ns = now_ns;
    state->request_count_minute = 0;
    state->request_count_hour = 0;
    state->minute_start_ns = now_ns;
    state->hour_start_ns = now_ns;
    state->last_access_time = time(NULL);
    state->next = NULL;
    
    return state;
}

/**
 * @brief 销毁客户端状态
 */
static void client_state_destroy(client_state_t* state) {
    if (!state) return;
    
    if (state->client_key) {
        free(state->client_key);
    }
    free(state);
}

/**
 * @brief 查找或创建客户端状态
 */
static client_state_t* get_or_create_client(gateway_rate_limiter_t* limiter, 
                                             const char* client_key,
                                             uint64_t now_ns) {
    uint32_t hash = hash_string(client_key, limiter->table_size);
    client_state_t* current = limiter->clients_table[hash];
    
    /* 查找现有客户端 */
    while (current) {
        if (strcmp(current->client_key, client_key) == 0) {
            current->last_access_time = time(NULL);
            return current;
        }
        current = current->next;
    }
    
    /* 创建新客户端 */
    client_state_t* new_client = client_state_create(client_key, now_ns);
    if (!new_client) return NULL;
    
    /* 插入到哈希表 */
    new_client->next = limiter->clients_table[hash];
    limiter->clients_table[hash] = new_client;
    
    atomic_fetch_add(&limiter->active_clients, 1);
    
    return new_client;
}

/* ========== 令牌桶算法 ========== */

/**
 * @brief 更新令牌数
 */
static void update_tokens(client_state_t* client, 
                          const gateway_rate_limit_config_t* config,
                          uint64_t now_ns) {
    if (now_ns <= client->last_update_ns) return;
    
    uint64_t elapsed_ns = now_ns - client->last_update_ns;
    
    /* 计算新增令牌数（每秒补充 config->requests_per_second 个令牌） */
    uint64_t new_tokens = (elapsed_ns * config->requests_per_second) / 1000000000ULL;
    
    /* 更新令牌数，不超过突发容量 */
    client->tokens += new_tokens;
    if (client->tokens > config->burst_size) {
        client->tokens = config->burst_size;
    }
    
    client->last_update_ns = now_ns;
}

/**
 * @brief 检查时间窗口计数器
 */
static bool check_time_windows(client_state_t* client,
                               const gateway_rate_limit_config_t* config,
                               uint64_t now_ns) {
    /* 检查分钟窗口 */
    uint64_t minute_elapsed = now_ns - client->minute_start_ns;
    if (minute_elapsed >= 60000000000ULL) {  /* 60秒 */
        client->request_count_minute = 0;
        client->minute_start_ns = now_ns;
    }
    
    if (config->requests_per_minute > 0 &&
        client->request_count_minute >= config->requests_per_minute) {
        return false;  /* 超过分钟限制 */
    }
    
    /* 检查小时窗口 */
    uint64_t hour_elapsed = now_ns - client->hour_start_ns;
    if (hour_elapsed >= 3600000000000ULL) {  /* 3600秒 */
        client->request_count_hour = 0;
        client->hour_start_ns = now_ns;
    }
    
    if (config->requests_per_hour > 0 &&
        client->request_count_hour >= config->requests_per_hour) {
        return false;  /* 超过小时限制 */
    }
    
    return true;
}

/* ========== 清理过期客户端 ========== */

/**
 * @brief 清理长时间未活跃的客户端
 */
static void cleanup_expired_clients(gateway_rate_limiter_t* limiter) {
    time_t now = time(NULL);
    time_t expire_threshold = now - 3600;  /* 1小时未活跃 */
    
    for (size_t i = 0; i < limiter->table_size; i++) {
        client_state_t** current = &limiter->clients_table[i];
        
        while (*current) {
            if ((*current)->last_access_time < expire_threshold) {
                client_state_t* to_delete = *current;
                *current = (*current)->next;
                client_state_destroy(to_delete);
                atomic_fetch_sub(&limiter->active_clients, 1);
            } else {
                current = &(*current)->next;
            }
        }
    }
    
    limiter->last_cleanup_time = now;
}

/* ========== 辅助函数：速率限制检查 ========== */

/**
 * @brief 前置条件检查
 */
static inline bool is_rate_limiter_valid(const gateway_rate_limiter_t* limiter, const char* client_key) {
    if (!limiter || !client_key) return false;
    if (!limiter->config.enabled) return true;
    return true;
}

/**
 * @brief 可能执行清理（基于时间间隔）
 */
static inline void maybe_cleanup_clients(gateway_rate_limiter_t* limiter, time_t now) {
    if (now - limiter->last_cleanup_time >= limiter->config.cleanup_interval_sec) {
        cleanup_expired_clients(limiter);
    }
}

/**
 * @brief 消耗令牌并更新计数器
 */
static inline void consume_token(client_state_t* client) {
    client->tokens--;
    client->request_count_minute++;
    client->request_count_hour++;
}

/**
 * @brief 检查速率限制（核心逻辑）
 */
static bool check_rate_limit(client_state_t* client, 
                             const gateway_rate_limit_config_t* config,
                             uint64_t now_ns) {
    /* 更新令牌 */
    update_tokens(client, config, now_ns);
    
    /* 检查时间窗口 */
    if (!check_time_windows(client, config, now_ns)) {
        gateway_rate_limiter_t* limiter = client->limiter;
        if (limiter) {
            atomic_fetch_add(&limiter->total_rejected, 1);
        }
        return false;
    }
    
    /* 检查令牌桶 */
    if (client->tokens > 0) {
        consume_token(client);
        gateway_rate_limiter_t* limiter = client->limiter;
        if (limiter) {
            atomic_fetch_add(&limiter->total_allowed, 1);
        }
        return true;
    }
    
    /* 令牌不足，拒绝 */
    gateway_rate_limiter_t* limiter = client->limiter;
    if (limiter) {
        atomic_fetch_add(&limiter->total_rejected, 1);
    }
    return false;
}

/* ========== 公共接口实现 ========== */

void gateway_rate_limiter_get_default_config(gateway_rate_limit_config_t* config) {
    if (!config) return;
    
    config->enabled = false;  /* 默认禁用 */
    config->requests_per_second = 100;
    config->requests_per_minute = 6000;
    config->requests_per_hour = 360000;
    config->burst_size = 150;  /* 允许一定的突发流量 */
    config->cleanup_interval_sec = 300;  /* 5 分钟清理一次 */
}

gateway_rate_limiter_t* gateway_rate_limiter_create(const gateway_rate_limit_config_t* config) {
    gateway_rate_limiter_t* limiter = calloc(1, sizeof(gateway_rate_limiter_t));
    if (!limiter) return NULL;
    
    /* 复制配置 */
    if (config) {
        limiter->config = *config;
    } else {
        gateway_rate_limiter_get_default_config(&limiter->config);
    }
    
    /* 创建哈希表（大小为质数，减少冲突） */
    limiter->table_size = 1021;  /* 质数 */
    limiter->clients_table = calloc(limiter->table_size, sizeof(client_state_t*));
    if (!limiter->clients_table) {
        free(limiter);
        return NULL;
    }
    
    atomic_init(&limiter->total_allowed, 0);
    atomic_init(&limiter->total_rejected, 0);
    atomic_init(&limiter->active_clients, 0);
    atomic_init(&limiter->running, true);
    
    limiter->last_cleanup_time = time(NULL);
    
    return limiter;
}

void gateway_rate_limiter_destroy(gateway_rate_limiter_t* limiter) {
    if (!limiter) return;
    
    atomic_store(&limiter->running, false);
    
    /* 清理所有客户端状态 */
    if (limiter->clients_table) {
        for (size_t i = 0; i < limiter->table_size; i++) {
            client_state_t* current = limiter->clients_table[i];
            while (current) {
                client_state_t* next = current->next;
                client_state_destroy(current);
                current = next;
            }
        }
        free(limiter->clients_table);
    }
    
    free(limiter);
}

bool gateway_rate_limiter_allow(gateway_rate_limiter_t* limiter, const char* client_key) {
    /* 步骤 1: 前置条件检查 */
    if (!is_rate_limiter_valid(limiter, client_key)) {
        return true;
    }
    
    /* 步骤 2: 获取当前时间 */
    uint64_t now_ns = gateway_time_ns();
    time_t now = time(NULL);
    
    /* 步骤 3: 可能执行清理 */
    maybe_cleanup_clients(limiter, now);
    
    /* 步骤 4: 获取或创建客户端状态 */
    client_state_t* client = get_or_create_client(limiter, client_key, now_ns);
    if (!client) {
        /* 内存分配失败，为安全起见允许通过 */
        return true;
    }
    
    /* 步骤 5: 执行速率限制检查 */
    return check_rate_limit(client, &limiter->config, now_ns);
}

void gateway_rate_limiter_get_stats(
    const gateway_rate_limiter_t* limiter,
    uint64_t* total_allowed,
    uint64_t* total_rejected,
    uint32_t* active_clients
) {
    if (!limiter) return;
    
    if (total_allowed) {
        *total_allowed = atomic_load(&limiter->total_allowed);
    }
    if (total_rejected) {
        *total_rejected = atomic_load(&limiter->total_rejected);
    }
    if (active_clients) {
        *active_clients = atomic_load(&limiter->active_clients);
    }
}

void gateway_rate_limiter_reset_client(gateway_rate_limiter_t* limiter, const char* client_key) {
    if (!limiter || !client_key) return;
    
    uint32_t hash = hash_string(client_key, limiter->table_size);
    client_state_t* current = limiter->clients_table[hash];
    
    while (current) {
        if (strcmp(current->client_key, client_key) == 0) {
            current->tokens = 0;
            current->request_count_minute = 0;
            current->request_count_hour = 0;
            current->last_update_ns = gateway_time_ns();
            return;
        }
        current = current->next;
    }
}
