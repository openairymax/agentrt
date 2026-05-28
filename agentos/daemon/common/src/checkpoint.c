/**
 * @file checkpoint.c
 * @brief AgentOS 任务检查点实现（生产级 v3.0）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * v3.0 变更：
 * - CROSS-01: agentos_mutex_t → agentos_mutex_t
 * - CROSS-03: time(NULL) → agentos_time_ns()
 * - 新增 auto-checkpoint hook 机制（CoreLoopThree 集成）
 * - 增强 JSON restore 解析健壮性
 */

#include "../include/checkpoint.h"

#include "../include/svc_logger.h"
#include "agentos.h"
#include "daemon_errors.h"
#include "platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include "agentos_dirent.h"

#include <sys/stat.h>
#endif

#include "atomic_compat.h"
#include "memory_compat.h"

#define CHECKPOINT_DIRECTORY "checkpoints"
#define CHECKPOINT_FILE_PREFIX "checkpoint_"
#define CHECKPOINT_FILE_EXTENSION ".json"
#define MAX_CHECKPOINT_PATH 1024
#define CHECKPOINT_VERSION 1
#define MAX_LINE_LENGTH 8192
#define MAX_VALUE_LENGTH 4096

static char g_checkpoint_storage_path[MAX_CHECKPOINT_PATH] = {0};
static atomic_int g_checkpoint_initialized = 0;
static agentos_mutex_t g_checkpoint_mutex;
static atomic_int g_checkpoint_mutex_initialized = 0;
static agentos_checkpoint_stats_t g_checkpoint_stats = {0};

static agentos_checkpoint_hook_fn g_auto_hook = NULL;
static void *g_auto_hook_user_data = NULL;
static uint64_t g_auto_interval_ms = 0;

static uint32_t calculate_checksum(const char *data, size_t len)
{
    if (!data || len == 0)
        return 0;
    uint32_t checksum = 0;
    for (size_t i = 0; i < len && data[i] != '\0'; i++) {
        checksum = (checksum << 1) ^ (uint32_t)(unsigned char)data[i];
    }
    return checksum;
}

static const char *state_to_string(agentos_checkpoint_state_t state)
{
    static const char *state_strings[] = {[CHECKPOINT_STATE_PENDING] = "pending",
                                          [CHECKPOINT_STATE_COMPLETED] = "completed",
                                          [CHECKPOINT_STATE_FAILED] = "failed",
                                          [CHECKPOINT_STATE_INVALID] = "invalid"};
    if (state >= 0 && state <= CHECKPOINT_STATE_INVALID)
        return state_strings[state];
    return "unknown";
}

static agentos_checkpoint_state_t string_to_state(const char *s)
{
    if (!s)
        return CHECKPOINT_STATE_INVALID;
    if (strcmp(s, "pending") == 0)
        return CHECKPOINT_STATE_PENDING;
    if (strcmp(s, "completed") == 0)
        return CHECKPOINT_STATE_COMPLETED;
    if (strcmp(s, "failed") == 0)
        return CHECKPOINT_STATE_FAILED;
    return CHECKPOINT_STATE_INVALID;
}

static char *safe_strdup(const char *src)
{
    if (!src)
        return NULL;
    size_t len = strlen(src);
    char *d = (char *)AGENTOS_MALLOC(len + 1);
    if (d) {
        memcpy(d, src, len);
        d[len] = '\0';
    }
    return d;
}

static char **safe_str_array_dup(char **src, size_t count)
{
    if (!src || count == 0)
        return NULL;
    char **dst = (char **)AGENTOS_CALLOC(count, sizeof(char *));
    if (!dst)
        return NULL;
    memset(dst, 0, sizeof(char *) * count);
    for (size_t i = 0; i < count; i++) {
        dst[i] = safe_strdup(src[i]);
        if (!dst[i] && src[i]) {
            for (size_t j = 0; j < i; j++)
                AGENTOS_FREE(dst[j]);
            AGENTOS_FREE(dst);
            return NULL;
        }
    }
    return dst;
}

static int build_filepath(const char *task_id, char *buf, size_t size)
{
    if (!task_id || !buf || size == 0)
        return AGENTOS_ERR_INVALID_PARAM;
    int n = snprintf(buf, size, "%s/%s%s%s", g_checkpoint_storage_path, CHECKPOINT_FILE_PREFIX,
                     task_id, CHECKPOINT_FILE_EXTENSION);
    return (n > 0 && (size_t)n < size) ? 0 : AGENTOS_ERR_OVERFLOW;
}

