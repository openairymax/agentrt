/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_gw_rpc.c
 * @brief 网关客户端 JSON-RPC 层：信封构造、result 提取、失败分诊。
 *
 * 分层（0.1.19 §452 拆分）：传输（HTTP/1.1 报文搬运）在 cli_gw.c，本文件
 * 只负责 JSON-RPC 语义与错误可读化。失败一律返回真实 AIRY_ERR_*（0.1.19
 * §3.7.2 N1），呈现层按 class 分诊，不再靠字符串嗅探；g_cli_gw_err 保留
 * 可读原文，二者互补。
 */

#include "airy_memory.h"
#include "airy_run_stream.h" /* D-10：失败载荷键名唯一权威 */
#include "cli_gw.h"
#include "cli_gw_int.h"
#include "cli_gw_json.h"
#include "error_codes.h" /* AIRY_ERR_CANCELED（S-02：取消语义与 sched.dag_cancel 对齐） */
#include "logger.h"

#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 网关 RPC 客户端错误描述缓冲（本模块唯一权威；外部经 cli_gw_last_err /
 * cli_gw_err_take / cli_gw_err_set 访问，禁止跨 TU 直接引用）。 */
static char g_cli_gw_err[256] = "";

const char *cli_gw_last_err(void)
{
    return g_cli_gw_err;
}

const char *cli_gw_err_take(void)
{
    static char taken[256];
    snprintf(taken, sizeof(taken), "%s", g_cli_gw_err);
    g_cli_gw_err[0] = '\0';
    return taken;
}

void cli_gw_err_set(const char *msg)
{
    snprintf(g_cli_gw_err, sizeof(g_cli_gw_err), "%s", msg ? msg : "");
}

/* N-3（0.1.16）：provider 400/422（请求体非法，典型是消息含无效 UTF-8 →
 * "invalid unicode code point"）在 llm_d 已归入 AIRY_ERR_LLM_BAD_REQUEST，
 * 但网关转发时把 daemon 错误码统一折叠为 -32603、只透传 message 文本，故
 * CLI 侧对 error.message 做特征分诊，避免把 provider 400 误报为"读写错误/
 * 网络错误"（R-1 已处理 401/403，R-5 已处理超时/连接失败，此处补齐 400/422）。
 * 返回：2=请求体被明确拒绝；1=provider 请求失败（请求体非法或不可达）；
 *       0=与 provider 无关。 */
static int cli_gw_provider_state(const char *m)
{
    if (!m || !m[0])
        return 0;
    static const char *const body_reject[] = {
        "Provider rejected the request body",
        "invalid unicode",
        "Failed to parse the request body",
        "request body as JSON",
        "invalid_request_error",
    };
    for (size_t i = 0; i < sizeof(body_reject) / sizeof(body_reject[0]); i++) {
        if (strstr(m, body_reject[i]))
            return 2;
    }
    if (strstr(m, "Network request to provider failed"))
        return 1;
    return 0;
}

/* ── JSON-RPC over HTTP POST / ─────────────────────────────────────── */
/* 请求体按实际长度动态构造：固定缓冲会在工具参数（如 fs_write 大内容）
 * 超过容量时静默截断，产生非法 JSON 导致调用失败（0.1.13 实机回归）。 */
static int cli_gw_body(const char *method, const char *params_json, char **out_body)
{
    size_t params_len = (params_json && *params_json) ? strlen(params_json) : 2;
    size_t blen = 64 + strlen(method) + params_len + 1;
    char *body = (char *)AIRY_MALLOC(blen);
    if (!body) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "请求缓冲分配失败（method=%s，%zu 字节）", method, blen);
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    int bn = snprintf(body, blen, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"%s\",\"params\":%s}",
                      method, (params_json && *params_json) ? params_json : "{}");
    if (bn > 0 && (size_t)bn < blen) {
        *out_body = body;
        return 0;
    }
    snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
             "请求信封构造失败（method=%s），参数过长", method);
    AIRY_FREE(body);
    return AIRY_ERR_INVALID_PARAM;
}

/* 传输层分诊：取消 / 超时 / 不可达各自成码，超时给出可诊断文案（S-02），
 * 不可达给出可执行提示。 */
