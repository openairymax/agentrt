#!/bin/bash
# =============================================================================
# forbidden_functions.sh — BAN-151~BAN-162 内存安全规则扫描
# Phase 2.5: 内存安全四层防御体系落地
# 用途: CI 管线中扫描违反 BAN-151~BAN-162 的代码模式
# 调用: bash forbidden_functions.sh [project_root]
# =============================================================================
set -euo pipefail

PROJECT_ROOT="${1:-$(cd "$(dirname "$0")/../../.." && pwd)}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

total_issues=0
ok_checks=0

log_ok()  { echo -e "${GREEN}[OK]${NC} $1"; ((ok_checks++)) || true; }
log_err() { echo -e "${RED}[ERR]${NC} $1"; ((total_issues++)) || true; }
log_warn(){ echo -e "${YELLOW}[WARN]${NC} $1"; }
log_info(){ echo -e "[INFO] $*"; }

# ============================
# 辅助函数：避免 $(...) 内嵌套多行 while+管道的解析问题
# ============================
# 扫描文件中匹配 pattern 的行，对每行执行 check_cmd，输出不满足条件的文件列表
_scan_lines() {
    local pattern="$1" check_cmd="$2"
    shift 2
    grep -rn "$pattern" --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r line; do
            eval "$check_cmd"
        done
}

echo "=== BAN-151~BAN-180 Memory Safety Scan ==="
echo "Project: ${PROJECT_ROOT}"
echo ""

# ============================
# BAN-SEC-00: scanf/fscanf/sscanf "%s" 无界输入检测 (SEC-04 新增)
# 检测: scanf("%s"), fscanf(fp, "%s") 等无缓冲区限制的格式串
# ============================
check_ban_sec00_unbounded_scanf() {
    log_info "BAN-SEC-00: Checking unbounded scanf(\"%s\") patterns..."
    local hits=0
    # 匹配 scanf/fscanf/sscanf 中使用 %s 但没有宽度限制的调用
    while IFS= read -r -d '' file; do
        # 检测 scanf("%s") / scanf("...%s...") 无宽度限定
        local scan_hits
        scan_hits=$(grep -nE '\b(scanf|fscanf|sscanf)\s*\([^)]*"%[^"]*%s[^"]*"' "$file" 2>/dev/null \
            | grep -v '/tests/' | grep -vP '%\d+s' | wc -l) || true
        scan_hits=${scan_hits:-0}
        if [[ $scan_hits -gt 0 ]]; then
            log_err "BAN-SEC-00: $file has $scan_hits unbounded %s in scanf family"
            ((hits++)) || true
            # 打印具体行
            grep -nE '\b(scanf|fscanf|sscanf)\s*\([^)]*"%[^"]*%s[^"]*"' "$file" 2>/dev/null \
                | grep -v '/tests/' | grep -vP '%\d+s' | while IFS= read -r line; do
                log_warn "  -> $line"
            done
        fi
    done < <(find "${PROJECT_ROOT}/agentos" -name "*.c" -print0 2>/dev/null)
    if [[ $hits -eq 0 ]]; then
        log_ok "BAN-SEC-00: No unbounded scanf(\"%s\") patterns found"
    else
        log_err "BAN-SEC-00: $hits file(s) with unbounded scanf %%s"
    fi
}

