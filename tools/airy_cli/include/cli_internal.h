// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_internal.h
 * @brief Internal shared header for airy_cli: macros, types, globals and prototypes.
 *
 * Shared compile-time constants (terminal colors, CLI_SEP, AIRY_CLI_VERSION,
 * CLI_HISTORY_MAX_MSGS), shared structs (cli_command_t / cli_cmd_ctx_t /
 * cli_dag_poll_rc_t), extern globals (g_cli_cancel / g_chat_adapter /
 * g_history_roles / g_history_contents / g_history_count / CLI_COMMANDS)
 * and cross-file function prototypes.
 */

#ifndef AIRY_CLI_INTERNAL_H
#define AIRY_CLI_INTERNAL_H

#include "airy_rt.h"
#include "loop.h"
#include "roadmap_sched.h"
#include "lang_gateway.h"
#include "platform.h"
#include "cognition.h"
#include "gccp.h"
#include "work_hall.h"
#include "hall_store.h"
#include "governance.h"
#include "plan_to_dag.h"
#include "taskflow_advanced.h"
#include "llm_svc_adapter.h"
#include "logger.h"
#include "logging.h"
#include "airy_memory.h"
#include "string_compat.h"
#include "daemon_rpc_client.h"
#include "daemon_cmds.h"
#include "cli_render.h"
#include "cli_tui.h"

#include <signal.h>
#include <stddef.h>

/* 对话记忆引擎（2.2.4）：由 main.c 在 loop 创建后注入，
 * cli_chat.c 在对话路径读写记忆（此前对话路径零记忆接线）。 */
struct airy_memory_engine;
typedef struct airy_memory_engine airy_memory_engine_t;
extern airy_memory_engine_t *g_cli_memory_engine;

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_SEP "  ──────────────────────────────────────────────"

/* 版本号 SSoT（2.6.2 Unify Design）：单一来源为 agentrt/VERSION 文件，
 * 由 airy_cli/CMakeLists.txt 构建期读取并通过 target_compile_definitions
 * 注入（AIRY_CLI_VERSION="x.y.z"）；此处仅保留缺省回退，防止未走 CMake
 * 注入的独立编译（如 IDE 单文件编译）出现未定义宏。版本更新只改
 * VERSION 文件一处，CLI 与 TUI 自动同步。 */
#ifndef AIRY_CLI_VERSION
#define AIRY_CLI_VERSION "0.1.5a"
#endif

/* 思考链折叠保留行数（2026-08-19：仅折叠思考链，结果完整展示）。
 * 思考链渲染为前 N 行 + 折叠尾，避免碎片刷屏；结果不折叠。 */
#define CLI_REPLY_FOLD_KEEP 4

/* Pinned startup header height (blue frame: top edge + brand/capabilities/
 * legend/models rows + bottom edge). Shared by cli_display.c (which renders
 * it) and main.c (which pins it). 2026-08-19: the hero became a blue box so
 * the header reads as a distinct block above the dialogue. */
#define CLI_HDR_LINES 6

/* Max chat history messages: 30 by default (~15 rounds); AIRY_CHAT_HISTORY_ROUNDS
  * overrides it in rounds (messages = rounds*2). Capped at 60, aligned with
  * the 64-message cap in build_llm_request_json. When full, the oldest round
  * (user+assistant) is dropped, keeping FIFO. */
#define CLI_HISTORY_MAX_MSGS 60

/* 命令类别：/help 按组展示，避免 26 个命令平铺淹没关键入口。 */
typedef enum {
    CLI_CAT_SESSION = 0, /* 会话控制：/help /clear /quit /tui /sanitize */
    CLI_CAT_SYSTEM,      /* 系统状态：status/chain/daemon/rpc/stats 等 */
    CLI_CAT_RESOURCE,    /* 资源查询：agents/tools/hooks/models/mem 等 */
    CLI_CAT_SECURITY,    /* 安全与权限：vault/perm/security/notify */
} cli_cmd_category_t;