static int cli_gw_xfer_err(int xerr, const char *method, int timeout_ms, int renewals,
                           const char *host, int port)
{
    if (xerr == CLI_GW_EXCH_CANCELED)
        return AIRY_ERR_CANCELED;
    if (xerr == CLI_GW_EXCH_TIMEOUT) {
        long long total_ms = (long long)timeout_ms * (renewals + 1);

        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关已连接但响应超时（等待 %lld 秒，method=%s）：服务端可能仍在推理，"
                 "可重试或排查模型负载",
                 total_ms / 1000, method);
        AIRY_LOG_WARN("cli_gw: response timeout after %lldms (method=%s) at %s:%d", total_ms,
                      method, host, port);
        return AIRY_ERR_TIMEOUT;
    }
    snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
             "网关不在线（%s:%d），请运行 airymaxrt 启动服务", host, port);
    AIRY_LOG_WARN("cli_gw: gateway unreachable at %s:%d (method=%s)", host, port, method);
    return AIRY_ERR_SYS_SOCKET;
}

/* C-10（0.1.21）：--verbose 打开后追加 daemon 原始错误 data；默认只给
 * 一行归因。定义在 main.c，此处只读。 */
extern int g_cli_verbose;

/* C-10（0.1.21）：stage 数字的中文呈现位。daemon 的 error.data 只带
 * grad_stage_e 数字与 err_detail，翻译放呈现层，不回头改协议。 */
static const char *const g_gw_stage_name[] = {
    "none", "input", "llm", "channel", "gen", "verify", "arbiter", "loop",
};

/* D-10（0.1.21）：失败语义中文呈现。daemon 已是唯一判据出处，CLI 只翻译，
 * 不改码、不推断；未知值原样透出，便于新原因上线后立即可见。 */
static const char *cli_fail_reason_name(const char *r)
{
    static const struct {
        const char *key;
        const char *name;
    } names[] = {
        {"budget_exhausted", "轮次预算耗尽"},
        {"tool_fuse", "工具连续失败已熔断"},
        {"approval_denied", "操作未获审批"},
        {"empty_result", "模型返回空结果"},
        {"provider_unreachable", "模型服务不可达"},
        {"agent_error", "执行失败"},
    };
    for (size_t i = 0; r && i < sizeof(names) / sizeof(names[0]); i++) {
        if (strcmp(r, names[i].key) == 0)
            return names[i].name;
    }
    return r;
}

/* 下一步动作 → 中文提示。 */
static const char *cli_fail_hint(const char *act)
{
    if (strcmp(act, AIRY_RS_NEXT_RETRY) == 0)
        return "可原样重试";
    if (strcmp(act, AIRY_RS_NEXT_RESUME) == 0)
        return "请用 continue 续跑，不要原样重发";
    return "人工排查后再试";
}

/* D-10：失败载荷 → 一行原因 + 一行下一步。返回原始 error code 供退出码使用。 */
static int cli_fail_render(const cJSON *root, char *out, size_t cap)
{
    cJSON *reason = cJSON_GetObjectItem(root, AIRY_RS_K_REASON);
    cJSON *code = cJSON_GetObjectItem(root, AIRY_RS_K_CODE);
    cJSON *used = cJSON_GetObjectItem(root, AIRY_RS_K_BUDGET_USED);
    cJSON *total = cJSON_GetObjectItem(root, AIRY_RS_K_BUDGET_TOTAL);
    cJSON *node = cJSON_GetObjectItem(root, AIRY_RS_K_NODE);
    cJSON *next = cJSON_GetObjectItem(root, AIRY_RS_K_NEXT_ACTION);
    int err = cJSON_IsNumber(code) ? (int)code->valuedouble : 0;
    const char *r = cJSON_IsString(reason) ? reason->valuestring : "";
    const char *n = cJSON_IsString(node) ? node->valuestring : "";
    const char *act = cJSON_IsString(next) ? next->valuestring : "";
    int off = snprintf(out, cap, "失败原因：%s", cli_fail_reason_name(r));

    if (off > 0 && (size_t)off < cap && cJSON_IsNumber(used) && cJSON_IsNumber(total))
        off += snprintf(out + off, cap - (size_t)off, "（预算 %d/%d）", (int)used->valuedouble,
                        (int)total->valuedouble);
    if (off > 0 && (size_t)off < cap && n[0])
        off += snprintf(out + off, cap - (size_t)off, "；影响节点 %s", n);
    if (off > 0 && (size_t)off < cap)
        snprintf(out + off, cap - (size_t)off, "；下一步：%s", cli_fail_hint(act));
    return err;
}

