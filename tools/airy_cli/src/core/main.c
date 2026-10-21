// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file main.c
 * @brief airy_cli - AgentRT interactive product entry.
 *
 * Full closed-loop product flow: natural-language instruction -> GCCP intent
 * confirmation (reasoning + four questions) -> cognition pipeline planning
 * (think_d over gateway) -> Plan -> DAG adaption -> gateway → sched_d submit/
 * board/wait -> agent_d drives real execution.
 *
 * Mechanism/strategy separation: the CLI is the product layer (interaction
 * strategy), agentrt is the mechanism layer.  Degrades gracefully when the
 * llm_d/agent_d daemons are not running (heuristic confirmation, agent
 * unavailable).
 *
 * Split layout (2026-08-27):
 *   main.c             — entry, arg parsing, command dispatch, main loop
 *   airy_cli_pipeline.c — runtime context assembly/teardown, blueprint fastpath
 *   airy_cli_exec.c     — task wait worker, stdin poll, result rendering
 *
 * 2026-08-27 二轮拆分（893 行 → 3 个职责模块）：
 *   main.c               本文件：入口、全局运行态与主循环骨架
 *   airy_cli_cmdline.c   命令面（CLI_COMMANDS 表 / 分发 / 参数解析）
 *   airy_cli_taskflow.c  任务执行管线（规划 → DAG → 提交 → 轮询 → 等待 → 结果）
 *
 * 2026-10-21 入口阶段化：main() 只保留「装配 → 主循环 → 收尾」骨架，
 *   启动、取行、内置命令、语言网关、单回合、收尾各为一个 static 阶段函数；
 *   取行与空白行判定在 -p 与交互两路共用，消除重复扫描。
 */

#include "cli_internal.h" /* 家族公共头：commons/平台/引擎/守护进程客户端面 */
#include "cli_gw.h"       /* 架构约束：统一经 gateway 派发 */
#include "cli_live.h"     /* N4 会话存活：后端重启后提示并重建会话视图 */

#include "airy_cli_pipeline.h"
#include "airy_cli_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#endif

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* Task-set cancellation flag: set by the SIGINT handler; run_to_completion checks
  * it each round (the engine holds this pointer); the current node finishes, then aborts.
  * Reset to 0 before each new task. */
volatile sig_atomic_t g_cli_cancel = 0;

/* Server one-shot mode (-p/--print) and --json structured output. */
int g_cli_print_mode = 0;
int g_cli_json_mode = 0;
/* 全屏 TUI 渲染层模式（--tui，0.1.17 R5-G6）。 */
int g_cli_tui_mode = 0;
/* 会话恢复视图模式（--continue / --resume，0.1.17 R5-G5）：启动时读一次
 * mem.recent 装配 g_history_*，前端不落任何本地会话状态。 */
int g_cli_resume_mode = 0;

#if !defined(_WIN32)
static void cli_sigint_handler(int sig)
{
    (void)sig;
    g_cli_cancel = 1;
}
#endif

llm_svc_adapter_t *g_chat_adapter = NULL;
airy_hall_store_t *g_cli_hall_store = NULL;

/* 1.3 推理语言网关：全局句柄（cli_setup_runtime 创建后赋值）+ 最新一轮
 * 语言约束注入物（输入环节 process 填充，cli_chat.c 消费；每轮覆盖前释放）。 */
char *g_cli_lang_sys_prompt = NULL;
int g_cli_lang_output = 0;

/* 会话开始时刻（TUI 状态栏耗时计算；交互模式才有意义）。 */
static uint64_t g_session_start_ms;

/* 取行结果：结束 / 跳过 / 已处理 / 有效输入。 */
enum {
    CLI_LOOP_EOF = 0,
    CLI_LOOP_SKIP,
    CLI_LOOP_NEXT,
    CLI_LOOP_TURN,
};

/* 主循环运行态：一次装配，逐轮复用（输入缓冲随结构体常驻，不逐轮分配）。 */
typedef struct {
    cli_tui_t *tui;
    cli_runtime_ctx_t rt;
    cli_cmd_ctx_t cmd;
    const char *prompt;
    int consumed;
    int switch_tui;
    int quit;
    char input[8192];
} cli_loop_t;

