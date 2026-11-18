// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_ev.h
 * @brief CLI 事件流出口（--events ndjson）契约：帧信封、序列号与续传。
 *
 * 设计（0.1.21 D-15）：stdout 只承载 NDJSON 帧，人类可读输出改道 stderr。
 * 帧不新造语义，payload 原样承载服务端 run-stream 事件对象；CLI 只补帧头
 * （schema/seq/ts/type）与本地控制帧（heartbeat/snapshot/notice/
 * control_request）。序列号单调、环形缓冲 64 帧支撑 hello{last_seq} 续传。
 *
 * 反向控制固定「CLI 问、TUI 答」：高风险管理动作提交前发 control_request，
 * 超时按拒绝处理（fail-closed）。
 */

#ifndef AIRY_CLI_EV_H
#define AIRY_CLI_EV_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_EV_SCHEMA 1
#define CLI_EV_MAX (1024u * 1024u) /* 单帧上限（超出截断并置 truncated） */
#define CLI_EV_KEEP (64u * 1024u)  /* 环形缓冲单帧保留上限 */
#define CLI_EV_RING 64             /* 续传窗口帧数 */
#define CLI_EV_BEAT_MS 15000u      /* 心跳间隔 */
#define CLI_EV_DROP_MS 800         /* 增量帧写预算 */
#define CLI_EV_TERM_MS 30000       /* 终态帧写预算 */

/* 输出写面：默认 stdout；测试可注入内存 sink（同一契约，非桩）。 */
typedef int (*cli_ev_wr_t)(const void *buf, size_t len, void *ud);

void cli_ev_io_set(cli_ev_wr_t wr, void *ud);
int cli_ev_init(void);
void cli_ev_fini(void);

/* 一轮运行的边界：重置终态标记与运行状态（不重置 seq，续传跨轮有效）。 */
void cli_ev_begin(void);
int cli_ev_emit(const char *type, const char *payload, size_t plen);
int cli_ev_note(const char *msg);
int cli_ev_beat(void);
void cli_ev_state(const char *st);
void cli_ev_snap(unsigned last_seq);

int cli_ev_closed(void);
unsigned cli_ev_seq(void);
unsigned cli_ev_dropped(void);
int cli_ev_terminal(void);

/* 纯函数面（无 I/O，单测直接覆盖）：帧编码与事件字段提取。 */
size_t cli_ev_build(char *dst, size_t cap, unsigned seq, uint64_t ts,
                    const char *type, const char *payload, size_t plen,
                    unsigned dropped, int *trunc);
int cli_ev_typeof(const char *json, size_t len, char *out, size_t cap);
int cli_ev_str(const char *json, size_t len, const char *key, char *out,
               size_t cap);
int cli_ev_num(const char *json, size_t len, const char *key,
               unsigned long *out);
int cli_ev_is_term(const char *type);
int cli_ev_esc(char *dst, size_t cap, const char *src);

/* 事件桥：服务端事件 → 帧；返回 1=终态，0=继续，-1=输出已关闭。 */
int cli_ev_bridge(const char *json, size_t len);
/* run_end 状态 → 进程退出码：0 完成/暂停，130 取消，1 失败。 */
int cli_ev_status(const char *json, size_t len);
/* 异常收尾：补齐 error + run_end（若尚无终态），保证恰好一个终态帧。 */
int cli_ev_finish(const char *code);

/* stdin 输入帧（hello/input/control_response）解析与分发。 */
int cli_ev_in_poll(void);
int cli_ev_in_prompt(char *out, size_t cap);
/* 输入帧 kind=cancel 已到达（粘性；运行循环据此中断当前轮）。 */
int cli_ev_in_cancel(void);

/* 高风险管理动作裁决：发 control_request 并等待 control_response。
 * 返回 1=允许，0=拒绝（超时或显式拒绝，fail-closed）。 */
int cli_ev_ask(const char *tool, const char *target, int timeout_ms);

/* events 模式整段会话（含多轮 prompt 帧）；返回进程退出码。 */
int cli_ev_run(const char *prompt);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_EV_H */