/* 旧式（无 reason 的结构化载荷）失败字段：一次取全，渲染与判空分治。 */
typedef struct {
    int stage;
    int err;
    unsigned rounds;
    unsigned rej;
    const char *node;
    const char *detail;
} cli_fail_f_t;

/* 数值键取值：缺失或非数值返回 0（不写 *v）。 */
static int cj_num(const cJSON *o, double *v)
{
    if (!o || !cJSON_IsNumber(o))
        return 0;
    *v = o->valuedouble;
    return 1;
}

/* 字符串键取值：缺失或非字符串按空串（调用方无需再判 NULL）。 */
static const char *cj_str(const cJSON *o)
{
    return (o && cJSON_IsString(o)) ? o->valuestring : "";
}

static void cli_fail_fields(cJSON *root, cli_fail_f_t *f)
{
    double v = 0;
    cJSON *stats = cJSON_GetObjectItem(root, "stats");

    f->stage = cj_num(cJSON_GetObjectItem(root, "stage"), &v) ? (int)v : 0;
    f->rounds = cj_num(cJSON_GetObjectItem(root, "grad_rounds"), &v) ? (unsigned)v : 0;
    f->rej = cj_num(cJSON_GetObjectItem(root, "grad_rejections"), &v) ? (unsigned)v : 0;
    f->err = cj_num(stats ? cJSON_GetObjectItem(stats, "err_code") : NULL, &v)
                 ? (int)v
                 : 0;
    f->node = cj_str(cJSON_GetObjectItem(root, "affected_node"));
    f->detail = cj_str(cJSON_GetObjectItem(root, "err_detail"));
}

/* 把旧式载荷渲染成一行：阶段名 + （轮次/驳回/节点）+ 细节或错误码。 */
static void cli_fail_render_leg(const cli_fail_f_t *f, char *out, size_t cap)
{
    int off;
    int st = f->stage;
    const char *sname = (st > 0 && (size_t)st < sizeof(g_gw_stage_name) / sizeof(g_gw_stage_name[0]))
                            ? g_gw_stage_name[st]
                            : "unknown";

    off = snprintf(out, cap, "失败于 %s", sname);
    if (off > 0 && (size_t)off < cap && (f->rounds || f->rej || f->node[0]))
        off += snprintf(out + off, cap - (size_t)off, "（第 %u 轮，驳回 %u 次%s%s）", f->rounds,
                        f->rej, f->node[0] ? "，节点 " : "", f->node);
    if (off <= 0 || (size_t)off >= cap)
        return;
    if (f->detail[0])
        snprintf(out + off, cap - (size_t)off, "：%s", f->detail);
    else if (f->err)
        snprintf(out + off, cap - (size_t)off, "：err=%d", f->err);
}

int cli_gw_fail_line(const char *data_json, char *out, size_t cap)
{
    if (out && cap)
        out[0] = '\0';
    if (!data_json || !data_json[0] || !out || cap == 0)
        return 0;

    cJSON *root = cJSON_Parse(data_json);
    if (!root)
        return 0;
    /* D-10：结构化失败载荷优先——它比 stage 更贴近调用方需要的决策。 */
    if (cJSON_IsString(cJSON_GetObjectItem(root, AIRY_RS_K_REASON))) {
        int rc = cli_fail_render(root, out, cap);
        cJSON_Delete(root);
        return rc;
    }
    cli_fail_f_t f;
    cli_fail_fields(root, &f);
    if (f.stage <= 0 && !f.detail[0] && f.err == 0) {
        cJSON_Delete(root);
        return 0;
    }
    cli_fail_render_leg(&f, out, cap);
    cJSON_Delete(root);
    return f.err;
}

/* provider 分诊结果的中文归因。 */
static const char *gw_provider_msg(int pr)
{
    return (pr == 2) ? "请求被模型服务拒绝（请求体非法，如消息含无效 UTF-8 字符）"
                     : "请求被模型服务拒绝或模型服务不可达（请检查消息内容与网络连通性）";
}

/* C-10（0.1.21）：-32603 只是网关的包装码，真实失败环在 error.data
 * 里（stage/rounds/rejections/affected_node/err_detail）。默认压成
 * 一行"失败在哪一环"；--verbose 再附原始 JSON。daemon 原始错误码
 * （如 -110 未收敛）原样返回，-32603 不覆盖根因，-31 只作退出码。 */
