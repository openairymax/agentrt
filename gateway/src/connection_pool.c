/**
 * @file connection_pool.c
 * @brief 连接池管理器实现
 *
 * 提供高效的连接复用和管理，支持动态扩缩容和自动清理。
 * 采用哈希表和链表相结合的方式管理连接，实现高效的连接获取和释放。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "connection_pool.h"
#include "logger.h"
#include "../../../commons/utils/network/include/network_common.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>

/* ========== 连接创建和销毁 ========== */

/**
 * @brief 创建新连接
 * @param pool 连接池
 * @return 连接对象，失败返回NULL
 */
static connection_t* connection_create(connection_pool_t* pool) {
    connection_t* conn = (connection_t*)calloc(1, sizeof(connection_t));
    if (!conn) return NULL;

    conn->type = pool->manager.type;
    conn->state = CONN_STATE_IDLE;
    conn->created_at_ns = agentos_time_monotonic_ns();
    conn->last_used_ns = conn->created_at_ns;
    conn->usage_count = 0;
    conn->bytes_sent = 0;
    conn->bytes_received = 0;
    conn->next = NULL;

    atomic_fetch_add(&pool->connections_created, 1);

    AGENTOS_LOG_DEBUG("Connection created: type=%d", conn->type);
    return conn;
}

/**
 * @brief 销毁连接
 * @param pool 连接池
 * @param conn 连接对象
 */
static void connection_destroy(connection_pool_t* pool, connection_t* conn) {
    if (!conn) return;

    /* 调用销毁回调 */
    if (pool->destroy_callback) {
        pool->destroy_callback(conn);
    }

    /* 清理连接特定数据 */
    switch (conn->type) {
        case CONN_TYPE_HTTP:
            if (conn->data.http.url) free(conn->data.http.url);
            if (conn->data.http.method) free(conn->data.http.method);
            break;
        case CONN_TYPE_WS:
            if (conn->data.ws.endpoint) free(conn->data.ws.endpoint);
            break;
        case CONN_TYPE_TCP:
            if (conn->data.tcp.remote_addr) free(conn->data.tcp.remote_addr);
            break;
        default:
            break;
    }

    free(conn);
    atomic_fetch_add(&pool->connections_destroyed, 1);

    AGENTOS_LOG_DEBUG("Connection destroyed: type=%d", conn->type);
}

/**
 * @brief 检查连接是否需要清理
 * @param pool 连接池
 * @param conn 连接
 * @return 1 需要清理，0 不需要清理
 */
static int connection_should_cleanup(connection_pool_t* pool, connection_t* conn) {
    uint64_t current_time = agentos_time_monotonic_ns();

    /* 检查空闲超时 */
    if (conn->state == CONN_STATE_IDLE) {
        uint64_t idle_time_ns = current_time - conn->last_used_ns;
        if (idle_time_ns > pool->manager.idle_timeout_ms * 1000000ULL) {
            return 1;
        }
    }

    /* 检查最大生命周期 */
    uint64_t lifetime_ns = current_time - conn->created_at_ns;
    if (lifetime_ns > pool->manager.max_lifetime_ms * 1000000ULL) {
        return 1;
    }

    /* 检查最大使用次数 */
    if (conn->usage_count >= pool->manager.max_usage_count) {
        return 1;
    }

    /* 检查内存使用 */
    if (pool->manager.max_memory_usage > 0) {
        size_t conn_memory = sizeof(connection_t);
        switch (conn->type) {
            case CONN_TYPE_HTTP:
                conn_memory += strlen(conn->data.http.url) + strlen(conn->data.http.method);
                break;
            case CONN_TYPE_WS:
                conn_memory += strlen(conn->data.ws.endpoint);
                break;
            case CONN_TYPE_TCP:
                conn_memory += strlen(conn->data.tcp.remote_addr);
                break;
            default:
                break;
        }

        if (conn_memory > pool->manager.max_memory_usage) {
            return 1;
        }
    }

    return 0;
}

/* ========== 连接池操作 ========== */

