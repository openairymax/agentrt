// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_tools.c
 * @brief airy_cli chat tool-use loop sub-module.
 *
 * 聊天工具回路（Claude Code / Codex tool-use 范式，2026-08-15）：
 * 日常对话同样可以调用工具——模型看到 web_search / web_fetch / fs_*
 * 定义，回答时可返回 tool_calls；CLI 执行工具、把结果以 role="tool"
 * 消息回填，再请求一轮，直到模型不再调用工具，最后流式渲染最终回复。
 *
 * 工具行为 SSoT 在 tool_d（builtin.c/builtin_net.c 真实实现，非桩），
 * CLI 只做协议编排：工具执行细节在 tool_d，对话策略在 CLI（机制/策略
 * 分离，Linux 哲学）。架构约束（2026-08-25）：工具执行统一经 gateway
 * 派发（tool.execute → gateway → SYS_SVC_CALL → tool_d，带 ACL
 * fail-closed），与 gateway agent.run 同一路径；不直连 tool.sock。
 *
 * 安全护栏：工具轮上限 CLI_CHAT_TOOL_MAX_ROUNDS（防模型无限工具循环）；
 * 工具结果按折叠摘要渲染到屏幕（全量文本仍回填模型上下文）。
 */

#include "cli_internal.h"

#include "cli_gw.h" /* 架构约束 2026-08-25：统一经 gateway 派发 */

#include "cli_prelude.h"
#include "err_class.h" /* 0.1.19 N1：工具失败按 class/code/diag 透传 */

#include "airy_tool_schema.h"

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>

/* tool_result_t 定义于 daemons/tool_d/include/tool_service.h（CLI 已含该
 * include 路径）；web_search_tool / web_fetch_tool 声明于 tool_d/src/
 * tool_builtin_internal.h（src 目录不对外），已编译进 airy_tool_service
 * 库（builtin_net.c），此处 extern 声明直接链接调用。 */
#include "tool_service.h"

#ifdef _WIN32
/* Windows：builtin_net.c 的 web_search_tool 仅在 POSIX 编译（#ifndef
 * _WIN32），此处提供与 web_fetch_tool 平台降级一致的实现——真实报告平台
 * 能力边界（网络工具在 Windows 暂缺 curl 子进程实现），非桩函数。 */
int web_search_tool(const char *params_json, tool_result_t *res)
{
    (void)params_json;
    if (!res)
        return AIRY_ERR_INVALID_PARAM;
    res->error = AIRY_STRDUP("web_search is not supported on this platform");
    return AIRY_ERR_NOT_SUPPORTED;
}
#else
int web_search_tool(const char *params_json, tool_result_t *res);
#endif
int web_fetch_tool(const char *params_json, tool_result_t *res);

/* 聊天工具回路 schema：单一权威在 commons airy_tool_schema.h（逐工具片段
 * 宏，与 tool_d BTOOL 注册一一对应），此处按子集策略引用宏拼装，禁止手写
 * schema 文本。子集 = 16 − 4：shell_run / git_exec / git_diff / git_apply
 * 高危不入聊天回路（留给任务管线审批链）。
 * 本地文件读写 + 联网检索 + 数学外挂构成超级智能体的日常能力：
 * fs_read/fs_write/fs_list/fs_glob/fs_grep/fs_edit/fs_delete +
 * web_search/web_fetch + maths_eval/maths_stats/maths_plot。 */
const char *cli_chat_tools_json =
    "[" AIRY_TOOL_JSON_FS_READ "," AIRY_TOOL_JSON_FS_WRITE ","
    AIRY_TOOL_JSON_FS_LIST "," AIRY_TOOL_JSON_FS_GLOB ","
    AIRY_TOOL_JSON_FS_GREP "," AIRY_TOOL_JSON_FS_EDIT ","
    AIRY_TOOL_JSON_FS_DELETE "," AIRY_TOOL_JSON_WEB_SEARCH ","
    AIRY_TOOL_JSON_WEB_FETCH "," AIRY_TOOL_JSON_MATHS_EVAL ","
    AIRY_TOOL_JSON_MATHS_STATS "," AIRY_TOOL_JSON_MATHS_PLOT "]";

