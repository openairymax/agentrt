/**
 * @file checkpoint_adapter.c
 * @brief C-L07: Checkpoint → CoreLoopThree 检查点适配器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现 CoreLoopThree 认知循环与检查点系统的集成，
 * 支持长任务的状态持久化和崩溃恢复。
 *
 * 数据流：
 *   CoreLoopThree loop iteration → checkpoint_adapter_save()
 *     → agentrt_checkpoint_save() → heapstore
 *     → incremental save (delta encoding)
 *
 *   Crash recovery → checkpoint_adapter_restore()
 *     → agentrt_checkpoint_restore() → heapstore
 *     → restore cognition_state + memory_context + tool_call_history
 */

#include "checkpoint_adapter.h"
#include "checkpoint.h"

#include "logger.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "agentrt_quality.h"

/* 跨平台路径常量（AGENTRT_DATA_DIR） */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

/* ==================== 默认配置 ==================== */

#define DEFAULT_STORAGE_PATH          AGENTRT_DATA_DIR "/checkpoints"
#define DEFAULT_SAVE_INTERVAL_TURNS   10
#define DEFAULT_SAVE_INTERVAL_MS      30000
#define DEFAULT_MAX_CHECKPOINTS       100
#define DEFAULT_MAX_AGE_SECONDS       (7 * 24 * 3600)  /* 7 天 */

/* ==================== 适配器内部结构 ==================== */

struct checkpoint_adapter_s {
    checkpoint_adapter_config_t config;
    char storage_path[512];
    bool initialized;

    /* 检查点追踪 */
    uint32_t current_turn;             /* 当前轮次 */
    uint64_t last_save_time_ms;        /* 最后保存时间 */
    uint64_t sequence_counter;         /* 序列号计数器 */

    /* 统计 */
    uint64_t total_saves;
    uint64_t total_restores;
    uint64_t total_errors;
    uint64_t last_save_timestamp;
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 获取当前时间戳（毫秒）
 */
static uint64_t get_current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief 将快照序列化为状态 JSON 字符串
 */
static char *snapshot_to_state_json(const checkpoint_snapshot_t *snapshot)
{
    if (!snapshot) return NULL;

    size_t estimated_size = 65536;
    if (snapshot->cognition_state_json) {
        estimated_size += strlen(snapshot->cognition_state_json);
    }
    if (snapshot->memory_context_json) {
        estimated_size += strlen(snapshot->memory_context_json);
    }
    if (snapshot->tool_call_history_json) {
        estimated_size += strlen(snapshot->tool_call_history_json);
    }
    if (snapshot->pending_nodes_json) {
        estimated_size += strlen(snapshot->pending_nodes_json);
    }
    if (snapshot->completed_nodes_json) {
        estimated_size += strlen(snapshot->completed_nodes_json);
    }

    char *json = (char *)AGENTRT_MALLOC(estimated_size + 1024);
    if (!json) return NULL;

    snprintf(json, estimated_size + 1024,
             "{"
             "\"task_id\":\"%s\","
             "\"session_id\":\"%s\","
             "\"sequence_num\":%llu,"
             "\"timestamp\":%llu,"
             "\"current_turn\":%u,"
             "\"total_turns\":%u,"
             "\"progress_percent\":%.2f,"
             "\"cognition_state\":%s,"
             "\"memory_context\":%s,"
             "\"tool_call_history\":%s,"
             "\"pending_nodes\":%s,"
             "\"completed_nodes\":%s"
             "}",
             snapshot->task_id ? snapshot->task_id : "",
             snapshot->session_id ? snapshot->session_id : "",
             (unsigned long long)snapshot->sequence_num,
             (unsigned long long)snapshot->timestamp,
             snapshot->current_turn,
             snapshot->total_turns,
             (double)snapshot->progress_percent,
             snapshot->cognition_state_json ? snapshot->cognition_state_json : "{}",
             snapshot->memory_context_json ? snapshot->memory_context_json : "{}",
             snapshot->tool_call_history_json ? snapshot->tool_call_history_json : "[]",
             snapshot->pending_nodes_json ? snapshot->pending_nodes_json : "[]",
             snapshot->completed_nodes_json ? snapshot->completed_nodes_json : "[]");

    return json;
}

/**
 * @brief 从 JSON 字符串中提取指定 key 的值（对象或数组）
 *
 * 使用平衡括号匹配提取完整的 JSON 对象 `{...}` 或数组 `[...]`，
 * 正确处理字符串内的括号和转义字符。适用于 checkpoint state_json
 * 这种扁平顶层结构的字段提取。
 *
 * @param json JSON 字符串
 * @param key  字段名（不含引号）
 * @return AGENTRT_MALLOC 分配的字符串（含外层括号/方括号），调用者释放；
 *         未找到或格式错误返回 NULL
 */
static char *extract_json_value(const char *json, const char *key)
{
    if (!json || !key) return NULL;

    /* 搜索 "key" — 带双引号的完整 key 不会误匹配前缀重叠的 key 名
     * （如 "turn" 不会匹配 "current_turn"，因为后者中 turn 前是 _ 不是 "） */
    char search[256];
    int sn = snprintf(search, sizeof(search), "\"%s\"", key);
    if (sn <= 0 || (size_t)sn >= sizeof(search))
        return NULL;

    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);

