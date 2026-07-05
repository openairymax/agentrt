/**
 * @file checkpoint.c
 * @brief AgentRT 任务检查点实现（生产级 v0.1.0）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * v0.1.0 变更：
 * - CROSS-01: agentrt_mutex_t → agentrt_mutex_t
 * - CROSS-03: time(NULL) → agentrt_time_ns()
 * - 新增 auto-checkpoint hook 机制（CoreLoopThree 集成）
 * - 增强 JSON restore 解析健壮性
 */

#include "../include/checkpoint.h"

#include "../include/svc_logger.h"
#include "agentrt.h"
#include "daemon_errors.h"
#include "platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

#ifdef _WIN32
#include <windows.h>
#else
#include "agentrt_dirent.h"

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
static agentrt_mutex_t g_checkpoint_mutex;
static atomic_int g_checkpoint_mutex_initialized = 0;
static agentrt_checkpoint_stats_t g_checkpoint_stats = {0};

static agentrt_checkpoint_hook_fn g_auto_hook = NULL;
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

static const char *state_to_string(agentrt_checkpoint_state_t state)
{
    static const char *state_strings[] = {[CHECKPOINT_STATE_PENDING] = "pending",
                                          [CHECKPOINT_STATE_COMPLETED] = "completed",
                                          [CHECKPOINT_STATE_FAILED] = "failed",
                                          [CHECKPOINT_STATE_INVALID] = "invalid"};
    if (state >= 0 && state <= CHECKPOINT_STATE_INVALID)
        return state_strings[state];
    return "unknown";
}

static agentrt_checkpoint_state_t string_to_state(const char *s)
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
    /* 参数校验：调用者通过 NULL 返回值处理，无需推入 error chain。 */
    if (!src) {
        return NULL;
    }
    size_t len = strlen(src);
    char *d = (char *)AGENTRT_MALLOC(len + 1);
    if (d) {
        __builtin_memcpy(d, src, len);
        d[len] = '\0';
    }
    return d;
}

static char **safe_str_array_dup(char **src, size_t count)
{
    /* 参数校验：调用者通过 NULL 返回值处理。 */
    if (!src || count == 0) {
        return NULL;
    }
    char **dst = (char **)AGENTRT_CALLOC(count, sizeof(char *));
    if (!dst) {
        SVC_LOG_ERROR("C-L07: Checkpoint: ARRAY-DUP-FAIL — OOM for count=%zu", count);
        return NULL;
    }
    __builtin_memset(dst, 0, sizeof(char *) * count);
    for (size_t i = 0; i < count; i++) {
        dst[i] = safe_strdup(src[i]);
        if (!dst[i] && src[i]) {
            SVC_LOG_ERROR("C-L07: Checkpoint: ARRAY-DUP-FAIL — OOM at index=%zu", i);
            for (size_t j = 0; j < i; j++)
                AGENTRT_FREE(dst[j]);
            AGENTRT_FREE(dst);
            return NULL;
        }
    }
    return dst;
}

/* 构建包含 sequence_num 的检查点文件路径。
 *
 * v0.1.1 变更：文件名从 checkpoint_{task_id}.json 改为
 * checkpoint_{task_id}_{seq}.json，使每个序列号的检查点独立落盘，
 * 不再相互覆盖。这是 list/restore_seq 的正确性基础 —— 旧格式每次
 * save 覆盖同一文件，导致 per-task 只能保留 1 个检查点。 */
static int build_filepath_with_seq(const char *task_id, uint64_t seq,
                                   char *buf, size_t size)
{
    if (!task_id || !buf || size == 0)
        return AGENTRT_ERR_INVALID_PARAM;
    int n = snprintf(buf, size, "%s/%s%s_%llu%s", g_checkpoint_storage_path,
                     CHECKPOINT_FILE_PREFIX, task_id,
                     (unsigned long long)seq, CHECKPOINT_FILE_EXTENSION);
    return (n > 0 && (size_t)n < size) ? 0 : AGENTRT_ERR_OVERFLOW;
}

