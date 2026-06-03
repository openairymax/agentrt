#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Shell Script Testing Framework
# Based on bats-core unit test library

###############################################################################
# Test Framework Initialization
###############################################################################

AGENTOS_TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENTOS_SCRIPTS_DIR="$(dirname "$AGENTOS_TEST_DIR")"

# Load dependencies
# shellcheck source=../library/common.sh
source "$AGENTOS_SCRIPTS_DIR/library/common.sh"

###############################################################################
# Test Configuration
###############################################################################

AGENTOS_TEST_VERBOSE="${AGENTOS_TEST_VERBOSE:-0}"
AGENTOS_TEST_COVERAGE="${AGENTOS_TEST_COVERAGE:-0}"
AGENTOS_TEST_TIMEOUT="${AGENTOS_TEST_TIMEOUT:-60}"

###############################################################################
# Test Statistics
###############################################################################

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0
TEST_FAILURES=()

###############################################################################
# Test Utility Functions
###############################################################################

# Print test start
test_start() {
    local test_name="$1"
    ((TESTS_RUN++))
    if [[ "$AGENTOS_TEST_VERBOSE" == "1" ]]; then
        echo -e "${COLOR_DIM}[RUN]${COLOR_NC} $test_name"
    fi
}

# Print test passed
test_pass() {
    local test_name="$1"
    ((TESTS_PASSED++))
    if [[ "$AGENTOS_TEST_VERBOSE" == "1" ]]; then
        echo -e "${COLOR_GREEN}[PASS]${COLOR_NC} $test_name"
    fi
}

# Print test failed
test_fail() {
    local test_name="$1"
    local message="${2:-}"
    ((TESTS_FAILED++))
    TEST_FAILURES+=("[$test_name] $message")
    echo -e "${COLOR_RED}[FAIL]${COLOR_NC} $test_name"
    if [[ -n "$message" ]]; then
        echo -e "    ${COLOR_RED}$message${COLOR_NC}"
    fi
}

# Print test skipped
test_skip() {
    local test_name="$1"
    local reason="${2:-}"
    ((TESTS_SKIPPED++))
    echo -e "${COLOR_YELLOW}[SKIP]${COLOR_NC} $test_name"
    if [[ -n "$reason" ]]; then
        echo -e "    ${COLOR_YELLOW}Reason: $reason${COLOR_NC}"
    fi
}

###############################################################################
# Assertion Functions
###############################################################################

# Assert true
assert_true() {
    local condition="$1"
    local message="${2:-Assertion failed: expected true}"

    if eval "$condition"; then
        return 0
    else
        echo -e "    ${COLOR_RED}$message${COLOR_NC}"
        return 1
    fi
}

# Assert false
assert_false() {
    local condition="$1"
    local message="${2:-Assertion failed: expected false}"

    if ! eval "$condition"; then
        return 0
    else
        echo -e "    ${COLOR_RED}$message${COLOR_NC}"
        return 1
    fi
}

# Assert equal
assert_equal() {
    local expected="$1"
    local actual="$2"
    local message="${3:-}"

    if [[ "$expected" == "$actual" ]]; then
        return 0
    else
        echo -e "    ${COLOR_RED}Expected: '$expected'${COLOR_NC}"
        echo -e "    ${COLOR_RED}Actual:   '$actual'${COLOR_NC}"
        if [[ -n "$message" ]]; then
            echo -e "    ${COLOR_RED}$message${COLOR_NC}"
        fi
        return 1
    fi
}

# Assert string contains
assert_contains() {
    local haystack="$1"
    local needle="$2"
    local message="${3:-}"

    if [[ "$haystack" == *"$needle"* ]]; then
        return 0
    else
        echo -e "    ${COLOR_RED}Haystack does not contain needle${COLOR_NC}"
        echo -e "    ${COLOR_RED}Haystack: '$haystack'${COLOR_NC}"
        echo -e "    ${COLOR_RED}Needle:   '$needle'${COLOR_NC}"
        return 1
    fi
}

# Assert file exists
assert_file_exists() {
    local file="$1"
    if [[ -f "$file" ]]; then
        return 0
    else
        echo -e "    ${COLOR_RED}File does not exist: $file${COLOR_NC}"
        return 1
    fi
}

# Assert directory exists
assert_dir_exists() {
    local dir="$1"
    if [[ -d "$dir" ]]; then
        return 0
    else
        echo -e "    ${COLOR_RED}Directory does not exist: $dir${COLOR_NC}"
        return 1
    fi
}

# Assert command exists
assert_command_exists() {
    local cmd="$1"
    if command -v "$cmd" &> /dev/null; then
        return 0
    else
        echo -e "    ${COLOR_RED}Command not found: $cmd${COLOR_NC}"
        return 1
    fi
}

# Assert not empty
assert_not_empty() {
    local value="$1"
    local message="${2:-Value should not be empty}"
    if [[ -n "$value" ]]; then
        return 0
    else
        echo -e "    ${COLOR_RED}$message${COLOR_NC}"
        return 1
    fi
}

# Assert pattern match
assert_match() {
    local pattern="$1"
    local value="$2"
    local message="${3:-}"

    if [[ "$value" =~ $pattern ]]; then
        return 0
    else
        echo -e "    ${COLOR_RED}Value does not match pattern${COLOR_NC}"
        echo -e "    ${COLOR_RED}Pattern: '$pattern'${COLOR_NC}"
        echo -e "    ${COLOR_RED}Value:   '$value'${COLOR_NC}"
        return 1
    fi
}

###############################################################################
# Test Runner
###############################################################################

run_test() {
    local test_name="$1"
    local test_func="$2"

    test_start "$test_name"

    if $test_func; then
        test_pass "$test_name"
        return 0
    else
        test_fail "$test_name"
        return 1
    fi
}

###############################################################################
# Test Report
###############################################################################

print_test_report() {
    echo ""
    echo "=========================================="
    echo "  Test Summary"
    echo "=========================================="
    echo ""
    echo -e "  ${COLOR_GREEN}Passed: $TESTS_PASSED${COLOR_NC}"
    echo -e "  ${COLOR_RED}Failed: $TESTS_FAILED${COLOR_NC}"
    echo -e "  ${COLOR_YELLOW}Skipped: $TESTS_SKIPPED${COLOR_NC}"
    echo -e "  Total: $TESTS_RUN"
    echo ""

    if [[ $TESTS_FAILED -gt 0 ]]; then
        echo "=========================================="
        echo "  Failed Tests"
        echo "=========================================="
        for failure in "${TEST_FAILURES[@]}"; do
            echo -e "  ${COLOR_RED}* $failure${COLOR_NC}"
        done
        echo ""
        return 1
    fi

    echo -e "${COLOR_GREEN}All tests passed!${COLOR_NC}"
    echo ""
    return 0
}

###############################################################################
# Export Functions
###############################################################################
export -f test_start test_pass test_fail test_skip
export -f assert_true assert_false assert_equal assert_contains
export -f assert_file_exists assert_dir_exists assert_command_exists
export -f assert_not_empty assert_match
export -f run_test print_test_report