    /* 跳过空白，期望冒号 */
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    if (*p != ':') return NULL;
    p++;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;

    /* 值必须是对象或数组 */
    if (*p != '{' && *p != '[') return NULL;

    char open_ch = *p;
    char close_ch = (open_ch == '{') ? '}' : ']';
    const char *start = p;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;

    while (*p) {
        if (escaped) {
            escaped = false;
            p++;
            continue;
        }
        if (in_string && *p == '\\') {
            escaped = true;
            p++;
            continue;
        }
        if (*p == '"') {
            in_string = !in_string;
        } else if (!in_string) {
            if (*p == open_ch) {
                depth++;
            } else if (*p == close_ch) {
                depth--;
                if (depth == 0) {
                    p++;  /* 包含闭括号 */
                    size_t len = (size_t)(p - start);
                    char *val = (char *)AGENTRT_MALLOC(len + 1);
                    if (!val) return NULL;
                    __builtin_memcpy(val, start, len);
                    val[len] = '\0';
                    return val;
                }
            }
        }
        p++;
    }
    return NULL;  /* 未闭合，格式错误 */
}

/**
 * @brief 从 JSON 字符串中提取 uint32 标量值
 */
static uint32_t extract_uint32_from_json(const char *json, const char *key)
{
    if (!json || !key) return 0;
    char search[256];
    int sn = snprintf(search, sizeof(search), "\"%s\"", key);
    if (sn <= 0 || (size_t)sn >= sizeof(search))
        return 0;
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n' || *p == '\r'))
        p++;
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10u + (uint32_t)(*p - '0');
        p++;
    }
    return v;
}

/**
 * @brief 从 JSON 字符串中提取 float 标量值
 */
static float extract_float_from_json(const char *json, const char *key)
{
    if (!json || !key) return 0.0f;
    char search[256];
    int sn = snprintf(search, sizeof(search), "\"%s\"", key);
    if (sn <= 0 || (size_t)sn >= sizeof(search))
        return 0.0f;
    const char *p = strstr(json, search);
    if (!p) return 0.0f;
    p += strlen(search);
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n' || *p == '\r'))
        p++;
    return (float)strtod(p, NULL);
}

/**
 * @brief 从 agentrt_task_checkpoint_t 恢复快照
 *
 * 从 ckpt->state_json（由 snapshot_to_state_json 生成的扁平 JSON 对象）
 * 中解析出全部字段：cognition_state、memory_context、tool_call_history、
 * pending_nodes、completed_nodes（嵌套 JSON 值），以及 current_turn、
 * total_turns、progress_percent（标量）。
 *
 * 回退路径：若 state_json 中未含 completed_nodes/pending_nodes（非 adapter
 * 路径直接创建的 checkpoint），则从 ckpt 的节点数组重建 JSON。
 */