/* 从文件名解析属于 task_id 的 sequence_num。
 * 文件名格式：checkpoint_{task_id}_{seq}.json
 *   prefix = "checkpoint_" + task_id + "_"
 *   suffix = ".json"
 *   middle = 全数字 seq
 * 返回 true 并设置 *out_seq；不匹配返回 false。
 *
 * 健壮性：task_id 中若含 '_'，prefix 仍能精确匹配完整 task_id 段，
 * 因为 middle 必须全数字且紧接 .json，可排除 task_id 前缀重叠的误匹配
 * （如 task_id="a" 不会匹配 task_id="a_b" 的文件，因为 "b_3" 非全数字）。 */
static bool parse_seq_from_filename(const char *filename, const char *task_id,
                                    uint64_t *out_seq)
{
    if (!filename || !task_id || !out_seq)
        return false;

    char prefix[MAX_CHECKPOINT_PATH];
    int pn = snprintf(prefix, sizeof(prefix), "%s%s%s",
                      CHECKPOINT_FILE_PREFIX, task_id, "_");
    if (pn <= 0 || (size_t)pn >= sizeof(prefix))
        return false;

    size_t prefix_len = (size_t)pn;
    if (strncmp(filename, prefix, prefix_len) != 0)
        return false;

    const char *suffix = CHECKPOINT_FILE_EXTENSION;
    size_t suffix_len = strlen(suffix);
    size_t flen = strlen(filename);
    if (flen < prefix_len + suffix_len + 1)
        return false;
    if (strcmp(filename + flen - suffix_len, suffix) != 0)
        return false;

    const char *mid = filename + prefix_len;
    size_t mid_len = flen - prefix_len - suffix_len;
    if (mid_len == 0)
        return false;
    for (size_t i = 0; i < mid_len; i++) {
        if (mid[i] < '0' || mid[i] > '9')
            return false;
    }

    uint64_t seq = 0;
    for (size_t i = 0; i < mid_len; i++) {
        seq = seq * 10 + (uint64_t)(mid[i] - '0');
    }
    *out_seq = seq;
    return true;
}

/* 扫描存储目录，收集 task_id 的全部 sequence_num。
 * 返回 AGENTRT_MALLOC 分配的数组（调用者 AGENTRT_FREE），*out_count 为数量；
 * 无匹配时返回 NULL 且 *out_count=0。
 * 不加锁：仅读目录，stats 由调用方在加锁区更新。 */
static uint64_t *collect_task_seqs(const char *task_id, size_t *out_count)
{
    *out_count = 0;
    if (!task_id)
        return NULL;

    size_t cap = 16;
    size_t cnt = 0;
    uint64_t *seqs = (uint64_t *)AGENTRT_MALLOC(cap * sizeof(uint64_t));
    if (!seqs)
        return NULL;

#ifdef _WIN32
    char pattern[MAX_CHECKPOINT_PATH];
    snprintf(pattern, sizeof(pattern), "%s/%s*.json",
             g_checkpoint_storage_path, CHECKPOINT_FILE_PREFIX);
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(pattern, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            uint64_t seq;
            if (parse_seq_from_filename(find_data.cFileName, task_id, &seq)) {
                if (cnt >= cap) {
                    cap *= 2;
                    uint64_t *ns = (uint64_t *)AGENTRT_REALLOC(seqs, cap * sizeof(uint64_t));
                    if (!ns) { AGENTRT_FREE(seqs); FindClose(hFind); return NULL; }
                    seqs = ns;
                }
                seqs[cnt++] = seq;
            }
        } while (FindNextFile(hFind, &find_data));
        FindClose(hFind);
    }
#else
    DIR *dir = opendir(g_checkpoint_storage_path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            uint64_t seq;
            if (parse_seq_from_filename(entry->d_name, task_id, &seq)) {
                if (cnt >= cap) {
                    cap *= 2;
                    uint64_t *ns = (uint64_t *)AGENTRT_REALLOC(seqs, cap * sizeof(uint64_t));
                    if (!ns) { AGENTRT_FREE(seqs); closedir(dir); return NULL; }
                    seqs = ns;
                }
                seqs[cnt++] = seq;
            }
        }
        closedir(dir);
    }