static void init_fields(agentos_task_checkpoint_t *cp, const char *task_id, const char *session_id,
                        uint64_t seq)
{
    if (!cp)
        return;
    memset(cp, 0, sizeof(*cp));
    if (task_id) {
        strncpy(cp->task_id, task_id, sizeof(cp->task_id) - 1);
        cp->task_id[sizeof(cp->task_id) - 1] = '\0';
    }
    if (session_id) {
        strncpy(cp->session_id, session_id, sizeof(cp->session_id) - 1);
        cp->session_id[sizeof(cp->session_id) - 1] = '\0';
    }
    cp->sequence_num = seq;
    cp->timestamp = agentos_time_ns();
    cp->state = CHECKPOINT_STATE_PENDING;
}

static agentos_error_t copy_nodes(agentos_task_checkpoint_t *cp, char **completed, size_t ccount,
                                  char **pending, size_t pcount)
{
    if (!cp)
        return AGENTOS_EINVAL;
    cp->completed_count = ccount;
    if (ccount > 0) {
        cp->completed_nodes = safe_str_array_dup(completed, ccount);
        if (!cp->completed_nodes)
            return AGENTOS_ENOMEM;
    }
    cp->pending_count = pcount;
    if (pcount > 0) {
        cp->pending_nodes = safe_str_array_dup(pending, pcount);
        if (!cp->pending_nodes) {
            if (cp->completed_nodes) {
                for (size_t i = 0; i < ccount; i++)
                    AGENTOS_FREE(cp->completed_nodes[i]);
                AGENTOS_FREE(cp->completed_nodes);
            }
            return AGENTOS_ENOMEM;
        }
    }
    return AGENTOS_SUCCESS;
}

static char *json_extract_string(const char *json, const char *key)
{
    if (!json || !key)
        return NULL;
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p)
        return NULL;
    p += strlen(search);
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p != ':')
        return NULL;
    p++;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p != '"')
        return NULL;
    p++;

    size_t cap = 512;
    char *val = (char *)AGENTOS_MALLOC(cap);
    if (!val)
        return NULL;
    size_t len = 0;

    while (*p && *p != '"' && *p != '\n') {
        if (*p == '\\' && *(p + 1)) {
            p++;
            char esc = '\\';
            switch (*p) {
            case 'n':
                esc = '\n';
                break;
            case 't':
                esc = '\t';
                break;
            case 'r':
                esc = '\r';
                break;
            case '"':
                esc = '"';
                break;
            default:
                break;
            }
            if (len + 2 >= cap) {
                cap *= 2;
                val = (char *)AGENTOS_REALLOC(val, cap);
                if (!val)
                    return NULL;
            }
            val[len++] = esc;
            p++;
        } else {
            if (len + 2 >= cap) {
                cap *= 2;
                val = (char *)AGENTOS_REALLOC(val, cap);
                if (!val)
                    return NULL;
            }
            val[len++] = *p;
            p++;
        }
    }
    val[len] = '\0';
    return val;
}

static uint64_t json_extract_uint64(const char *json, const char *key)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p)
        return 0;
    p += strlen(search);
    while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n' || *p == '\r'))
        p++;
    uint64_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (uint64_t)(*p - '0');
        p++;
    }
    return v;
}

/* ==================== Public API ==================== */

