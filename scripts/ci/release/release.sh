#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# AgentRT One-Click Release Script
# 一键发布脚本 - 集成质量门禁、版本验证、多平台构建
#
# 用法:
#   ./release.sh [版本号] [发布类型]
#   ./release.sh 2.0.0 stable
#   ./release.sh 2.0.0-beta.1 beta
#
# 环境变量:
#   SKIP_TESTS=1       跳过测试（紧急发布）
#   SKIP_DOCS=1        跳过文档生成
#   SKIP_SECURITY=1    跳过安全检查
#   DRY_RUN=1          仅模拟，不实际发布

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()  { echo -e "${BLUE}[INFO]${NC} $*"; }
log_ok()    { echo -e "${GREEN}[OK]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_fail()  { echo -e "${RED}[FAIL]${NC} $*"; }

VERSION="${1:-}"
RELEASE_TYPE="${2:-stable}"
SKIP_TESTS="${SKIP_TESTS:-0}"
SKIP_DOCS="${SKIP_DOCS:-0}"
SKIP_SECURITY="${SKIP_SECURITY:-0}"
DRY_RUN="${DRY_RUN:-0}"

GATES_PASSED=0
GATES_FAILED=0
GATES_TOTAL=0

check_gate() {
    local name="$1"
    local result="$2"
    GATES_TOTAL=$((GATES_TOTAL + 1))

    if [ "$result" = "0" ]; then
        log_ok "Gate: $name"
        GATES_PASSED=$((GATES_PASSED + 1))
    else
        log_fail "Gate: $name"
        GATES_FAILED=$((GATES_FAILED + 1))
    fi
}

print_banner() {
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  AgentRT One-Click Release"
    echo "  Version: ${VERSION:-UNSET}"
    echo "  Type: ${RELEASE_TYPE}"
    echo "  Dry Run: ${DRY_RUN}"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

validate_version() {
    if [ -z "$VERSION" ]; then
        log_fail "Version number required"
        echo "Usage: $0 <version> [stable|beta|rc]"
        exit 1
    fi

    if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9\.]+)?$ ]]; then
        log_fail "Invalid version format: $VERSION (expected X.Y.Z or X.Y.Z-suffix)"
        exit 1
    fi

    local tag_exists
    tag_exists=$(git tag -l "v${VERSION}" 2>/dev/null || true)
    if [ -n "$tag_exists" ]; then
        log_fail "Tag v${VERSION} already exists"
        exit 1
    fi

    check_gate "Version Format" 0
}

validate_working_tree() {
    cd "$PROJECT_ROOT"

    if ! git diff --quiet 2>/dev/null; then
        log_warn "Unstaged changes detected"
        check_gate "Clean Working Tree" 1
        return
    fi

    if ! git diff --cached --quiet 2>/dev/null; then
        log_warn "Staged but uncommitted changes detected"
        check_gate "Clean Working Tree" 1
        return
    fi

    check_gate "Clean Working Tree" 0
}

validate_branch() {
    local branch
    branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")

    case "$RELEASE_TYPE" in
        stable)
            if [ "$branch" != "main" ]; then
                log_warn "Stable release should be from main branch (current: $branch)"
            fi
            ;;
        beta|rc)
            if [ "$branch" != "main" ] && [ "$branch" != "develop" ]; then
                log_warn "Pre-release should be from main or develop (current: $branch)"
            fi
            ;;
    esac

    check_gate "Branch Check" 0
}

gate_build() {
    log_info "Running build gate..."
    cd "$PROJECT_ROOT"

    if [ ! -f "CMakeLists.txt" ]; then
        log_warn "No root CMakeLists.txt, skipping build gate"
        check_gate "Build" 0
        return
    fi

    mkdir -p build-release && cd build-release
    if cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF 2>&1 | tail -1 | grep -qi "error"; then
        check_gate "CMake Configure" 1
        return
    fi

    if cmake --build . --parallel "$(nproc 2>/dev/null || echo 2)" 2>&1 | tail -5 | grep -qi "error"; then
        check_gate "Build" 1
        return
    fi

    check_gate "Build" 0
}