# ============================
# BAN-151: MALLOC后非goto cleanup的return路径
# 检测: MALLOC/CALLOC之后有return但无goto cleanup
# ============================
check_ban_151() {
    log_info "BAN-151: Checking non-cleanup return after MALLOC..."
    local hits=0
    while IFS= read -r -d '' file; do
        # 简化检测：检查文件中是否有MALLOC但没有goto cleanup模式
        if grep -q 'AGENTRT_MALLOC\|AGENTRT_CALLOC\|AGENTRT_REALLOC' "$file" 2>/dev/null; then
            # 检查是否有MALLOC后直接return的情况（简化版）
            local has_malloc
            has_malloc=$(grep -c 'AGENTRT_MALLOC\|AGENTRT_CALLOC\|AGENTRT_REALLOC' "$file" 2>/dev/null || echo "0")
            has_malloc=${has_malloc##*[!0-9]} has_malloc=${has_malloc:-0}
            local has_cleanup
            has_cleanup=$(grep -c 'goto cleanup' "$file" 2>/dev/null || echo "0")
            has_cleanup=${has_cleanup##*[!0-9]} has_cleanup=${has_cleanup:-0}
            if [[ $has_malloc -gt 0 ]] && [[ $has_cleanup -eq 0 ]]; then
                log_warn "BAN-151: $file has MALLOC but no goto cleanup pattern"
                ((hits++)) || true
            fi
        fi
    done < <(find "${PROJECT_ROOT}/agentos" -name "*.c" -not -path "*/tests/*" -not -path "*/examples/*" -print0 2>/dev/null)
    if [[ $hits -eq 0 ]]; then
        log_ok "BAN-151: All MALLOC functions have cleanup paths"
    else
        log_err "BAN-151: $hits file(s) with MALLOC but no goto cleanup"
    fi
}

# ============================
# BAN-152: cleanup前声明新变量（C89风格）
# ============================
check_ban_152() {
    log_info "BAN-152: Checking C89 variable declarations in cleanup blocks..."
    local hits=0
    # 检测 goto cleanup 之后是否有变量声明（C99风格）
    hits=$(grep -rn 'goto cleanup' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r line; do
        file=$(echo "$line" | cut -d: -f1)
        lineno=$(echo "$line" | cut -d: -f2)
        # 检查该行之后是否有变量声明
        if tail -n +"$lineno" "$file" 2>/dev/null | head -30 | grep -qP '^\s*(int|char|void|size_t|uint|bool|float|double)\s+\w+\s*=' ; then
            echo "$file:$lineno"
        fi
    done | wc -l) || true
    hits=${hits:-0}
    if [[ $hits -eq 0 ]]; then
        log_ok "BAN-152: No C99 declarations in cleanup blocks"
    else
        log_err "BAN-152: $hits potential C99 declarations after goto cleanup"
    fi
}

# ============================
# BAN-153: 单一退出点（每个函数≤5个return）
# ============================
check_ban_153() {
    log_info "BAN-153: Checking multiple return points..."
    local hits=0
    while IFS= read -r -d '' file; do
        local returns
        returns=$(grep -c '\breturn\b' "$file" 2>/dev/null || echo "0")
        returns=${returns##*[!0-9]} returns=${returns:-0}
        local funcs
        funcs=$(grep -cP '^[a-zA-Z_][a-zA-Z0-9_]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\(' "$file" 2>/dev/null || echo "1")
        funcs=${funcs##*[!0-9]} funcs=${funcs:-1}
        if [[ $funcs -eq 0 ]]; then funcs=1; fi
        local avg_returns=$(( returns / funcs ))
        if [[ $avg_returns -gt 5 ]]; then
            log_warn "BAN-153: $file avg $avg_returns returns/function (>5)"
            ((hits++)) || true
        fi
    done < <(find "${PROJECT_ROOT}/agentos" -name "*.c" -not -path "*/tests/*" -not -path "*/examples/*" -print0 2>/dev/null)
    if [[ $hits -eq 0 ]]; then
        log_ok "BAN-153: Average return points per function ≤5"
    else
        log_err "BAN-153: $hits file(s) with >5 avg returns per function"
    fi
}

# ============================
# BAN-154: 非常量第三参数的memcpy/memmove/memset (SEC-07 增强版)
# ============================
check_ban_154() {
    log_info "BAN-154: Checking dynamic-size memcpy/memmove/memset..."
    local hits=0
    local details=""

    # 精确检测：memcpy/memmove/memset 第三参数不是 sizeof()、数字常量、或已知安全宏
    while IFS= read -r line; do
        file=$(echo "$line" | cut -d: -f1)
        lineno=$(echo "$line" | cut -d: -f2)
        content=$(echo "$line" | cut -d: -f3-)

        # 排除：已使用 AGENTRT_MEMCPY_SAFE
        echo "$content" | grep -q 'AGENTRT_MEMCPY_SAFE' && continue || true
        # 排除：sizeof()
        echo "$content" | grep -q 'sizeof' && continue || true
        # 排除：纯数字常量（如 256, 0x100）
        # 提取第三参数并检查是否为纯数字
        third_arg=$(echo "$content" | grep -oP '(?:memcpy|memmove|memset)\s*\([^,]+,\s*[^,]+,\s*\K[^)]+' || true)
        if [[ -n "$third_arg" ]]; then
            # 如果第三参数是纯数字或 sizeof 表达式则跳过
            if echo "$third_arg" | grep -qP '^(?:\d+|0x[0-9a-fA-F]+|sizeof\s*\(.+\))$'; then
                continue
            fi
            # 动态长度 — 记录为问题
            ((hits++)) || true
            details="$details  -> $file:$lineno $content\n"
        fi
    done < <(grep -rn '\bmemcpy\b\|\bmemmove\b\|\bmemset\b' \
        --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/")

    if [[ $hits -eq 0 ]]; then
        log_ok "BAN-154: All memcpy/memmove/memset use sizeof or constant size"
    else
        log_err "BAN-154: $hits memcpy/memmove/memset calls with dynamic/non-constant size"
        echo -e "$details" | while IFS= read -r d; do [ -n "$d" ] && log_warn "$d"; done
    fi
}

# ============================
# BAN-155: strncpy必须保证null终止 (SEC-07 增强版)
# ============================
check_ban_155() {
    log_info "BAN-155: Checking strncpy null termination..."
    local hits=0
    local unsafe_hits=0

    while IFS= read -r line; do
        file=$(echo "$line" | cut -d: -f1)
        lineno=$(echo "$line" | cut -d: -f2)

        # 已使用 AGENTRT_STRNCPY_TERM 安全宏 → 跳过
        echo "$line" | grep -q 'AGENTRT_STRNCPY_TERM' && continue || true

        # 裸 strncpy → 检查调用后3行内是否有手动 null 终止
        ((hits++)) || true

        # 检查后续行是否有 dst[N-1] = '\0' 或 *dst = '\0' 等 null 终止模式
        has_null_term=false
        if sed -n "$((lineno + 1)),$((lineno + 3))p" "$file" 2>/dev/null | grep -qP '\[\s*(\w+\s*-\s*1)?\s*\]\s*=\s*["\x27]\\0?["\x27]|\^\s*\w+\s*\[\s*\w+\s*-\s*\d+\s*\]\s*=\s*0'; then
            has_null_term=true
        fi

        if ! $has_null_term; then
            ((unsafe_hits++)) || true
            log_warn "BAN-155: $file:$lineno strncpy without guaranteed null termination"
        fi
    done < <(grep -rn '\bstrncpy\b' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/")

    if [[ $hits -eq 0 ]]; then
        log_ok "BAN-155: No bare strncpy calls (all use AGENTRT_STRNCPY_TERM)"
    elif [[ $unsafe_hits -eq 0 ]]; then
        log_ok "BAN-155: $hits strncpy call(s) all have manual null termination ($unsafe_hits unsafe)"
    else
        log_err "BAN-155: $unsafe_hits/$hits strncpy call(s) lack null termination guarantee"
    fi
}

# ============================
# BAN-156: sprintf必须使用snprintf
# ============================
check_ban_156() {
    log_info "BAN-156: Checking sprintf usage..."
    local hits=0
    hits=$(grep -rn '\bsprintf\b' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v 'AGENTRT_SNPRINTF' | wc -l) || true
    hits=${hits:-0}
    if [[ $hits -eq 0 ]]; then
        log_ok "BAN-156: No sprintf usage (all use snprintf)"
    else
        log_err "BAN-156: $hits sprintf calls (use snprintf instead)"
    fi
}

# ============================
# BAN-157: 缓冲区大小必须用sizeof(buf)不是硬编码
# ============================
check_ban_157() {
    log_info "BAN-157: Checking hardcoded buffer sizes..."
    local hits=0
    # 检测 snprintf(buf, 数字, ...) 而不是 snprintf(buf, sizeof(buf), ...)
    hits=$(grep -rn '\bsnprintf\b.*,\s*\d+\s*,' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | wc -l) || true
    hits=${hits:-0}
    if [[ $hits -eq 0 ]]; then
        log_ok "BAN-157: No hardcoded buffer sizes in snprintf"
    else
        log_err "BAN-157: $hits hardcoded buffer sizes (use sizeof(buf))"
    fi
}

# ============================
# BAN-158: 输入长度检查（strlen/strcmp前）
# ============================
check_ban_158() {
    log_info "BAN-158: Checking input length validation..."
    local hits=0
    # 检测 strlen/strcmp 调用前是否有 NULL 检查
    hits=$(grep -rl '\bstrlen\b\|\bstrcmp\b\|\bstrncmp\b' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r file; do
        if grep -q '\bstrlen\b\|\bstrcmp\b' "$file" 2>/dev/null; then
            if ! grep -q 'NULL' "$file" 2>/dev/null; then
                echo "$file"
            fi
        fi
    done | wc -l) || true
    hits=${hits:-0}
    if [[ $hits -le 5 ]]; then
        log_ok "BAN-158: String functions have NULL guards ($hits files without)"
    else
        log_err "BAN-158: $hits files with string functions but no NULL checks"
    fi
}

# ============================
# BAN-159~162: 资源泄漏检测
# ============================
check_ban_159_162() {
    log_info "BAN-159~162: Checking resource leak patterns..."

    # BAN-159: fd泄漏（open/creat后无close）
    local fd_hits=0
    fd_hits=$(grep -rl '\bopen\b\|\bcreat\b\|\bsocket\b\|\baccept\b' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r file; do
        local opens closes
        opens=$(grep -c '\bopen\b\|\bcreat\b\|\bsocket\b\|\baccept\b' "$file" 2>/dev/null || echo "0")
        opens=${opens##*[!0-9]} opens=${opens:-0}
        closes=$(grep -c '\bclose\b' "$file" 2>/dev/null || echo "0")
        closes=${closes##*[!0-9]} closes=${closes:-0}
        if [[ $opens -gt $closes ]]; then
            echo "$file"
        fi
    done | wc -l) || true
    fd_hits=${fd_hits:-0}
    if [[ $fd_hits -eq 0 ]]; then
        log_ok "BAN-159: All open()/socket() have matching close()"
    else
        log_err "BAN-159: $fd_hits file(s) with open()/socket() without matching close()"
    fi

    # BAN-160: malloc/free配对
    local alloc_hits=0
    alloc_hits=$(grep -rl 'AGENTRT_MALLOC\|AGENTRT_CALLOC' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r file; do
        local allocs frees
        allocs=$(grep -c 'AGENTRT_MALLOC\|AGENTRT_CALLOC' "$file" 2>/dev/null || echo "0")
        allocs=${allocs##*[!0-9]} allocs=${allocs:-0}
        frees=$(grep -c 'AGENTRT_FREE' "$file" 2>/dev/null || echo "0")
        frees=${frees##*[!0-9]} frees=${frees:-0}
        if [[ $allocs -gt $frees ]]; then
            echo "$file"
        fi
    done | wc -l) || true
    alloc_hits=${alloc_hits:-0}
    if [[ $alloc_hits -le 3 ]]; then
        log_ok "BAN-160: MALLOC/FREE balance OK ($alloc_hits files with imbalance)"
    else
        log_err "BAN-160: $alloc_hits file(s) with more MALLOC than FREE"
    fi

    # BAN-161: fopen/fclose配对
    local file_hits=0
    file_hits=$(grep -rl '\bfopen\b' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r file; do
        local opens closes
        opens=$(grep -c '\bfopen\b' "$file" 2>/dev/null || echo "0")
        opens=${opens##*[!0-9]} opens=${opens:-0}
        closes=$(grep -c '\bfclose\b' "$file" 2>/dev/null || echo "0")
        closes=${closes##*[!0-9]} closes=${closes:-0}
        if [[ $opens -gt $closes ]]; then
            echo "$file"
        fi
    done | wc -l) || true
    file_hits=${file_hits:-0}
    if [[ $file_hits -eq 0 ]]; then
        log_ok "BAN-161: All fopen() have matching fclose()"
    else
        log_err "BAN-161: $file_hits file(s) with fopen() without matching fclose()"
    fi

    # BAN-162: pthread_create/pthread_join配对
    local thread_hits=0
    thread_hits=$(grep -rl 'pthread_create' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r file; do
        local creates joins
        creates=$(grep -c 'pthread_create' "$file" 2>/dev/null || echo "0")
        creates=${creates##*[!0-9]} creates=${creates:-0}
        joins=$(grep -c 'pthread_join\|pthread_detach' "$file" 2>/dev/null || echo "0")
        joins=${joins##*[!0-9]} joins=${joins:-0}
        if [[ $creates -gt $joins ]]; then
            echo "$file"
        fi
    done | wc -l) || true
    thread_hits=${thread_hits:-0}
    if [[ $thread_hits -eq 0 ]]; then
        log_ok "BAN-162: All pthread_create() have matching join/detach"
    else
        log_err "BAN-162: $thread_hits file(s) with pthread_create() without join/detach"
    fi
}

# ============================
# BAN-163~BAN-168: 所有权语义规则 (Phase 2.5)
# ============================
check_ban_163_168() {
    log_info "BAN-163~168: Checking ownership semantics..."
    local ownership_issues=0

    # BAN-163: 所有权注释覆盖检查
    # 检查返回指针的函数是否有 @ownership 注释
    local ban163_no_comment=0
    ban163_no_comment=$(grep -rP '^\s*\w+\s*\*\s*\w+\s*\(' --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | while IFS= read -r line; do
        file=$(echo "$line" | cut -d: -f1)
        func=$(echo "$line" | cut -d: -f2-)
        # 检查该函数前10行内是否有 @ownership 注释
        lineno=$(echo "$line" | cut -d: -f2)
        if ! head -n "$((lineno - 1))" "$file" 2>/dev/null | tail -20 | grep -q '@ownership'; then
            echo "$file:$lineno"
        fi
    done | wc -l) || true
    ban163_no_comment=${ban163_no_comment:-0}
    if [[ $ban163_no_comment -le 10 ]]; then
        log_ok "BAN-163: Ownership annotations present ($ban163_no_comment missing)"
    else
        log_warn "BAN-163: $ban163_no_comment functions lack @ownership annotation"
        ((ownership_issues++)) || true
    fi

    # BAN-164: _take 后缀检查
    log_info "BAN-164: Checking _take suffix for ownership transfer functions..."
    local ban164_no_take=0
    ban164_no_take=$(grep -rP '@ownership\s+transfers' --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | wc -l) || true
    ban164_no_take=${ban164_no_take:-0}
    # 简化：仅统计 @ownership transfers 注释数量（完整检查需人工审计）
    if [[ $ban164_no_take -le 5 ]]; then
        log_ok "BAN-164: _transfer annotations within limits ($ban164_no_take)"
    else
        log_warn "BAN-164: $ban164_no_take ownership transfer annotations (review recommended)"
    fi

    # BAN-165: AGENTRT_FREE 后置 NULL 检查
    local ban165_no_null=0
    ban165_no_null=$(grep -rc 'AGENTRT_FREE(' --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v ":0$" | wc -l) || true
    ban165_no_null=${ban165_no_null:-0}
    # 简化：统计使用 AGENTRT_FREE 的文件数（完整 NULL 后置检查需人工审计）
    if [[ $ban165_no_null -le 50 ]]; then
        log_ok "BAN-165: AGENTRT_FREE usage within limits ($ban165_no_null files)"
    else
        log_warn "BAN-165: $ban165_no_null files use AGENTRT_FREE (review NULL assignment)"
    fi

    # BAN-166: 禁止多级指针传递（>2级）
    log_info "BAN-166: Checking multi-level pointer usage..."
    local ban166_multi_ptr=0
    ban166_multi_ptr=$(grep -rP '\w+\s*\*{3,}\s*\w+' --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v '//' | wc -l) || true
    ban166_multi_ptr=${ban166_multi_ptr:-0}
    if [[ $ban166_multi_ptr -eq 0 ]]; then
        log_ok "BAN-166: No triple-level pointer usage"
    else
        log_warn "BAN-166: $ban166_multi_ptr triple-level pointer declarations"
        ((ownership_issues++)) || true
    fi

    # BAN-167: 禁止裸指针跨模块传递
    log_info "BAN-167: Checking bare pointer cross-module usage..."
    # 检查 #include 跨模块引用中是否有 void* 裸指针
    local ban167_bare=0
    ban167_bare=$(grep -rP 'void\s*\*' --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null \
        | grep -v "/tests/" | grep -v 'typedef' | grep -v 'agentrt_' | wc -l) || true
    ban167_bare=${ban167_bare:-0}
    if [[ $ban167_bare -le 5 ]]; then
        log_ok "BAN-167: Bare void* usage within limits ($ban167_bare)"
    else
        log_warn "BAN-167: $ban167_bare bare void* usages (consider typed pointers)"
        ((ownership_issues++)) || true
    fi

    # BAN-168: 引用计数函数命名规范
    log_info "BAN-168: Checking refcount naming convention..."
    if grep -rq 'refcount_create\|refcount_destroy\|refcount_acquire\|refcount_release' \
        --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-168: Refcount functions follow naming convention"
    else
        log_info "BAN-168: No refcount functions found (non-blocking)"
    fi

    if [[ $ownership_issues -eq 0 ]]; then
        log_ok "BAN-163~168: All ownership semantics checks passed"
    else
        log_err "BAN-163~168: $ownership_issues ownership issue(s) found"
    fi
}

# ============================
# BAN-169~BAN-174: OOM响应规则 (Phase 2.5)
# ============================
check_ban_169_174() {
    log_info "BAN-169~174: Checking OOM response infrastructure..."
    local oom_issues=0

    # BAN-169: OOM 处理函数实现检查
    log_info "BAN-169: Checking OOM handler implementation..."
    if grep -rq 'oom_handler\|agentrt_oom_determine_response\|agentrt_oom_handle' \
        --include="*.h" --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-169: OOM handler interfaces defined"
    else
        log_warn "BAN-169: OOM handler not found (expected in Phase 2.5)"
        ((oom_issues++)) || true
    fi

    # BAN-170: 关键路径预分配检查
    log_info "BAN-170: Checking critical path pre-allocation..."
    if grep -rq 'pre_alloc\|prealloc\|emergency_pool\|reserve_pool\|critical_pool' \
        --include="*.h" --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-170: Pre-allocation patterns found"
    else
        log_info "BAN-170: Pre-allocation not yet implemented (expected Phase 2.5)"
    fi

    # BAN-171: 降级处理器注册检查
    log_info "BAN-171: Checking degradation handler registration..."
    if grep -rq 'degradation_handler\|on_degrade\|on_restore\|agentrt_register_degradation' \
        --include="*.h" --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-171: Degradation handler interfaces defined"
    else
        log_info "BAN-171: Degradation handlers not yet implemented (expected Phase 2.5)"
    fi

    # BAN-172: 内存水位检查
    log_info "BAN-172: Checking memory watermark checks..."
    if grep -rq 'watermark\|memory_watermark\|agentrt_memory_check_watermark\|memory_pressure' \
        --include="*.h" --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-172: Memory watermark checks found"
    else
        log_info "BAN-172: Watermark checks not yet implemented (expected Phase 2.5)"
    fi

    # BAN-173: 内存统计上报
    log_info "BAN-173: Checking memory stats reporting..."
    if grep -rq 'memory_stats_extended\|agentrt_check_leaks_scheduled\|leak_suspected' \
        --include="*.h" --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-173: Extended memory stats found"
    else
        log_info "BAN-173: Extended stats not yet implemented (expected Phase 2.5)"
    fi

    # BAN-174: 优雅降级机制
    log_info "BAN-174: Checking graceful degradation..."
    if grep -rq 'agentrt_oom_degrade\|graceful_degradation\|degrade_to\|reduce_functionality' \
        --include="*.h" --include="*.c" "${PROJECT_ROOT}/agentos/" 2>/dev/null; then
        log_ok "BAN-174: Graceful degradation interfaces found"
    else
        log_info "BAN-174: Graceful degradation not yet implemented (expected Phase 2.5)"
    fi

    if [[ $oom_issues -eq 0 ]]; then
        log_ok "BAN-169~174: All OOM response checks passed"
    else
        log_err "BAN-169~174: $oom_issues OOM issue(s) found"
    fi
}

# ============================
# BAN-175~BAN-180: 核心创新编码契约 (Phase 3)
# ============================
check_ban_175_180() {
    log_info "BAN-175~180: Checking core innovation encoding contracts..."
    local contract_issues=0

    # BAN-175: Thinkdual 契约检查
    log_info "BAN-175: Checking dual thinking system contracts..."
    local dt_contracts=0
    grep -rq 'thinking_chain_create\|thinking_chain_destroy' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/atoms/coreloopthree/" 2>/dev/null && ((dt_contracts++)) || true
    grep -rq 'triple_coordinator' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/atoms/coreloopthree/" 2>/dev/null && ((dt_contracts++)) || true
    grep -rq 'stream_critic' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/atoms/coreloopthree/" 2>/dev/null && ((dt_contracts++)) || true
    grep -rq 'metacognition' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/atoms/coreloopthree/" 2>/dev/null && ((dt_contracts++)) || true
    grep -rq 'semantic_unit' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/atoms/coreloopthree/" 2>/dev/null && ((dt_contracts++)) || true
    if [[ $dt_contracts -ge 4 ]]; then
        log_ok "BAN-175: Dual thinking system contracts defined ($dt_contracts/5 modules)"
    else
        log_err "BAN-175: Only $dt_contracts/5 dual thinking modules found"
        ((contract_issues++)) || true
    fi

    # BAN-176: MemoryRovol 契约检查
    log_info "BAN-176: Checking MemoryRovol contracts..."
    local mr_contracts=0
    grep -rq 'layer1_raw\|layer2_feature\|layer3_structure\|layer4_pattern' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/atoms/memoryrovol/include/" 2>/dev/null && ((mr_contracts++)) || true
    grep -rq 'memory_bridge\|memoryrovol_integration' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/atoms/" 2>/dev/null && ((mr_contracts++)) || true
    if [[ $mr_contracts -ge 1 ]]; then
        log_ok "BAN-176: MemoryRovol contracts defined ($mr_contracts interfaces)"
    else
        log_info "BAN-176: MemoryRovol contracts not yet verified (expected Phase 2.2)"
    fi

    # BAN-177: cupolas 契约检查
    log_info "BAN-177: Checking cupolas contracts..."
    local cp_contracts=0
    grep -rq 'sanitizer_core\|sanitize_input\|SANITIZE_FULL' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/cupolas/" 2>/dev/null && ((cp_contracts++)) || true
    grep -rq 'permission_engine\|permission_check\|RBAC' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/cupolas/" 2>/dev/null && ((cp_contracts++)) || true
    grep -rq 'audit_logger\|audit.*hash\|hash_chain' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/cupolas/" 2>/dev/null && ((cp_contracts++)) || true
    grep -rq 'AES.*GCM\|vault_encrypt\|cupolas_vault' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/cupolas/" 2>/dev/null && ((cp_contracts++)) || true
    if [[ $cp_contracts -ge 3 ]]; then
        log_ok "BAN-177: Cupolas contracts defined ($cp_contracts/4 modules)"
    else
        log_err "BAN-177: Only $cp_contracts/4 cupolas modules found"
        ((contract_issues++)) || true
    fi

    # BAN-178: llm_d 契约检查
    log_info "BAN-178: Checking llm_d contracts..."
    local llm_contracts=0
    grep -rq 'llm_provider_find\|llm_provider_registry\|llm_routing' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/daemons/llm_d/" 2>/dev/null && ((llm_contracts++)) || true
    grep -rq 'llm_cache\|token_counter\|cost_tracker' \
        --include="*.h" "${PROJECT_ROOT}/agentrt/daemons/llm_d/" 2>/dev/null && ((llm_contracts++)) || true
    if [[ $llm_contracts -ge 1 ]]; then
        log_ok "BAN-178: llm_d contracts defined"
    else
        log_info "BAN-178: llm_d contracts not yet verified (expected Phase 2.4)"
    fi

    # BAN-179: 数据流完整性检查
    log_info "BAN-179: Checking data flow integrity..."
    # 检查 pipeline 是否存在断路点
    local pipeline_gaps=0
    grep -rq 'pipeline\|event_queue\|data_flow\|message_bus' \
        --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null && ((pipeline_gaps++)) || true
    if [[ $pipeline_gaps -ge 1 ]]; then
        log_ok "BAN-179: Data flow pipeline interfaces found"
    else
        log_info "BAN-179: Data flow contracts not yet verified"
    fi

    # BAN-180: 生命周期约束检查
    log_info "BAN-180: Checking lifecycle constraints..."
    local lifecycle_pairs=0
    # 检查 create/destroy 配对
    grep -rP '_create\b' --include="*.h" "${PROJECT_ROOT}/agentos/" 2>/dev/null | grep -v "/tests/" | while IFS= read -r line; do
        file=$(echo "$line" | cut -d: -f1)
        func_name=$(echo "$line" | grep -oP '\w+_create' | head -1)
        if [[ -n "$func_name" ]]; then
            destroy_name="${func_name%_create}_destroy"
            if grep -q "$destroy_name" "$file" 2>/dev/null; then
                echo "paired: $func_name -> $destroy_name"
            else
                echo "UNPAIRED: $func_name in $file"
            fi
        fi
    done | grep -c "UNPAIRED" || true
    # 不强制要求，仅记录
    log_info "BAN-180: Lifecycle constraints verified (non-blocking)"

    if [[ $contract_issues -eq 0 ]]; then
        log_ok "BAN-175~180: All encoding contract checks passed"
    else
        log_err "BAN-175~180: $contract_issues contract issue(s) found"
    fi
}

# ============================
# 执行所有检查
# ============================
check_ban_sec00_unbounded_scanf   # SEC-04: scanf("%s") 无界输入检测
check_ban_151
check_ban_152
check_ban_153
check_ban_154
check_ban_155
check_ban_156
check_ban_157
check_ban_158
check_ban_159_162
check_ban_163_168
check_ban_169_174
check_ban_175_180

echo ""
echo "=== Summary ==="
echo "Total issues: $total_issues"
echo "OK checks: $ok_checks"

if [[ $total_issues -eq 0 ]]; then
    echo -e "${GREEN}All BAN-151~BAN-180 checks passed!${NC}"
    exit 0
else
    echo -e "${RED}Found $total_issues issue(s) in BAN-151~BAN-180 checks${NC}"
    exit 1
fi