/* 动态消息缓冲：工具轮需要逐步追加 assistant tool_calls 与 role="tool"
 * 结果消息。content / tool_call_id / tool_calls_json 的副本由 owned 数组
 * 统一管理，msgbuf_free 时一并释放。（类型定义在 cli_internal.h，供
 * cli_chat.c 的 cli_chat_reply 直接声明栈上实例。） */
void cli_msgbuf_free(cli_chat_msgbuf_t *b)
{
    if (!b)
        return;
    for (size_t i = 0; i < b->owned_count; i++)
        AIRY_FREE(b->owned[i]);
    AIRY_FREE(b->owned);
    AIRY_FREE(b->msgs);
    AIRY_MEMSET(b, 0, sizeof(*b));
}

static const char *cli_msgbuf_own(cli_chat_msgbuf_t *b, const char *s)
{
    if (!s)
        return NULL;
    char *copy = AIRY_STRDUP(s);
    if (!copy)
        return NULL;
    if (b->owned_count >= b->owned_cap) {
        size_t new_cap = b->owned_cap ? b->owned_cap * 2 : 16;
        char **grown = (char **)AIRY_REALLOC(b->owned, new_cap * sizeof(char *));
        if (!grown) {
            AIRY_FREE(copy);
            return NULL;
        }
        b->owned = grown;
        b->owned_cap = new_cap;
    }
    b->owned[b->owned_count++] = copy;
    return copy;
}

void cli_msgbuf_push(cli_chat_msgbuf_t *b, const char *role, const char *content,
                     const char *tool_call_id, const char *tool_calls_json,
                     const char *reasoning_content)
{
    if (b->count >= b->cap) {
        size_t new_cap = b->cap ? b->cap * 2 : 16;
        llm_message_t *grown = (llm_message_t *)AIRY_REALLOC(b->msgs, new_cap * sizeof(llm_message_t));
        if (!grown)
            return;
        b->msgs = grown;
        b->cap = new_cap;
    }
    llm_message_t *m = &b->msgs[b->count++];
    m->role = role;
    m->content = cli_msgbuf_own(b, content);
    m->tool_call_id = cli_msgbuf_own(b, tool_call_id);
    m->tool_calls_json = cli_msgbuf_own(b, tool_calls_json);
    /* DeepSeek thinking mode requires the assistant turn's reasoning_content
     * to be echoed verbatim on tool-loop re-send (else HTTP 400). */
    m->reasoning_content = cli_msgbuf_own(b, reasoning_content);
}

/* 工具执行主体：CLI 聊天回路的默认身份（ACL permission_rules.yaml 的
 * coding_v1 标准工作集）。tool_d 的 execute RPC 按该主体做 fail-closed
 * 权限判定，未显式 allow 的工具一律拒绝。 */
#define CLI_TOOL_AGENT "coding_v1"
#define CLI_TOOL_RPC_TIMEOUT_MS 30000

/* 原地规范化绝对路径：空段与 "." 丢弃，".." 回退上一段，其余段保留。
 * dst 恒 ≤ 当前段起点，故边读边写不破坏未消费内容。 */
static void path_trim_dots(char *path)
{
    char *dst = path;
    const char *p = path;

    while (*p) {
        while (*p == '/')
            p++;
        const char *seg = p;
        while (*p && *p != '/')
            p++;
        size_t len = (size_t)(p - seg);

        if (len == 0)
            break;
        if (len == 1 && seg[0] == '.')
            continue;
        if (len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (dst > path + 1) {
                dst -= 1;
                while (dst > path && dst[-1] != '/')
                    dst -= 1;
            }
            continue;
        }
        dst[0] = '/';
        AIRY_MEMCPY(dst + 1, seg, len);
        dst += 1 + len;
    }
    *dst = '\0';
}

/* 相对路径绝对化：tool_d 在 daemon 启动目录解析相对路径，与 CLI 的 cwd
 * 不一致（2026-08-16 实测：模型传 "test_edit.txt" 被 tool_d 按自己的 cwd
 * 解析失败）。CLI 是用户会话的语义边界——用户说"当前目录"就是 CLI 的
 * cwd，故将 fs 工具的路径参数基于 CLI cwd 转为绝对路径（并规范化
 * "." / ".." 段）。 */
