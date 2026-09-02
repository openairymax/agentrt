/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file depgraph_links.h
 * @brief 链接白名单门禁（M1-1b，M3 x-cutting-b 拆分）。
 *
 *   whitelist: "目标: 允许链接的项目内库..."（link-whitelist.txt 单一权威）
 *   actual   : CMake 侧 get_target_property(LINK_LIBRARIES) 生成的实际链接
 * 规则（fail-closed）：
 *   - 实际链接库在白名单"库全集"内但不在该目标允许集 → 链接越权，fail
 *   - 实际链接库不在库全集但以项目库前缀开头 → 未登记项目库，fail
 *   - 系统/第三方库（cjson/microhttpd/... 或路径形式）→ 忽略
 */

#ifndef AIRY_DG_LINKS_H
#define AIRY_DG_LINKS_H

#include "depgraph_core.h"

/**
 * @brief 链接白名单校验。
 * @return 0 通过；2 存在越权/未登记（fail-closed，错误已输出到 stderr）
 */
int dg_check_links(const dg_manifest_t *wl, const dg_manifest_t *actual);

#endif /* AIRY_DG_LINKS_H */
