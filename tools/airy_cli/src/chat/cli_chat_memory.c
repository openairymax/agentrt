// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_memory.c
 * @brief airy_cli chat memory sub-module: memory inject and record.
 *
 * 2.2.4 对话记忆读写（2026-08-25 统一）：CLI 对话路径的记忆统一经
 * gateway mem_d（与 TUI 共享记忆库），失败回退进程内 L1 记忆引擎。
 * 此前 CLI 用 coreloop L1 记忆引擎、TUI 用 gateway mem_d，两套存储——
 * CLI/TUI 切换后"记不住上一句"的直接根因；统一后共享同一记忆库。
 *
 * 记忆注入（cli_chat_mem_inject_system）：检索相关历史记忆拼成 system
 * 追加段（最多 3 条、每条截断 200 字符），随本轮消息发送给模型。
 * 记忆写回（cli_chat_mem_record）：一轮对话完成且有回复时落盘（用户输入
 * + 回复 + 思考链），供下轮/下次会话检索注入。
 */

#include "cli_internal.h"

#include "cli_gw.h" /* 架构约束 2026-08-25：统一经 gateway 派发 */

#include "memory.h" /* 对话记忆引擎 API（2.2.4 对话路径读写） */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>

/* 对话记忆引擎（2.2.4）：main.c 在 loop 创建后注入，见 cli_internal.h */
airy_memory_engine_t *g_cli_memory_engine = NULL;

/* 2.2.2.1：无信息量寒暄启发式——此类对话不应进记忆，避免垃圾累积
 * 污染语义检索（匹配时返回 1）。仅作写入门槛，不做强制语义判断。 */
static int cli_chat_is_greeting(const char *s)
{
    if (!s)
        return 0;
    size_t n = strlen(s);
    if (n > 12)
        return 0;
    static const char *const greets[] = {
        "你好", "您好", "哈喽", "嗨", "hello", "hi", "hey",
        "谢谢", "感谢", "多谢", "thanks", "thank you", "thx",
        "再见", "拜拜", "bye", "goodbye", "ok", "好的", "嗯", "收到",
    };
    for (size_t i = 0; i < sizeof(greets) / sizeof(greets[0]); i++) {
        if (strcmp(s, greets[i]) == 0 || strncmp(s, greets[i], strlen(greets[i])) == 0)
            return 1;
    }
    return 0;
}

/* gateway 统一记忆注入（mem_d，与 TUI/gateway 共享召回；失败回退 L1）。
 * CLI 曾用 coreloop L1 记忆引擎（memoryrovol），与 gateway 的 mem_d 是两套
 * 存储——CLI/TUI 切换后"记不住上一句"的直接根因。统一后共享同一记忆库。 */
static int cli_chat_mem_inject_gw(const char *input, char *out_buf, size_t out_size)
{
    if (!input || !input[0] || !out_buf || out_size < 16)
        return -1;
    out_buf[0] = '\0';

    cJSON *params = cJSON_CreateObject();
    if (!params)
        return -1;
    cJSON_AddStringToObject(params, "query", input);
    cJSON_AddNumberToObject(params, "limit", 3);
    char *pstr = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!pstr)
        return -1;

    char *res_json = NULL;
    if (cli_gw_call("mem.search", pstr, 6000, &res_json) != 0) {
        AIRY_FREE(pstr);
        return -1;
    }
    AIRY_FREE(pstr);

    cJSON *root = cJSON_Parse(res_json);
    AIRY_FREE(res_json);
    if (!root)
        return -1;
    cJSON *results = cJSON_GetObjectItem(root, "results");
    if (!cJSON_IsArray(results)) {
        cJSON_Delete(root);
        return -1;
    }

    size_t off = 0;
    if (out_size > 0) {
        int w0 = snprintf(out_buf + off, out_size - off, "\n\n[相关历史记忆]");
        if (w0 > 0)
            off += ((size_t)w0 < out_size - off) ? (size_t)w0 : (out_size - off - 1);
    }

    /* 防自我回灌：跳过与当前输入相同/几乎相同的记忆（"用户: "=9 字节 UTF-8） */
    size_t input_prefix_len = strlen(input) + 9;
    int n_items = cJSON_GetArraySize(results);
    for (int i = 0; i < n_items && off < out_size - 1; i++) {
        cJSON *item = cJSON_GetArrayItem(results, i);
        cJSON *rid = cJSON_GetObjectItem(item, "record_id");
        if (!cJSON_IsString(rid) || !rid->valuestring[0])
            continue;

        cJSON *gparams = cJSON_CreateObject();
        if (!gparams)
            continue;
        cJSON_AddStringToObject(gparams, "record_id", rid->valuestring);
        char *gstr = cJSON_PrintUnformatted(gparams);
        cJSON_Delete(gparams);
        if (!gstr)
            continue;
        char *gjson = NULL;
        if (cli_gw_call("mem.get", gstr, 6000, &gjson) != 0) {
            AIRY_FREE(gstr);
            continue;
        }
        AIRY_FREE(gstr);
        cJSON *groot = cJSON_Parse(gjson);
        AIRY_FREE(gjson);
        if (!groot)
            continue;
        cJSON *data = cJSON_GetObjectItem(groot, "data");
        if (cJSON_IsString(data)) {
            const char *rec = data->valuestring;
            size_t dlen = strlen(rec);
            if (!(dlen >= input_prefix_len && strncmp(rec, "用户: ", 9) == 0 &&
                  strncmp(rec + 9, input, strlen(input)) == 0)) {
                size_t n = dlen < 200 ? dlen : 200;
                if (off < out_size - 1) {
                    int w1 = snprintf(out_buf + off, out_size - off, "\n- %.*s", (int)n, rec);
                    if (w1 > 0)
                        off += ((size_t)w1 < out_size - off) ? (size_t)w1 : (out_size - off - 1);
                }
            }
        }
        cJSON_Delete(groot);
        if (off >= out_size - 1)
            break;
    }
    cJSON_Delete(root);
    return off > 0 ? 0 : -1;
}

