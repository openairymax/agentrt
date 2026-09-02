/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file depgraph_manifest.h
 * @brief deps.txt manifest 解析（M3 x-cutting-b 拆分）。
 */

#ifndef AIRY_DG_MANIFEST_H
#define AIRY_DG_MANIFEST_H

#include "depgraph_core.h"

/**
 * @brief 解析 deps.txt（手写 tokenizer，strtok 被 poison）。
 * @param allow_unknown 允许未在图中的依赖名（链接白名单模式）
 * @return 0 成功；-1 解析错误（错误已输出到 stderr）
 */
int dg_parse_manifest(const char *path, dg_manifest_t *mf, bool allow_unknown);

#endif /* AIRY_DG_MANIFEST_H */