static void cli_tool_absolutize_path(cJSON *args, const char *key)
{
    cJSON *v = cJSON_GetObjectItem(args, key);
    if (!v || !cJSON_IsString(v) || !v->valuestring || !v->valuestring[0])
        return;
    const char *p = v->valuestring;
    if (p[0] == '/')
        return;
#if !defined(_WIN32)
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd)))
        return;
    size_t cl = strlen(cwd);
    size_t pl = strlen(p);
    if (cl + pl + 2 > sizeof(cwd))
        return;
    char out[4096];
    snprintf(out, sizeof(out), "%s/%s", cwd, p);
    path_trim_dots(out);
    if (out[0] == '\0')
        snprintf(out, sizeof(out), "%s/", cwd);
    cJSON_SetValuestring(v, out);
#else
    (void)p; /* Windows 平台：保持原样（daemon 与 CLI 同盘时语义一致） */
#endif
}

/* 统一工具失败载荷（0.1.19 N1 L2）：失败必须携带 err 三元组供上层按
 * class 分诊；用 cJSON 构造，避免原文含引号破坏 JSON（原 snprintf 拼接
 * 在网关文案带引号时会产出非法 JSON）。 */
static char *chat_tool_fail(airy_err_t code, const char *diag)
{
    cJSON *r = cJSON_CreateObject();
    if (!r)
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"out of memory\"}");
    cJSON_AddBoolToObject(r, "ok", 0);
    cJSON_AddStringToObject(r, "error", diag);
    airy_err_json_put(r, airy_err_class_of(code), code, diag, NULL);
    char *s = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return s ? s : AIRY_STRDUP("{\"ok\":false,\"error\":\"out of memory\"}");
}

static char *chat_tool_deny(airy_err_t code, const char *diag, int *out_ok)
{
    if (out_ok)
        *out_ok = 0;
    return chat_tool_fail(code, diag);
}

/* fs 工具的路径参数按 CLI cwd 绝对化：fs_glob 取 base（模型常省略，缺省
 * 补 CLI cwd），其余 fs_* 取 path；非 fs 工具不动参数（key 缺失即 no-op）。 */
static void chat_fs_paths(const char *id, cJSON *pargs)
{
    if (strncmp(id, "fs_", 3) != 0)
        return;
    int glob = strcmp(id, "fs_glob") == 0;
    cli_tool_absolutize_path(pargs, glob ? "base" : "path");
    if (!glob || cJSON_GetObjectItem(pargs, "base"))
        return;
#if !defined(_WIN32)
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)))
        cJSON_AddStringToObject(pargs, "base", cwd);
#endif
}

static char *chat_params(const char *id, const char *args_json, airy_err_t *perr)
{
    cJSON *pargs = cJSON_Parse(args_json);
    cJSON *params = pargs ? cJSON_CreateObject() : NULL;
    if (!pargs || !params) {
        /* 0.1.14 诊断增强：解析失败记录 cJSON 错误位置与参数前缀，
         * 定位上游（模型输出/流帧截断）问题。 */
        const char *epos = pargs ? NULL : cJSON_GetErrorPtr();
        if (epos)
            cli_trace("chat", "%s args parse fail len=%zu err@%d head=%.*s", CLI_ICON_CROSS,
                      strlen(args_json), (int)(epos - args_json),
                      (int)cli_utf8_safe_len(args_json, 100), args_json);
        *perr = pargs ? AIRY_ERR_OUT_OF_MEMORY : AIRY_ERR_INVALID_PARAM;
        cJSON_Delete(pargs);
        cJSON_Delete(params);
        return NULL;
    }
    cJSON_AddStringToObject(params, "tool_id", id);
    chat_fs_paths(id, pargs);
    cJSON_AddItemToObject(params, "params", pargs);
    cJSON_AddStringToObject(params, "agent_id", CLI_TOOL_AGENT);
    char *s = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!s)
        *perr = AIRY_ERR_OUT_OF_MEMORY;
    return s;
}