/* 检索相关历史记忆，拼成 system 追加段（最多 3 条、每条截断 200 字符）。
 * 统一经 gateway（mem_d）召回，与 TUI 共享记忆库；gateway 不可用时回退
 * 进程内 L1 记忆引擎（离线/单机降级）。 */
void cli_chat_mem_inject_system(const char *input, char *out_buf, size_t out_size)
{
    out_buf[0] = '\0';
    if (!input || !input[0] || out_size < 16)
        return;

    if (cli_chat_mem_inject_gw(input, out_buf, out_size) == 0)
        return;

    /* ── 回退：进程内 L1 记忆引擎（gateway/mem_d 不可用） ── */
    if (!g_cli_memory_engine)
        return;

    airy_memory_query_t q;
    __builtin_memset(&q, 0, sizeof(q));
    q.memory_query_text = (char *)input;
    q.memory_query_text_len = strlen(input);
    q.memory_query_limit = 3;
    q.memory_query_include_raw = 1;

    airy_memory_result_ext_t *res = NULL;
    if (airy_memory_query(g_cli_memory_engine, &q, &res) != AIRY_EOK || !res)
        return;

    size_t off = 0;
    /* P3-3：snprintf 返回"应写长度"，钳制累加防 pos 越过容量（无符号
     * 下溢越界写） */
    if (out_size > 0) {
        int w0 = snprintf(out_buf + off, out_size - off, "\n\n[相关历史记忆]");
        if (w0 > 0)
            off += ((size_t)w0 < out_size - off) ? (size_t)w0 : (out_size - off - 1);
    }
    /* 2.2.2.1：防自我回灌——跳过与当前输入相同/几乎相同的记忆（CLI 把
     * 整轮写为 "用户: <input>\nAgentRT: <reply>"，查询常命中上一条自身）。 */
    size_t input_prefix_len = strlen(input) + 9; /* "用户: "(9 字节 UTF-8) + input */
    for (size_t i = 0; i < res->memory_result_count; i++) {
        airy_memory_result_item_t *it =
            (res->memory_result_items && i < res->memory_result_count)
                ? res->memory_result_items[i]
                : NULL;
        if (!it || !it->memory_result_item_record)
            continue;
        airy_memory_record_t *r = it->memory_result_item_record;
        if (!r->memory_record_data || r->memory_record_data_len == 0)
            continue;
        const char *data = (const char *)r->memory_record_data;
        size_t dlen = r->memory_record_data_len;
        /* "用户: " 在 UTF-8 下为 9 字节（"用户"=6 字节 + 空格冒号空格=3）。
         * 前缀比较与偏移必须用 9，否则过滤分支永不命中，自我回灌继续发生。 */
        if (dlen >= input_prefix_len &&
            strncmp(data, "用户: ", 9) == 0 &&
            strncmp(data + 9, input, strlen(input)) == 0)
            continue;
        size_t n = dlen < 200 ? dlen : 200;
        if (off < out_size - 1) {
            int w1 = snprintf(out_buf + off, out_size - off, "\n- %.*s", (int)n, data);
            if (w1 > 0)
                off += ((size_t)w1 < out_size - off) ? (size_t)w1 : (out_size - off - 1);
        }
        if (off >= out_size - 1)
            break;
    }
    airy_memory_result_free(res);
}

