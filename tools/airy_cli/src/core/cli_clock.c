// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_clock.c
 * @brief 单调时钟实现（0.1.21 D-15：自渲染域抽出，机制面独立成件）。
 */

#include "cli_clock.h"

#include "platform_misc.h"

uint64_t cli_now_ms(void)
{
    return airy_time_ns() / 1000000ULL;
}
