// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_pipeline.h
 * @brief Runtime context assembly/teardown and blueprint fastpath.
 *
 * cli_runtime_ctx_t aggregates every long-lived component the CLI main loop
 * needs (event-stream hall store, TUI panel data sources, task workspace).
 * cli_setup_runtime builds them; cli_teardown_runtime releases them.
 *
 * cli_blueprint_fastpath implements the three-tier blueprint routing
 * (L1 state-machine / L2 semantic-cache / L3 miss) that short-circuits
 * the full cognition pipeline when a repeated or similar task is detected.
 * 0.1.9 M3（roadmap CLI 切断）：CLI 不再进程内持有 roadmap 调度器实例，
 * 三级路由经 gateway → sched_d RPC（sched.plan）完成，L2 语义缓存由
 * sched_d 唯一持有（消除双进程双写同一缓存文件的 SSoT 违例）。
 */

#ifndef AIRY_CLI_PIPELINE_H
#define AIRY_CLI_PIPELINE_H

#include "airy_rt.h"
#include "loop.h"
#include "platform.h"
#include "cognition.h"
#include "hall_store.h"
#include "cli_tui.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Runtime context: every long-lived component the CLI main loop needs.
 * cli_setup_runtime fills it; cli_teardown_runtime releases it.
 * main() owns the struct on its stack and passes it by pointer.
 * 0.1.9 M1-1c：本地 work_hall/reviewer/governance/validator 已退役——
 * 任务执行唯一经 gateway → sched_d，查询面（/status、TUI board）迁
 * sched.dag_list；此处仅保留事件流 hall_store、面板 ud 与任务目录。 */
typedef struct {
    airy_hall_store_t *hall_store;
    void *board_ud;
    void *events_ud;
    void *mem_ud;
    const char *main_workspace_dir;
} cli_runtime_ctx_t;

/* Core loop assembly: loop create + chat memory engine attach.
 * 0.1.9 M1-1c：CLI 退役本地 cog 装配（GCCP/TC3/GRAD 接线随 C2d-2 移除，
 * 认知规划唯一经 gateway → think_d），loop 内引擎集由 airy_loop_create
 * 自动装配。Returns NULL on failure. */
airy_core_loop_t *cli_setup_core_engines(void);

/* Full runtime assembly: event-stream hall_store → chat adapter → TUI panels.
 * Returns AIRY_EOK on success, error code on failure (caller must clean up). */
airy_err_t cli_setup_runtime(airy_core_loop_t *loop, cli_tui_t *tui,
                              cli_runtime_ctx_t *rt);

/* Symmetric teardown of cli_setup_runtime.  Idempotent: zeroes the struct
 * after release so repeated calls are safe. */
void cli_teardown_runtime(cli_runtime_ctx_t *rt);

/* Blueprint three-tier fastpath: L1 (zero-token state-machine hit),
 * L2 (low-token semantic-cache hit), L3 miss (semantic hint).
 * 0.1.9 M3：路由判定经 gateway → sched_d sched.plan RPC；网关/调度器
 * 不可达时静默按 L3 miss 处理（与 lang_process 同款降级）。
 * Returns 1 when the fastpath handled the input (caller should continue
 * the main loop), 0 when the full pipeline must run. */
int cli_blueprint_fastpath(const char *input, uint64_t turn_start);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_PIPELINE_H */