/* 1.3 推理语言 wire 值 → 展示名（lang_gateway.h 枚举：0=未知/1=中文/2=英文）。
 * M1-1c 后 CLI 不再直连 lang_gateway 库，本地保留渲染层映射。 */
static const char *cli_lang_name(int lang)
{
    switch (lang) {
    case 1:
        return "中文";
    case 2:
        return "英文";
    default:
        return "未知";
    }
}

/* 全部为空白（含空串）：-p 与交互两路共用的跳过判据。 */
static int cli_blank(const char *s, size_t len)
{
    size_t i = 0;

    while (i < len && (s[i] == ' ' || s[i] == '\t'))
        i++;
    return i == len;
}

/* 去除行尾 CR/LF，返回新的有效长度。 */
static size_t cli_strip_eol(char *s, size_t len)
{
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
    return len;
}

/* 帮助入口：'?' 或全角 '？'(EF BC 9F)，尾部空白不计。 */
static int cli_qmark(const char *s, size_t len)
{
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t'))
        len--;
    if (len == 1 && s[0] == '?')
        return 1;
    return len == 3 && (unsigned char)s[0] == 0xEF &&
           (unsigned char)s[1] == 0xBC && (unsigned char)s[2] == 0x9F;
}

/* -p 模式取行：一次性提示词只取一次；stdin 流式输入跳过空行与 EOF。 */
static int cli_read_print(cli_loop_t *l, size_t *len)
{
    if (l->prompt && l->prompt[0]) {
        if (l->consumed)
            return CLI_LOOP_EOF;
        l->consumed = 1;
        AIRY_STRNCPY_TERM(l->input, l->prompt, sizeof(l->input));
        *len = strlen(l->input);
        return CLI_LOOP_NEXT;
    }
    if (!fgets(l->input, sizeof(l->input), stdin))
        return CLI_LOOP_EOF;
    *len = cli_strip_eol(l->input, strlen(l->input));
    return cli_blank(l->input, *len) ? CLI_LOOP_SKIP : CLI_LOOP_NEXT;
}

/* 交互取行：行式 readline → 提交 → 续接提示；EOF 与空行分别收敛。 */
static int cli_read_tty(cli_loop_t *l, size_t *len)
{
    if (!cli_term_input_begin() && !cli_term_is_tty())
        cli_outf("\n\n%sairy>%s ", cli_c(CLR_CYAN), cli_c(CLR_RESET));
    fflush(stdout);
    if (cli_tui_readline(l->tui, l->input, sizeof(l->input), len) == 0) {
        cli_term_input_submit();
        return CLI_LOOP_EOF;
    }
    /* 0.1.17 R5-G2：行式是 CLI 唯一交互形态，readline 只返回 0/1；
     * 全屏切换（原 rl==2 / rl==3）随 C 侧全屏套件冻结退役。 */
    cli_term_input_submit();
    if (*len > 0 && cli_term_input_on()) {
        cli_term_input_begin();
        cli_outf("%sairy>%s", cli_c(CLR_DIM), cli_c(CLR_RESET));
        cli_term_input_hop();
    }
    return cli_blank(l->input, *len) ? CLI_LOOP_SKIP : CLI_LOOP_NEXT;
}

/* 命令已处理：quit 请求退出主循环，其余（含 /tui 请求）取下一行。 */
static int cli_cmd_done(cli_loop_t *l)
{
    return l->quit ? CLI_LOOP_EOF : CLI_LOOP_NEXT;
}

/* -p 单次执行面：不解释 quit/exit/'?'——单跑语义下这些词按普通输入处理，
 * 未命中命令表即进入对话回合。 */
static int cli_batch_cmd(cli_loop_t *l)
{
    if (!cli_dispatch_command(l->input, &l->cmd))
        return CLI_LOOP_TURN;
    return cli_cmd_done(l);
}

