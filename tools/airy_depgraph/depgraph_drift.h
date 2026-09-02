/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file depgraph_drift.h
 * @brief include 漂移门禁：头文件索引 + 双向校验（M3 x-cutting-b 拆分）。
 *
 * deps.txt 头部声明"公共头引用的依赖必须在此声明，否则 include 漂移无法
 * 被门禁捕获"——本模块据实双向校验：① 子域公共头 include 了其他子域公共
 * 头而未声明 → 错误；② 声明的依赖在公共头中零实际 include → 陈旧边。
 */

#ifndef AIRY_DG_DRIFT_H
#define AIRY_DG_DRIFT_H

#include "depgraph_core.h"

/**
 * @brief 依据 manifest 节点名建立公共头索引（utils/<域>/、platform/include、
 *        include/ + include/airymax 契约层）。0d 扁平化后 utils/<域>/ 即公共头目录。
 */
void dg_index_build(dg_index_t *idx, const dg_manifest_t *mf);

/**
 * @brief 扫描子域全部公共头，返回未声明跨子域引用数；同时登记已使用的
 *        声明边（used_dep[]，防陈旧边）。
 */
size_t dg_scan_domain_includes(dg_index_t *idx, const dg_manifest_t *mf,
                               const dg_domain_t *dom, size_t *used_dep);

#endif /* AIRY_DG_DRIFT_H */