/**
 * @brief 从空闲列表获取连接
 * @param pool 连接池
 * @return 连接对象，失败返回NULL
 */
static connection_t* get_idle_connection(connection_pool_t* pool) {
    connection_t* conn = pool->idle_list;
    if (!conn) return NULL;

    /* 从空闲列表移除 */
    pool->idle_list = conn->next;
    pool->idle_count--;

    /* 重置连接状态 */
    conn->state = CONN_STATE_ACTIVE;
    conn->last_used_ns = agentos_time_monotonic_ns();
    conn->usage_count++;

    atomic_fetch_add(&pool->connections_reused, 1);

    AGENTOS_LOG_DEBUG("Connection reused: type=%d, usage_count=%zu", conn->type, conn->usage_count);
    return conn;
}

/**
 * @brief 添加连接到空闲列表
 * @param pool 连接池
 * @param conn 连接对象
 */
static void add_idle_connection(connection_pool_t* pool, connection_t* conn) {
    conn->state = CONN_STATE_IDLE;
    conn->next = pool->idle_list;
    pool->idle_list = conn;
    pool->idle_count++;
}

/**
 * @brief 创建新连接并添加到活跃列表
 * @param pool 连接池
 * @return 连接对象，失败返回NULL
 */
static connection_t* create_and_activate_connection(connection_pool_t* pool) {
    /* 检查最大连接数限制 */
    if (pool->total_count >= pool->manager.max_size) {
        return NULL;
    }

    /* 创建新连接 */
    connection_t* conn = connection_create(pool);
    if (!conn) return NULL;

    /* 添加到活跃列表 */
    conn->next = pool->active_list;
    pool->active_list = conn;
    pool->active_count++;
    pool->total_count++;

    return conn;
}

/* ========== 清理线程 ========== */

/**
 * @brief 清理线程函数
 * @param arg 连接池
 * @return NULL
 */
static void* cleanup_thread_func(void* arg) {
    connection_pool_t* pool = (connection_pool_t*)arg;

    while (atomic_load(&pool->running)) {
        pthread_mutex_lock(&pool->lock);

        /* 清理空闲连接 */
        connection_t* current = pool->idle_list;
        connection_t* prev = NULL;

        while (current) {
            if (connection_should_cleanup(pool, current)) {
                /* 从列表中移除 */
                if (prev) {
                    prev->next = current->next;
                } else {
                    pool->idle_list = current->next;
                }
                pool->idle_count--;

                /* 销毁连接 */
                connection_destroy(pool, current);
                pool->total_count--;

                current = prev ? prev->next : pool->idle_list;
            } else {
                prev = current;
                current = current->next;
            }
        }

        /* 如果空闲连接太少，创建新的连接 */
        if (pool->idle_count < pool->manager.min_size) {
            size_t needed = pool->manager.min_size - pool->idle_count;
            for (size_t i = 0; i < needed && pool->total_count < pool->manager.max_size; i++) {
                connection_t* conn = create_and_activate_connection(pool);
                if (conn) {
                    add_idle_connection(pool, conn);
                    pool->active_count--;
                }
            }
        }

        pthread_mutex_unlock(&pool->lock);

        /* 等待一段时间 */
        sleep(30);
    }

    return NULL;
}

/* ========== 公共API实现 ========== */

