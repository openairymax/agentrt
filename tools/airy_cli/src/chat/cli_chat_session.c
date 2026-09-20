// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_session.c
 * @brief --continue / --resume 会话恢复视图：读 mem.recent 装配 g_history_*。
 *
 * 会话权威在 mem_d（经 gateway 只读转发 mem.recent），本层只在启动时做一次
 * 装配，前端不落任何本地会话状态，与 agentrt-tui --resume 同源同契约。记录
 * 还原规则与 Rust 侧 split_cli_turn / hydrate 逐字对偶：metadata.role 存在
 * 则按该角色单条还原（仅 user/assistant 入会话历史，其余跳过）；否则按 CLI
 * 整轮线格式（CLI_TURN_*，见 cli_internal.h）拆成 user/assistant 两条。
 *
 * 装配结果直接写 g_history_*：cli_chat.c 每轮请求按序回灌历史（0.1.18 B4 起
 * 不含 reasoning）、cli_tui.c 退出全屏时按同一数组重建三区，main.c 状态行按
 * 同一计数显示轮数——故此处一次装配即全链路生效，无需任何渲染/请求层改动。
 */

#include "cli_internal.h"

#include "cli_gw.h"

#include <stdio.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>

#define CLI_SESSION_GW_TIMEOUT_MS 6000

/* 装配上限：cli_history_capacity() 是消息条数预算，一条记录最多展开为
 * user + assistant 两条，故记录数取预算的一半，恰好不触发 FIFO 成对丢弃。 */
static size_t cli_session_records_want(void)
{
    size_t want = cli_history_capacity() / 2;
    if (want == 0)
        want = 1;
    if (want > CLI_HISTORY_MAX_MSGS / 2)
        want = CLI_HISTORY_MAX_MSGS / 2;
    return want;
}

static int cli_session_blank(const char *s)
{
    for (; *s; s++) {
        if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r')
            return 0;
    }
    return 1;
}

/* CLI 整轮记录拆分：三条分支与 Rust 侧 split_cli_turn 对偶（前缀缺失 →
 * 原样 user 单条；有前缀无回复分隔 → 仅用户侧 user 单条；正常 → user +
 * assistant 两条）。0.1.18 B4：CLI_TURN_REASON_SEP 段只读——仅用于从旧记录
 * 正文中剥离思考链（拆除结果丢弃，不回灌），避免历史推理原文经 system 段
 * 再次进入请求上下文（§12.4 步 4）。 */
static void cli_session_add_turn(const char *data)
{
    if (strncmp(data, CLI_TURN_USER_PREFIX, CLI_TURN_USER_PREFIX_LEN) != 0) {
        cli_history_add("user", data, NULL);
        return;
    }
    const char *rest = data + CLI_TURN_USER_PREFIX_LEN;
    const char *sep = strstr(rest, CLI_TURN_AGENT_SEP);
    if (!sep) {
        cli_history_add("user", rest, NULL);
        return;
    }

    char *user = AIRY_STRNDUP(rest, (size_t)(sep - rest));
    const char *reply_part = sep + CLI_TURN_AGENT_SEP_LEN;
    const char *rsep = strstr(reply_part, CLI_TURN_REASON_SEP);
    char *reply = rsep ? AIRY_STRNDUP(reply_part, (size_t)(rsep - reply_part))
                       : AIRY_STRDUP(reply_part);

    if (user && reply) {
        cli_history_add("user", user, NULL);
        cli_history_add("assistant", reply, NULL);
    }
    AIRY_FREE(user);
    AIRY_FREE(reply);
}

/* 单条记录还原。metadata 在 mem.recent 契约中是字符串形态的 stored JSON，
 * 亦容忍对象形态；无 metadata / 无法解析即按 CLI 整轮线格式拆分。 */
static void cli_session_add_record(cJSON *item)
{
    cJSON *dataj = cJSON_GetObjectItem(item, "data");
    if (!cJSON_IsString(dataj) || !dataj->valuestring || cli_session_blank(dataj->valuestring))
        return;
    const char *data = dataj->valuestring;

    cJSON *mdj = cJSON_GetObjectItem(item, "metadata");
    int md_owned = 0;
    cJSON *md = NULL;
    if (cJSON_IsString(mdj) && mdj->valuestring) {
        md = cJSON_Parse(mdj->valuestring);
        md_owned = 1;
    } else if (cJSON_IsObject(mdj)) {
        md = mdj;
    }

    const char *role = NULL;
    if (md) {
        cJSON *rj = cJSON_GetObjectItem(md, "role");
        if (cJSON_IsString(rj) && rj->valuestring)
            role = rj->valuestring;
        /* metadata.reasoning 有意不读：0.1.18 B4 禁止恢复时回灌思考链
         * （§12.4 步 4）。旧记录中该字段仍存在，忽略即可。 */
    }

    if (role) {
        if (strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0)
            cli_history_add(role, data, NULL);
    } else {
        cli_session_add_turn(data);
    }

    if (md_owned)
        cJSON_Delete(md);
}

void cli_session_restore(void)
{
    char params[48];
    snprintf(params, sizeof(params), "{\"limit\":%zu}", cli_session_records_want());

    char *res = NULL;
    if (cli_gw_call("mem.recent", params, CLI_SESSION_GW_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res);
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "session",
                                  "会话恢复失败：记忆服务不可达（mem.recent）");
        return;
    }

    cJSON *root = cJSON_Parse(res);
    AIRY_FREE(res);
    if (!root) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "session",
                                  "会话恢复失败：mem.recent 响应无法解析");
        return;
    }

    /* 权威返回新→旧（mem_d 倒序遍历记录表），倒序回放即得旧→新，与 TUI
     * 恢复顺序一致；同一秒内的多轮因倒序回放亦回到写入先后。 */
    cJSON *records = cJSON_GetObjectItem(root, "records");
    int n = cJSON_IsArray(records) ? cJSON_GetArraySize(records) : 0;
    for (int i = n - 1; i >= 0; i--)
        cli_session_add_record(cJSON_GetArrayItem(records, i));
    cJSON_Delete(root);

    if (g_history_count == 0) {
        cli_render_sub_agent_line(CLI_ROLE_STATUS, "session",
                                  "无历史会话可恢复（mem.recent 为空）");
        return;
    }
    char msg[96];
    snprintf(msg, sizeof(msg), "已恢复上次会话（%zu 条历史），继续对话或发送新消息",
             g_history_count);
    cli_render_sub_agent_line(CLI_ROLE_STATUS, "session", msg);
}

#endif /* AIRY_HAS_CJSON */
