/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_prelude.h
 * @brief airy_cli 家族 include 前导块唯一真相源（SSoT）。
 *
 * chat/、core/、render/ 多个 .c 逐字重抄同一段「标准 C 头 + 平台屏蔽头
 * （Windows / POSIX）」前导块，构成全树 3-copy 克隆簇。本文件收敛该公共
 * 面：各消费 .c 在项目公共头之后仅保留本单行引用。
 *
 * 契约：
 *   - 仅收纳跨文件共有的标准 C 与平台屏蔽 include 面，不做任何类型、宏
 *     或函数定义
 *   - 专属头（cli_chat_internal.h、llm_service.h、cjson 等）一律留在消费
 *     方，不得并入本面以抬高家族扇入
 *   - 平台屏蔽口径对齐 commons/platform（airy 平台抽象层）：Windows 腿只
 *     暴露 <windows.h>，POSIX 腿给 <unistd.h> 与 <signal.h>
 *
 * 依据：0.1.19 架构改进方案 §4.3（机制件收敛）、L4 归位消解；台账 §150。
 */

#ifndef AIRY_CLI_PRELUDE_H
#define AIRY_CLI_PRELUDE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

#endif /* AIRY_CLI_PRELUDE_H */
