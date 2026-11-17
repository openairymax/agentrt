#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
# ============================================================================
# doctor GPU 段三场景回归（0.1.21 B-3 / gate:runtime）
#
# 用桩 airy_cli 注入三种加速器结论，断言 airymaxrt doctor 的 GPU 段与选路
# 结论互不相同且各自可执行：
#   1) 有 GPU 且后端就绪      → GPU PASS + 选路「能力匹配」
#   2) 无 GPU                 → GPU PASS（明示按 CPU）+ 选路「远端 API」
#   3) GPU 在但驱动不可用     → GPU WARN（驱动未就绪）+ 选路「CPU 自托管」
#
# doctor 其余检查在本机可能 FAIL（无安装态二进制），本脚本只断言 GPU 段
# 与选路行，故以 `|| true` 吞掉退出码。
# ============================================================================

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
RT="${AIRYRT_LAUNCHER:-$HERE/../../latest/airymaxrt}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/bin" "$TMP/home/config"

PASS=0
FAIL=0
ok() { PASS=$((PASS + 1)); printf '  [PASS] %s\n' "$*"; }
bad() { FAIL=$((FAIL + 1)); printf '  [FAIL] %s\n' "$*"; }

cat >"$TMP/bin/airy_cli" <<'STUB'
#!/usr/bin/env bash
case "${1:-}" in
    paths)
        printf '{"schema":1,"home":"%s","bin":"%s/bin","lib":"%s/lib","run":"%s/run","log":"%s/log","config":"%s/config","data":"%s/data","tmp":"%s/tmp","cache":"%s/cache","workspace":"%s/ws"}\n' \
            "$TMPD" "$TMPD" "$TMPD" "$TMPD" "$TMPD" "$TMPD" "$TMPD" "$TMPD" "$TMPD" "$TMPD"
        ;;
    --version) printf 'airy_cli 0.1.21\n' ;;
    models) cat "$AIRY_TEST_ROWS" ;;
    gpu) cat "$AIRY_TEST_GPU" ;;
esac
STUB
chmod +x "$TMP/bin/airy_cli"
mkdir -p "$TMP/home/bin"
ln -s "$TMP/bin/airy_cli" "$TMP/home/bin/airy_cli"
export TMPD="$TMP"
# install.env 存在才会进入安装态检查分支；缺失时 doctor 的 sed 取值在
# pipefail 下会中止后续段落，GPU 段就读不到。
printf 'AIRY_VERSION=0.1.21\nAIRY_BIN_LINK=%s/bin/airymaxrt\n' "$TMP" \
    >"$TMP/home/config/install.env"
printf '{"name":"cpu-local","mode":"local","base_url":"","key_env":"","has_key":0}\n' \
    >"$TMP/rows_local.ndjson"
printf '{"name":"remote-a","mode":"api","base_url":"https://api.example/v1","key_env":"K1","has_key":1}\n' \
    >"$TMP/rows_api.ndjson"
printf '{"name":"remote-b","mode":"api","base_url":"https://api.example/v1","key_env":"K2","has_key":0}\n' \
    >"$TMP/rows_nokey.ndjson"
: >"$TMP/model.yaml"

# run_doctor <gpu-json> <rows-file> —— 返回 doctor 全部输出（含 stderr）
run_doctor() {
    printf '%s\n' "$1" >"$TMP/gpu.json"
    AIRY_HOME="$TMP/home" AIRY_MODEL_CONFIG="$TMP/model.yaml" \
        AIRY_CLI_BIN="$TMP/bin/airy_cli" AIRY_TEST_GPU="$TMP/gpu.json" \
        AIRY_TEST_ROWS="$2" AIRY_AUDIT_MAX_BYTES=1 \
        bash "$RT" doctor 2>&1 || true
}

expect() {
    local out="$1" pat="$2" desc="$3"
    if printf '%s' "$out" | grep -qF -- "$pat"; then ok "$desc"; else bad "$desc（缺: $pat）"; fi
}

# 场景 1：有 GPU，L1/L2/L3 打通，未声明 local → 能力匹配
G1='{"schema":1,"verdict":"r1","accel":"nvidia","devices":1,"drv_bound":1,"vram_mb":24576,"layers":7,"backend":"127.0.0.1:8000","drivers":"nvidia(555.42)","reason":""}'
O1="$(run_doctor "$G1" "$TMP/rows_api.ndjson")"
expect "$O1" "GPU: 加速器 nvidia，驱动 nvidia(555.42)，显存 24576MiB，后端 127.0.0.1:8000" "场景1 GPU 段 PASS"
expect "$O1" "选路: 能力匹配 → 127.0.0.1:8000" "场景1 选路=能力匹配"

# 场景 2：无 GPU，有已配密钥的远端 → 远端 API，并给出 GPU 降级原因
G2='{"schema":1,"verdict":"r3","accel":"none","devices":0,"drv_bound":0,"vram_mb":0,"layers":0,"backend":"","drivers":"","reason":"sysfs/drm 未检出 PCI 加速器"}'
O2="$(run_doctor "$G2" "$TMP/rows_api.ndjson")"
expect "$O2" "GPU: 未检出加速器" "场景2 GPU 段明示无卡"
expect "$O2" "选路: 远端 API → remote-a" "场景2 选路=远端 API"

# 场景 3：GPU 在但驱动未绑定，且远端无密钥 → CPU 自托管
G3='{"schema":1,"verdict":"r3","accel":"nvidia","devices":1,"drv_bound":0,"vram_mb":0,"layers":1,"backend":"","drivers":"unbound","reason":"加速器在，但 127.0.0.1:8000 无 OpenAI 兼容应答"}'
O3="$(run_doctor "$G3" "$TMP/rows_nokey.ndjson")"
expect "$O3" "驱动不可用" "场景3 GPU 段判驱动不可用"
expect "$O3" "选路: CPU 自托管" "场景3 选路=CPU 自托管"

# 显式配置优先：local provider 存在时即使 GPU 可用也回落显式配置
O4="$(run_doctor "$G1" "$TMP/rows_local.ndjson")"
expect "$O4" "选路: 显式配置 → cpu-local" "显式配置优先于能力匹配"

# 三场景结论必须两两不同（同一条 GPU 行不能混用）
g1="$(printf '%s' "$O1" | grep -F 'GPU:')"
g2="$(printf '%s' "$O2" | grep -F 'GPU:')"
g3="$(printf '%s' "$O3" | grep -F 'GPU:')"
if [ "$g1" != "$g2" ] && [ "$g2" != "$g3" ] && [ "$g1" != "$g3" ]; then
    ok "三场景 GPU 结论互不相同"
else
    bad "三场景 GPU 结论出现重复"
fi

printf '\ndoctor GPU 场景: %d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