connection_pool_t* connection_pool_create(const pool_config_t* manager) {
    if (!manager) return NULL;

    /* 分配连接池 */
    connection_pool_t* pool = (connection_pool_t*)calloc(1, sizeof(connection_pool_t));
    if (!pool) return NULL;

    /* 初始化配置 */
    memcpy(&pool->manager, manager, sizeof(pool_config_t));

    /* 初始化同步原语 */
    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool);
        return NULL;
    }

    /* 初始化统计信息 */
    atomic_init(&pool->connections_created, 0);
    atomic_init(&pool->connections_destroyed, 0);
    atomic_init(&pool->connections_reused, 0);
    atomic_init(&pool->bytes_sent, 0);
    atomic_init(&pool->bytes_received, 0);
    atomic_init(&pool->running, true);

    /* 初始化连接列表 */
    pool->active_list = NULL;
    pool->idle_list = NULL;
    pool->active_count = 0;
    pool->idle_count = 0;
    pool->total_count = 0;

    /* 创建初始连接 */
    for (size_t i = 0; i < pool->manager.initial_size; i++) {
        connection_t* conn = create_and_activate_connection(pool);
        if (conn) {
            add_idle_connection(pool, conn);
            pool->active_count--;
        } else {
            break;
        }
    }

    /* 启动清理线程 */
    if (pthread_create(&pool->cleanup_thread, NULL, cleanup_thread_func, pool) != 0) {
        pthread_mutex_destroy(&pool->lock);
        free(pool);
        return NULL;
    }

    AGENTOS_LOG_INFO("Connection pool created: type=%d, initial_size=%zu, max_size=%zu",
                     pool->manager.type, pool->manager.initial_size, pool->manager.max_size);

    return pool;
}

void connection_pool_destroy(connection_pool_t* pool) {
    if (!pool) return;

    /* 停止清理线程 */
    atomic_store(&pool->running, false);
    pthread_join(pool->cleanup_thread, NULL);

    pthread_mutex_lock(&pool->lock);

    /* 销毁所有连接 */
    connection_t* current = pool->active_list;
    while (current) {
        connection_t* next = current->next;
        connection_destroy(pool, current);
        current = next;
    }

    current = pool->idle_list;
    while (current) {
        connection_t* next = current->next;
        connection_destroy(pool, current);
        current = next;
    }

    pthread_mutex_unlock(&pool->lock);

    /* 销毁同步原语 */
    pthread_mutex_destroy(&pool->lock);

    free(pool);

    AGENTOS_LOG_INFO("Connection pool destroyed");
}

agentos_error_t connection_pool_get(connection_pool_t* pool, connection_t** out_connection) {
    if (!pool || !out_connection) return AGENTOS_EINVAL;

    pthread_mutex_lock(&pool->lock);

    /* 尝试从空闲列表获取连接 */
    connection_t* conn = get_idle_connection(pool);

    if (!conn) {
        /* 创建新连接 */
        conn = create_and_activate_connection(pool);
        if (!conn) {
            pthread_mutex_unlock(&pool->lock);
            return AGENTOS_ENOENT;
        }
    }

    /* 添加到活跃列表 */
    conn->next = pool->active_list;
    pool->active_list = conn;
    pool->active_count++;

    pthread_mutex_unlock(&pool->lock);

    *out_connection = conn;
    return AGENTOS_SUCCESS;
}