static int gw_err_internal(cJSON *err, const char *m, const char *home)
{
    cJSON *data = cJSON_GetObjectItem(err, "data");
    char *data_json = data ? cJSON_PrintUnformatted(data) : NULL;
    char line[320];
    int real = cli_gw_fail_line(data_json, line, sizeof(line));

    if (g_cli_verbose && data_json)
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err), "%s%s；原始错误 data：%s",
                 line[0] ? line : "网关服务内部错误", line[0] ? "" : m, data_json);
    else if (line[0])
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err), "%s", line);
    else
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关服务内部错误（%s），请查看 %s/logs/gateway_d.out",
                 m[0] ? m : "Internal error", home);
    AIRY_FREE(data_json);
    return real != 0 ? real : AIRY_ERR_GENERIC_FAIL;
}

/* JSON-RPC error 对象分诊：网关已应答即链路可达，绝不外溢为 transport（N1）。 */
static int cli_gw_rpc_err(cJSON *err, const char *method)
{
    cJSON *code = cJSON_GetObjectItem(err, "code");
    cJSON *msg = cJSON_GetObjectItem(err, "message");
    int c = code && cJSON_IsNumber(code) ? (int)code->valuedouble : 0;
    const char *m = msg && cJSON_IsString(msg) ? msg->valuestring : "";
    const char *home = getenv("AIRY_HOME") ? getenv("AIRY_HOME") : "~/.airymaxrt";
    int pr = cli_gw_provider_state(m);

    if (pr > 0) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err), "%s；网关原文：%s", gw_provider_msg(pr), m);
        return AIRY_ERR_GENERIC_FAIL;
    }
    if (c == -32603)
        return gw_err_internal(err, m, home);
    if (c == -32601) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err), "网关不支持该方法（%s）", method);
        return AIRY_ERR_NOT_SUPPORTED;
    }
    snprintf(g_cli_gw_err, sizeof(g_cli_gw_err), "网关服务异常（code=%d%s%s）", c, m[0] ? " " : "",
             m);
    return (c == -32600 || c == -32602) ? AIRY_ERR_INVALID_PARAM : AIRY_ERR_GENERIC_FAIL;
}

/* 提取 JSON-RPC result（忽略 id 字段）。 */
static int cli_gw_take(cJSON *root, const char *method, const char *host, int port,
                       char **out_result)
{
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!result) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关响应缺少 result 字段（method=%s），服务面实现异常", method);
        AIRY_LOG_WARN("cli_gw: response without result (method=%s) at %s:%d", method, host, port);
        return AIRY_ERR_PROTOCOL;
    }
    char *out = cJSON_PrintUnformatted(result);
    if (!out) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err), "网关响应序列化失败（method=%s）", method);
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    *out_result = out;
    return 0;
}

int cli_gw_call_q(const char *method, const char *params_json, int timeout_ms, int renewals,
                  char **out_result)
{
    if (!method || !out_result) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关调用参数非法（method%s，out_result%s）",
                 method ? " 已给出" : " 为空", out_result ? " 已给出" : " 为空");
        return AIRY_ERR_INVALID_PARAM;
    }
    *out_result = NULL;
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);

    char *body = NULL;
    int rc = cli_gw_body(method, params_json, &body);
    if (rc != 0)
        return rc;

    char *resp = NULL;
    size_t rlen = 0;
    int status = 0;
    int exchange_err = cli_gw_exchange("POST", host, port, "/", body, timeout_ms, renewals, &resp,
                                       &rlen, &status);
    AIRY_FREE(body);
    if (exchange_err != 0) {
        AIRY_FREE(resp);
        /* S-02：取消 / 超时 / 不可达分别在 cli_gw_xfer_err 中具名，
         * 描述由 cli_err_desc 统一渲染；各处调用面均按非 0 判失败。 */
        return cli_gw_xfer_err(exchange_err, method, timeout_ms, renewals, host, port);
    }
    if (!resp) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关响应为空，请检查 %s 服务状态", host);
        return AIRY_ERR_PROTOCOL;
    }
    /* 上游 5xx：网关自身或其后端不可用（典型为 daemon 重启期间的空体
     * 503）。此前状态行被丢弃，空体降级为"响应不是合法 JSON"，用户无法
     * 分辨"后端重启"与"协议损坏"；此处按 N1 传输层具名。 */
    if (status >= 500) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关上游不可用（HTTP %d，method=%s）：服务可能正在重启，请稍后重试",
                 status, method);
        AIRY_LOG_WARN("cli_gw: upstream unavailable (HTTP %d, method=%s) at %s:%d", status, method,
                      host, port);
        AIRY_FREE(resp);
        return AIRY_ERR_SVC_NOT_READY;
    }
    /* 提取 JSON-RPC result（忽略 id 字段） */
    cJSON *root = cJSON_Parse(resp);
    AIRY_FREE(resp);
    if (!root) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关响应不是合法 JSON（method=%s），请查看 %s/logs/gateway_d.out",
                 method, getenv("AIRY_HOME") ? getenv("AIRY_HOME") : "~/.airymaxrt");
        AIRY_LOG_WARN("cli_gw: malformed json response (method=%s) at %s:%d", method, host, port);
        return AIRY_ERR_PROTOCOL;
    }
    cJSON *err = cJSON_GetObjectItem(root, "error");
    if (err) {
        rc = cli_gw_rpc_err(err, method);
        cJSON_Delete(root);
        return rc;
    }
    rc = cli_gw_take(root, method, host, port, out_result);
    cJSON_Delete(root);
    return rc;
}

