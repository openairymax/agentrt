/**
 * @file checkpoint.c
 * @brief AgentOS 任务检查点实现（优化版）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 实现任务检查点机制，支持长时间任务的状态保存和恢复。
 *       符合 ARCHITECTURAL_PRINCIPLES.md 中的 S-1 反馈闭环原则。
 *
 * @version 2.0 (优化版)
 * - 降低圈复杂度至7以下
 * - 消除重复代码（DRY原则）
 * - 增强边界条件验证
 */

#include "../include/checkpoint.h"
#include <agentos/agentos.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

/* Unified base library compatibility layer */
#include <agentos/memory.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#endif

/* ==================== 常量定义 ==================== */

#define CHECKPOINT_DIRECTORY "checkpoints"
#define CHECKPOINT_FILE_PREFIX "checkpoint_"
#define CHECKPOINT_FILE_EXTENSION ".json"
#define MAX_CHECKPOINT_PATH 1024
#define CHECKPOINT_VERSION 1
#define MAX_LINE_LENGTH 2048
#define MAX_VALUE_LENGTH 1024

/* ==================== 内部状态 ==================== */

static char g_checkpoint_storage_path[MAX_CHECKPOINT_PATH] = {0};
static int g_checkpoint_initialized = 0;

#ifndef _WIN32
static pthread_mutex_t g_checkpoint_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static agentos_checkpoint_stats_t g_checkpoint_stats = {0};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 计算数据校验和（CRC32简化版）
 *
 * 圈复杂度: 3 (<7)
 */
static uint32_t calculate_checksum(const char* data, size_t len) {
    if (!data || len == 0) {
        return 0;
    }

    uint32_t checksum = 0;
    for (size_t i = 0; i < len && data[i] != '\0'; i++) {
        checksum = (checksum << 1) ^ (uint32_t)(unsigned char)data[i];
    }
    return checksum;
}

/**
 * @brief 状态枚举转字符串
 */
static const char* state_to_string(agentos_checkpoint_state_t state) {
    static const char* state_strings[] = {
        [CHECKPOINT_STATE_PENDING] = "pending",
        [CHECKPOINT_STATE_COMPLETED] = "completed",
        [CHECKPOINT_STATE_FAILED] = "failed",
        [CHECKPOINT_STATE_INVALID] = "invalid"
    };

    if (state >= 0 && state <= CHECKPOINT_STATE_INVALID) {
        return state_strings[state];
    }
    return "unknown";
}

/**
 * @brief 字符串转状态枚举
 *
 * 圈复杂度: 5 (<7)
 */
static agentos_checkpoint_state_t string_to_state(const char* state_str) {
    if (!state_str) {
        return CHECKPOINT_STATE_INVALID;
    }

    if (strcmp(state_str, "pending") == 0) return CHECKPOINT_STATE_PENDING;
    if (strcmp(state_str, "completed") == 0) return CHECKPOINT_STATE_COMPLETED;
    if (strcmp(state_str, "failed") == 0) return CHECKPOINT_STATE_FAILED;

    return CHECKPOINT_STATE_INVALID;
}

/**
 * @brief 安全字符串复制（带长度检查和NULL验证）
 *
 * 符合 DRY 原则，避免重复的 strdup + NULL 检查模式
 */
static char* safe_strdup(const char* src) {
    if (!src) {
        return NULL;
    }

    size_t len = strlen(src);
    char* dest = (char*)AGENTOS_MALLOC(len + 1);
    if (!dest) {
        return NULL;
    }

    memcpy(dest, src, len);
    dest[len] = '\0';
    return dest;
}

/**
 * @brief 安全字符串数组复制
 *
 * 用于 completed_nodes 和 pending_nodes 的深拷贝
 */
static char** safe_str_array_dup(char** src, size_t count) {
    if (!src || count == 0) {
        return NULL;
    }

    char** dest = (char**)AGENTOS_CALLOC(1, sizeof(char*) * count);
    if (!dest) {
        return NULL;
    }

    memset(dest, 0, sizeof(char*) * count);

    for (size_t i = 0; i < count; i++) {
        dest[i] = safe_strdup(src[i]);
        if (!dest[i] && src[i]) {
            for (size_t j = 0; j < i; j++) {
                AGENTOS_FREE(dest[j]);
            }
            AGENTOS_FREE(dest);
            return NULL;
        }
    }

    return dest;
}

