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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

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

/* OpenAI function-calling schema（聊天工具回路）。工具行为 SSoT 在 tool_d
 * （builtin.c 真实实现）；本 schema 与 commons 契约层
 * （commons/include/airy_tool_schema.h）保持同构（2026-08-16 对齐）。
 * 本地文件读写 + 联网检索构成超级智能体的日常能力：
 * fs_read/fs_write/fs_list/fs_glob/fs_grep/fs_edit/fs_delete + web_search/web_fetch。
 * shell_run / git_* 不入聊天回路（高危，留给任务管线审批链）。 */
const char *cli_chat_tools_json =
    "["
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_read\","
    "\"description\":\"Read a file's content from the local filesystem\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_write\","
    "\"description\":\"Write content to a local file (creates or overwrites)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},"
    "\"required\":[\"path\",\"content\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_list\","
    "\"description\":\"List entries of a local directory (JSON array)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"}},\"required\":[]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_glob\","
    "\"description\":\"List files matching a glob pattern (supports * ? and **)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"pattern\":{\"type\":\"string\"},\"base\":{\"type\":\"string\"}},"
    "\"required\":[\"pattern\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_grep\","
    "\"description\":\"Search file contents with a regular expression (relpath:line:text)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"pattern\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"},"
    "\"glob\":{\"type\":\"string\"},\"max_results\":{\"type\":\"integer\"}},"
    "\"required\":[\"pattern\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_edit\","
    "\"description\":\"Replace an exact string in a file (search-and-replace edit)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},\"old\":{\"type\":\"string\"},"
    "\"new\":{\"type\":\"string\"},\"count\":{\"type\":\"integer\"}},"
    "\"required\":[\"path\",\"old\",\"new\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_delete\","
    "\"description\":\"Delete a local file, or a directory (recursive=1 for "
    "non-empty trees; destructive)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},\"recursive\":{\"type\":\"boolean\"}},"
    "\"required\":[\"path\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"web_search\","
    "\"description\":\"Search the web for up-to-date information relevant to "
    "the user's question. Returns ranked result titles, URLs and snippets.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"query\":{\"type\":\"string\",\"description\":\"Search query\"},"
    "\"max_results\":{\"type\":\"integer\",\"description\":\"Max result count, "
    "1-8\",\"minimum\":1,\"maximum\":8}},"
    "\"required\":[\"query\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"web_fetch\","
    "\"description\":\"Fetch and read the text content of a web page by URL.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"url\":{\"type\":\"string\",\"description\":\"Full http(s) URL\"}},"
    "\"required\":[\"url\"]}}}"
    "]";

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
    /* 逐段规范化：空段与 "." 跳过；".." 回退上一段（cwd 为绝对路径）；*/
    /* 其余段原样保留。dst 指向当前写入位置，回退即移回上一 '/'。 */
    char *dst = out;
    const char *src = out;
    for (;;) {
        const char *seg = src;
        const char *slash = strchr(seg, '/');
        size_t seg_len = slash ? (size_t)(slash - seg) : strlen(seg);
        if (seg_len == 0 || (seg_len == 1 && seg[0] == '.')) {
            src = slash ? slash + 1 : seg + seg_len;
            if (!slash)
                break;
            continue;
        }
        if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (dst > out + 1) {
                dst -= 1;
                while (dst > out && dst[-1] != '/')
                    dst -= 1;
            }
            src = slash ? slash + 1 : seg + seg_len;
            if (!slash)
                break;
            continue;
        }
        dst[0] = '/';
        __builtin_memcpy(dst + 1, seg, seg_len);
        dst += 1 + seg_len;
        src = slash ? slash + 1 : seg + seg_len;
        if (!slash)
            break;
    }
    *dst = '\0';
    if (out[0] == '\0')
        snprintf(out, sizeof(out), "%s/", cwd);
    cJSON_SetValuestring(v, out);
#else
    (void)p; /* Windows 平台：保持原样（daemon 与 CLI 同盘时语义一致） */
#endif
}

/* 执行一个工具调用，返回回填模型的 result JSON（OWNER，AIRY_FREE）。
 * 架构约束（2026-08-25）：统一经 gateway 派发（tool.execute → gateway →
 * SYS_SVC_CALL → tool_d，带 ACL fail-closed），与 gateway agent.run 同一
 * 路径；不直连 tool.sock / builtin 库（那会绕过权限边界）。 */
