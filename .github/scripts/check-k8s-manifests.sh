#!/usr/bin/env bash
#
# check-k8s-manifests.sh — Kubernetes 清单静态校验（0.1.21 S30-P1-10）
#
# 收敛前：gateway/deploy/k8s 只有清单、没有任何校验作业，语法/字段错误只能
#   在客户集群 apply 时才暴露（「仅清单、未验证」）。本门禁用 kubeconform
#   严格模式（-strict，未知字段判错）在合入前否决非法清单。
# 判据：
#   - 目录不存在 → 环境错误（exit 2），防「目录被挪走却仍显示绿」；
#   - 目录存在但无清单 → 明确输出「不适用」并 exit 0；
#   - 清单存在 → 全部通过 kubeconform 才 exit 0。
# 未知 CRD（集群侧安装的 API）以 -ignore-missing-schemas 放行，汇总里的
#   skipped 计数照实输出，不吞掉信息。
# 依赖：curl + sha256sum/shasum；kubeconform 版本与校验和在下方固定，
#   下载后先验后再执行（供应链最小信任）。
# 退出：0 通过/不适用，1 清单不合规，2 环境或下载失败
set -uo pipefail

KC_VER="0.8.0"
KC_SHA="9bc2bffbf71f261128533edaf912153948b7ff238f9a531ae6d34466ec287883"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MAN="${ROOT}/gateway/deploy/k8s"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

die() {
    printf 'check-k8s: %s\n' "$*" >&2
    exit 2
}

# 摘要工具：Linux 用 sha256sum，macOS 用 shasum（同算法）。
sum256() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | cut -d' ' -f1
    else
        die "缺 sha256sum/shasum，无法校验下载物"
    fi
}

[ -d "$MAN" ] || die "清单目录缺失: ${MAN}"
shopt -s nullglob
FILES=("$MAN"/*.yaml "$MAN"/*.yml)
if [ ${#FILES[@]} -eq 0 ]; then
    printf 'check-k8s: 不适用——%s 下无清单\n' "$MAN"
    exit 0
fi

URL="https://github.com/yannh/kubeconform/releases/download/v${KC_VER}/kubeconform-linux-amd64.tar.gz"
curl -fsSL --connect-timeout 20 --max-time 180 -o "$TMP/kc.tgz" "$URL" \
    || die "下载失败: ${URL}"
GOT="$(sum256 "$TMP/kc.tgz")"
[ "$GOT" = "$KC_SHA" ] || die "校验和不符（期望 ${KC_SHA}，实得 ${GOT}）"
tar -xzf "$TMP/kc.tgz" -C "$TMP" kubeconform || die "解包失败"

printf 'check-k8s: 校验 %d 份清单（kubeconform %s, strict）\n' "${#FILES[@]}" "$KC_VER"
"$TMP/kubeconform" -strict -summary -ignore-missing-schemas "${FILES[@]}"
rc=$?
if [ $rc -ne 0 ]; then
    printf 'check-k8s: 清单不合规（rc=%s）\n' "$rc" >&2
    exit 1
fi