static void chat_fill(cJSON *r, cJSON *root, int ok)
{
    cJSON *out = cJSON_GetObjectItem(root, "output");
    if (ok && out && cJSON_IsString(out) && out->valuestring) {
        cJSON_AddStringToObject(r, "output", out->valuestring);
        return;
    }
    cJSON *err = cJSON_GetObjectItem(root, "error");
    cJSON_AddStringToObject(r, "error",
                            (err && cJSON_IsString(err) && err->valuestring)
                                ? err->valuestring
                                : "tool failed");
    /* L0 产出的 err 三元组原样透传，上层据此分诊（N1 契约）。 */
    cJSON *eobj = cJSON_GetObjectItem(root, "err");
    cJSON *dup = cJSON_IsObject(eobj) ? cJSON_Duplicate(eobj, 1) : NULL;
    if (dup)
        cJSON_AddItemToObject(r, "err", dup);
}

static char *chat_reply(const char *result_json, int *ok)
{
    cJSON *root = cJSON_Parse(result_json);
    if (!root)
        return NULL;
    cJSON *succ = cJSON_GetObjectItem(root, "success");
    /* tool_d 返回 "success":1（数字），cJSON_IsTrue 只认 true 字面量；
     * 数字与布尔都按非 0 判定（2026-08-16 实测误判 ok=0）。 */
    *ok = (succ && (cJSON_IsTrue(succ) || (cJSON_IsNumber(succ) && succ->valueint != 0))) ? 1 : 0;
    cJSON *r = cJSON_CreateObject();
    char *out = NULL;
    if (r) {
        cJSON_AddBoolToObject(r, "ok", *ok);
        chat_fill(r, root, *ok);
        out = cJSON_PrintUnformatted(r);
        cJSON_Delete(r);
    }
    cJSON_Delete(root);
    return out;
}

/* 执行一个工具调用，返回回填模型的 result JSON（OWNER，AIRY_FREE）。
 * 架构约束（2026-08-25）：统一经 gateway 派发（tool.execute → gateway →
 * SYS_SVC_CALL → tool_d，带 ACL fail-closed），与 gateway agent.run 同一
 * 路径；不直连 tool.sock / builtin 库（那会绕过权限边界）。 */
static char *cli_chat_exec_tool(const char *tool_id, const char *args_json, int *out_ok)
{
    if (!tool_id || !args_json)
        return chat_tool_deny(AIRY_ERR_INVALID_PARAM, "missing tool arguments", out_ok);

    airy_err_t perr = AIRY_ERR_OUT_OF_MEMORY;
    char *params_json = chat_params(tool_id, args_json, &perr);
    if (!params_json) {
        return chat_tool_deny(perr,
                              perr == AIRY_ERR_INVALID_PARAM ? "invalid tool arguments"
                                                             : "out of memory",
                              out_ok);
    }

    char *result_json = NULL;
    int rpc_ret = cli_gw_call("tool.execute", params_json, CLI_TOOL_RPC_TIMEOUT_MS,
                              &result_json);
    AIRY_FREE(params_json);
    if (rpc_ret != 0 || !result_json) {
        cli_trace("chat", "%s tool rpc fail tool=%s ret=%d (via gateway)", CLI_ICON_CROSS,
                  tool_id, rpc_ret);
        AIRY_FREE(result_json);
        /* cli_gw_call 已返回真实 AIRY 码（0.1.19 N1）：此处只补人类可读
         * 原文，网关未应答与工具业务失败由 class 分诊，不再统一误报不可达。 */
        const char *gw_err = cli_gw_last_err();
        return chat_tool_deny(rpc_ret != 0 ? rpc_ret : AIRY_ERR_PROTOCOL,
                              gw_err[0] ? gw_err : "tool rpc failed (via gateway)", out_ok);
    }

    /* daemon_rpc_call 已解包 JSON-RPC 的 result 字段：result_json 即
     * {"success":..,"output":"..","error":"..","exit_code":..}（2026-08-16
     * 实测：误以为带外层 "result" 导致解析失败，工具全部报 ✗）。 */
    int ok = 0;
    char *out_json = chat_reply(result_json, &ok);
    AIRY_FREE(result_json);
    if (!out_json)
        return chat_tool_deny(AIRY_ERR_PROTOCOL, "tool response parse failed", out_ok);

    cli_trace("chat", "%s tool exec tool=%s ok=%d resp=%.*s", ok ? CLI_ICON_CHECK : CLI_ICON_CROSS,
              tool_id, ok, (int)cli_utf8_safe_len(out_json, 120), out_json ? out_json : "");
    if (out_ok)
        *out_ok = ok;
    return out_json;
}