#endif

    if (cnt == 0) {
        AGENTRT_FREE(seqs);
        return NULL;
    }
    *out_count = cnt;
    return seqs;
}

/* 查找 task_id 的最高 sequence_num。返回 0 表示无检查点。
 * 注：生产代码（adapter/loop/engine）保存时 sequence_num 恒 > 0，
 * 故 0 可安全作为"未找到"哨兵。 */
static uint64_t find_latest_seq(const char *task_id)
{
    size_t cnt = 0;
    uint64_t *seqs = collect_task_seqs(task_id, &cnt);
    if (!seqs)
        return 0;

    uint64_t max_seq = 0;
    for (size_t i = 0; i < cnt; i++) {
        if (seqs[i] > max_seq)
            max_seq = seqs[i];
    }
    AGENTRT_FREE(seqs);
    return max_seq;
}

static void init_fields(agentrt_task_checkpoint_t *cp, const char *task_id, const char *session_id,
                        uint64_t seq)
{
    if (!cp)
        return;
    __builtin_memset(cp, 0, sizeof(*cp));
    if (task_id) {
        AGENTRT_STRNCPY_TERM(cp->task_id, task_id, sizeof(cp->task_id));
    }
    if (session_id) {
        AGENTRT_STRNCPY_TERM(cp->session_id, session_id, sizeof(cp->session_id));
    }
    cp->sequence_num = seq;
    cp->timestamp = agentrt_time_ns();
    cp->state = CHECKPOINT_STATE_PENDING;
}

static agentrt_error_t copy_nodes(agentrt_task_checkpoint_t *cp, char **completed, size_t ccount,
                                  char **pending, size_t pcount)
{
    if (!cp)
        return AGENTRT_EINVAL;
    cp->completed_count = ccount;
    if (ccount > 0) {
        cp->completed_nodes = safe_str_array_dup(completed, ccount);
        if (!cp->completed_nodes)
            return AGENTRT_ENOMEM;
    }
    cp->pending_count = pcount;
    if (pcount > 0) {
        cp->pending_nodes = safe_str_array_dup(pending, pcount);
        if (!cp->pending_nodes) {
            if (cp->completed_nodes) {
                for (size_t i = 0; i < ccount; i++)
                    AGENTRT_FREE(cp->completed_nodes[i]);
                AGENTRT_FREE(cp->completed_nodes);
            }
            return AGENTRT_ENOMEM;
        }
    }
    return AGENTRT_SUCCESS;
}