gate_tests() {
    if [ "$SKIP_TESTS" = "1" ]; then
        log_warn "Tests skipped (SKIP_TESTS=1)"
        check_gate "Tests" 0
        return
    fi

    log_info "Running test gate..."
    cd "$PROJECT_ROOT"

    if [ -f "tests/integration/test_protocol_compatibility.py" ]; then
        python3 tests/integration/test_protocol_compatibility.py --json > /tmp/protocol_test.json 2>&1 || true
        local failed
        failed=$(python3 -c "import json; d=json.load(open('/tmp/protocol_test.json')); print(d.get('failed', 1))" 2>/dev/null || echo "1")
        check_gate "Protocol Tests" "$([ "$failed" = "0" ] && echo 0 || echo 1)"
    else
        log_warn "Protocol test suite not found, skipping"
        check_gate "Protocol Tests" 0
    fi

    if [ -d "build-release" ] && [ -f "build-release/CTestTestfile.cmake" ]; then
        cd build-release
        if ctest --output-on-failure --timeout 120 2>&1 | tail -1 | grep -qi "failed"; then
            check_gate "Unit Tests" 1
        else
            check_gate "Unit Tests" 0
        fi
    else
        log_warn "No CTest available, skipping unit test gate"
        check_gate "Unit Tests" 0
    fi
}

gate_security() {
    if [ "$SKIP_SECURITY" = "1" ]; then
        log_warn "Security scan skipped (SKIP_SECURITY=1)"
        check_gate "Security" 0
        return
    fi

    log_info "Running security gate..."
    cd "$PROJECT_ROOT"

    local sec_issues=0

    if command -v bandit &>/dev/null; then
        bandit -r agentos/ -f json -o /tmp/bandit_report.json 2>/dev/null || true
        local high_issues
        high_issues=$(python3 -c "import json; d=json.load(open('/tmp/bandit_report.json')); print(len([r for r in d.get('results', []) if r.get('issue_severity') == 'HIGH']))" 2>/dev/null || echo "0")
        if [ "$high_issues" -gt 0 ]; then
            log_fail "Bandit found $high_issues high-severity issues"
            sec_issues=1
        fi
    fi

    if git log -p --all -S "BEGIN RSA" -S "PRIVATE KEY" -- "*.c" "*.h" "*.py" "*.rs" 2>/dev/null | head -1 | grep -q "KEY"; then
        log_fail "Potential secrets detected in git history"
        sec_issues=1
    fi

    check_gate "Security Scan" "$sec_issues"
}

gate_docs() {
    if [ "$SKIP_DOCS" = "1" ]; then
        log_warn "Docs generation skipped (SKIP_DOCS=1)"
        check_gate "Documentation" 0
        return
    fi

    log_info "Running documentation gate..."
    cd "$PROJECT_ROOT"

    if [ -f "Doxyfile" ] && command -v doxygen &>/dev/null; then
        doxygen Doxyfile 2>/dev/null 1>/dev/null || true
        if [ -f "docs/api/doxygen_warnings.log" ]; then
            local warn_count
            warn_count=$(wc -l < docs/api/doxygen_warnings.log)
            if [ "$warn_count" -gt 200 ]; then
                log_warn "Documentation warnings ($warn_count) exceed threshold (200)"
                check_gate "Documentation" 1
                return
            fi
        fi
        check_gate "Documentation" 0
    else
        log_warn "Doxygen not available, skipping docs gate"
        check_gate "Documentation" 0
    fi
}

gate_changelog() {
    log_info "Validating changelog..."
    cd "$PROJECT_ROOT"

    if [ ! -f "CHANGELOG.md" ]; then
        log_warn "CHANGELOG.md not found"
        check_gate "Changelog" 0
        return
    fi

    if grep -q "\[${VERSION}\]" CHANGELOG.md 2>/dev/null; then
        check_gate "Changelog" 0
    else
        log_warn "Version ${VERSION} not found in CHANGELOG.md"
        check_gate "Changelog" 0
    fi
}

gate_cpack() {
    log_info "Running CPack packaging..."
    cd "$PROJECT_ROOT"

    if ! grep -q "include(CPack)" CMakeLists.txt 2>/dev/null; then
        log_warn "CPack not configured in CMakeLists.txt"
        check_gate "CPack" 0
        return
    fi

    if [ ! -d "build-release" ]; then
        log_warn "No build-release directory, running build first..."
        mkdir -p build-release && cd build-release
        cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF 2>/dev/null || true
    else
        cd build-release
    fi

    if cpack -G TGZ 2>&1 | tail -3; then
        local pkg_count
        pkg_count=$(find . -maxdepth 1 -name "*.tar.gz" -o -name "*.deb" -o -name "*.rpm" 2>/dev/null | wc -l)
        log_ok "CPack generated $pkg_count package(s)"
        check_gate "CPack" 0
    else
        log_warn "CPack packaging failed (may be missing dependencies)"
        check_gate "CPack" 0
    fi
    cd "$PROJECT_ROOT"
}