/**
 * @brief 构建检查点文件路径
 *
 * 圈复杂度: 2 (<7)
 */
static int build_checkpoint_filepath(const char* task_id, char* filepath, size_t size) {
    if (!task_id || !filepath || size == 0) {
        return -1;
    }

    int written = snprintf(filepath, size, "%s/%s%s%s",
                         g_checkpoint_storage_path,
                         CHECKPOINT_FILE_PREFIX,
                         task_id,
                         CHECKPOINT_FILE_EXTENSION);

    return (written > 0 && (size_t)written < size) ? 0 : -1;
}

/**
 * @brief 初始化检查点结构体字段
 *
 * 圈复杂度: 4 (<7)
 */
static void init_checkpoint_fields(
    agentos_task_checkpoint_t* cp,
    const char* task_id,
    const char* session_id,
    uint64_t sequence_num) {

    if (!cp) {
        return;
    }

    memset(cp, 0, sizeof(*cp));

    if (task_id) {
        strncpy(cp->task_id, task_id, sizeof(cp->task_id) - 1);
        cp->task_id[sizeof(cp->task_id) - 1] = '\0';
    }

    if (session_id) {
        strncpy(cp->session_id, session_id, sizeof(cp->session_id) - 1);
        cp->session_id[sizeof(cp->session_id) - 1] = '\0';
    }

    cp->sequence_num = sequence_num;
    cp->timestamp = (uint64_t)time(NULL) * 1000000000ULL;
    cp->state = CHECKPOINT_STATE_PENDING;
}

/**
 * @brief 深拷贝节点列表到检查点
 *
 * 圈复杂度: 4 (<7)
 */
static agentos_error_t copy_node_lists(
    agentos_task_checkpoint_t* cp,
    char** completed_nodes,
    size_t completed_count,
    char** pending_nodes,
    size_t pending_count) {

    if (!cp) {
        return AGENTOS_EINVAL;
    }

    cp->completed_count = completed_count;
    if (completed_count > 0) {
        cp->completed_nodes = safe_str_array_dup(completed_nodes, completed_count);
        if (!cp->completed_nodes) {
            return AGENTOS_ENOMEM;
        }
    }

    cp->pending_count = pending_count;
    if (pending_count > 0) {
        cp->pending_nodes = safe_str_array_dup(pending_nodes, pending_count);
        if (!cp->pending_nodes) {
            if (cp->completed_nodes) {
                for (size_t i = 0; i < completed_count; i++) {
                    AGENTOS_FREE(cp->completed_nodes[i]);
                }
                AGENTOS_FREE(cp->completed_nodes);
            }
            return AGENTOS_ENOMEM;
        }
    }

    return AGENTOS_SUCCESS;
}

/* ==================== 公共 API 实现 ==================== */

agentos_error_t agentos_checkpoint_init(const char* storage_path) {
    if (g_checkpoint_initialized) {
        return AGENTOS_SUCCESS;
    }

    const char* path = storage_path ? storage_path : "./" CHECKPOINT_DIRECTORY;
    size_t len = strlen(path);

    if (len == 0 || len >= sizeof(g_checkpoint_storage_path)) {
        return AGENTOS_EINVAL;
    }

    memcpy(g_checkpoint_storage_path, path, len);
    g_checkpoint_storage_path[len] = '\0';

#ifndef _WIN32
    pthread_mutex_init(&g_checkpoint_mutex, NULL);
#endif

    memset(&g_checkpoint_stats, 0, sizeof(g_checkpoint_stats));
    g_checkpoint_initialized = 1;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_shutdown(void) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_SUCCESS;
    }

#ifndef _WIN32
    pthread_mutex_destroy(&g_checkpoint_mutex);
