// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_clock.h
 * @brief 单调时钟：CLI 内所有毫秒级截止时间、心跳与超时的唯一时间源。
 *
 * 时钟是机制件而非渲染件：事件流、SSE、存活节拍与生命周期协调都依赖
 * 它，但不应因此把整个渲染域拖入链接面（0.1.21 D-15 实测）。
 */

#ifndef AIRY_CLI_CLOCK_H
#define AIRY_CLI_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 单调毫秒计数（不受系统时间回拨影响）。
 */
uint64_t cli_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_CLOCK_H */
