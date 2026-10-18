// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_test_globals.c
 * @brief CLI 测试目标共享的跨 TU 符号桩：g_cli_cancel / g_cli_print_mode。
 *
 * 两量正常由 src/core/main.c 拥有（S-02 SIGINT 取消标志与呈现模式开关）。
 * 测试可执行体不含 main.c，此前由各测试 TU 手写定义副本，目标源集扩张时
 * 漏配即链接失败（test_cli_errdesc 全缺、test_cli_live 漏 print_mode）。
 * 收敛为单一 stub 翻译单元，由 CMakeLists 按目标显式编入。
 */

#include <signal.h>

volatile sig_atomic_t g_cli_cancel = 0;
int g_cli_print_mode = 0;