typedef struct {
    const char *name;
    const char *desc;
    cli_cmd_category_t category;
    int needs_args;
    int (*fn)(const char *arg, void *ctx);
} cli_command_t;

typedef struct {
    airy_work_hall_t *hall;
    int *quit;
    /* 2026-08-17：/tui 切换请求——退出 CLI 主循环后 exec agentrt-tui
     * （进程替换，无嵌套进程；仅 CLI 全屏 TUI 页面被激活时可用）。 */
    int *switch_tui;
} cli_cmd_ctx_t;

typedef enum { CLI_DAG_POLL_ACTIVE = 0, CLI_DAG_POLL_DONE, CLI_DAG_POLL_ERROR } cli_dag_poll_rc_t;

/* Global runtime state (defined in main.c) */
extern volatile sig_atomic_t g_cli_cancel;
extern llm_svc_adapter_t *g_chat_adapter;

/* 1.3 推理语言网关：全局句柄 + 最新一轮语言约束注入物。main.c 输入环节
 * 填充（process 标准化 + tick 计数），cli_chat.c 消费（System Prompt 注入
 * 与输出后处理）。g_cli_lang_sys_prompt 为 OWNER，每轮输入覆盖前释放。 */
extern airy_lang_gateway_t *g_cli_lang_gateway;
extern char *g_cli_lang_sys_prompt;
extern airy_lang_t g_cli_lang_output;

/* 阶段 4（2026-08-15）：决策链事件流句柄（⑥单一真相源事件流底座）。
 * main.c 创建 hall_store 后赋值；CLI 决策点（GCCP 确认/蓝图命中/计划/提交）
 * 以 "cognition" 角色写入 CHAIN/COMMAND 事件，供 /chain 决策链可视化回放。 */
extern airy_hall_store_t *g_cli_hall_store;

/* Server one-shot mode (-p/--print): no banner/prompt/TUI, single-turn
 * execution then exit. --json switches the final result to JSON. Defined
 * in main.c; read by the render layer to keep output clean. */
extern int g_cli_print_mode;
extern int g_cli_json_mode;

/* Chat history buffer (defined in cli_chat.c) */
extern char *g_history_roles[CLI_HISTORY_MAX_MSGS];
extern char *g_history_contents[CLI_HISTORY_MAX_MSGS];
extern char *g_history_reasonings[CLI_HISTORY_MAX_MSGS];
extern size_t g_history_count;

/* Command table (defined in airy_cli_cmdline.c; enumerated by cmd_help in
 * cli_cmds.c and by TUI Tab completion) */
extern const cli_command_t CLI_COMMANDS[];
size_t cli_commands_count(void);

/* 命令分发与参数解析（airy_cli_cmdline.c；main.c 主循环调用）：
 * cli_dispatch_command 匹配 /name，未命中返回 0 以便落回普通输入；
 * cli_parse_args 返回 0=继续启动，1=已输出帮助/错误并退出。 */
int cli_dispatch_command(const char *input, void *ctx);
int cli_parse_args(int argc, char *argv[], const char **out_print_prompt);

char *cli_gccp_interact(const airy_gccp_probe_t *probe, void *user_data);
void cli_history_clear(void);
/* 2.5.x 意图分辨：纯字符串启发式（consult > task > chat 三级优先），
 * 返回 1=task / 0=chat / -1=未命中（调用方交 LLM 兜底） */
int cli_classify_heuristic(const char *input);
int cli_classify_input(const char *input);
void cli_chat_reply(const char *input);
/* 2.1.1.5：读取最近一轮对话的真实 token/费用统计（main.c 回合分隔处展示） */
void cli_chat_usage_get(uint64_t *tokens, double *cost);
/* 1.7：读取全链路真实 token/费用会话差值（llm_d cost_tracker 真相源，
 * 覆盖 chat + task 双思考路径；llm_d 离线回退 chat 累计） */
void cli_chat_usage_get_session(uint64_t *tokens, double *cost);

/* ===== cli_chat.c 域拆分（2026-08-27：2040 行 → 6 个职责模块） ===== */