static checkpoint_snapshot_t *checkpoint_to_snapshot(
    const agentrt_task_checkpoint_t *ckpt)
{
    if (!ckpt) return NULL;

    checkpoint_snapshot_t *snapshot =
        (checkpoint_snapshot_t *)AGENTRT_CALLOC(1, sizeof(checkpoint_snapshot_t));
    if (!snapshot) return NULL;

    snapshot->task_id = AGENTRT_STRDUP(ckpt->task_id);
    snapshot->session_id = AGENTRT_STRDUP(ckpt->session_id);
    snapshot->sequence_num = ckpt->sequence_num;
    snapshot->timestamp = ckpt->timestamp;

    if (ckpt->state_json) {
        /* 提取嵌套 JSON 值（对象/数组） */
        snapshot->cognition_state_json =
            extract_json_value(ckpt->state_json, "cognition_state");
        snapshot->memory_context_json =
            extract_json_value(ckpt->state_json, "memory_context");
        snapshot->tool_call_history_json =
            extract_json_value(ckpt->state_json, "tool_call_history");
        snapshot->pending_nodes_json =
            extract_json_value(ckpt->state_json, "pending_nodes");
        snapshot->completed_nodes_json =
            extract_json_value(ckpt->state_json, "completed_nodes");

        /* 提取标量字段 */
        snapshot->current_turn =
            extract_uint32_from_json(ckpt->state_json, "current_turn");
        snapshot->total_turns =
            extract_uint32_from_json(ckpt->state_json, "total_turns");
        snapshot->progress_percent =
            extract_float_from_json(ckpt->state_json, "progress_percent");

        /* 回退：若 state_json 中未含 completed_nodes（非 adapter 路径
         * 创建的 checkpoint），从 ckpt 数组重建 JSON 字符串。 */
        if (!snapshot->completed_nodes_json && ckpt->completed_count > 0 &&
            ckpt->completed_nodes) {
            size_t total_len = 0;
            for (size_t i = 0; i < ckpt->completed_count; i++) {
                if (ckpt->completed_nodes[i]) {
                    total_len += strlen(ckpt->completed_nodes[i]) + 4;
                }
            }
            snapshot->completed_nodes_json =
                (char *)AGENTRT_MALLOC(total_len + 64);
            if (snapshot->completed_nodes_json) {
                snapshot->completed_nodes_json[0] = '[';
                size_t offset = 1;
                for (size_t i = 0; i < ckpt->completed_count; i++) {
                    if (ckpt->completed_nodes[i]) {
                        if (i > 0) {
                            snapshot->completed_nodes_json[offset++] = ',';
                        }
                        offset += (size_t)snprintf(
                            snapshot->completed_nodes_json + offset,
                            total_len + 64 - offset,
                            "\"%s\"", ckpt->completed_nodes[i]);
                    }
                }
                snapshot->completed_nodes_json[offset++] = ']';
                snapshot->completed_nodes_json[offset] = '\0';
            }
        }

        if (!snapshot->pending_nodes_json && ckpt->pending_count > 0 &&
            ckpt->pending_nodes) {
            size_t total_len = 0;
            for (size_t i = 0; i < ckpt->pending_count; i++) {
                if (ckpt->pending_nodes[i]) {
                    total_len += strlen(ckpt->pending_nodes[i]) + 4;
                }
            }
            snapshot->pending_nodes_json =
                (char *)AGENTRT_MALLOC(total_len + 64);
            if (snapshot->pending_nodes_json) {
                snapshot->pending_nodes_json[0] = '[';
                size_t offset = 1;
                for (size_t i = 0; i < ckpt->pending_count; i++) {
                    if (ckpt->pending_nodes[i]) {
                        if (i > 0) {
                            snapshot->pending_nodes_json[offset++] = ',';
                        }
                        offset += (size_t)snprintf(
                            snapshot->pending_nodes_json + offset,
                            total_len + 64 - offset,
                            "\"%s\"", ckpt->pending_nodes[i]);
                    }
                }
                snapshot->pending_nodes_json[offset++] = ']';
                snapshot->pending_nodes_json[offset] = '\0';
            }
        }
    }

    return snapshot;
}