/* gateway 统一记忆写回（mem_d，与 TUI 共享存储；失败回退 L1）。 */
static int cli_chat_mem_record_gw(const char *input, const char *reply, const char *reasoning)
{
    if (!input || !input[0] || !reply || !reply[0])
        return -1;
    /* 寒暄/无信息量回复不写记忆（避免垃圾条目累积抬高检索噪声） */
    if (strlen(reply) < 8 || cli_chat_is_greeting(input))
        return 0; /* 有意跳过，非失败 */

    char content[1800];
    int n = snprintf(content, sizeof(content), "用户: %s\nAgentRT: %s", input, reply);
    if (n <= 0)
        return -1;
    if (reasoning && reasoning[0]) {
        int rn = snprintf(content + n, (size_t)(sizeof(content) - n), "\n[reasoning] %s", reasoning);
        if (rn > 0)
            n += (rn < (int)(sizeof(content) - n - 1)) ? rn : (int)(sizeof(content) - n - 1);
    }
    if (n > 1600)
        n = 1600;
    content[n] = '\0';

    cJSON *params = cJSON_CreateObject();
    if (!params)
        return -1;
    cJSON_AddStringToObject(params, "data", content);
    cJSON *meta = cJSON_CreateObject();
    if (meta) {
        cJSON_AddStringToObject(meta, "source", "cli");
        cJSON_AddStringToObject(meta, "kind", "chat");
        cJSON_AddItemToObject(params, "metadata", meta);
    }
    char *pstr = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!pstr)
        return -1;
    char *res = NULL;
    int rc = cli_gw_call("mem.write", pstr, 6000, &res);
    AIRY_FREE(pstr);
    AIRY_FREE(res);
    return rc == 0 ? 0 : -1;
}

/* 一轮对话完成后写入记忆：用户输入 + 回复（截断防噪声，只记事实）。
 * 2.1.1.6 修订：携带思考链（reasoning）——记忆检索按 content 匹配，
 * 拼接进记录后思考 token 可被下轮/下次会话召回（与 TUI 记忆的
 * reasoning 语义对齐），不再"只存档不可用"。
 * 统一经 gateway（mem_d）写回，与 TUI 共享记忆库；gateway 不可用时
 * 回退进程内 L1 记忆引擎（离线/单机降级）。 */
void cli_chat_mem_record(const char *input, const char *reply, const char *reasoning)
{
    if (!input || !input[0] || !reply || !reply[0])
        return;

    if (cli_chat_mem_record_gw(input, reply, reasoning) == 0)
        return;

    /* ── 回退：进程内 L1 记忆引擎 ── */
    if (!g_cli_memory_engine)
        return;

    /* 2.2.2.1：寒暄/无信息量回复不写记忆（避免垃圾条目累积抬高检索噪声） */
    if (strlen(reply) < 8 || cli_chat_is_greeting(input))
        return;

    char content[1800];
    int n = snprintf(content, sizeof(content), "用户: %s\nAgentRT: %s", input, reply);
    if (n <= 0)
        return;
    if (reasoning && reasoning[0]) {
        int rn = snprintf(content + n, (size_t)(sizeof(content) - n), "\n[reasoning] %s", reasoning);
        if (rn > 0)
            n += (rn < (int)(sizeof(content) - n - 1)) ? rn : (int)(sizeof(content) - n - 1);
    }
    if (n > 1600)
        n = 1600;

    airy_memory_record_t rec;
    __builtin_memset(&rec, 0, sizeof(rec));
    rec.memory_record_type = AIRY_MEMTYPE_TEXT;
    rec.memory_record_data = content;
    rec.memory_record_data_len = (size_t)n;
    rec.memory_record_importance = 0.6f;

    char *rid = NULL;
    (void)airy_memory_write(g_cli_memory_engine, &rec, &rid);
    AIRY_FREE(rid);
}

#endif /* AIRY_HAS_CJSON */