/* cli_chat_usage.c：对话轮 token/费用统计与思考链累计/清零 */
void cli_chat_usage_add(const llm_response_t *resp);
void cli_chat_usage_reset(void);
void cli_chat_reasoning_add(const char *reasoning);
const char *cli_chat_reasoning_peek(void);

/* cli_chat_memory.c：对话记忆注入/写回（gateway mem_d 优先，L1 回退） */
void cli_chat_mem_inject_system(const char *input, char *out_buf, size_t out_size);
void cli_chat_mem_record(const char *input, const char *reply, const char *reasoning);

/* cli_chat_gccp.c：GCCP 逐问交互（cli_chat_t1p_cached 实现仍在 cli_chat.c） */
const char *cli_chat_t1p_cached(void);

/* cli_chat_history.c：历史缓冲 / 错误描述 / 系统提示词 */
const char *cli_chat_err_desc(int err);
void cli_history_add(const char *role, const char *content, const char *reasoning);
const char *cli_system_prompt_now(void);
void cli_chat_reasoning_persist(const char *text);

/* cli_chat_tools.c：聊天工具回路（schema / 消息缓冲 / 工具执行 / 工具轮） */
#define CLI_CHAT_TOOL_MAX_ROUNDS 8
#define CLI_CHAT_TOOL_RESULT_CAP 12000 /* 单工具结果回填模型的最大字节数 */
typedef struct {
    llm_message_t *msgs;
    size_t count;
    size_t cap;
    char **owned;
    size_t owned_count;
    size_t owned_cap;
} cli_chat_msgbuf_t;
extern const char *cli_chat_tools_json;
void cli_msgbuf_free(cli_chat_msgbuf_t *b);
void cli_msgbuf_push(cli_chat_msgbuf_t *b, const char *role, const char *content,
                     const char *tool_call_id, const char *tool_calls_json,
                     const char *reasoning_content);
int cli_chat_tool_round(cli_chat_msgbuf_t *b, const llm_response_t *resp);

airy_err_t cli_think_process_remote(const char *think_sock, const char *input, size_t input_len,
                                    airy_task_plan_t **out_plan);

airy_err_t cli_dag_submit_remote(const char *sched_sock, const taskflow_workflow_t *wf,
                                 const char *task_input, const char *workspace_dir,
                                 char **out_dag_id);
cli_dag_poll_rc_t cli_dag_poll_remote(const char *sched_sock, const char *dag_id,
                                      double *out_progress, char *out_state, size_t state_cap,
                                      char **out_result);
airy_err_t cli_dag_wait_remote(const char *sched_sock, const char *dag_id, char **out_result);

/* Node-level progress board for remote DAGs (opaque; cli_dag.c). */
typedef struct cli_dag_board_s cli_dag_board_t;
cli_dag_board_t *cli_dag_node_board_create(void);
void cli_dag_node_board_destroy(cli_dag_board_t *board);
int cli_dag_node_board_tick(cli_dag_board_t *board, const char *sched_sock,
                            const char *dag_id);
/* Remote DAG per-node state snapshot: reports (node_id, state) via cb without
 * printing (feeds the live plan board). Returns 1 on terminal state. */
int cli_dag_board_snapshot(const char *sched_sock, const char *dag_id,
                           void (*cb)(const char *node_id, const char *state));

void cli_print_system_header(const char *t2, const char *t1f, const char *t1p);
void cli_print_result(const char *result);
void cli_print_plan_list(const taskflow_workflow_t *wf);
void cli_progress_cb(const char *execution_id, const char *node_id, taskflow_state_t state,
                     double progress, void *user_data);
void cli_board_line(const char *tag, const char *id, const char *state, double progress);