/**
 * @brief 解析 JSON 字符串数组（如 ["a","b","c"]）为 char** 数组
 *
 * 自包含解析器，不依赖 cJSON，处理由 checkpoint_to_snapshot 生成的标准格式。
 * 调用者负责通过 AGENTRT_FREE 释放返回的数组及其中的每个字符串。
 *
 * @param json JSON 数组字符串（可为 NULL 或 "[]"）
 * @param out_array 输出字符串数组（NULL 表示空数组）
 * @param out_count 输出元素数量
 * @return 0 成功，-1 解析错误，-2 内存不足
 */
static int parse_json_string_array(const char *json, char ***out_array, size_t *out_count)
{
    *out_array = NULL;
    *out_count = 0;

    if (!json || !json[0])
        return 0;

    /* 跳过前导空白 */
    const char *p = json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

    if (*p != '[')
        return -1;  /* 不是 JSON 数组 */
    p++;  /* 跳过 '[' */

    /* 空数组检查 */
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == ']')
        return 0;  /* 空数组 */

    /* 第一遍：计算元素数量 */
    const char *scan = p;
    size_t count = 0;
    bool in_string = false;
    while (*scan) {
        if (*scan == '"') {
            if (!in_string) {
                count++;
                in_string = true;
            } else {
                in_string = false;
            }
        } else if (*scan == ']' && !in_string) {
            break;
        }
        scan++;
    }

    if (count == 0)
        return 0;

    /* 分配数组 */
    char **array = (char **)AGENTRT_CALLOC(count, sizeof(char *));
    if (!array)
        return -2;

    /* 第二遍：提取每个字符串 */
    size_t idx = 0;
    while (*p && idx < count) {
        /* 跳过到下一个引号 */
        while (*p && *p != '"')
            p++;
        if (*p != '"')
            break;
        p++;  /* 跳过开引号 */

        /* 计算字符串长度（处理转义） */
        const char *start = p;
        size_t len = 0;
        while (*p) {
            if (*p == '\\' && *(p + 1)) {
                len++;
                p += 2;
            } else if (*p == '"') {
                break;
            } else {
                len++;
                p++;
            }
        }

        if (*p != '"') {
            /* 格式错误：未闭合的字符串 */
            for (size_t i = 0; i < idx; i++)
                AGENTRT_FREE(array[i]);
            AGENTRT_FREE(array);
            return -1;
        }

        /* 分配并复制字符串（处理转义） */
        char *str = (char *)AGENTRT_MALLOC(len + 1);
        if (!str) {
            for (size_t i = 0; i < idx; i++)
                AGENTRT_FREE(array[i]);
            AGENTRT_FREE(array);
            return -2;
        }

        const char *src = start;
        size_t di = 0;
        while (src < p) {
            if (*src == '\\' && *(src + 1)) {
                src++;
                switch (*src) {
                case 'n':  str[di++] = '\n'; break;
                case 't':  str[di++] = '\t'; break;
                case 'r':  str[di++] = '\r'; break;
                case '"':  str[di++] = '"';  break;
                case '\\': str[di++] = '\\'; break;
                default:   str[di++] = *src; break;
                }
                src++;
            } else {
                str[di++] = *src++;
            }
        }
        str[di] = '\0';
        array[idx++] = str;

        p++;  /* 跳过闭引号 */

        /* 跳过到下一个元素或数组结束 */
        while (*p && *p != ',' && *p != ']')
            p++;
        if (*p == ',')
            p++;
    }

    *out_array = array;
    *out_count = idx;
    return 0;
}

/**
 * @brief 构建 completed_nodes 和 pending_nodes 数组
 *
 * 从 snapshot 的 JSON 字符串字段解析出节点名数组。
 * 调用者负责释放返回的数组。
 */
