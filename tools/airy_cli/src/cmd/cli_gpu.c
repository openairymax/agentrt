// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_gpu.c
 * @brief 加速器能力快照（0.1.21 B-3）：doctor 的 GPU 段数据来源。
 *
 * 只读、无联网副作用，一次调用即完成 B-0 的三层探测。输出单行 JSON，
 * 供 POSIX shell 用 sed 取字段——容器内未必有 python/jq。驱动未绑定
 * （driver 为空）与版本缺失分别以 drv_bound、drivers 体现，doctor 据此
 * 区分「无 GPU / GPU 在但驱动不可用 / GPU 可用」三种结论。
 */

#include "cli_internal.h"
#include "platform_gpu.h"

#include <stdio.h>
#include <string.h>

static const char *gpu_verdict_name(airy_gpu_verdict_t v)
{
    switch (v) {
    case AIRY_GPU_R1:
        return "r1";
    case AIRY_GPU_R2:
        return "r2";
    default:
        return "r3";
    }
}

static const char *gpu_kind_tag(airy_accel_kind_t kind)
{
    switch (kind) {
    case AIRY_ACCEL_NVIDIA:
        return "nvidia";
    case AIRY_ACCEL_AMD:
        return "amd";
    case AIRY_ACCEL_INTEL:
        return "intel";
    case AIRY_ACCEL_VIRTUAL:
        return "virt";
    default:
        return "none";
    }
}

/* "nvidia(555.42),unbound" 形式：消费端按 , 切分。
 * 驱动未绑定的设备记为 unbound，供 doctor 计入不可用原因。 */
static void gpu_join_devs(const airy_gpu_cap_t *gpu, char *out, size_t osz)
{
    size_t n = 0;

    out[0] = '\0';
    for (uint32_t i = 0; i < gpu->dev_count && n + 1 < osz; i++) {
        const airy_gpu_dev_t *d = &gpu->devs[i];
        int w;

        if (!d->driver[0])
            w = snprintf(out + n, osz - n, "unbound,");
        else if (d->driver_ver[0])
            w = snprintf(out + n, osz - n, "%s(%s),", d->driver, d->driver_ver);
        else
            w = snprintf(out + n, osz - n, "%s,", d->driver);
        if (w < 0)
            break;
        n += (size_t)w;
    }
    if (n > 0 && out[n - 1] == ',')
        out[n - 1] = '\0';
}

/* 加速器类型去重后以 / 连接（多卡异构时 "nvidia/amd"）。 */
static void gpu_join_kinds(const airy_gpu_cap_t *gpu, char *out, size_t osz)
{
    size_t n = 0;

    out[0] = '\0';
    for (uint32_t i = 0; i < gpu->dev_count && n + 1 < osz; i++) {
        const char *tag = gpu_kind_tag(gpu->devs[i].kind);
        if (strstr(out, tag))
            continue;
        n += (size_t)snprintf(out + n, osz - n, n ? "/%s" : "%s", tag);
    }
}

int cli_cmd_gpu(int argc, char *argv[])
{
    airy_gpu_cap_t cap;
    char drivers[256];
    char accel[64];
    char esc[2][512];
    uint64_t vram = 0;
    uint32_t bound = 0;
    int json = g_cli_json_mode;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json = 1;
            continue;
        }
        cli_outf("airy_cli: gpu 不认识参数 %s\n", argv[i]);
        return 2;
    }
    if (airy_gpu_probe(&cap) != AIRY_SUCCESS) {
        cli_outf("airy_cli: 加速器探测失败\n");
        return 2;
    }
    for (uint32_t i = 0; i < cap.dev_count; i++) {
        if (cap.devs[i].driver[0])
            bound++;
        vram += cap.devs[i].vram_bytes;
    }
    gpu_join_devs(&cap, drivers, sizeof(drivers));
    gpu_join_kinds(&cap, accel, sizeof(accel));

    if (!json) {
        cli_outf("verdict=%s accel=%s devices=%u drv_bound=%u vram_mb=%llu "
                 "layers=0x%x backend=%s drivers=%s reason=%s\n",
                 gpu_verdict_name(cap.verdict), accel, cap.dev_count, bound,
                 (unsigned long long)(vram >> 20), cap.layers, cap.backend,
                 drivers, cap.reason);
        return 0;
    }
    cli_json_esc(esc[0], sizeof(esc[0]), cap.backend);
    cli_json_esc(esc[1], sizeof(esc[1]), cap.reason);
    cli_outf("{\"schema\":1,\"verdict\":\"%s\",\"accel\":\"%s\",\"devices\":%u,"
             "\"drv_bound\":%u,\"vram_mb\":%llu,\"layers\":%u,\"backend\":\"%s\","
             "\"drivers\":\"%s\",\"reason\":\"%s\"}\n",
             gpu_verdict_name(cap.verdict), accel, cap.dev_count, bound,
             (unsigned long long)(vram >> 20), cap.layers, esc[0], drivers, esc[1]);
    return 0;
}
