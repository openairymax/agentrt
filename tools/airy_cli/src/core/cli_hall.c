// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_hall.c
 * @brief airy_cli 决策链事件发射唯一机制件（SSoT）。
 *
 * 决策点（GCCP 确认 / 蓝图命中 / 计划 / 校验 / 提交）此前各自重抄同一
 * 仪式：判空 hall_store 句柄、局部缓冲、snprintf 组装内容、以固定
 * tenant="default"、node=NULL、role="cognition"、无 out 参数落写。该仪式
 * 在 chat/、core/ 多文件逐字重复，构成同文件与跨文件的克隆簇。
 *
 * 本件收敛发射机制：调用方只声明「会话（task_id）+ 事件类别 + 内容格式」，
 * 其余固定面（写入者角色、node、out 参数）由机制独占。契约：
 *   - fmt 为 printf 风格，可带 ≤1 组变参；内容上限 512 字节（与既有
 *     调用点最大缓冲一致，超限按 snprintf 截断，语义与改造前一致）
 *   - hall_store 句柄为空时静默丢弃（CLI 未装配事件流时的正常态）
 *   - 内容须为合法 JSON 对象字面量，本件不做转义（调用方保证）
 *
 * 依据：0.1.19 架构改进方案 §4.3（机制件收敛）、L5 塌缩；台账 §151。
 */

#include "cli_internal.h"

#include <stdarg.h>
#include <stdio.h>

void cli_hall_emit(const char *task_id, airy_hall_category_t cat,
                   const char *fmt, ...)
{
    if (!g_cli_hall_store || !fmt)
        return;

    char ev[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ev, sizeof(ev), fmt, ap);
    va_end(ap);

    airy_hall_store_write(g_cli_hall_store, "default", task_id, NULL, cat,
                          AIRY_HALL_WRITER_COGNITION, ev, NULL, 0);
}
