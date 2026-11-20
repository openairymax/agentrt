// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_think.c
 * @brief airy_cli dual-thinking slot resolution and the think wait budget.
 *
 * Plan generation moved to gateway `task.submit` (airy_cli_taskflow.c), so
 * the local think.process chain is retired and no longer assembled here.
 * Two mechanisms remain consumed: slot resolution shared with think_d (the
 * chat surface renders the effective models) and the /orch wait budget
 * resolved by the timeout ladder (0.1.21 D-8).
 */

#include "cli_internal.h"

#include "airy_defaults.h"
#include "cli_cfg.h"    /* 0.1.21 D-8：model.yaml 子集读取已抽到 cfg 模块 */
#include "cli_timeout.h" /* 0.1.21 D-8：超时阶梯单一事实源 */

#include "cli_prelude.h"

/* ==================== 双思考三模型配置统一读取 ==================== */

/* env 读取仪式（AIRY_MODEL_T2/T1F/T1P）：清零后逐项覆盖非空值。
 * 两配置读取路径（load/explicit）共用，避免逐字重复。 */
static void cli_think_env(char *t2, size_t t2c, char *t1f, size_t t1fc,
                          char *t1p, size_t t1pc)
{
    t2[0] = t1f[0] = t1p[0] = '\0';

    const char *e2 = getenv("AIRY_MODEL_T2");
    const char *e1f = getenv("AIRY_MODEL_T1F");
    const char *e1p = getenv("AIRY_MODEL_T1P");
    if (e2 && e2[0])
        snprintf(t2, t2c, "%s", e2);
    if (e1f && e1f[0])
        snprintf(t1f, t1fc, "%s", e1f);
    if (e1p && e1p[0])
        snprintf(t1p, t1pc, "%s", e1p);
}

/* 三项均非空即视为已由 env/slots 确定。 */
static int think_all_set(const char *t2, const char *t1f, const char *t1p)
{
    return t2[0] && t1f[0] && t1p[0];
}

/* slots 段覆盖空槽位。 */
static void think_fill_slots(const char *path, char *t2, size_t t2c,
                             char *t1f, size_t t1fc, char *t1p, size_t t1pc)
{
    if (t2[0] == '\0')
        cli_cfg_yaml_get(path, "slots", "t2", t2, t2c);
    if (t1f[0] == '\0')
        cli_cfg_yaml_get(path, "slots", "t1f", t1f, t1fc);
    if (t1p[0] == '\0')
        cli_cfg_yaml_get(path, "slots", "t1p", t1p, t1pc);
}

/* 兼容窗口：slots 未覆盖时回落废弃 think.*_model（think_d 侧同源
 * 回落并告警，CLI 只读展示不再提示第二次）。 */
static void think_fill_legacy(const char *path, char *t2, size_t t2c,
                              char *t1f, size_t t1fc, char *t1p, size_t t1pc)
{
    if (t2[0] == '\0')
        cli_cfg_yaml_get(path, "think", "think2_slow_model", t2, t2c);
    if (t1f[0] == '\0')
        cli_cfg_yaml_get(path, "think", "think1_fast_model", t1f, t1fc);
    if (t1p[0] == '\0')
        cli_cfg_yaml_get(path, "think", "think1_prof_model", t1p, t1pc);
}

/* think 段留空 = 默认模型（v2：顶层 default_model 或 llm.model）。 */
static void think_fill_default(const char *path, char *t2, size_t t2c,
                               char *t1f, size_t t1fc, char *t1p, size_t t1pc)
{
    char def_model[128] = "";

    cli_cfg_yaml_get(path, "llm", "model", def_model, sizeof(def_model));
    if (def_model[0] == '\0')
        cli_cfg_yaml_get(path, "", "default_model", def_model, sizeof(def_model));
    if (def_model[0]) {
        if (t2[0] == '\0')
            snprintf(t2, t2c, "%s", def_model);
        if (t1f[0] == '\0')
            snprintf(t1f, t1fc, "%s", def_model);
        if (t1p[0] == '\0')
            snprintf(t1p, t1pc, "%s", def_model);
    }
}

/* 双思考三模型配置统一读取，CLI 侧真相源对齐 think_d。
 *
 * 优先级（与 think_d svc_slot_resolve 同序）：env AIRY_MODEL_T2/T1F/T1P
 * > $AIRY_MODEL_CONFIG（或 $AIRY_HOME/config/model.yaml）顶层 slots 段
 * > 废弃 think.*_model 键 > 该文件 llm.model 默认（槽位留空 = 使用
 * 默认模型）。
 *
 * 2026-08-19：CLI 此前只读 env，与 think_d 配置脱节——think 段留空
 * 时 CLI 显示"默认"而 think_d 实际用 llm.model；现统一为真实生效
 * 模型。调用方对空串按原语义处理（显示"默认"/传 NULL 走 provider
 * default）。 */
void cli_think_cfg_load(char *t2, size_t t2c, char *t1f, size_t t1fc,
                        char *t1p, size_t t1pc)
{
    if (!t2 || !t1f || !t1p || t2c == 0 || t1fc == 0 || t1pc == 0)
        return;
    cli_think_env(t2, t2c, t1f, t1fc, t1p, t1pc);
    if (think_all_set(t2, t1f, t1p))
        return;

    char path[AIRY_PATH_MAX];
    cli_cfg_path(path, sizeof(path));

    think_fill_slots(path, t2, t2c, t1f, t1fc, t1p, t1pc);
    think_fill_legacy(path, t2, t2c, t1f, t1fc, t1p, t1pc);
    if (think_all_set(t2, t1f, t1p))
        return;
    think_fill_default(path, t2, t2c, t1f, t1fc, t1p, t1pc);
}

/* ── /orch 等待预算（T-19）────────────────────────────────────────── */
/* D-8（0.1.21）：解析与钳制统一由 cli_timeout 阶梯承担（think 档 =
 * outer(think 层)，env AIRY_THINK_TIMEOUT_MS / model.yaml [think]
 * timeout_ms 仍按原优先级生效）。此处保留原签名供 /orch 等调用方。 */
uint32_t cli_think_timeout_ms(void)
{
    return (uint32_t)cli_tmo_ms(CLI_TMO_THINK);
}
