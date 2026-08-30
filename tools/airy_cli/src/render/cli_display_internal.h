// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_display_internal.h
 * @brief Internal contract between the airy_cli display-domain files.
 *
 * cli_display.c 自 2026-08-27 起按职责域拆分（963 行 → 3 个展示模块）：
 *
 *   cli_display.c      结果/计划列表/进度回调呈现门面（含拓扑序 helper）
 *   cli_live_board.c   live plan board：TTY 原位重绘任务看板
 *   cli_banner.c       启动横幅：蓝框 hero / 角色图例 / 模型行
 *
 * 跨编译单元共享的内部符号统一经本头文件以原名 extern 导出；公共 API
 * 保持不变（仍由 include/cli_internal.h 声明）。
 *
 * NOT part of the public API.
 */

#ifndef AIRY_CLI_DISPLAY_INTERNAL_H
#define AIRY_CLI_DISPLAY_INTERNAL_H

#include "taskflow_advanced.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fill `order` with the topological order of the first `count`
 *        workflow nodes (dependencies first, cycles/missing sources fall
 *        back to natural order).
 *
 * cli_print_plan_list() 与 cli_live_board_begin() 共用同一排序结果，
 * 保证静态计划与原位重绘看板的节点次序一致。`scratch` 是调用方提供的
 * count 字节工作区（需零初始化，如 AIRY_CALLOC 产物）。
 *
 * Returns 1 on success, 0 when the caller should skip rendering entirely
 * (OOM already released the scratch).
 */
int cli_plan_topo_build(const taskflow_workflow_t *wf, size_t count, size_t *order,
                            unsigned char *scratch);

/**
 * @brief Width-aware, UTF-8-safe truncation of `s` into buf (cap bytes) so
 *        it fits max_w display cells; appends "…" (1 cell) when text was
 *        cut. Returns the display width of what was emitted.
 *
 * 实现位于 cli_banner.c（横幅域是该工具的最重度使用方）；live board 的
 * 节点行复用之以保持统一的截断观感。
 */
size_t cli_hero_clip(const char *s, size_t max_w, char *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_DISPLAY_INTERNAL_H */