/* 交互命令面：退出词与帮助入口先于命令表，其余落回同一命令分发。 */
static int cli_tty_cmd(cli_loop_t *l, size_t len)
{
    if (strcmp(l->input, "quit") == 0 || strcmp(l->input, "exit") == 0)
        return CLI_LOOP_EOF;
    if (cli_qmark(l->input, len)) {
        cmd_help(NULL, &l->cmd);
        return CLI_LOOP_NEXT;
    }
    if (!cli_dispatch_command(l->input, &l->cmd))
        return CLI_LOOP_TURN;
    return cli_cmd_done(l);
}

/* 0.1.17 R5-G6：/tui 切换到全屏 TUI 渲染层。fork agentrt-tui 子进程
 * （唯一实现 cli_run_tui_frontend），TUI 退出后返回行式对话；不 exec
 * 替换 CLI 进程，故会话上下文与历史保留。 */
static int cli_loop_step(cli_loop_t *l, size_t *len)
{
    int r;

    if (l->switch_tui) {
        l->switch_tui = 0;
        cli_term_header_unpin();
        (void)cli_run_tui_frontend(0);
        return CLI_LOOP_NEXT;
    }
    if (g_cli_print_mode) {
        r = cli_read_print(l, len);
        return r == CLI_LOOP_NEXT ? cli_batch_cmd(l) : r;
    }
    r = cli_read_tty(l, len);
    return r == CLI_LOOP_NEXT ? cli_tty_cmd(l, *len) : r;
}

#ifdef AIRY_HAS_CJSON
/* think.lang_process 应答落位：系统提示词与输出语言，失败静默保持上一轮。 */
static void cli_lang_apply(const char *res)
{
    cJSON *lr = cJSON_Parse(res);

    if (lr) {
        cJSON *sp = cJSON_GetObjectItem(lr, "system_prompt");
        cJSON *ol = cJSON_GetObjectItem(lr, "output_lang");
        cJSON *rl = cJSON_GetObjectItem(lr, "reasoning_lang");
        cJSON *dr = cJSON_GetObjectItem(lr, "decision_reason");
        int r_lang = cJSON_IsNumber(rl) ? (int)rl->valuedouble : 0;
        char line[192];

        if (cJSON_IsString(sp) && sp->valuestring) {
            AIRY_FREE(g_cli_lang_sys_prompt);
            g_cli_lang_sys_prompt = AIRY_STRDUP(sp->valuestring);
        }
        if (cJSON_IsNumber(ol))
            g_cli_lang_output = (int)ol->valuedouble;
        snprintf(line, sizeof(line), "推理语言: %s · 输出语言: %s · %s",
                 cli_lang_name(r_lang), cli_lang_name(g_cli_lang_output),
                 cJSON_IsString(dr) && dr->valuestring ? dr->valuestring : "");
        cli_print_status("Sub lang Agent", line);
        cli_trace("lang", "reasoning=%s output=%s", cli_lang_name(r_lang),
                  cli_lang_name(g_cli_lang_output));
        cJSON_Delete(lr);
    }
}

/* 1.3 推理语言网关服务面化（M1-1c）：CLI 不持 lang_gateway 句柄，
 * 输入标准化经 gateway → think.lang_process（think_d 承载）。
 * 网关不可达/失败时静默降级：语言约束缺失不阻塞对话主流程。 */
static void cli_lang_gw(const char *input)
{
    cJSON *lp = cJSON_CreateObject();
    cJSON *lt = cJSON_CreateString(input);
    char *lp_res = NULL;
    char *lp_json;

    if (lp && lt)
        cJSON_AddItemToObject(lp, "text", lt);
    else
        cJSON_Delete(lt);
    lp_json = lp ? cJSON_PrintUnformatted(lp) : NULL;
    cJSON_Delete(lp);
    if (lp_json && cli_gw_call("think.lang_process", lp_json, 6000, &lp_res) == 0 &&
        lp_res)
        cli_lang_apply(lp_res);
    else
        cli_trace("lang", "think.lang_process unavailable (gateway offline)");
    AIRY_FREE(lp_res);
    AIRY_FREE(lp_json);
}
#endif /* AIRY_HAS_CJSON */