static char *json_extract_string(const char *json, const char *key)
{
    /* 参数校验：调用者通过 NULL 返回值处理。 */
    if (!json || !key) {
        return NULL;
    }
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    /* 键未找到：JSON 解析的正常控制流（如非必填字段），返回 NULL 即可。 */
    if (!p) {
        return NULL;
    }
    p += strlen(search);
    while (*p && isspace((unsigned char)*p))
        p++;
    /* 缺少冒号：JSON 格式异常但属解析分支，返回 NULL 让上层判断。 */
    if (*p != ':') {
        return NULL;
    }
    p++;
    while (*p && isspace((unsigned char)*p))
        p++;
    /* 缺少起始引号：JSON 格式异常但属解析分支，返回 NULL 让上层判断。 */
    if (*p != '"') {
        return NULL;
    }
    p++;

    size_t cap = 512;
    char *val = (char *)AGENTRT_MALLOC(cap);
    if (!val) {
        SVC_LOG_ERROR("C-L07: Checkpoint: JSON-EXTRACT-FAIL — OOM (malloc) for key=%s", key);
        return NULL;
    }
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
                val = (char *)AGENTRT_REALLOC(val, cap);
                if (!val) {
                    SVC_LOG_ERROR("C-L07: Checkpoint: JSON-EXTRACT-FAIL — OOM (realloc escape) for key=%s",
                                  key);
                    return NULL;
                }
            }
            val[len++] = esc;
            p++;
        } else {
            if (len + 2 >= cap) {
                cap *= 2;
                val = (char *)AGENTRT_REALLOC(val, cap);
                if (!val) {
                    SVC_LOG_ERROR("C-L07: Checkpoint: JSON-EXTRACT-FAIL — OOM (realloc char) for key=%s",
                                  key);
                    return NULL;
                }
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

agentrt_error_t agentrt_checkpoint_init(const char *storage_path)
{
    if (g_checkpoint_initialized)
        return AGENTRT_SUCCESS;
    const char *path = storage_path ? storage_path : "./" CHECKPOINT_DIRECTORY;
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(g_checkpoint_storage_path)) {
        SVC_LOG_ERROR("C-L07: Checkpoint: INIT-FAIL — invalid storage path "
                      "len=%zu max=%zu", len, sizeof(g_checkpoint_storage_path));
        return AGENTRT_EINVAL;
    }
    __builtin_memcpy(g_checkpoint_storage_path, path, len);
    g_checkpoint_storage_path[len] = '\0';

    {
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(&g_checkpoint_mutex_initialized, &expected, 1,
                                                    memory_order_seq_cst, memory_order_seq_cst)) {
            if (agentrt_mutex_init(&g_checkpoint_mutex) != 0) {
                atomic_store_explicit(&g_checkpoint_mutex_initialized, 0, memory_order_seq_cst);
                SVC_LOG_ERROR("C-L07: Checkpoint: INIT-FAIL — mutex init failed "
                              "path=%s", g_checkpoint_storage_path);
                return DAEMON_EINIT;
            }
        }
    }

    __builtin_memset(&g_checkpoint_stats, 0, sizeof(g_checkpoint_stats));
    atomic_store_explicit(&g_checkpoint_initialized, 1, memory_order_seq_cst);
    SVC_LOG_INFO("C-L07: Checkpoint: INIT-OK path=%s", g_checkpoint_storage_path);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_checkpoint_shutdown(void)
{
    if (!atomic_load_explicit(&g_checkpoint_initialized, memory_order_acquire))
        return AGENTRT_SUCCESS;
    if (atomic_load_explicit(&g_checkpoint_mutex_initialized, memory_order_acquire)) {
        agentrt_mutex_destroy(&g_checkpoint_mutex);
        atomic_store_explicit(&g_checkpoint_mutex_initialized, 0, memory_order_seq_cst);
    }
    g_auto_hook = NULL;
    g_auto_hook_user_data = NULL;
    atomic_store_explicit(&g_checkpoint_initialized, 0, memory_order_seq_cst);
    SVC_LOG_INFO("C-L07: Checkpoint: SHUTDOWN-OK "
                 "total=%llu success=%llu failed=%llu",
                 (unsigned long long)g_checkpoint_stats.total_checkpoints,
                 (unsigned long long)g_checkpoint_stats.successful_checkpoints,
                 (unsigned long long)g_checkpoint_stats.failed_checkpoints);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_checkpoint_create(const char *task_id, const char *session_id,
                                          uint64_t sequence_num, const char *state_json,
                                          char **completed_nodes, size_t completed_count,
                                          char **pending_nodes, size_t pending_count,
                                          agentrt_task_checkpoint_t **out_cp)
{

    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;
    if (!task_id || !state_json || !out_cp)
        return AGENTRT_EINVAL;

    agentrt_task_checkpoint_t *cp =
        (agentrt_task_checkpoint_t *)AGENTRT_CALLOC(1, sizeof(agentrt_task_checkpoint_t));
    if (!cp) {
        SVC_LOG_ERROR("C-L07: Checkpoint: CREATE-FAIL — OOM for task_id=%s", task_id);
        return AGENTRT_ENOMEM;
    }

    init_fields(cp, task_id, session_id, sequence_num);

    cp->state_json = safe_strdup(state_json);
    if (!cp->state_json) {
        AGENTRT_FREE(cp);
        return AGENTRT_ENOMEM;
    }
    cp->state_size = strlen(state_json);

    agentrt_error_t err =
        copy_nodes(cp, completed_nodes, completed_count, pending_nodes, pending_count);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_FREE(cp->state_json);
        AGENTRT_FREE(cp);
        return err;
    }

    cp->checksum = calculate_checksum(state_json, strlen(state_json));
    *out_cp = cp;
    return AGENTRT_SUCCESS;
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

agentrt_error_t agentrt_checkpoint_save(agentrt_task_checkpoint_t *cp)
{
    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;
    if (!cp || !cp->task_id[0])
        return AGENTRT_EINVAL;

    char filepath[MAX_CHECKPOINT_PATH];
    if (build_filepath_with_seq(cp->task_id, cp->sequence_num,
                                filepath, sizeof(filepath)) != 0)
        return AGENTRT_EINVAL;

    char tmppath[MAX_CHECKPOINT_PATH];
    snprintf(tmppath, sizeof(tmppath), "%s.tmp", filepath);

    FILE *fp = fopen(tmppath, "w");
    if (!fp) {
        agentrt_mutex_lock(&g_checkpoint_mutex);
        g_checkpoint_stats.failed_checkpoints++;
        agentrt_mutex_unlock(&g_checkpoint_mutex);
        SVC_LOG_ERROR("C-L07: Checkpoint: SAVE-FAIL — cannot open file "
                      "path=%s task_id=%s errno=%d", tmppath, cp->task_id, errno);
        return AGENTRT_EIO;
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
        agentrt_mutex_lock(&g_checkpoint_mutex);
        g_checkpoint_stats.failed_checkpoints++;
        agentrt_mutex_unlock(&g_checkpoint_mutex);
        SVC_LOG_ERROR("C-L07: Checkpoint: SAVE-FAIL — rename failed "
                      "tmp=%s dst=%s task_id=%s errno=%d",
                      tmppath, filepath, cp->task_id, errno);
        return AGENTRT_EIO;
    }

    agentrt_mutex_lock(&g_checkpoint_mutex);
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
    agentrt_mutex_unlock(&g_checkpoint_mutex);

    SVC_LOG_DEBUG("C-L07: Checkpoint: SAVE-OK task_id=%s seq=%llu size=%zu",
                  cp->task_id, (unsigned long long)cp->sequence_num, cp->state_size);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_checkpoint_restore(const char *task_id, uint64_t sequence_num,
                                           agentrt_task_checkpoint_t **out_cp)
{

    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;
    if (!task_id || !out_cp)
        return AGENTRT_EINVAL;

    /* sequence_num == 0 表示恢复最新检查点：扫描目录取最高 seq。
     * sequence_num > 0 表示恢复指定序列号的检查点。 */
    uint64_t actual_seq = sequence_num;
    if (sequence_num == 0) {
        actual_seq = find_latest_seq(task_id);
        if (actual_seq == 0) {
            SVC_LOG_WARN("C-L07: Checkpoint: RESTORE-FAIL — no checkpoint found "
                         "task_id=%s", task_id);
            return AGENTRT_ENOENT;
        }
    }

    char filepath[MAX_CHECKPOINT_PATH];
    if (build_filepath_with_seq(task_id, actual_seq, filepath, sizeof(filepath)) != 0)
        return AGENTRT_EINVAL;

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        SVC_LOG_WARN("C-L07: Checkpoint: RESTORE-FAIL — file not found "
                     "path=%s task_id=%s seq=%llu errno=%d",
                     filepath, task_id, (unsigned long long)actual_seq, errno);
        return AGENTRT_ENOENT;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {
        fclose(fp);
        SVC_LOG_ERROR("C-L07: Checkpoint: RESTORE-FAIL — invalid file size "
                      "path=%s size=%ld task_id=%s", filepath, file_size, task_id);
        return AGENTRT_EIO;
    }

    char *json_buf = (char *)AGENTRT_MALLOC((size_t)(file_size + 1));
    if (!json_buf) {
        fclose(fp);
        SVC_LOG_ERROR("C-L07: Checkpoint: RESTORE-FAIL — OOM "
                      "path=%s size=%ld task_id=%s", filepath, file_size, task_id);
        return AGENTRT_ENOMEM;
    }

    size_t read_len = fread(json_buf, 1, (size_t)file_size, fp);
    if (read_len != (size_t)file_size) {
        AGENTRT_FREE(json_buf);
        fclose(fp);
        SVC_LOG_ERROR("C-L07: Checkpoint: RESTORE-FAIL — read error "
                      "path=%s expected=%ld actual=%zu task_id=%s",
                      filepath, file_size, read_len, task_id);
        return AGENTRT_EIO;
    }
    json_buf[read_len] = '\0';
    fclose(fp);

    agentrt_task_checkpoint_t *cp =
        (agentrt_task_checkpoint_t *)AGENTRT_CALLOC(1, sizeof(agentrt_task_checkpoint_t));
    if (!cp) {
        AGENTRT_FREE(json_buf);
        return AGENTRT_ENOMEM;
    }

    char *state_str = json_extract_string(json_buf, "state");
    char *sj = json_extract_string(json_buf, "state_json");

    init_fields(cp, task_id, "", 0);

    if (state_str) {
        cp->state = string_to_state(state_str);
        AGENTRT_FREE(state_str);
    }
    if (sj) {
        cp->state_json = sj;
        cp->state_size = strlen(sj);
        cp->checksum = calculate_checksum(sj, strlen(sj));
    }

    char *sid = json_extract_string(json_buf, "session_id");
    if (sid) {
        AGENTRT_STRNCPY_TERM(cp->session_id, sid, sizeof(cp->session_id));
        AGENTRT_FREE(sid);
        sid = NULL;
    }
    cp->sequence_num = json_extract_uint64(json_buf, "sequence_num");
    cp->timestamp = json_extract_uint64(json_buf, "timestamp");

    AGENTRT_FREE(json_buf);
    json_buf = NULL;
    agentrt_mutex_lock(&g_checkpoint_mutex);
    g_checkpoint_stats.total_restore_ops++;
    agentrt_mutex_unlock(&g_checkpoint_mutex);
    *out_cp = cp;
    SVC_LOG_DEBUG("C-L07: Checkpoint: RESTORE-OK task_id=%s seq=%llu state=%s",
                  task_id, (unsigned long long)cp->sequence_num,
                  state_to_string(cp->state));
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_checkpoint_delete(const char *task_id, uint64_t seq_num)
{
    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;
    if (!task_id)
        return AGENTRT_EINVAL;

    /* seq_num > 0: 删除指定序列号的检查点文件 */
    if (seq_num > 0) {
        char filepath[MAX_CHECKPOINT_PATH];
        if (build_filepath_with_seq(task_id, seq_num, filepath, sizeof(filepath)) != 0)
            return AGENTRT_EINVAL;

        agentrt_mutex_lock(&g_checkpoint_mutex);
        int result = unlink(filepath);
        if (result == 0) {
            if (g_checkpoint_stats.total_checkpoints > 0)
                g_checkpoint_stats.total_checkpoints--;
            SVC_LOG_INFO("C-L07: Checkpoint: DELETE-OK task_id=%s seq=%llu",
                         task_id, (unsigned long long)seq_num);
        } else {
            SVC_LOG_WARN("C-L07: Checkpoint: DELETE-FAIL — unlink failed "
                         "task_id=%s seq=%llu errno=%d",
                         task_id, (unsigned long long)seq_num, errno);
        }
        agentrt_mutex_unlock(&g_checkpoint_mutex);

        return (result == 0) ? AGENTRT_SUCCESS : AGENTRT_ENOENT;
    }

    /* seq_num == 0: 删除该 task_id 的全部检查点文件 */
    size_t cnt = 0;
    uint64_t *seqs = collect_task_seqs(task_id, &cnt);
    if (!seqs) {
        SVC_LOG_INFO("C-L07: Checkpoint: DELETE-ALL — no checkpoints for task_id=%s",
                     task_id);
        return AGENTRT_ENOENT;
    }

    size_t deleted = 0;
    agentrt_mutex_lock(&g_checkpoint_mutex);
    for (size_t i = 0; i < cnt; i++) {
        char filepath[MAX_CHECKPOINT_PATH];
        if (build_filepath_with_seq(task_id, seqs[i], filepath, sizeof(filepath)) == 0) {
            if (unlink(filepath) == 0) {
                deleted++;
                if (g_checkpoint_stats.total_checkpoints > 0)
                    g_checkpoint_stats.total_checkpoints--;
            }
        }
    }
    agentrt_mutex_unlock(&g_checkpoint_mutex);
    AGENTRT_FREE(seqs);

    SVC_LOG_INFO("C-L07: Checkpoint: DELETE-ALL task_id=%s deleted=%zu/%zu",
                 task_id, deleted, cnt);
    return (deleted > 0) ? AGENTRT_SUCCESS : AGENTRT_ENOENT;
}

agentrt_error_t agentrt_checkpoint_list(const char *task_id, agentrt_task_checkpoint_t ***out_cps,
                                        size_t *out_count)
{
    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;
    if (!task_id || !out_cps || !out_count)
        return AGENTRT_EINVAL;

    *out_cps = NULL;
    *out_count = 0;

    /* 扫描目录，收集 task_id 的全部 sequence_num */
    size_t cnt = 0;
    uint64_t *seqs = collect_task_seqs(task_id, &cnt);
    if (!seqs || cnt == 0) {
        if (seqs) AGENTRT_FREE(seqs);
        return AGENTRT_ENOENT;
    }

    /* 按 seq 升序排序（插入排序，cnt 通常很小） */
    for (size_t i = 1; i < cnt; i++) {
        uint64_t key = seqs[i];
        size_t j = i;
        while (j > 0 && seqs[j - 1] > key) {
            seqs[j] = seqs[j - 1];
            j--;
        }
        seqs[j] = key;
    }

    /* 逐个恢复检查点。collect_task_seqs 给出目录扫描时的快照，
     * 其间文件可能被并发删除；restore 失败时跳过该条目而非整体失败，
     * 保证返回的是实际可恢复的检查点集合。 */
    agentrt_task_checkpoint_t **arr =
        (agentrt_task_checkpoint_t **)AGENTRT_CALLOC(cnt, sizeof(agentrt_task_checkpoint_t *));
    if (!arr) {
        AGENTRT_FREE(seqs);
        return AGENTRT_ENOMEM;
    }

    size_t restored_count = 0;
    for (size_t i = 0; i < cnt; i++) {
        agentrt_task_checkpoint_t *cp = NULL;
        agentrt_error_t err = agentrt_checkpoint_restore(task_id, seqs[i], &cp);
        if (err == AGENTRT_SUCCESS && cp) {
            arr[restored_count++] = cp;
        }
    }
    AGENTRT_FREE(seqs);

    if (restored_count == 0) {
        AGENTRT_FREE(arr);
        return AGENTRT_ENOENT;
    }

    *out_cps = arr;
    *out_count = restored_count;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_checkpoint_get_stats(agentrt_checkpoint_stats_t *stats)
{
    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;
    if (!stats)
        return AGENTRT_EINVAL;
    agentrt_mutex_lock(&g_checkpoint_mutex);
    __builtin_memcpy(stats, &g_checkpoint_stats, sizeof(agentrt_checkpoint_stats_t));
    agentrt_mutex_unlock(&g_checkpoint_mutex);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_checkpoint_verify(const agentrt_task_checkpoint_t *cp, bool *is_valid)
{

    if (!cp || !is_valid)
        return AGENTRT_EINVAL;
    *is_valid = false;
    if (cp->state == CHECKPOINT_STATE_INVALID)
        return AGENTRT_SUCCESS;
    if (!cp->state_json || cp->state_size == 0)
        return AGENTRT_SUCCESS;
    uint32_t calc = calculate_checksum(cp->state_json, cp->state_size);
    *is_valid = (calc == cp->checksum);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_checkpoint_destroy(agentrt_task_checkpoint_t *cp)
{
    if (!cp)
        return AGENTRT_SUCCESS;
    if (cp->state_json) {
        AGENTRT_FREE(cp->state_json);
        cp->state_json = NULL;
    }
    if (cp->completed_nodes) {
        for (size_t i = 0; i < cp->completed_count; i++)
            AGENTRT_FREE(cp->completed_nodes[i]);
        AGENTRT_FREE(cp->completed_nodes);
        cp->completed_nodes = NULL;
    }
    if (cp->pending_nodes) {
        for (size_t i = 0; i < cp->pending_count; i++)
            AGENTRT_FREE(cp->pending_nodes[i]);
        AGENTRT_FREE(cp->pending_nodes);
        cp->pending_nodes = NULL;
    }
    AGENTRT_FREE(cp);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_checkpoint_cleanup(uint64_t max_age_sec, size_t max_cnt)
{
    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;

    agentrt_mutex_lock(&g_checkpoint_mutex);
    /* 注意：必须使用 CLOCK_REALTIME 基准（time(NULL)）与文件 st_mtime 比较。
     * 之前误用 agentrt_time_ms()（CLOCK_MONOTONIC，系统启动以来的毫秒数），
     * 与 st_mtime（CLOCK_REALTIME，自 1970 年以来的秒数）基准不一致，
     * 导致 uint64_t 减法下溢，所有文件被误判为过期而删除。 */
    uint64_t now_sec = (uint64_t)time(NULL);

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

    agentrt_mutex_unlock(&g_checkpoint_mutex);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_snapshot_create(const char *task_id, const char *snap_path)
{
    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;
    if (!task_id || !snap_path)
        return AGENTRT_EINVAL;

    agentrt_task_checkpoint_t *cp = NULL;
    agentrt_error_t err = agentrt_checkpoint_restore(task_id, 0, &cp);
    if (err != AGENTRT_SUCCESS)
        return err;

    FILE *fp = fopen(snap_path, "wb");
    if (!fp) {
        agentrt_checkpoint_destroy(cp);
        SVC_LOG_ERROR("C-L07: Checkpoint: SNAPSHOT-CREATE-FAIL — cannot open file "
                      "path=%s task_id=%s errno=%d", snap_path, task_id, errno);
        return AGENTRT_EIO;
    }

    char _cp_buf[2048];
    fputs("SNAPSHOT_V1\n", fp);
    fprintf_sanitized(fp, "TaskID", cp->task_id); /* BAN-70 EXEMPT: checkpoint snapshot output */
    fprintf_sanitized(fp, "SessionID", cp->session_id); /* BAN-70 EXEMPT: checkpoint snapshot output */
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
    agentrt_checkpoint_destroy(cp);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_snapshot_restore(const char *snap_path, char **tid)
{
    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;
    if (!snap_path || !tid)
        return AGENTRT_EINVAL;

    FILE *fp = fopen(snap_path, "rb");
    if (!fp) {
        SVC_LOG_WARN("C-L07: Checkpoint: SNAPSHOT-RESTORE-FAIL — file not found "
                     "path=%s errno=%d", snap_path, errno);
        return AGENTRT_ENOENT;
    }

    char header[64];
    if (!fgets(header, sizeof(header), fp) || strncmp(header, "SNAPSHOT_V1", 11) != 0) {
        fclose(fp);
        SVC_LOG_ERROR("C-L07: Checkpoint: SNAPSHOT-RESTORE-FAIL — bad header "
                      "path=%s header=%.20s", snap_path, header);
        return AGENTRT_EIO;
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
        return AGENTRT_EIO;
    return AGENTRT_SUCCESS;
}

/* ========== Auto-checkpoint hooks (CoreLoopThree integration) ========== */

agentrt_error_t agentrt_checkpoint_set_auto_hook(agentrt_checkpoint_hook_fn hook, void *user_data,
                                                 uint64_t interval_ms)
{

    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;
    agentrt_mutex_lock(&g_checkpoint_mutex);
    g_auto_hook = hook;
    g_auto_hook_user_data = user_data;
    g_auto_interval_ms = interval_ms;
    agentrt_mutex_unlock(&g_checkpoint_mutex);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_checkpoint_trigger_auto(const char *task_id)
{
    if (!g_checkpoint_initialized)
        return AGENTRT_ENOTINIT;

    agentrt_mutex_lock(&g_checkpoint_mutex);
    agentrt_checkpoint_hook_fn hook = g_auto_hook;
    void *hook_udata = g_auto_hook_user_data;
    agentrt_mutex_unlock(&g_checkpoint_mutex);

    if (!hook || !task_id)
        return AGENTRT_EINVAL;

    hook(task_id, NULL, hook_udata);
    return AGENTRT_SUCCESS;
}