agentos_error_t connection_pool_release(connection_pool_t* pool, connection_t* connection) {
    if (!pool || !connection) return AGENTOS_EINVAL;

    pthread_mutex_lock(&pool->lock);

    /* 从活跃列表移除 */
    connection_t* current = pool->active_list;
    connection_t* prev = NULL;

    while (current) {
        if (current == connection) {
            if (prev) {
                prev->next = current->next;
            } else {
                pool->active_list = current->next;
            }
            pool->active_count--;
            break;
        }
        prev = current;
        current = current->next;
    }

    /* 添加到空闲列表 */
    add_idle_connection(pool, connection);
    pool->active_count--;

    pthread_mutex_unlock(&pool->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t connection_pool_close(connection_pool_t* pool, connection_t* connection) {
    if (!pool || !connection) return AGENTOS_EINVAL;

    pthread_mutex_lock(&pool->lock);

    /* 从活跃列表移除 */
    connection_t* current = pool->active_list;
    connection_t* prev = NULL;

    while (current) {
        if (current == connection) {
            if (prev) {
                prev->next = current->next;
            } else {
                pool->active_list = current->next;
            }
            pool->active_count--;
            break;
        }
        prev = current;
        current = current->next;
    }

    /* 销毁连接 */
    connection_destroy(pool, connection);
    pool->total_count--;

    pthread_mutex_unlock(&pool->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t connection_pool_get_stats(connection_pool_t* pool, char** out_json) {
    if (!pool || !out_json) return AGENTOS_EINVAL;

    pthread_mutex_lock(&pool->lock);

    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "type", pool->manager.type);
    cJSON_AddNumberToObject(stats, "active_count", pool->active_count);
    cJSON_AddNumberToObject(stats, "idle_count", pool->idle_count);
    cJSON_AddNumberToObject(stats, "total_count", pool->total_count);
    cJSON_AddNumberToObject(stats, "connections_created", (double)atomic_load(&pool->connections_created));
    cJSON_AddNumberToObject(stats, "connections_destroyed", (double)atomic_load(&pool->connections_destroyed));
    cJSON_AddNumberToObject(stats, "connections_reused", (double)atomic_load(&pool->connections_reused));
    cJSON_AddNumberToObject(stats, "bytes_sent", (double)atomic_load(&pool->bytes_sent));
    cJSON_AddNumberToObject(stats, "bytes_received", (double)atomic_load(&pool->bytes_received));

    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);

    pthread_mutex_unlock(&pool->lock);

    *out_json = json_str;
    return AGENTOS_SUCCESS;
}

agentos_error_t connection_pool_cleanup(connection_pool_t* pool, size_t max_idle_count) {
    if (!pool) return AGENTOS_EINVAL;

    pthread_mutex_lock(&pool->lock);

    /* 清理空闲连接 */
    connection_t* current = pool->idle_list;
    connection_t* prev = NULL;

    while (current && pool->idle_count > max_idle_count) {
        connection_t* next = current->next;

        /* 从列表中移除 */
        if (prev) {
            prev->next = next;
        } else {
            pool->idle_list = next;
        }
        pool->idle_count--;

        /* 销毁连接 */
        connection_destroy(pool, current);
        pool->total_count--;

        current = next;
    }

    pthread_mutex_unlock(&pool->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t connection_pool_reset(connection_pool_t* pool) {
    if (!pool) return AGENTOS_EINVAL;

    pthread_mutex_lock(&pool->lock);

    /* 销毁所有连接 */
    connection_t* current = pool->active_list;
    while (current) {
        connection_t* next = current->next;
        connection_destroy(pool, current);
        current = next;
    }

    current = pool->idle_list;
    while (current) {
        connection_t* next = current->next;
        connection_destroy(pool, current);
        current = next;
    }

    /* 重置列表 */
    pool->active_list = NULL;
    pool->idle_list = NULL;
    pool->active_count = 0;
    pool->idle_count = 0;
    pool->total_count = 0;

    /* 重新创建初始连接 */
    for (size_t i = 0; i < pool->manager.initial_size; i++) {
        connection_t* conn = create_and_activate_connection(pool);
        if (conn) {
            add_idle_connection(pool, conn);
            pool->active_count--;
        } else {
            break;
        }
    }

    pthread_mutex_unlock(&pool->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t connection_pool_set_callbacks(
    connection_pool_t* pool,
    connection_t* (*create_callback)(connection_type_t type),
    void (*destroy_callback)(connection_t* connection),
    void* user_data) {

    if (!pool) return AGENTOS_EINVAL;

    pthread_mutex_lock(&pool->lock);

    pool->create_callback = create_callback;
    pool->destroy_callback = destroy_callback;
    pool->user_data = user_data;

    pthread_mutex_unlock(&pool->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t connection_pool_get_config(connection_pool_t* pool, pool_config_t* out_config) {
    if (!pool || !out_config) return AGENTOS_EINVAL;

    pthread_mutex_lock(&pool->lock);

    memcpy(out_config, &pool->manager, sizeof(pool_config_t));

    pthread_mutex_unlock(&pool->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t connection_pool_update_config(connection_pool_t* pool, const pool_config_t* manager) {
    if (!pool || !manager) return AGENTOS_EINVAL;

    pthread_mutex_lock(&pool->lock);

    /* 更新配置 */
    memcpy(&pool->manager, manager, sizeof(pool_config_t));

    pthread_mutex_unlock(&pool->lock);

    return AGENTOS_SUCCESS;
}