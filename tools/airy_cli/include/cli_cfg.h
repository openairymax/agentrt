/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_cfg.h
 * @brief airy_cli 配置面读取（model.yaml 子集解析）。
 *
 * 从 cli_think.c 抽出（0.1.21 D-8）：超时阶梯也要读 [timeout] 段，再在
 * 调用方各抄一份解析器会形成 3-copy 克隆簇。约定见 cli_cfg.c。
 */

#ifndef AIRY_CLI_CFG_H
#define AIRY_CLI_CFG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 取 section.key 标量；0 找到（含空串），-1 未找到/不可读。 */
int cli_cfg_yaml_get(const char *path, const char *section, const char *key,
                     char *out, size_t cap);

/** @brief 定位 model.yaml：AIRY_MODEL_CONFIG > $AIRY_HOME/config/model.yaml。 */
void cli_cfg_path(char *path, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_CFG_H */