/* ---- 工具过程直印（0.1.19 自 airy_cli_output.c 迁入消费点） ----
 *
 * 0.1.18 B3（V3.4）：未登记工具标识符不得越过渲染边界进入用户面——
 * 白名单外一律显示兜底串并记日志，禁止原样透传。与 TUI 侧
 * app/dispatch.rs::tool_action 同源同语义；机制跟消费走，render 面不再
 * 持有工具领域知识。 */

static const char CLI_TOOL_HIDDEN[] = "未知工具（已隐藏）";

static const char *cli_tool_action(const char *name)
{
    static const struct { const char *tool; const char *action; } map[] = {
        {"web_search", "搜索网络"}, {"web_fetch", "抓取网页"},
        {"fs_read", "读取文件"},   {"fs_write", "写入文件"},
        {"fs_list", "列出目录"},   {"fs_ls", "列出目录"},
        {"fs_info", "查看文件信息"}, {"fs_mkdir", "创建目录"},
        {"fs_rm", "删除文件"},     {"agent.spawn", "派生智能体"},
        {"agent.invoke", "调用智能体"}, {"think.depth", "深度思考"},
        {"memory.get", "读取记忆"}, {"memory.put", "写入记忆"},
    };
    if (!name || !name[0])
        return CLI_TOOL_HIDDEN;
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (strcmp(name, map[i].tool) == 0)
            return map[i].action;
    }
    /* 只记长度、不记标识符本身：日志不应成为第二处泄漏面（与 TUI 侧一致）。 */
    AIRY_LOG_WARN("airy_cli: 未登记工具标识符已按白名单隐藏 (len=%zu)", strlen(name));
    return CLI_TOOL_HIDDEN;
}

/* 失败摘要：常见网络错误映射为可读中文，其余取首行（plain 直印单行语义）。 */
static void cli_tool_error_summary(const char *raw, char *out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!raw || !raw[0]) return;
    const char *p = raw;
    if (strncmp(p, "Web fetch failed", 16) == 0) {
        const char *close = strchr(p, ')');
        if (close && close[1] == ':') p = close + 2;
        while (*p == ' ') p++;
    }
    const char *mapped = NULL;
    if (strstr(p, "timed out") || strstr(p, "Timeout") || strstr(p, "timeout"))
        mapped = "连接超时";
    else if (strstr(p, "refused") || strstr(p, "Failed to connect") || strstr(p, "couldn't connect"))
        mapped = "连接被拒绝";
    else if (strstr(p, "Could not resolve") || strstr(p, "getaddrinfo") || strstr(p, "resolve host"))
        mapped = "无法解析域名";
    else if (strstr(p, "404") || strstr(p, "Not Found"))
        mapped = "页面不存在 (404)";
    if (mapped) {
        snprintf(out, cap, "%s", mapped);
        return;
    }
    size_t o = 0;
    for (; p[o] && o + 1 < cap; o++) {
        if (p[o] == '\n' || p[o] == '\r') break;
        out[o] = p[o];
    }
    out[o] = '\0';
}

/* 工具过程行：-p 走 stderr trace（stdout 留给最终答案，脚本可解析）；
 * 交互 plain 直印（无 icon 无色，0.1.19 体验面清退）。 */
static void cli_print_tool(const char *name, const char *detail, int ok, int done)
{
    const char *action = cli_tool_action(name);
    char line[192];
    if (!done)
        snprintf(line, sizeof(line), "%s…", action);
    else if (ok || !detail || !detail[0])
        snprintf(line, sizeof(line), "%s%s", action, ok ? " 完成" : " 失败");
    else
        snprintf(line, sizeof(line), "%s — %s", action, detail);
    cli_trace("tool", "%s", line);
    if (!g_cli_print_mode)
        cli_print_line("Tool", line);
}