agentos_error_t agentos_checkpoint_init(const char *storage_path)
{
    if (g_checkpoint_initialized)
        return AGENTOS_SUCCESS;
    const char *path = storage_path ? storage_path : "./" CHECKPOINT_DIRECTORY;
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(g_checkpoint_storage_path))
        return AGENTOS_EINVAL;
    memcpy(g_checkpoint_storage_path, path, len);
    g_checkpoint_storage_path[len] = '\0';

    {
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(&g_checkpoint_mutex_initialized, &expected, 1,
                                                    memory_order_seq_cst, memory_order_seq_cst)) {
            if (agentos_mutex_init(&g_checkpoint_mutex) != 0) {
                atomic_store_explicit(&g_checkpoint_mutex_initialized, 0, memory_order_seq_cst);
                return DAEMON_EINIT;
            }
        }
    }

    memset(&g_checkpoint_stats, 0, sizeof(g_checkpoint_stats));
    atomic_store_explicit(&g_checkpoint_initialized, 1, memory_order_seq_cst);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_shutdown(void)
{
    if (!atomic_load_explicit(&g_checkpoint_initialized, memory_order_acquire))
        return AGENTOS_SUCCESS;
    if (atomic_load_explicit(&g_checkpoint_mutex_initialized, memory_order_acquire)) {
        agentos_mutex_destroy(&g_checkpoint_mutex);
        atomic_store_explicit(&g_checkpoint_mutex_initialized, 0, memory_order_seq_cst);
    }
    g_auto_hook = NULL;
    g_auto_hook_user_data = NULL;
    atomic_store_explicit(&g_checkpoint_initialized, 0, memory_order_seq_cst);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_create(const char *task_id, const char *session_id,
                                          uint64_t sequence_num, const char *state_json,
                                          char **completed_nodes, size_t completed_count,
                                          char **pending_nodes, size_t pending_count,
                                          agentos_task_checkpoint_t **out_cp)
{

    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    if (!task_id || !state_json || !out_cp)
        return AGENTOS_EINVAL;

    agentos_task_checkpoint_t *cp =
        (agentos_task_checkpoint_t *)AGENTOS_CALLOC(1, sizeof(agentos_task_checkpoint_t));
    if (!cp)
        return AGENTOS_ENOMEM;

    init_fields(cp, task_id, session_id, sequence_num);

    cp->state_json = safe_strdup(state_json);
    if (!cp->state_json) {
        AGENTOS_FREE(cp);
        return AGENTOS_ENOMEM;
    }
    cp->state_size = strlen(state_json);

    agentos_error_t err =
        copy_nodes(cp, completed_nodes, completed_count, pending_nodes, pending_count);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(cp->state_json);
        AGENTOS_FREE(cp);
        return err;
    }

    cp->checksum = calculate_checksum(state_json, strlen(state_json));
    *out_cp = cp;
    return AGENTOS_SUCCESS;
}

static void write_json_escaped_str(FILE *fp, const char *str)
{
    if (!str)
        return;
    for (const char *p = str; *p; p++) {
        switch (*p) {
        case '"':
            fputs("\\\"", fp);
            break;
        case '\\':
            fputs("\\\\", fp);
            break;
        case '\n':
            fputs("\\n", fp);
            break;
        case '\r':
            fputs("\\r", fp);
            break;
        case '\t':
            fputs("\\t", fp);
            break;
        default:
            fputc(*p, fp);
            break;
        }
    }
}

static void fprintf_sanitized(FILE *fp, const char *label, const char *str)
{
    char _cp_buf[2048];
    snprintf(_cp_buf, sizeof(_cp_buf), "%s: ", label);
    fputs(_cp_buf, fp);
    if (!str) {
        fputc('\n', fp);
        return;
    }
    for (const char *p = str; *p; p++) {
        fputc((*p == '\n' || *p == '\r') ? ' ' : *p, fp);
    }
    fputc('\n', fp);
}