int cli_gw_call(const char *method, const char *params_json, int timeout_ms, char **out_result)
{
    return cli_gw_call_q(method, params_json, timeout_ms, 0, out_result);
}

/* ── /health 可达性 + 传输代次 ────────────────────────────────────── */
int cli_gw_probe(int timeout_ms, unsigned long long *epoch_out)
{
    if (epoch_out)
        *epoch_out = 0;
    char host[128];
    int port = 0;
    cli_gw_endpoint(host, sizeof(host), &port);
    char *resp = NULL;
    size_t rlen = 0;
    int status = 0;
    /* /health 仅支持 GET；此前误用 POST 得 404，导致网关健康时仍误报
     * "gateway unreachable"（v0.1.16 现场缺陷根因）。 */
    if (cli_gw_exchange("GET", host, port, "/health", NULL, timeout_ms, 0, &resp, &rlen,
                        &status) != 0) {
        AIRY_FREE(resp);
        return 0;
    }
    int ok = (status == 200 && resp && strstr(resp, "\"healthy\"")) ? 1 : 0;
    if (ok && epoch_out) {
        const char *p = strstr(resp, "\"epoch\":");
        if (p)
            *epoch_out = strtoull(p + 8, NULL, 10);
    }
    AIRY_FREE(resp);
    return ok;
}

int cli_gw_health(int timeout_ms)
{
    return cli_gw_probe(timeout_ms, NULL);
}

/* ── cJSON 参数面（0.1.19 §459）────────────────────────────────────── */
/* 业务模块的通用骨架：建 params → 序列化 → 调 → 解析 result。params
 * 所有权在函数内结束（成功与失败路径都释放），调用方只需管 result 树。 */
int cli_gw_json_q(const char *method, cJSON *params, int timeout_ms, int renewals,
                  cJSON **out_root)
{
    char *params_json = NULL;
    char *result_json = NULL;
    int rc;

    if (!method || !out_root) {
        cJSON_Delete(params);
        cli_gw_err_set("网关调用参数非法（method 或 out_root 为空）");
        return AIRY_ERR_INVALID_PARAM;
    }
    *out_root = NULL;
    if (params && !(params_json = cJSON_PrintUnformatted(params))) {
        cJSON_Delete(params);
        cli_gw_err_set("请求参数序列化失败（内存不足）");
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    cJSON_Delete(params);
    rc = cli_gw_call_q(method, params_json, timeout_ms, renewals, &result_json);
    AIRY_FREE(params_json);
    if (rc != 0)
        return rc;
    *out_root = result_json ? cJSON_Parse(result_json) : NULL;
    AIRY_FREE(result_json);
    if (!*out_root) {
        snprintf(g_cli_gw_err, sizeof(g_cli_gw_err),
                 "网关返回格式异常（method=%s），result 不是合法 JSON", method);
        return AIRY_ERR_PROTOCOL;
    }
    return 0;
}

int cli_gw_json(const char *method, cJSON *params, int timeout_ms, cJSON **out_root)
{
    return cli_gw_json_q(method, params, timeout_ms, 0, out_root);
}
