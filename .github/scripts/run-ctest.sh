#!/usr/bin/env bash
#
# run-ctest.sh — 三平台 ctest 运行入口（唯一实现，0.1.21 S30-P1-9）
#
# 收敛前：Linux 两腿、macOS、Windows、sanitizer 各自内联 ctest 调用，
#   并发数、超时、失败取证、重试放行四处各写一份；任一处调整都会在另
#   三处留下不一致（超时口径不同 → 同一用例跨平台时红时绿）。
# 收敛后：workflow 只声明「跑哪条腿」，参数语义只在本文件定义；调用点
#   唯一，判据（含 flaky 一次性放行）可被单一处审计。
#
# 用法：bash .github/scripts/run-ctest.sh [选项] [-- <ctest 追加参数>]
#   --build-dir DIR  构建目录（默认 $BUILD_DIR，否则 ../build）
#   --log FILE       首轮日志（默认 $RUNNER_TEMP/ctest.log）
#   --timeout SEC    单用例超时（默认 240）
#   --jobs N         并发度（默认按 CPU 自动，绝不硬编码常量）
#   --config CFG     多配置生成器配置（Windows 传 Release）
#   --regex RE       仅跑匹配用例（ctest -R）
#   --exclude RE     排除匹配用例（ctest -E）
#   --rerun          仅重跑上次失败用例（ctest --rerun-failed）
#   --retry          首轮失败自动 rerun-failed 一次，全过按 flaky 放行
#   --tag NAME       注解前缀（默认 ctest）
#   --wrap CMD       以命令前缀包裹 ctest（如 HOME 隔离门禁 run --）
# 退出：0 通过（含 flaky 放行）/ 1 用例失败 / 2 参数或环境错误
set -uo pipefail

BLD="${BUILD_DIR:-../build}"
LOG="${RUNNER_TEMP:-/tmp}/ctest.log"
TMO=240
JOBS=""
CFG=""
RE=""
EX=""
RERUN=0
RETRY=0
TAG=ctest
WRAP=""
EXTRA=()

die() {
    printf 'run-ctest: %s\n' "$*" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) BLD="$2"; shift 2 ;;
        --log)       LOG="$2"; shift 2 ;;
        --timeout)   TMO="$2"; shift 2 ;;
        --jobs)      JOBS="$2"; shift 2 ;;
        --config)    CFG="$2"; shift 2 ;;
        --regex)     RE="$2"; shift 2 ;;
        --exclude)   EX="$2"; shift 2 ;;
        --rerun)     RERUN=1; shift ;;
        --retry)     RETRY=1; shift ;;
        --tag)       TAG="$2"; shift 2 ;;
        --wrap)      WRAP="$2"; shift 2 ;;
        --)          shift; EXTRA=("$@"); break ;;
        *)           die "未知参数: $1" ;;
    esac
done

[ -d "$BLD" ] || die "构建目录不存在: ${BLD}"
case "$TMO" in ''|*[!0-9]*) die "超时须为整数秒: ${TMO}" ;; esac
case "$LOG" in ''|*[!A-Za-z0-9_./:-]*) die "日志路径含非法字符: ${LOG}" ;; esac

# 并发度按实际核数自适应：容器/受限端侧不因硬编码 -j 把内存打爆，
# 大机器也不靠在 workflow 里重复写三份 nproc。
pcore() {
    if [ -n "$JOBS" ]; then printf '%s' "$JOBS"; return 0; fi
    if command -v nproc >/dev/null 2>&1; then nproc; return 0; fi
    if command -v sysctl >/dev/null 2>&1; then
        v=$(sysctl -n hw.ncpu 2>/dev/null) && [ -n "$v" ] && { printf '%s' "$v"; return 0; }
    fi
    printf '%s' "${NUMBER_OF_PROCESSORS:-2}"
}

# $1: 0 = 首轮/普通，1 = 仅重跑失败
build_cmd() {
    CMD=(ctest --test-dir "$BLD" --output-on-failure)
    [ -n "$CFG" ] && CMD+=(-C "$CFG")
    CMD+=(-j"$(pcore)" --timeout "$TMO")
    [ -n "$RE" ] && CMD+=(-R "$RE")
    [ -n "$EX" ] && CMD+=(-E "$EX")
    [ "$1" = 1 ] && CMD+=(--rerun-failed)
    CMD+=("${EXTRA[@]}")
}

exec_run() {
    local out="$1"
    if [ -n "$WRAP" ]; then
        bash -c "$WRAP \"\$@\"" _ "${CMD[@]}" >"$out" 2>&1
    else
        "${CMD[@]}" >"$out" 2>&1
    fi
}

show_fail() {
    local l="$1" rc="$2"
    printf '::error::%s ctest FAILED (rc=%s, log=%s)\n' "$TAG" "$rc" "$l"
    grep -E 'The following tests FAILED:|^[[:space:]]+[0-9]+ - |Errors while running CTest' \
        "$l" 2>/dev/null | head -40 | sed 's/^/::error::  /'
    grep -E '\[FAIL\]' "$l" 2>/dev/null | head -30 | sed 's/^/::error::  /'
    tail -n 40 "$l" 2>/dev/null | sed 's/^/::error::  /'
}

build_cmd "$RERUN"
exec_run "$LOG"
rc=$?
if [ $rc -eq 0 ]; then exit 0; fi

show_fail "$LOG" "$rc"
[ "$RETRY" = 1 ] || exit "$rc"

RL="${LOG%.log}-rerun.log"
[ "$RL" = "$LOG" ] && RL="${LOG}.rerun"
build_cmd 1
exec_run "$RL"
rc2=$?
if [ $rc2 -eq 0 ]; then
    printf '::warning::%s 首轮失败但 rerun-failed 全过（flaky 一次性放行）\n' "$TAG"
    exit 0
fi
show_fail "$RL" "$rc2"
exit "$rc2"