static int build_node_arrays(const checkpoint_snapshot_t *snapshot,
                             char ***out_completed_nodes,
                             size_t *out_completed_count,
                             char ***out_pending_nodes,
                             size_t *out_pending_count)
{
    int rc = 0;

    if (out_completed_nodes && out_completed_count) {
        rc = parse_json_string_array(snapshot ? snapshot->completed_nodes_json : NULL,
                                     out_completed_nodes, out_completed_count);
        if (rc != 0) {
            *out_completed_nodes = NULL;
            *out_completed_count = 0;
            return rc;
        }
    }

    if (out_pending_nodes && out_pending_count) {
        rc = parse_json_string_array(snapshot ? snapshot->pending_nodes_json : NULL,
                                     out_pending_nodes, out_pending_count);
        if (rc != 0) {
            /* 释放已分配的 completed 数组 */
            if (out_completed_nodes && *out_completed_nodes) {
                for (size_t i = 0; i < *out_completed_count; i++)
                    AGENTRT_FREE((*out_completed_nodes)[i]);
                AGENTRT_FREE(*out_completed_nodes);
                *out_completed_nodes = NULL;
                *out_completed_count = 0;
            }
            *out_pending_nodes = NULL;
            *out_pending_count = 0;
            return rc;
        }
    }

    return 0;
}

/* ==================== 生命周期实现 ==================== */

checkpoint_adapter_t *checkpoint_adapter_create(
    const checkpoint_adapter_config_t *config)
{
    checkpoint_adapter_t *adapter =
        (checkpoint_adapter_t *)AGENTRT_CALLOC(1, sizeof(checkpoint_adapter_t));
    if (!adapter) {
        AGENTRT_LOG_ERROR("C-L07: checkpoint_adapter_create: OOM");
        return NULL;
    }

    /* 应用配置 */
    const char *path = (config && config->storage_path)
                           ? config->storage_path : DEFAULT_STORAGE_PATH;
    safe_strcpy(adapter->storage_path, sizeof(adapter->storage_path), path);

    adapter->config.save_interval_turns =
        (config && config->save_interval_turns > 0)
            ? config->save_interval_turns : DEFAULT_SAVE_INTERVAL_TURNS;
    adapter->config.save_interval_ms =
        (config && config->save_interval_ms > 0)
            ? config->save_interval_ms : DEFAULT_SAVE_INTERVAL_MS;
    adapter->config.enable_incremental_save =
        config ? config->enable_incremental_save : false;
    adapter->config.enable_compression =
        config ? config->enable_compression : false;
    adapter->config.max_checkpoints_per_task =
        (config && config->max_checkpoints_per_task > 0)
            ? config->max_checkpoints_per_task : DEFAULT_MAX_CHECKPOINTS;
    adapter->config.max_age_seconds =
        (config && config->max_age_seconds > 0)
            ? config->max_age_seconds : DEFAULT_MAX_AGE_SECONDS;

    /* 初始化检查点存储 */
    agentrt_error_t ret = agentrt_checkpoint_init(adapter->storage_path);
    if (ret != AGENTRT_SUCCESS) {
        AGENTRT_LOG_WARN("C-L07: checkpoint_init failed for '%s': ret=%d",
                         adapter->storage_path, ret);
        /* 非致命 — 检查点功能降级 */
    }

    adapter->current_turn = 0;
    adapter->last_save_time_ms = get_current_time_ms();
    adapter->sequence_counter = 0;
    adapter->total_saves = 0;
    adapter->total_restores = 0;
    adapter->total_errors = 0;
    adapter->last_save_timestamp = 0;
    adapter->initialized = true;

    AGENTRT_LOG_INFO("C-L07: Checkpoint adapter created (path=%s, "
                     "interval_turns=%u, interval_ms=%u, incremental=%d, "
                     "max_per_task=%u, max_age=%llus)",
                     adapter->storage_path,
                     adapter->config.save_interval_turns,
                     adapter->config.save_interval_ms,
                     adapter->config.enable_incremental_save,
                     adapter->config.max_checkpoints_per_task,
                     (unsigned long long)adapter->config.max_age_seconds);
    return adapter;
}

