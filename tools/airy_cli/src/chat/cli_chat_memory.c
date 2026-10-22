// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_memory.c
 * @brief airy_cli chat memory sub-module: memory inject and record.
 *
 * 2.2.4 对话记忆读写（2026-08-25 统一）：CLI 对话路径记忆唯一经
 * gateway mem_d。0.1.19 C3 双栈塌缩后进程内 L1 回退通道退役——gateway
 * 不可达即无注入/无写回，绝不做本地伪存储。CLI 与 gateway/mem_d/TUI
 * 共享同一记忆库（架构级：TUI 为 CLI 的可选渲染层）。
 *
 * 记忆注入（cli_chat_mem_inject_system）：检索相关历史记忆拼成 system
 * 追加段（最多 3 条、每条截断 200 字符），随本轮消息发送给模型。
 * 记忆写回（cli_chat_mem_record）：一轮对话完成且有回复时落盘（用户输入
 * + 回复，只记事实）。0.1.18 B4：思考链不写入共享长期记忆——该库为 CLI
 * 与 TUI 共用，写入即等于经 system 段回灌到后续所有会话。
 */

#include "cli_internal.h"

#include "cli_gw.h" /* 架构约束 2026-08-25：统一经 gateway 派发 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include "cli_gw_json.h"

#include <cjson/cJSON.h>
#include <stdarg.h>

/* 记忆库调用超时：mem_d 为本地服务，6 s 足够覆盖冷缓存首查。 */
#define MEM_TIMEOUT_MS 6000
/* 单轮注入的召回条数与单条字节上限（UTF-8 边界安全截断，见 seg_put）。 */
#define MEM_LIMIT 3
#define MEM_ITEM_MAX 200

/* 有界追加：snprintf 的返回值是"若空间足够会写多少"，直接累加会越过
 * 缓冲边界；此处统一按实际可用空间收敛，调用方无需重复这段算术。 */
static void seg_put(char *buf, size_t cap, size_t *off, const char *fmt, ...)
{
    if (*off >= cap - 1)
        return;
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(buf + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (w > 0)
        *off += ((size_t)w < cap - *off) ? (size_t)w : (cap - *off - 1);
}

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

/* 取单条记忆正文（mem.get）。返回 AIRY_MALLOC 副本，调用方 AIRY_FREE；
 * 失败返回 NULL（该条跳过，不影响其余检索结果）。 */
static char *mem_get_text(const char *rid)
{
    cJSON *params = cJSON_CreateObject();
    if (!params)
        return NULL;
    cJSON_AddStringToObject(params, "record_id", rid);
    cJSON *root = NULL;
    if (cli_gw_json("mem.get", params, MEM_TIMEOUT_MS, &root) != 0)
        return NULL;
    cJSON *data = cJSON_GetObjectItem(root, "data");
    char *text = cJSON_IsString(data) ? AIRY_STRDUP(data->valuestring) : NULL;
    cJSON_Delete(root);
    return text;
}

/* 防自我回灌：跳过以 "用户: <当前输入>" 开头的条目（"用户: " = 9 字节）。
 * strncmp 遇到 NUL 即判定不等，无需额外长度比较。 */
static int mem_echo(const char *rec, const char *input)
{
    static const char k_user[] = "用户: ";
    return strncmp(rec, k_user, sizeof(k_user) - 1) == 0 &&
           strncmp(rec + sizeof(k_user) - 1, input, strlen(input)) == 0;
}

/* gateway 统一记忆注入（mem_d）。0.1.19 C3 起进程内 L1 回退通道退役，
 * gateway 不可达即不注入——记忆唯一事实源为 mem_d，不做本地伪存储。
 * 段头只在确有命中时输出：零命中仍注入空段会让模型看到无内容的提示块。 */
static int cli_chat_mem_inject_gw(const char *input, char *out_buf, size_t out_size)
{
    if (!input || !input[0] || !out_buf || out_size < 16)
        return -1;
    out_buf[0] = '\0';

    cJSON *params = cJSON_CreateObject();
    if (!params)
        return -1;
    cJSON_AddStringToObject(params, "query", input);
    cJSON_AddNumberToObject(params, "limit", MEM_LIMIT);
    cJSON *root = NULL;
    if (cli_gw_json("mem.search", params, MEM_TIMEOUT_MS, &root) != 0)
        return -1;

    cJSON *results = cJSON_GetObjectItem(root, "results");
    int n_items = cJSON_IsArray(results) ? cJSON_GetArraySize(results) : 0;
    size_t off = 0;
    int kept = 0;
    for (int i = 0; i < n_items && off < out_size - 1; i++) {
        cJSON *rid = cJSON_GetObjectItem(cJSON_GetArrayItem(results, i), "record_id");
        if (!cJSON_IsString(rid) || !rid->valuestring[0])
            continue;
        char *rec = mem_get_text(rid->valuestring);
        if (!rec)
            continue;
        if (!mem_echo(rec, input)) {
            if (kept == 0)
                seg_put(out_buf, out_size, &off, "\n\n[相关历史记忆]");
            /* UTF-8 边界回退：%.*s 精度是字节数，直接取 200 会切断
             * 汉字/emoji，产生非法序列经 messages[].content 上报 400。 */
            seg_put(out_buf, out_size, &off, "\n- %.*s", (int)cli_utf8_safe_len(rec, MEM_ITEM_MAX),
                    rec);
            kept++;
        }
        AIRY_FREE(rec);
    }
    cJSON_Delete(root);
    return kept > 0 ? 0 : -1;
}

/* 检索相关历史记忆，拼成 system 追加段（最多 3 条、每条截断 200 字符）。
 * 唯一经 gateway（mem_d）召回；gateway 不可达降级为无注入（0.1.19 C3 单轨）。 */
void cli_chat_mem_inject_system(const char *input, char *out_buf, size_t out_size)
{
    out_buf[0] = '\0';
    if (!input || !input[0] || out_size < 16)
        return;

    (void)cli_chat_mem_inject_gw(input, out_buf, out_size);
}

/* gateway 统一记忆写回（mem_d）。0.1.18 B4：只写用户输入与回复——思考链
 * 不得进入共享长期记忆（§12.4 步 3），否则下轮检索注入会把模型内部推导
 * 当事实回灌。 */
static int cli_chat_mem_record_gw(const char *input, const char *reply)
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
    /* UTF-8 边界回退：1600 是字节数，直接截断会切坏多字节序列。 */
    n = (int)cli_utf8_safe_len(content, 1600);
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
    cJSON *root = NULL;
    int rc = cli_gw_json("mem.write", params, MEM_TIMEOUT_MS, &root);
    cJSON_Delete(root);
    return rc == 0 ? 0 : -1;
}

/* 一轮对话完成后写入记忆：用户输入 + 回复（截断防噪声，只记事实）。
 * 2.1.1.6 曾携带思考链（reasoning）以便被检索召回；0.1.18 B4 撤销该设计：
 * 共享记忆是 CLI 与 TUI 的共同事实来源，模型内部推导不属于事实，写入后
 * 还会经 system 段在后续会话持续回灌。诊断留存走 airy_reasoning.log。
 * 唯一经 gateway（mem_d）写回；gateway 不可达则本轮跳过写入（0.1.19 C3 单轨）。 */
void cli_chat_mem_record(const char *input, const char *reply)
{
    if (!input || !input[0] || !reply || !reply[0])
        return;

    (void)cli_chat_mem_record_gw(input, reply);
}

#endif /* AIRY_HAS_CJSON */
