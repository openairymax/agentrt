/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_cfg.c
 * @brief model.yaml 子集读取（手写解析，BAN 合规：禁 sscanf）。
 *
 * 结构约定（ecosystem/manager/model/model.yaml，C-19 起 slots 为单一
 * 事实源；think.*_model 三键已废弃，仅作兼容回落）：
 *   llm:
 *     model: "your-model-id"
 *   slots:
 *     t2: ""
 * 无缩进段标题（"name:"）切换当前段；目标段内缩进的 "key: \"value\""
 * 行命中即返回。
 */

#include "cli_cfg.h"

#include "airy_memory.h"
#include "platform_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 原地去行尾 CR/LF，跳过行首空白，再把 "key: value" 切成键（拷入 kbuf）
 * 与值起点；非 "key:" 行（空行、无冒号、键为空或超长）返回 -1。 */
static int yaml_kv(char *line, char *kbuf, size_t kcap, const char **val)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
    const char *p = line;
    while (*p == ' ' || *p == '\t')
        p++;
    const char *ks = p;
    while (*p && *p != ':' && *p != ' ' && *p != '\t')
        p++;
    size_t kl = (size_t)(p - ks);
    if (*p != ':' || kl == 0 || kl >= kcap)
        return -1;
    AIRY_MEMCPY(kbuf, ks, kl);
    kbuf[kl] = '\0';
    const char *v = p + 1;
    while (*v == ' ' || *v == '\t')
        v++;
    *val = v;
    return 0;
}

/* 取标量值：引号包裹截到闭引号；无引号截到空白、注释符或行尾
 * （timeout_ms: 120000 等数字键）。 */
static void yaml_scalar(const char *after, char *out, size_t cap)
{
    const char *v;
    size_t vl = 0;

    out[0] = '\0';
    if (*after == '"') {
        v = after + 1;
        while (v[vl] && v[vl] != '"' && vl + 1 < cap)
            vl++;
    } else {
        v = after;
        while (v[vl] && v[vl] != ' ' && v[vl] != '\t' && v[vl] != '#' && vl + 1 < cap)
            vl++;
    }
    AIRY_MEMCPY(out, v, vl);
    out[vl] = '\0';
}

int cli_cfg_yaml_get(const char *path, const char *section, const char *key,
                     char *out, size_t cap)
{
    if (!path || !section || !key || !out || cap == 0)
        return -1;
    out[0] = '\0';

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    char line[512];
    char cur[32] = "";
    char kbuf[64];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        const char *after = NULL;

        if (yaml_kv(line, kbuf, sizeof(kbuf), &after) != 0)
            continue;
        if (*after == '\0') {
            /* 段标题行：key:（后无其他非空白） */
            if (strlen(kbuf) < sizeof(cur))
                AIRY_STRNCPY_TERM(cur, kbuf, sizeof(cur));
            continue;
        }
        if (strcmp(cur, section) != 0 || strcmp(key, kbuf) != 0)
            continue;
        yaml_scalar(after, out, cap);
        found = 1;
        break;
    }
    fclose(f);
    return found ? 0 : -1;
}

void cli_cfg_path(char *path, size_t cap)
{
    const char *cfg = getenv("AIRY_MODEL_CONFIG");
    if (cfg && cfg[0]) {
        snprintf(path, cap, "%s", cfg);
        return;
    }
    snprintf(path, cap, "%s/model.yaml", airy_config_dir());
}