void checkpoint_adapter_destroy(checkpoint_adapter_t *adapter)
{
    if (!adapter) return;

    AGENTRT_LOG_INFO("C-L07: Checkpoint adapter destroyed "
                     "(saves=%llu restores=%llu errors=%llu)",
                     (unsigned long long)adapter->total_saves,
                     (unsigned long long)adapter->total_restores,
                     (unsigned long long)adapter->total_errors);

    agentrt_checkpoint_shutdown();
    AGENTRT_FREE(adapter);
}

/* ==================== 检查点保存实现 ==================== */

int checkpoint_adapter_save(checkpoint_adapter_t *adapter,
                            const char *task_id,
                            const char *session_id,
                            uint64_t sequence_num,
                            const checkpoint_snapshot_t *snapshot)
{
    if (!adapter || !task_id || !snapshot) return -1;

    if (!adapter->initialized) {
        AGENTRT_LOG_WARN("C-L07: Checkpoint adapter not initialized");
        return -1;
    }

    /* 序列化快照为状态 JSON */
    char *state_json = snapshot_to_state_json(snapshot);
    if (!state_json) {
        AGENTRT_LOG_ERROR("C-L07: Failed to serialize snapshot for task '%s'",
                          task_id);
        adapter->total_errors++;
        return -1;
    }

    /* 构建节点数组 */
    char **completed_nodes = NULL;
    size_t completed_count = 0;
    char **pending_nodes = NULL;
    size_t pending_count = 0;

    build_node_arrays(snapshot, &completed_nodes, &completed_count,
                      &pending_nodes, &pending_count);

    /* 创建检查点 */
    agentrt_task_checkpoint_t *checkpoint = NULL;
    agentrt_error_t ret = agentrt_checkpoint_create(
        task_id,
        session_id ? session_id : "",
        sequence_num > 0 ? sequence_num : ++adapter->sequence_counter,
        state_json,
        completed_nodes,
        completed_count,
        pending_nodes,
        pending_count,
        &checkpoint);

    AGENTRT_FREE(state_json);

    /* agentrt_checkpoint_create 通过 copy_nodes 深拷贝了节点数组，
     * 调用者负责释放 build_node_arrays 分配的原数组及其字符串。 */
    if (completed_nodes) {
        for (size_t i = 0; i < completed_count; i++)
            AGENTRT_FREE(completed_nodes[i]);
        AGENTRT_FREE(completed_nodes);
        completed_nodes = NULL;
    }
    if (pending_nodes) {
        for (size_t i = 0; i < pending_count; i++)
            AGENTRT_FREE(pending_nodes[i]);
        AGENTRT_FREE(pending_nodes);
        pending_nodes = NULL;
    }

    if (ret != AGENTRT_SUCCESS || !checkpoint) {
        AGENTRT_LOG_ERROR("C-L07: Failed to create checkpoint for task '%s': "
                          "ret=%d", task_id, ret);
        adapter->total_errors++;
        return -1;
    }

    /* 保存检查点到 heapstore */
    ret = agentrt_checkpoint_save(checkpoint);
    if (ret != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("C-L07: Failed to save checkpoint for task '%s': "
                          "ret=%d", task_id, ret);
        agentrt_checkpoint_destroy(checkpoint);
        adapter->total_errors++;
        return -1;
    }

    /* 更新追踪状态 */
    adapter->current_turn++;
    adapter->last_save_time_ms = get_current_time_ms();
    adapter->last_save_timestamp = checkpoint->timestamp;
    adapter->total_saves++;

    /* 清理旧检查点 */
    if (adapter->config.max_age_seconds > 0) {
        agentrt_checkpoint_cleanup(adapter->config.max_age_seconds,
                                   adapter->config.max_checkpoints_per_task);
    }

    AGENTRT_LOG_DEBUG("C-L07: Checkpoint saved for task '%s' (seq=%llu, "
                      "turn=%u, size=%zu)",
                      task_id,
                      (unsigned long long)checkpoint->sequence_num,
                      adapter->current_turn,
                      checkpoint->state_size);

    agentrt_checkpoint_destroy(checkpoint);
    return 0;
}

/* ==================== 检查点恢复实现 ==================== */