/* 从 JSON 对象取字符串字段；缺键或类型不符返回 NULL。 */
static const char *cli_json_str(cJSON *o, const char *key)
{
    cJSON *it = o ? cJSON_GetObjectItem(o, key) : NULL;
    return (it && cJSON_IsString(it)) ? it->valuestring : NULL;
}

/* 结果折叠摘要：ok 取 output、失败取 error。树必须在直印之后释放
 * （UAF 修复 2026-08-16：detail 指向 cJSON 树内字符串）。 */
static void cli_tool_report(const char *name, const char *json, int ok)
{
    char err_sum[128] = "";
    const char *detail = NULL;
#ifdef AIRY_HAS_CJSON
    cJSON *root = cJSON_Parse(json);
    if (root)
        detail = ok ? cli_json_str(root, "output") : cli_json_str(root, "error");
#else
    detail = ok ? NULL : json;
#endif
    if (!ok)
        cli_tool_error_summary(detail, err_sum, sizeof(err_sum));
    cli_print_tool(name, err_sum, ok, 1);
#ifdef AIRY_HAS_CJSON
    if (root)
        cJSON_Delete(root);
#endif
}

/* 解析一轮 LLM 返回的 tool_calls，渲染 + 执行 + 回填消息缓冲。
 * 返回 0 = 本轮已消费（调用方应继续下一轮）；-1 = 终止（解析失败/无工具）。 */
int cli_chat_tool_round(cli_chat_msgbuf_t *b, const llm_response_t *resp)
{
    if (!resp || resp->choice_count == 0 || !resp->choices[0].tool_calls_json)
        return -1;
    cJSON *calls = cJSON_Parse(resp->choices[0].tool_calls_json);
    if (!calls || !cJSON_IsArray(calls)) {
        if (calls)
            cJSON_Delete(calls);
        return -1;
    }
    if (cJSON_GetArraySize(calls) == 0) {
        cJSON_Delete(calls);
        return -1;
    }

    /* assistant 消息：保留 content 与 reasoning_content，附 tool_calls
     * （OpenAI 续轮必需 tool_calls；DeepSeek thinking 缺 reasoning 报 400）。 */
    const llm_message_t *ch = &resp->choices[0];
    cli_trace("chat", "tool-round reasoning=%s tools=%zu",
              ch->reasoning_content ? "yes" : "no", strlen(ch->tool_calls_json));
    cli_msgbuf_push(b, "assistant", ch->content, NULL, ch->tool_calls_json,
                    ch->reasoning_content);

    cJSON *call = NULL;
    cJSON_ArrayForEach(call, calls)
    {
        cJSON *fn = cJSON_GetObjectItem(call, "function");
        const char *name = cli_json_str(fn, "name");
        const char *args = cli_json_str(fn, "arguments");
        const char *call_id = cli_json_str(call, "id");
        name = name ? name : "unknown";
        args = args ? args : "{}";
        /* 过程行只出动作名；参数与返回内容经 cli_trace 留档供诊断。 */
        cli_trace("chat", "tool call %s args=%.*s", name,
                  (int)cli_utf8_safe_len(args, 160), args);
        cli_print_tool(name, NULL, 0, 0);

        int ok = 0;
        char *rj = cli_chat_exec_tool(name, args, &ok);
        if (!rj)
            rj = AIRY_STRDUP("{\"ok\":false,\"error\":\"tool failed\"}");
        cli_tool_report(name, rj, ok);

        /* role="tool" 回填：带匹配 tool_call_id，超长按 UTF-8 边界截断。 */
        char trunc_buf[CLI_CHAT_TOOL_RESULT_CAP + 1];
        size_t n = cli_utf8_safe_len(rj, CLI_CHAT_TOOL_RESULT_CAP);
        AIRY_MEMCPY(trunc_buf, rj, n);
        trunc_buf[n] = '\0';
        cli_msgbuf_push(b, "tool", trunc_buf,
                        call_id ? call_id : "call_unknown", NULL, NULL);
        AIRY_FREE(rj);
    }
    cJSON_Delete(calls);
    return 0;
}

#endif /* AIRY_HAS_CJSON */