/* 单回合：状态渲染 → 语言网关 → 蓝图三级路由 → 对话/任务分支。 */
static void cli_turn(cli_loop_t *l)
{
    uint64_t t0 = cli_now_ms();
    int is_task;

    cli_print_status("For Thee", l->input);
#ifdef AIRY_HAS_CJSON
    cli_lang_gw(l->input);
#endif
    /* 4.0b Blueprint scheduling three-tier routing（0.1.9 M3：经
     * gateway → sched_d sched.plan RPC，CLI 不再持有本地 roadmap） */
    if (cli_blueprint_fastpath(l->input, t0))
        return;
    is_task = cli_classify_input(l->input);
    cli_trace("intent", "%s", is_task ? "task" : "chat");
    if (is_task == 0) {
        char metrics[192];

        cli_chat_reply(l->input);
        cli_chat_usage_metrics(metrics, sizeof(metrics));
        cli_print_turn(cli_now_ms() - t0, metrics[0] ? metrics : NULL);
        return;
    }
    /* === 任务回合：认知规划（gateway → think_d）→ DAG 适配 → 提交 →
     * 轮询 → 等待 → 结果汇总（airy_cli_taskflow.c） === */
    (void)cli_run_task_pipeline(&l->rt, l->input, t0);
}

#ifndef _WIN32
/* stderr 落盘：交互与 TTY 下的 -p 统一进 airy_cli.log；失败回退终端直连。 */
static void cli_boot_log(void)
{
    char logpath[512];
    const char *logdir;
    FILE *lf;

    if (g_cli_print_mode && !isatty(STDERR_FILENO))
        return;
    logdir = airy_log_dir();
    if (airy_mkdir_p(logdir) != 0) {
        fprintf(stderr, "[airy_cli] 无法创建日志目录: %s（stderr 将直连终端）\n", logdir);
        return;
    }
    snprintf(logpath, sizeof(logpath), "%s/airy_cli.log", logdir);
    lf = fopen(logpath, "a");
    if (!lf) {
        fprintf(stderr, "[airy_cli] 无法打开日志文件: %s（stderr 将直连终端）\n", logpath);
        return;
    }
    fflush(stderr);
    dup2(fileno(lf), STDERR_FILENO);
    fclose(lf);
}
#endif

/* 终端装配：改性点、主题、标题、路径、日志级别与 SIGINT 取消钩子。 */
static void cli_boot_term(void)
{
    cli_term_init();
#ifndef _WIN32
    cli_term_crash_guard_install(); /* T-19：崩溃前还原终端改性 */
#endif
    cli_theme_init();
    cli_term_title("AgentRT · airy_cli");
    (void)airy_paths_init();
    log_set_module_level("*", LOG_LEVEL_ERROR);
#if !defined(_WIN32)
    {
        struct sigaction sa;

        __builtin_memset(&sa, 0, sizeof(sa));
        sa.sa_handler = cli_sigint_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
    }
    cli_boot_log();
#endif
    g_session_start_ms = cli_now_ms();
}

/* 模型面：三档模型名装载到页头与系统横幅（空串归一为 NULL）。 */
static void cli_boot_models(cli_tui_t *tui)
{
    char s2[128], verify[128], expert[128];
    const char *p_s2, *p_vf, *p_ex;

    cli_think_cfg_load(s2, sizeof(s2), verify, sizeof(verify),
                       expert, sizeof(expert));
    p_s2 = s2[0] ? s2 : NULL;
    p_vf = verify[0] ? verify : NULL;
    p_ex = expert[0] ? expert : NULL;
    cli_tui_set_header_models(tui, p_s2, p_vf, p_ex);
    cli_print_system_header(p_s2, p_vf, p_ex);
}