int checkpoint_adapter_restore(checkpoint_adapter_t *adapter,
                               const char *task_id,
                               checkpoint_snapshot_t **out_snapshot)
{
    if (!adapter || !task_id || !out_snapshot) return -1;

    *out_snapshot = NULL;

    if (!adapter->initialized) {
        AGENTRT_LOG_WARN("C-L07: Checkpoint adapter not initialized");
        return -1;
    }

    /* 恢复最新检查点（sequence_num = 0 表示最新） */
    agentrt_task_checkpoint_t *checkpoint = NULL;
    agentrt_error_t ret = agentrt_checkpoint_restore(task_id, 0, &checkpoint);

    if (ret != AGENTRT_SUCCESS) {
        if (ret == AGENTRT_ENOENT) {
            AGENTRT_LOG_INFO("C-L07: No checkpoint found for task '%s'",
                             task_id);
        } else {
            AGENTRT_LOG_ERROR("C-L07: Failed to restore checkpoint for task "
                              "'%s': ret=%d", task_id, ret);
            adapter->total_errors++;
        }
        return -1;
    }

    if (!checkpoint) {
        return -1;
    }

    /* 转换为快照 */
    *out_snapshot = checkpoint_to_snapshot(checkpoint);
    agentrt_checkpoint_destroy(checkpoint);

    if (!*out_snapshot) {
        AGENTRT_LOG_ERROR("C-L07: Failed to convert checkpoint to snapshot "
                          "for task '%s'", task_id);
        adapter->total_errors++;
        return -1;
    }

    adapter->total_restores++;

    AGENTRT_LOG_INFO("C-L07: Checkpoint restored for task '%s' (seq=%llu, "
                     "state_size=%zu)",
                     task_id,
                     (unsigned long long)(*out_snapshot)->sequence_num,
                     (*out_snapshot)->cognition_state_json
                         ? strlen((*out_snapshot)->cognition_state_json) : 0);

    return 0;
}

int checkpoint_adapter_restore_seq(checkpoint_adapter_t *adapter,
                                   const char *task_id,
                                   uint64_t sequence_num,
                                   checkpoint_snapshot_t **out_snapshot)
{
    if (!adapter || !task_id || !out_snapshot) return -1;

    *out_snapshot = NULL;

    if (!adapter->initialized) return -1;

    agentrt_task_checkpoint_t *checkpoint = NULL;
    agentrt_error_t ret = agentrt_checkpoint_restore(
        task_id, sequence_num, &checkpoint);

    if (ret != AGENTRT_SUCCESS || !checkpoint) {
        return -1;
    }

    *out_snapshot = checkpoint_to_snapshot(checkpoint);
    agentrt_checkpoint_destroy(checkpoint);

    if (!*out_snapshot) {
        adapter->total_errors++;
        return -1;
    }

    adapter->total_restores++;
    return 0;
}

int checkpoint_adapter_list(checkpoint_adapter_t *adapter,
                            const char *task_id,
                            checkpoint_snapshot_t ***out_snapshots,
                            size_t *out_count)
{
    if (!adapter || !task_id || !out_snapshots || !out_count) return -1;

    *out_snapshots = NULL;
    *out_count = 0;

    if (!adapter->initialized) return -1;

    agentrt_task_checkpoint_t **checkpoints = NULL;
    size_t count = 0;
    agentrt_error_t ret = agentrt_checkpoint_list(
        task_id, &checkpoints, &count);

    if (ret != AGENTRT_SUCCESS || !checkpoints || count == 0) {
        return -1;
    }

    *out_snapshots = (checkpoint_snapshot_t **)AGENTRT_CALLOC(
        count, sizeof(checkpoint_snapshot_t *));
    if (!*out_snapshots) {
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        (*out_snapshots)[i] = checkpoint_to_snapshot(checkpoints[i]);
        agentrt_checkpoint_destroy(checkpoints[i]);
    }

    AGENTRT_FREE(checkpoints);
    *out_count = count;

    return 0;
}