static char *cli_chat_exec_tool(const char *tool_id, const char *args_json, int *out_ok)
{
    if (!tool_id || !args_json) {
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"missing tool arguments\"}");
    }

    cJSON *params = cJSON_CreateObject();
    if (!params) {
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"out of memory\"}");
    }
    cJSON_AddStringToObject(params, "tool_id", tool_id);
    cJSON *pargs = cJSON_Parse(args_json);
    if (!pargs) {
        cJSON_Delete(params);
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"invalid tool arguments\"}");
    }
    /* 相对路径绝对化（基于 CLI cwd；tool_d 按自身 cwd 解析）：
     * fs 工具的 path 参数与 fs_glob 的 base 参数。 */
    if (strcmp(tool_id, "fs_read") == 0 || strcmp(tool_id, "fs_write") == 0 ||
        strcmp(tool_id, "fs_list") == 0 || strcmp(tool_id, "fs_edit") == 0 ||
        strcmp(tool_id, "fs_grep") == 0 || strcmp(tool_id, "fs_delete") == 0) {
        cli_tool_absolutize_path(pargs, "path");
    } else if (strcmp(tool_id, "fs_glob") == 0) {
        cli_tool_absolutize_path(pargs, "base");
        if (!cJSON_GetObjectItem(pargs, "base")) {
            char cwd[4096];
#if !defined(_WIN32)
            if (getcwd(cwd, sizeof(cwd)))
                cJSON_AddStringToObject(pargs, "base", cwd);
#else
            (void)cwd;
#endif
        }
    }
    cJSON_AddItemToObject(params, "params", pargs);
    cJSON_AddStringToObject(params, "agent_id", CLI_TOOL_AGENT);
    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json) {
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"out of memory\"}");
    }

    char *result_json = NULL;
    int rpc_ret = cli_gw_call("tool.execute", params_json, CLI_TOOL_RPC_TIMEOUT_MS,
                              &result_json);
    AIRY_FREE(params_json);
    if (rpc_ret != 0 || !result_json) {
        cli_trace("chat", "%s tool rpc fail tool=%s ret=%d (via gateway)", CLI_ICON_CROSS,
                  tool_id, rpc_ret);
        AIRY_FREE(result_json);
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"tool_d unreachable (via gateway)\"}");
    }

    /* daemon_rpc_call 已解包 JSON-RPC 的 result 字段：result_json 即
     * {"success":..,"output":"..","error":"..","exit_code":..}（2026-08-16
     * 实测：误以为带外层 "result" 导致解析失败，工具全部报 ✗）。 */
    cJSON *root = cJSON_Parse(result_json);
    AIRY_FREE(result_json);
    int ok = 0;
    char *out_json = NULL;
    if (root) {
        cJSON *succ = cJSON_GetObjectItem(root, "success");
        /* tool_d 返回 "success":1（数字），cJSON_IsTrue 只认 true 字面量；
         * 数字与布尔都按非 0 判定（2026-08-16 实测误判 ok=0）。 */
        ok = (succ && (cJSON_IsTrue(succ) || (cJSON_IsNumber(succ) && succ->valueint != 0))) ? 1
                                                                                            : 0;
        cJSON *out = cJSON_GetObjectItem(root, "output");
        cJSON *err = cJSON_GetObjectItem(root, "error");
        cJSON *r = cJSON_CreateObject();
        cJSON_AddBoolToObject(r, "ok", ok);
        if (ok && out && cJSON_IsString(out) && out->valuestring) {
            cJSON_AddStringToObject(r, "output", out->valuestring);
        } else {
            cJSON_AddStringToObject(r, "error",
                                    (err && cJSON_IsString(err) && err->valuestring)
                                        ? err->valuestring
                                        : "tool failed");
        }
        out_json = cJSON_PrintUnformatted(r);
        cJSON_Delete(r);
        cJSON_Delete(root);
    }
    if (!out_json)
        out_json = AIRY_STRDUP("{\"ok\":false,\"error\":\"tool response parse failed\"}");
    cli_trace("chat", "%s tool exec tool=%s ok=%d resp=%.*s", ok ? CLI_ICON_CHECK : CLI_ICON_CROSS,
              tool_id, ok, (int)cli_utf8_safe_len(out_json, 120), out_json ? out_json : "");
    if (out_ok)
        *out_ok = ok;
    return out_json;
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
    size_t n = (size_t)cJSON_GetArraySize(calls);
    if (n == 0) {
        cJSON_Delete(calls);
        return -1;
    }

    /* assistant 消息：保留原 content 与 reasoning_content，附 tool_calls
     * （OpenAI 要求续轮必需 tool_calls；DeepSeek thinking 要求回传
     * reasoning_content，否则 tool 续轮 HTTP 400） */
    cli_trace("chat", "tool-round reasoning=%s tools=%zu",
              resp->choices[0].reasoning_content ? "yes" : "no",
              strlen(resp->choices[0].tool_calls_json));
    cli_msgbuf_push(b, "assistant", resp->choices[0].content, NULL,
                    resp->choices[0].tool_calls_json,
                    resp->choices[0].reasoning_content);

    cJSON *call = NULL;
    cJSON_ArrayForEach(call, calls)
    {
        cJSON *fn = cJSON_GetObjectItem(call, "function");
        cJSON *id = cJSON_GetObjectItem(call, "id");
        const char *name = fn && cJSON_IsObject(fn) ? cJSON_GetObjectItem(fn, "name")->valuestring : NULL;
        const char *args = fn && cJSON_IsObject(fn) ? cJSON_GetObjectItem(fn, "arguments")->valuestring : NULL;
        const char *call_id = (id && cJSON_IsString(id)) ? id->valuestring : "call_unknown";
        if (!name)
            name = "unknown";

        /* 工具调用过程卡片：⚙ 动作名…（参数/返回内容不暴露在对话中；
         * 操作细节经 cli_trace 留档供 -p 管道与日志诊断）。 */
        cli_trace("chat", "tool call %s args=%.*s", name, (int)cli_utf8_safe_len(args, 160), args ? args : "{}");
        cli_render_tool_use(name, args);

        int ok = 0;
        char *result_json = cli_chat_exec_tool(name, args ? args : "{}", &ok);
        if (!result_json)
            result_json = AIRY_STRDUP("{\"ok\":false,\"error\":\"tool failed\"}");

        /* 折叠摘要渲染（全量文本已回填模型上下文） */
        const char *detail = NULL;
#ifdef AIRY_HAS_CJSON
        cJSON *rroot = cJSON_Parse(result_json);
        if (rroot) {
            cJSON *out = cJSON_GetObjectItem(rroot, "output");
            cJSON *err = cJSON_GetObjectItem(rroot, "error");
            detail = (ok && out && cJSON_IsString(out)) ? out->valuestring
                     : (err && cJSON_IsString(err)) ? err->valuestring
                                                    : NULL;
        }
#else
        detail = ok ? NULL : result_json;
#endif
        /* UAF 修复（2026-08-16，ASan heap-use-after-free）：detail 指向
         * cJSON 树内字符串（out->valuestring），必须在渲染消费完之后再
         * 释放树；此前 cJSON_Delete(rroot) 先于 cli_render_tool_result
         * 执行，工具回路偶发崩溃。 */
        cli_render_tool_result(name, detail, ok);
#ifdef AIRY_HAS_CJSON
        if (rroot)
            cJSON_Delete(rroot);
#endif

        /* role="tool" 消息回填：携带匹配的 tool_call_id；超长结果在 UTF-8
         * 字符边界截断，防止上下文无界膨胀（web_fetch 页面可能很大）。 */
        char trunc_buf[CLI_CHAT_TOOL_RESULT_CAP + 1];
        const char *feed = result_json;
        size_t rl = strlen(result_json);
        if (rl > CLI_CHAT_TOOL_RESULT_CAP) {
            size_t n = CLI_CHAT_TOOL_RESULT_CAP;
            while (n > 0 && ((unsigned char)result_json[n] & 0xC0) == 0x80)
                n--;
            AIRY_MEMCPY(trunc_buf, result_json, n);
            trunc_buf[n] = '\0';
            feed = trunc_buf;
        }
        cli_msgbuf_push(b, "tool", feed, call_id, NULL, NULL);
        AIRY_FREE(result_json);
    }
    cJSON_Delete(calls);
    return 0;
}

#endif /* AIRY_HAS_CJSON */