/* 0.1.16 B2 (design §5.2): the CLI is a pure gateway client — no
 * in-process microkernel (airy_init() withdrawn, 2026-09-14 ruling).
 * Boot evidence is a gateway reachability probe (HTTP /health); the
 * retired "badge=0 degraded" concept no longer applies: offline gateway
 * only means calls will fail until it is up, not a degraded kernel.
 * 证据走 stderr：main() 把模块级别钉在 LOG_LEVEL_ERROR，INFO/WARN 会被静默。 */
static void cli_boot_probe(void)
{
    if (cli_gw_health(2000) == 1)
        fprintf(stderr, "[airy_cli] gateway reachable - client mode ready\n");
    else
        fprintf(stderr, "[airy_cli] gateway unreachable -"
                        " calls will fail until it is up\n");
}

/* 自愈守护：显式开启才接管守护进程生命周期（AIRY_SELF_HEAL[_AGENTS]）。 */
static void cli_boot_heal(void)
{
    const char *sh = getenv("AIRY_SELF_HEAL");
    const char *agents = getenv("AIRY_SELF_HEAL_AGENTS");

    if ((sh && sh[0] && strcmp(sh, "0") != 0) || (agents && agents[0]))
        cli_daemon_lifecycle_init(agents);
}

/* 收尾：适配器 → 运行时 → TUI → 告别行 → 释放页头钉扎。 */
static void cli_shutdown(cli_loop_t *l)
{
    if (g_chat_adapter)
        llm_svc_adapter_destroy(g_chat_adapter);
    cli_teardown_runtime(&l->rt);
    cli_tui_destroy(l->tui);
    if (!g_cli_print_mode) {
        if (cli_term_input_on())
            cli_term_input_submit();
        else
            cli_outc('\n');
        cli_print_line("Super Agent",
                       "AgentRT has exited. Thank you for using it.");
    }
    cli_term_header_unpin();
}

int main(int argc, char *argv[])
{
    cli_loop_t loop = {0};

    /* cli_parse_args 以 *out_print_prompt == NULL 判定「prompt 尚未给出」，
     * 故运行态必须先清零再解析（-p "" 的空串亦为合法给定值）。 */
    if (cli_parse_args(argc, argv, &loop.prompt) != 0)
        return 1;
    /* 0.1.17 R5-G6 入口唯一化：--tui 为 CLI 的一个模式——调起 agentrt-tui
     * 子进程并回传退出码；CLI 父进程不初始化终端（保持零终端改性），
     * TUI 异常退出仅报错，不做接力。R5-G5：--resume/--continue 原样转发，
     * 使 `airy_cli --tui --resume` 与 `agentrt-tui --resume` 呈现同一会话
     * （会话权威在 mem_d，两个前端各自只读装配，均不落本地会话状态）。 */
    if (g_cli_tui_mode)
        return cli_run_tui_frontend(g_cli_resume_mode);
    cli_boot_term();
    if (!g_cli_print_mode)
        cli_tui_create(&loop.tui);
    cli_boot_models(loop.tui);
    cli_boot_probe();
    if (cli_setup_runtime(&loop.rt) != AIRY_EOK)
        return 1;
    /* 0.1.17 R5-G5：--continue/--resume 会话恢复视图——启动时一次性只读
     * 装配（mem.recent → g_history_*），此后请求/渲染路径自动同源生效。 */
    if (g_cli_resume_mode)
        cli_session_restore();
    /* N4：绑定后端重启后的会话重建入口，并启用空闲节拍探活。首次 tick
     * 只建立代次/可用性基线，不触发重建。 */
    cli_live_bind(cli_session_rebind);
    cli_live_init();
    loop.cmd.quit = &loop.quit;
    loop.cmd.switch_tui = &loop.switch_tui;
    cli_boot_heal();
    for (;;) {
        size_t len = 0;
        int step;

        (void)cli_daemon_lifecycle_reconcile_once();
        step = cli_loop_step(&loop, &len);
        if (step == CLI_LOOP_EOF)
            break;
        if (step == CLI_LOOP_TURN)
            cli_turn(&loop);
    }
    cli_shutdown(&loop);
    return 0;
}