int checkpoint_adapter_delete(checkpoint_adapter_t *adapter,
                              const char *task_id,
                              uint64_t sequence_num)
{
    if (!adapter || !task_id) return -1;

    if (!adapter->initialized) return -1;

    agentrt_error_t ret = agentrt_checkpoint_delete(task_id, sequence_num);
    if (ret != AGENTRT_SUCCESS) {
        AGENTRT_LOG_WARN("C-L07: Failed to delete checkpoint for task '%s' "
                         "(seq=%llu): ret=%d",
                         task_id, (unsigned long long)sequence_num, ret);
        return -1;
    }

    AGENTRT_LOG_INFO("C-L07: Checkpoint deleted for task '%s' (seq=%llu)",
                     task_id, (unsigned long long)sequence_num);
    return 0;
}

/* ==================== 快照管理实现 ==================== */

int checkpoint_adapter_snapshot_create(checkpoint_adapter_t *adapter,
                                       const char *task_id,
                                       const char *snapshot_path)
{
    if (!adapter || !task_id || !snapshot_path) return -1;

    if (!adapter->initialized) return -1;

    agentrt_error_t ret = agentrt_snapshot_create(task_id, snapshot_path);
    if (ret != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("C-L07: Failed to create snapshot for task '%s' "
                          "at '%s': ret=%d",
                          task_id, snapshot_path, ret);
        adapter->total_errors++;
        return -1;
    }

    AGENTRT_LOG_INFO("C-L07: Snapshot created for task '%s' at '%s'",
                     task_id, snapshot_path);
    return 0;
}

int checkpoint_adapter_snapshot_restore(checkpoint_adapter_t *adapter,
                                        const char *snapshot_path,
                                        char **out_task_id)
{
    if (!adapter || !snapshot_path || !out_task_id) return -1;

    *out_task_id = NULL;

    if (!adapter->initialized) return -1;

    agentrt_error_t ret = agentrt_snapshot_restore(snapshot_path, out_task_id);
    if (ret != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("C-L07: Failed to restore snapshot from '%s': "
                          "ret=%d", snapshot_path, ret);
        adapter->total_errors++;
        return -1;
    }

    adapter->total_restores++;

    AGENTRT_LOG_INFO("C-L07: Snapshot restored from '%s' (task=%s)",
                     snapshot_path, *out_task_id ? *out_task_id : "?");
    return 0;
}

/* ==================== 快照内存管理 ==================== */

void checkpoint_snapshot_free(checkpoint_snapshot_t *snapshot)
{
    if (!snapshot) return;

    if (snapshot->task_id) AGENTRT_FREE(snapshot->task_id);
    if (snapshot->session_id) AGENTRT_FREE(snapshot->session_id);
    if (snapshot->cognition_state_json) AGENTRT_FREE(snapshot->cognition_state_json);
    if (snapshot->memory_context_json) AGENTRT_FREE(snapshot->memory_context_json);
    if (snapshot->tool_call_history_json) AGENTRT_FREE(snapshot->tool_call_history_json);
    if (snapshot->pending_nodes_json) AGENTRT_FREE(snapshot->pending_nodes_json);
    if (snapshot->completed_nodes_json) AGENTRT_FREE(snapshot->completed_nodes_json);
    AGENTRT_FREE(snapshot);
}

/* ==================== 状态查询 ==================== */

void checkpoint_adapter_get_stats(checkpoint_adapter_t *adapter,
                                  uint64_t *out_total_saves,
                                  uint64_t *out_total_restores,
                                  uint64_t *out_total_errors,
                                  uint64_t *out_last_save_time)
{
    if (!adapter) {
        if (out_total_saves) *out_total_saves = 0;
        if (out_total_restores) *out_total_restores = 0;
        if (out_total_errors) *out_total_errors = 0;
        if (out_last_save_time) *out_last_save_time = 0;
        return;
    }

    if (out_total_saves) *out_total_saves = adapter->total_saves;
    if (out_total_restores) *out_total_restores = adapter->total_restores;
    if (out_total_errors) *out_total_errors = adapter->total_errors;
    if (out_last_save_time) *out_last_save_time = adapter->last_save_timestamp;
}

bool checkpoint_adapter_is_ready(checkpoint_adapter_t *adapter)
{
    return adapter ? adapter->initialized : false;
}