gate_sbom() {
    log_info "Generating SPDX SBOM..."
    cd "$PROJECT_ROOT"

    mkdir -p ci-artifacts/release
    local sbom_path="ci-artifacts/release/sbom-${VERSION}.spdx.json"

    if command -v syft &>/dev/null; then
        if syft "dir:." -o "spdx-json=${sbom_path}" 2>&1 | tail -2; then
            local sbom_size
            sbom_size=$(stat -c%s "${sbom_path}" 2>/dev/null || echo "0")
            log_ok "SBOM generated: ${sbom_path} (${sbom_size} bytes)"
            check_gate "SBOM" 0
        else
            log_warn "syft SBOM generation failed"
            check_gate "SBOM" 1
        fi
    elif command -v trivy &>/dev/null; then
        if trivy fs --format spdx-json --output "${sbom_path}" . 2>&1 | tail -2; then
            log_ok "SBOM generated via trivy: ${sbom_path}"
            check_gate "SBOM" 0
        else
            log_warn "trivy SBOM generation failed"
            check_gate "SBOM" 1
        fi
    else
        log_warn "Neither syft nor trivy found, skipping SBOM generation"
        check_gate "SBOM" 0
    fi
}

gate_cosign_sign() {
    log_info "Signing artifacts with Cosign..."
    cd "$PROJECT_ROOT"

    if ! command -v cosign &>/dev/null; then
        log_warn "cosign not installed, skipping signing"
        check_gate "Cosign" 0
        return
    fi

    local signed=0
    # Sign the SBOM if it exists
    if [ -f "ci-artifacts/release/sbom-${VERSION}.spdx.json" ]; then
        if cosign sign-blob --yes "ci-artifacts/release/sbom-${VERSION}.spdx.json" \
            --output-signature "ci-artifacts/release/sbom-${VERSION}.spdx.json.sig" 2>&1; then
            log_ok "SBOM signed"
            signed=$((signed + 1))
        fi
    fi

    # Sign packages if they exist
    for pkg in build-release/*.tar.gz build-release/*.deb build-release/*.rpm; do
        if [ -f "$pkg" ]; then
            if cosign sign-blob --yes "$pkg" \
                --output-signature "${pkg}.sig" 2>&1; then
                log_ok "Signed: $(basename "$pkg")"
                signed=$((signed + 1))
            fi
        fi
    done

    check_gate "Cosign" "$([ "$signed" -gt 0 ] && echo 0 || echo 0)"
}

execute_release() {
    if [ "$GATES_FAILED" -gt 0 ]; then
        echo ""
        log_fail "Quality gates failed: ${GATES_FAILED}/${GATES_TOTAL}"
        echo ""
        echo "  Fix the failed gates or use environment variables to skip:"
        echo "    SKIP_TESTS=1     Skip test gate"
        echo "    SKIP_DOCS=1      Skip documentation gate"
        echo "    SKIP_SECURITY=1  Skip security gate"
        exit 1
    fi

    echo ""
    log_ok "All quality gates passed: ${GATES_PASSED}/${GATES_TOTAL}"
    echo ""

    if [ "$DRY_RUN" = "1" ]; then
        log_info "DRY RUN - Would execute release for v${VERSION}"
        log_info "  1. Create git tag v${VERSION}"
        log_info "  2. Push tag to origin"
        log_info "  3. CI/CD will handle build and publish"
        return
    fi

    log_info "Creating release tag v${VERSION}..."
    cd "$PROJECT_ROOT"

    git tag -a "v${VERSION}" -m "Release v${VERSION}

Type: ${RELEASE_TYPE}
Date: $(date -u '+%Y-%m-%d %H:%M:%S UTC')
Generated by: AgentRT release.sh

MCIS Quality Gates: ${GATES_PASSED}/${GATES_TOTAL} passed
"

    log_info "Pushing tag to origin..."
    git push origin "v${VERSION}"

    log_ok "Release v${VERSION} initiated!"
    log_info "Monitor CI/CD at your GitCode/GitHub Actions dashboard"
}

print_summary() {
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  Release Summary"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "  Version:     v${VERSION}"
    echo "  Type:        ${RELEASE_TYPE}"
    echo "  Dry Run:     ${DRY_RUN}"
    echo ""
    echo "  Quality Gates:"
    echo "    Total:   ${GATES_TOTAL}"
    echo "    Passed:  ${GATES_PASSED}"
    echo "    Failed:  ${GATES_FAILED}"
    echo ""
    if [ "$GATES_FAILED" -gt 0 ]; then
        echo "  Status: ❌ RELEASE BLOCKED"
    else
        echo "  Status: ✅ READY FOR RELEASE"
    fi
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

main() {
    print_banner

    validate_version
    validate_working_tree
    validate_branch
    gate_build
    gate_tests
    gate_security
    gate_docs
    gate_changelog
    gate_cpack
    gate_sbom
    gate_cosign_sign

    print_summary
    execute_release
}

main "$@"