agentos_error_t agentos_checkpoint_save(agentos_task_checkpoint_t *cp)
{
    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    if (!cp || !cp->task_id[0])
        return AGENTOS_EINVAL;

    char filepath[MAX_CHECKPOINT_PATH];
    if (build_filepath(cp->task_id, filepath, sizeof(filepath)) != 0)
        return AGENTOS_EINVAL;

    char tmppath[MAX_CHECKPOINT_PATH];
    snprintf(tmppath, sizeof(tmppath), "%s.tmp", filepath);

    FILE *fp = fopen(tmppath, "w");
    if (!fp) {
        agentos_mutex_lock(&g_checkpoint_mutex);
        g_checkpoint_stats.failed_checkpoints++;
        agentos_mutex_unlock(&g_checkpoint_mutex);
        return AGENTOS_EIO;
    }

    char _cp_buf[2048];
    fputs("{\n", fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"version\": %d,\n", CHECKPOINT_VERSION);
    fputs(_cp_buf, fp);
    fputs("  \"task_id\": \"", fp);
    write_json_escaped_str(fp, cp->task_id);
    fputs("\",\n", fp);
    fputs("  \"session_id\": \"", fp);
    write_json_escaped_str(fp, cp->session_id);
    fputs("\",\n", fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"sequence_num\": %lu,\n",
             (unsigned long)cp->sequence_num);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"timestamp\": %lu,\n", (unsigned long)cp->timestamp);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"state\": \"%s\",\n", state_to_string(cp->state));
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"checksum\": %u,\n", cp->checksum);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"state_size\": %zu,\n", cp->state_size);
    fputs(_cp_buf, fp);

    fputs("  \"state_json\": ", fp);
    if (cp->state_json) {
        fputc('"', fp);
        for (const char *p = cp->state_json; *p; p++) {
            switch (*p) {
            case '"':
                fputs("\\\"", fp);
                break;
            case '\\':
                fputs("\\\\", fp);
                break;
            case '\n':
                fputs("\\n", fp);
                break;
            case '\r':
                fputs("\\r", fp);
                break;
            case '\t':
                fputs("\\t", fp);
                break;
            default:
                fputc(*p, fp);
                break;
            }
        }
        fputc('"', fp);
    } else {
        fputs("null", fp);
    }
    fputs(",\n", fp);

    snprintf(_cp_buf, sizeof(_cp_buf), "  \"completed_count\": %zu,\n", cp->completed_count);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "  \"pending_count\": %zu,\n", cp->pending_count);
    fputs(_cp_buf, fp);
    fputs("  \"metadata\": \"", fp);
    write_json_escaped_str(fp, cp->metadata);
    fputs("\"\n", fp);
    fputs("}\n", fp);
    fclose(fp);

    if (rename(tmppath, filepath) != 0) {
        unlink(tmppath);
        agentos_mutex_lock(&g_checkpoint_mutex);
        g_checkpoint_stats.failed_checkpoints++;
        agentos_mutex_unlock(&g_checkpoint_mutex);
        return AGENTOS_EIO;
    }

    agentos_mutex_lock(&g_checkpoint_mutex);
    cp->state = CHECKPOINT_STATE_COMPLETED;
    g_checkpoint_stats.successful_checkpoints++;
    g_checkpoint_stats.total_checkpoints++;
    g_checkpoint_stats.last_checkpoint_time = cp->timestamp;

    if (g_checkpoint_stats.total_checkpoints > 0) {
        g_checkpoint_stats.avg_checkpoint_size =
            (g_checkpoint_stats.avg_checkpoint_size * (g_checkpoint_stats.total_checkpoints - 1) +
             cp->state_size) /
            g_checkpoint_stats.total_checkpoints;
    } else {
        g_checkpoint_stats.avg_checkpoint_size = cp->state_size;
    }
    agentos_mutex_unlock(&g_checkpoint_mutex);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_restore(const char *task_id, uint64_t sequence_num,
                                           agentos_task_checkpoint_t **out_cp)
{

    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    if (!task_id || !out_cp)
        return AGENTOS_EINVAL;

    char filepath[MAX_CHECKPOINT_PATH];
    if (build_filepath(task_id, filepath, sizeof(filepath)) != 0)
        return AGENTOS_EINVAL;

    FILE *fp = fopen(filepath, "r");
    if (!fp)
        return AGENTOS_ENOENT;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {
        fclose(fp);
        return AGENTOS_EIO;
    }

    char *json_buf = (char *)AGENTOS_MALLOC((size_t)(file_size + 1));
    if (!json_buf) {
        fclose(fp);
        return AGENTOS_ENOMEM;
    }

    size_t read_len = fread(json_buf, 1, (size_t)file_size, fp);
    if (read_len != (size_t)file_size) {
        AGENTOS_FREE(json_buf);
        fclose(fp);
        return AGENTOS_EIO;
    }
    json_buf[read_len] = '\0';
    fclose(fp);

    agentos_task_checkpoint_t *cp =
        (agentos_task_checkpoint_t *)AGENTOS_CALLOC(1, sizeof(agentos_task_checkpoint_t));
    if (!cp) {
        AGENTOS_FREE(json_buf);
        return AGENTOS_ENOMEM;
    }

    char *state_str = json_extract_string(json_buf, "state");
    char *sj = json_extract_string(json_buf, "state_json");

    init_fields(cp, task_id, "", 0);

    if (state_str) {
        cp->state = string_to_state(state_str);
        AGENTOS_FREE(state_str);
    }
    if (sj) {
        cp->state_json = sj;
        cp->state_size = strlen(sj);
        cp->checksum = calculate_checksum(sj, strlen(sj));
    }

    char *sid = json_extract_string(json_buf, "session_id");
    if (sid) {
        strncpy(cp->session_id, sid, sizeof(cp->session_id) - 1);
        cp->session_id[sizeof(cp->session_id) - 1] = '\0';
        AGENTOS_FREE(sid);
    }
    cp->sequence_num = json_extract_uint64(json_buf, "sequence_num");
    cp->timestamp = json_extract_uint64(json_buf, "timestamp");

    AGENTOS_FREE(json_buf);
    agentos_mutex_lock(&g_checkpoint_mutex);
    g_checkpoint_stats.total_restore_ops++;
    agentos_mutex_unlock(&g_checkpoint_mutex);
    *out_cp = cp;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_delete(const char *task_id, uint64_t seq_num)
{
    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    if (!task_id)
        return AGENTOS_EINVAL;

    char filepath[MAX_CHECKPOINT_PATH];
    if (build_filepath(task_id, filepath, sizeof(filepath)) != 0)
        return AGENTOS_EINVAL;

    agentos_mutex_lock(&g_checkpoint_mutex);
    int result = unlink(filepath);
    if (result == 0) {
        g_checkpoint_stats.total_checkpoints--;
    }
    agentos_mutex_unlock(&g_checkpoint_mutex);

    if (result == 0)
        return AGENTOS_SUCCESS;
    return AGENTOS_ENOENT;
}

agentos_error_t agentos_checkpoint_list(const char *task_id, agentos_task_checkpoint_t ***out_cps,
                                        size_t *out_count)
{

    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    if (!task_id || !out_cps || !out_count)
        return AGENTOS_EINVAL;

    *out_cps = NULL;
    *out_count = 0;

    agentos_task_checkpoint_t *cp = NULL;
    agentos_error_t err = agentos_checkpoint_restore(task_id, 0, &cp);
    if (err != AGENTOS_SUCCESS)
        return err;

    *out_cps = (agentos_task_checkpoint_t **)AGENTOS_MALLOC(sizeof(agentos_task_checkpoint_t *));
    if (!*out_cps) {
        agentos_checkpoint_destroy(cp);
        return AGENTOS_ENOMEM;
    }
    (*out_cps)[0] = cp;
    *out_count = 1;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_get_stats(agentos_checkpoint_stats_t *stats)
{
    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    if (!stats)
        return AGENTOS_EINVAL;
    agentos_mutex_lock(&g_checkpoint_mutex);
    memcpy(stats, &g_checkpoint_stats, sizeof(agentos_checkpoint_stats_t));
    agentos_mutex_unlock(&g_checkpoint_mutex);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_verify(const agentos_task_checkpoint_t *cp, bool *is_valid)
{

    if (!cp || !is_valid)
        return AGENTOS_EINVAL;
    *is_valid = false;
    if (cp->state == CHECKPOINT_STATE_INVALID)
        return AGENTOS_SUCCESS;
    if (!cp->state_json || cp->state_size == 0)
        return AGENTOS_SUCCESS;
    uint32_t calc = calculate_checksum(cp->state_json, cp->state_size);
    *is_valid = (calc == cp->checksum);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_destroy(agentos_task_checkpoint_t *cp)
{
    if (!cp)
        return AGENTOS_SUCCESS;
    if (cp->state_json) {
        AGENTOS_FREE(cp->state_json);
        cp->state_json = NULL;
    }
    if (cp->completed_nodes) {
        for (size_t i = 0; i < cp->completed_count; i++)
            AGENTOS_FREE(cp->completed_nodes[i]);
        AGENTOS_FREE(cp->completed_nodes);
        cp->completed_nodes = NULL;
    }
    if (cp->pending_nodes) {
        for (size_t i = 0; i < cp->pending_count; i++)
            AGENTOS_FREE(cp->pending_nodes[i]);
        AGENTOS_FREE(cp->pending_nodes);
        cp->pending_nodes = NULL;
    }
    AGENTOS_FREE(cp);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_cleanup(uint64_t max_age_sec, size_t max_cnt)
{
    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;

    agentos_mutex_lock(&g_checkpoint_mutex);
    uint64_t now_sec = agentos_time_ms() / 1000ULL;

    if (max_age_sec > 0) {
        char pattern[MAX_CHECKPOINT_PATH];
        snprintf(pattern, sizeof(pattern), "%s/*.json", g_checkpoint_storage_path);

#ifdef _WIN32
        WIN32_FIND_DATAA find_data;
        HANDLE hFind = FindFirstFileA(pattern, &find_data);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                char filepath[MAX_CHECKPOINT_PATH];
                snprintf(filepath, sizeof(filepath), "%s/%s", g_checkpoint_storage_path,
                         find_data.cFileName);
                ULARGE_INTEGER ft;
                ft.LowPart = find_data.ftLastWriteTime.dwLowDateTime;
                ft.HighPart = find_data.ftLastWriteTime.dwHighDateTime;
                uint64_t mod_sec = (ft.QuadPart / 10000000ULL) - 11644473600ULL;
                if ((now_sec - mod_sec) > max_age_sec)
                    DeleteFileA(filepath);
            } while (FindNextFile(hFind, &find_data));
            FindClose(hFind);
        }
#else
        DIR *dir = opendir(g_checkpoint_storage_path);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                size_t nlen = strlen(entry->d_name);
                if (nlen < 5 || strcmp(entry->d_name + nlen - 5, ".json") != 0)
                    continue;
                char filepath[MAX_CHECKPOINT_PATH];
                snprintf(filepath, sizeof(filepath), "%s/%s", g_checkpoint_storage_path,
                         entry->d_name);
                struct stat st;
                if (stat(filepath, &st) == 0) {
                    if ((now_sec - (uint64_t)st.st_mtime) > max_age_sec)
                        remove(filepath);
                }
            }
            closedir(dir);
        }
#endif
    }

    if (max_cnt > 0 && g_checkpoint_stats.total_checkpoints > max_cnt)
        g_checkpoint_stats.total_checkpoints = max_cnt;

    agentos_mutex_unlock(&g_checkpoint_mutex);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_snapshot_create(const char *task_id, const char *snap_path)
{
    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    if (!task_id || !snap_path)
        return AGENTOS_EINVAL;

    agentos_task_checkpoint_t *cp = NULL;
    agentos_error_t err = agentos_checkpoint_restore(task_id, 0, &cp);
    if (err != AGENTOS_SUCCESS)
        return err;

    FILE *fp = fopen(snap_path, "wb");
    if (!fp) {
        agentos_checkpoint_destroy(cp);
        return AGENTOS_EIO;
    }

    char _cp_buf[2048];
    fputs("SNAPSHOT_V1\n", fp);
    fprintf_sanitized(fp, "TaskID", cp->task_id);
    fprintf_sanitized(fp, "SessionID", cp->session_id);
    snprintf(_cp_buf, sizeof(_cp_buf), "SequenceNum: %lu\n", (unsigned long)cp->sequence_num);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "Timestamp: %lu\n", (unsigned long)cp->timestamp);
    fputs(_cp_buf, fp);
    snprintf(_cp_buf, sizeof(_cp_buf), "StateSize: %zu\n", cp->state_size);
    fputs(_cp_buf, fp);
    fputs("---DATA---\n", fp);
    if (cp->state_json && cp->state_size > 0)
        fwrite(cp->state_json, 1, cp->state_size, fp);
    fputs("\n---END---\n", fp);
    fclose(fp);
    agentos_checkpoint_destroy(cp);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_snapshot_restore(const char *snap_path, char **tid)
{
    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    if (!snap_path || !tid)
        return AGENTOS_EINVAL;

    FILE *fp = fopen(snap_path, "rb");
    if (!fp)
        return AGENTOS_ENOENT;

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
                size_t tlen = strlen(*tid);
                if (tlen > 0 && (*tid)[tlen - 1] == '\n')
                    (*tid)[tlen - 1] = '\0';
            }
        } else if (strncmp(line, "---DATA---", 10) == 0) {
            break;
        }
    }
    fclose(fp);
    if (!*tid)
        return AGENTOS_EIO;
    return AGENTOS_SUCCESS;
}

/* ========== Auto-checkpoint hooks (CoreLoopThree integration) ========== */

agentos_error_t agentos_checkpoint_set_auto_hook(agentos_checkpoint_hook_fn hook, void *user_data,
                                                 uint64_t interval_ms)
{

    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    agentos_mutex_lock(&g_checkpoint_mutex);
    g_auto_hook = hook;
    g_auto_hook_user_data = user_data;
    g_auto_interval_ms = interval_ms;
    agentos_mutex_unlock(&g_checkpoint_mutex);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_trigger_auto(const char *task_id)
{
    if (!g_checkpoint_initialized)
        return AGENTOS_ENOTINIT;

    agentos_mutex_lock(&g_checkpoint_mutex);
    agentos_checkpoint_hook_fn hook = g_auto_hook;
    void *hook_udata = g_auto_hook_user_data;
    agentos_mutex_unlock(&g_checkpoint_mutex);

    if (!hook || !task_id)
        return AGENTOS_EINVAL;

    hook(task_id, NULL, hook_udata);
    return AGENTOS_SUCCESS;
}
