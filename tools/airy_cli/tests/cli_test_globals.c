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
/* C-10（0.1.21）：cli_gw_rpc.c 的 --verbose 只读开关，主程序侧由 main.c 定义。 */
int g_cli_verbose = 0;
/* D-15（0.1.21）：事件流出口与静音开关，cli_render.c 与命令行解析共用。 */
int g_cli_events_mode = 0;
int g_cli_quiet = 0;
/* S30-P0-2（0.1.21）：--legacy-tui 开关，命令行解析读、运行时选前端； */
/* 默认宿主切 Rust TUI 后由 main.c 定义，测试目标同源补齐。 */
int g_cli_legacy_tui = 0;