#endif

    g_checkpoint_initialized = 0;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_create(
    const char* task_id,
    const char* session_id,
    uint64_t sequence_num,
    const char* state_json,
    char** completed_nodes,
    size_t completed_count,
    char** pending_nodes,
    size_t pending_count,
    agentos_task_checkpoint_t** out_checkpoint) {

    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!task_id || !state_json || !out_checkpoint) {
        return AGENTOS_EINVAL;
    }

    agentos_task_checkpoint_t* cp = (agentos_task_checkpoint_t*)
        AGENTOS_CALLOC(1, sizeof(agentos_task_checkpoint_t));
    if (!cp) {
        return AGENTOS_ENOMEM;
    }

    init_checkpoint_fields(cp, task_id, session_id, sequence_num);

    cp->state_json = safe_strdup(state_json);
    if (!cp->state_json) {
        AGENTOS_FREE(cp);
        return AGENTOS_ENOMEM;
    }
    cp->state_size = strlen(state_json);

    agentos_error_t err = copy_node_lists(
        cp,
        completed_nodes, completed_count,
        pending_nodes, pending_count
    );

    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(cp->state_json);
        AGENTOS_FREE(cp);
        return err;
    }

    cp->checksum = calculate_checksum(state_json, strlen(state_json));
    *out_checkpoint = cp;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_save(const agentos_task_checkpoint_t* cp) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!cp || !cp->task_id[0]) {
        return AGENTOS_EINVAL;
    }

    char filepath[MAX_CHECKPOINT_PATH];
    if (build_checkpoint_filepath(cp->task_id, filepath, sizeof(filepath)) != 0) {
        return AGENTOS_EINVAL;
    }

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        g_checkpoint_stats.failed_checkpoints++;
        return AGENTOS_EIO;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"version\": %d,\n", CHECKPOINT_VERSION);
    fprintf(fp, "  \"task_id\": \"%s\",\n", cp->task_id);
    fprintf(fp, "  \"session_id\": \"%s\",\n", cp->session_id);
    fprintf(fp, "  \"sequence_num\": %lu,\n", (unsigned long)cp->sequence_num);
    fprintf(fp, "  \"timestamp\": %lu,\n", (unsigned long)cp->timestamp);
    fprintf(fp, "  \"state\": \"%s\",\n", state_to_string(cp->state));
    fprintf(fp, "  \"checksum\": %u,\n", cp->checksum);
    fprintf(fp, "  \"state_size\": %zu,\n", cp->state_size);
    fprintf(fp, "  \"state_json\": %s,\n", cp->state_json ? cp->state_json : "");
    fprintf(fp, "  \"completed_count\": %zu,\n", cp->completed_count);
    fprintf(fp, "  \"pending_count\": %zu,\n", cp->pending_count);
    fprintf(fp, "  \"metadata\": \"%s\"\n", cp->metadata);
    fprintf(fp, "}\n");

    fclose(fp);

    cp->state = CHECKPOINT_STATE_COMPLETED;
    g_checkpoint_stats.successful_checkpoints++;
    g_checkpoint_stats.total_checkpoints++;
    g_checkpoint_stats.last_checkpoint_time = cp->timestamp;

    if (g_checkpoint_stats.total_checkpoints > 0) {
        g_checkpoint_stats.avg_checkpoint_size =
            (g_checkpoint_stats.avg_checkpoint_size * (g_checkpoint_stats.total_checkpoints - 1) +
             cp->state_size) / g_checkpoint_stats.total_checkpoints;
    } else {
        g_checkpoint_stats.avg_checkpoint_size = cp->state_size;
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_restore(
    const char* task_id,
    uint64_t sequence_num,
    agentos_task_checkpoint_t** out_cp) {

    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!task_id || !out_cp) {
        return AGENTOS_EINVAL;
    }

    (void)sequence_num;

    char filepath[MAX_CHECKPOINT_PATH];
    if (build_checkpoint_filepath(task_id, filepath, sizeof(filepath)) != 0) {
        return AGENTOS_EINVAL;
    }

    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        return AGENTOS_ENOENT;
    }

    agentos_task_checkpoint_t* cp = (agentos_task_checkpoint_t*)
        AGENTOS_CALLOC(1, sizeof(agentos_task_checkpoint_t));
    if (!cp) {
        fclose(fp);
        return AGENTOS_ENOMEM;
    }

    char line[MAX_LINE_LENGTH];
    char state_str[64] = {0};

    while (fgets(line, sizeof(line), fp)) {
        char key[128], value[MAX_VALUE_LENGTH];

        if (sscanf(line, "  \"%127[^\"]\": \"%1023[^\"]\"", key, value) == 2) {
            if (strcmp(key, "task_id") == 0) {
                strncpy(cp->task_id, value, sizeof(cp->task_id) - 1);
                cp->task_id[sizeof(cp->task_id) - 1] = '\0';
            } else if (strcmp(key, "session_id") == 0) {
                strncpy(cp->session_id, value, sizeof(cp->session_id) - 1);
                cp->session_id[sizeof(cp->session_id) - 1] = '\0';
            } else if (strcmp(key, "state") == 0) {
                strncpy(state_str, value, sizeof(state_str) - 1);
                state_str[sizeof(state_str) - 1] = '\0';
            } else if (strcmp(key, "state_json") == 0) {
                cp->state_json = safe_strdup(value);
                if (cp->state_json) {
                    cp->state_size = strlen(value);
                }
            }
        }
    }

    fclose(fp);

    cp->state = string_to_state(state_str);
    g_checkpoint_stats.total_restore_ops++;
    *out_cp = cp;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_delete(const char* task_id, uint64_t seq_num) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!task_id) {
        return AGENTOS_EINVAL;
    }

    (void)seq_num;

    char filepath[MAX_CHECKPOINT_PATH];
    if (build_checkpoint_filepath(task_id, filepath, sizeof(filepath)) != 0) {
        return AGENTOS_EINVAL;
    }

    if (unlink(filepath) == 0) {
        return AGENTOS_SUCCESS;
    }

    return AGENTOS_ENOENT;
}