/* Live plan board (structured task progress + icon choreography, 2026-08-19):
 * prints the plan once (□ 待处理) and re-renders the block in place as node
 * states change (□ → ◐ 执行中 → ✓/✗ 完成/失败), footer 汇总进度。非 TTY /
 * TUI 激活时退化为静态计划 + 追加看板行（原语义）。
 *   begin    —— 打印计划块（header + 节点行 + footer），TTY 下开启原位重绘
 *   set_node —— 运行时节点状态写入（progress 回调线程 → 轮询线程读取）
 *   refresh  —— 轮询循环调用：有变化则原位重绘，返回 1；退化为 0（调用方
 *               回退 cli_board_line）
 *   done     —— 结束看板会话（插入对话等场景主动退场） */
void cli_live_board_begin(const taskflow_workflow_t *wf);
void cli_live_board_set_node(const char *node_id, const char *state);
int cli_live_board_refresh(const char *agg_state, double agg_progress);
int cli_live_board_active(void);
void cli_live_board_extra(void);
void cli_live_board_done(void);
int cmd_help(const char *arg, void *ctx);
/* S-5 编排管线用户入口（cli_orch.c）：orchestrator 七阶段编排 */
int cmd_orch(const char *arg, void *ctx);

/* Dual-thinking three-model config, unified with think_d's model.yaml.
 * Priority: env AIRY_MODEL_T2/T1F/T1P > model.yaml think section >
 * model.yaml llm.model default. Outputs are NUL-terminated; an empty
 * string means "unset" (callers render "默认" or pass NULL). */
void cli_think_cfg_load(char *t2, size_t t2c, char *t1f, size_t t1fc,
                        char *t1p, size_t t1pc);
/* Env + model.yaml think section only (no llm.model backfill). Returns 1
 * when at least one role is explicitly configured. Used by exec review to
 * keep the no-self-review guarantee (a backfilled default equals the main
 * generator model). */
int cli_think_cfg_explicit(char *t2, size_t t2c, char *t1f, size_t t1fc,
                           char *t1p, size_t t1pc);
int cmd_clear(const char *arg, void *ctx);
int cmd_status(const char *arg, void *ctx);
int cmd_chain(const char *arg, void *ctx);
int cmd_quit(const char *arg, void *ctx);
int cmd_tui(const char *arg, void *ctx);

/* 决策链事件解析 helpers（cli_cmds.c 实现；/chain 命令与 cli_panel.c 事件流面板共用） */
uint64_t cli_chain_extract_gseq(const char *json);
void cli_chain_extract_content(const char *json, char *out, size_t cap);
int cli_chain_str_field(const char *json, const char *key, char *out, size_t cap);
/* 提取 "seq":<digits>（事件文件序号，header 首现；供跨进程事件流按
 * (ts_utc, seq) 排序——gseq 为进程内单调，跨进程会撞号） */
uint32_t cli_chain_extract_seq(const char *json);
void cli_chain_label(int cat, const char *content, char *out, size_t cap);

/* 阶段 4 面板数据源（cli_panel.c 实现；main.c 绑定到 TUI） */
void cli_panel_board_create(airy_work_hall_t *hall, void **out_ud);
void cli_panel_board_destroy(void *ud);
void cli_panel_events_create(airy_hall_store_t *hs, void **out_ud);
void cli_panel_events_destroy(void *ud);
size_t cli_panel_board_count(void *ud);
int cli_panel_board_line(void *ud, size_t idx, char *out, size_t cap);
size_t cli_panel_events_count(void *ud);
int cli_panel_events_line(void *ud, size_t idx, char *out, size_t cap);
/* 记忆链面板（2026-08-25）：经 gateway mem.recent 拉取，1s 节流刷新 */
void cli_panel_mem_create(void **out_ud);
void cli_panel_mem_destroy(void *ud);
size_t cli_panel_mem_count(void *ud);
int cli_panel_mem_line(void *ud, size_t idx, char *out, size_t cap);
/* 面板可操作动作（2026-08-19）：TUI 引擎按键触发，动作在 CLI 层执行 */
int cli_panel_board_action(void *ud, int action, size_t sel, char *out, size_t cap);
int cli_panel_events_action(void *ud, int action, size_t sel, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_INTERNAL_H */