agentos_error_t agentos_checkpoint_list(
    const char* task_id,
    agentos_task_checkpoint_t*** out_cps,
    size_t* out_count) {

    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!task_id || !out_cps || !out_count) {
        return AGENTOS_EINVAL;
    }

    *out_cps = NULL;
    *out_count = 0;

    agentos_task_checkpoint_t* cp = NULL;
    agentos_error_t err = agentos_checkpoint_restore(task_id, 0, &cp);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    *out_cps = (agentos_task_checkpoint_t**)AGENTOS_MALLOC(sizeof(agentos_task_checkpoint_t*));
    if (!*out_cps) {
        agentos_checkpoint_destroy(cp);
        return AGENTOS_ENOMEM;
    }

    (*out_cps)[0] = cp;
    *out_count = 1;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_get_stats(agentos_checkpoint_stats_t* stats) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!stats) {
        return AGENTOS_EINVAL;
    }

    memcpy(stats, &g_checkpoint_stats, sizeof(agentos_checkpoint_stats_t));
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_verify(
    const agentos_task_checkpoint_t* cp,
    bool* is_valid) {

    if (!cp || !is_valid) {
        return AGENTOS_EINVAL;
    }

    *is_valid = false;

    if (cp->state == CHECKPOINT_STATE_INVALID) {
        return AGENTOS_SUCCESS;
    }

    if (!cp->state_json || cp->state_size == 0) {
        return AGENTOS_SUCCESS;
    }

    uint32_t calc = calculate_checksum(cp->state_json, cp->state_size);
    *is_valid = (calc == cp->checksum);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_destroy(agentos_task_checkpoint_t* cp) {
    if (!cp) {
        return AGENTOS_SUCCESS;
    }

    if (cp->state_json) {
        AGENTOS_FREE(cp->state_json);
        cp->state_json = NULL;
    }

    if (cp->completed_nodes) {
        for (size_t i = 0; i < cp->completed_count; i++) {
            AGENTOS_FREE(cp->completed_nodes[i]);
        }
        AGENTOS_FREE(cp->completed_nodes);
        cp->completed_nodes = NULL;
    }

    if (cp->pending_nodes) {
        for (size_t i = 0; i < cp->pending_count; i++) {
            AGENTOS_FREE(cp->pending_nodes[i]);
        }
        AGENTOS_FREE(cp->pending_nodes);
        cp->pending_nodes = NULL;
    }

    AGENTOS_FREE(cp);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_cleanup(uint64_t max_age_sec, size_t max_cnt) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    pthread_mutex_lock(&g_checkpoint_mutex);
    uint64_t now = (uint64_t)time(NULL);

    if (max_age_sec > 0) {
        char pattern[MAX_CHECKPOINT_PATH];
        snprintf(pattern, sizeof(pattern), "%s/*.chk", g_checkpoint_storage_path);

#ifdef _WIN32
        WIN32_FIND_DATAA find_data;
        HANDLE hFind = FindFirstFileA(pattern, &find_data);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                char filepath[MAX_CHECKPOINT_PATH];
                snprintf(filepath, sizeof(filepath), "%s/%s",
                         g_checkpoint_storage_path, find_data.cFileName);

                ULARGE_INTEGER file_time;
                file_time.LowPart = find_data.ftLastWriteTime.dwLowDateTime;
                file_time.HighPart = find_data.ftLastWriteTime.dwHighDateTime;
                uint64_t mod_time = (file_time.QuadPart / 10000000ULL) - 11644473600ULL;

                if ((now - mod_time) > max_age_sec) {
                    DeleteFileA(filepath);
                    g_checkpoint_stats.total_checkpoints =
                        (g_checkpoint_stats.total_checkpoints > 0)
                            ? g_checkpoint_stats.total_checkpoints - 1 : 0;
                }
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#else
        DIR* dir = opendir(g_checkpoint_storage_path);
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                size_t len = strlen(entry->d_name);
                if (len < 4 || strcmp(entry->d_name + len - 4, ".chk") != 0)
                    continue;

                char filepath[MAX_CHECKPOINT_PATH];
                snprintf(filepath, sizeof(filepath), "%s/%s",
                         g_checkpoint_storage_path, entry->d_name);

                struct stat st;
                if (stat(filepath, &st) == 0) {
                    uint64_t mod_time = (uint64_t)st.st_mtime;
                    if ((now - mod_time) > max_age_sec) {
                        remove(filepath);
                        g_checkpoint_stats.total_checkpoints =
                            (g_checkpoint_stats.total_checkpoints > 0)
                                ? g_checkpoint_stats.total_checkpoints - 1 : 0;
                    }
                }
            }
            closedir(dir);
        }
#endif
    }

    if (max_cnt > 0 && g_checkpoint_stats.total_checkpoints > max_cnt) {
        g_checkpoint_stats.total_checkpoints = max_cnt;
    }

    pthread_mutex_unlock(&g_checkpoint_mutex);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_snapshot_create(const char* task_id, const char* snap_path) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!task_id || !snap_path) {
        return AGENTOS_EINVAL;
    }

    agentos_task_checkpoint_t* cp = NULL;
    agentos_error_t err = agentos_checkpoint_restore(task_id, 0, &cp);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    FILE* fp = fopen(snap_path, "wb");
    if (!fp) {
        agentos_checkpoint_destroy(cp);
        return AGENTOS_EIO;
    }

    fprintf(fp, "SNAPSHOT_V1\n");
    fprintf(fp, "TaskID: %s\n", cp->task_id);
    fprintf(fp, "SessionID: %s\n", cp->session_id);
    fprintf(fp, "SequenceNum: %lu\n", (unsigned long)cp->sequence_num);
    fprintf(fp, "Timestamp: %lu\n", (unsigned long)cp->timestamp);
    fprintf(fp, "StateSize: %zu\n", cp->state_size);
    fprintf(fp, "---DATA---\n");

    if (cp->state_json && cp->state_size > 0) {
        fwrite(cp->state_json, 1, cp->state_size, fp);
    }

    fprintf(fp, "\n---END---\n");

    fclose(fp);
    agentos_checkpoint_destroy(cp);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_snapshot_restore(const char* snap_path, char** tid) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!snap_path || !tid) {
        return AGENTOS_EINVAL;
    }

    FILE* fp = fopen(snap_path, "rb");
    if (!fp) {
        return AGENTOS_ENOENT;
    }

    char header[64];
    if (!fgets(header, sizeof(header), fp) || strncmp(header, "SNAPSHOT_V1", 11) != 0) {
        fclose(fp);
        return AGENTOS_EIO;
    }

    char line[256];
    *tid = NULL;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "TaskID: ", 8) == 0) {
            *tid = safe_strdup(line + 8);
            if (*tid) {
                size_t len = strlen(*tid);
                if (len > 0 && (*tid)[len - 1] == '\n') {
                    (*tid)[len - 1] = '\0';
                }
            }
        } else if (strncmp(line, "---DATA---", 10) == 0) {
            break;
        }
    }

    fclose(fp);

    if (!*tid) {
        return AGENTOS_EIO;
    }

    return AGENTOS_SUCCESS;
}
