﻿﻿﻿#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 通用工具函数测试

# 加载测试框架
AGENTOS_TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=./test_framework.sh
source "$AGENTOS_TEST_DIR/test_framework.sh"

# 加载被测模块
AGENTOS_SCRIPTS_DIR="$(dirname "$AGENTOS_TEST_DIR")"
# shellcheck source=../lib/commons.sh
source "$AGENTOS_SCRIPTS_DIR/lib/commons.sh"

###############################################################################
# 测试：字符串工具
###############################################################################

test_to_lower() {
    local result
    result=$(agentos_to_lower "HELLO WORLD")
    assert_equal "hello world" "$result" "to_lower should convert to lowercase"
}

test_to_upper() {
    local result
    result=$(agentos_to_upper "hello world")
    assert_equal "HELLO WORLD" "$result" "to_upper should convert to uppercase"
}

test_trim() {
    local result
    result=$(agentos_trim "  hello world  ")
    assert_equal "hello world" "$result" "trim should remove leading/trailing whitespace"
}

test_contains() {
    local result
    result=$(agentos_contains "hello world" "world")
    assert_equal "0" "$?" "contains should return 0 when substring exists"
}

test_random_string() {
    local result
    result=$(agentos_random_string 16)
    assert_equal 16 "${#result}" "random_string should generate correct length"
}

###############################################################################
# 测试：路径工�?
###############################################################################

test_mkdir() {
    local test_dir="/tmp/agentos_test_$$"
    agentos_mkdir "$test_dir"
    assert_dir_exists "$test_dir"
    rm -rf "$test_dir"
}

test_safe_rm() {
    local test_file="/tmp/agentos_test_file_$$"
    echo "test" > "$test_file"
    agentos_safe_rm "$test_file"
    assert_false "[[ -f $test_file ]]" "safe_rm should remove file"
}

test_backup_file() {
    local test_file="/tmp/agentos_test_backup_$$"
    echo "test content" > "$test_file"
    local backup
    backup=$(agentos_backup_file "$test_file")
    assert_true "[[ -f $backup ]]" "backup_file should create backup"
    rm -rf "$test_file" "$backup"
}

###############################################################################
# 测试：版本比�?
###############################################################################

test_version_compare_equal() {
    agentos_version_compare "1.0.0" "1.0.0"
    assert_equal "0" "$?" "version_compare should return 0 for equal versions"
}

test_version_compare_greater() {
    agentos_version_compare "2.0.0" "1.0.0"
    assert_equal "1" "$?" "version_compare should return 1 when v1 > v2"
}

test_version_compare_less() {
    agentos_version_compare "1.0.0" "2.0.0"
    assert_equal "2" "$?" "version_compare should return 2 when v1 < v2"
}

test_version_check() {
    assert_true "agentos_version_check '1.0.0' '1.0.0'" "version_check should pass for equal version"
    assert_true "agentos_version_check '1.0.0' '2.0.0'" "version_check should pass when actual > required"
    assert_false "agentos_version_check '2.0.0' '1.0.0'" "version_check should fail when actual < required"
}

###############################################################################
# 测试：数组工�?
###############################################################################

test_in_array() {
    local arr=("apple" "banana" "cherry")
    agentos_in_array "banana" "${arr[@]}"
    assert_equal "0" "$?" "in_array should return 0 when element exists"

    agentos_in_array "grape" "${arr[@]}"
    assert_equal "1" "$?" "in_array should return 1 when element doesn't exist"
}

###############################################################################
# 测试：配置工�?
###############################################################################

test_config_get() {
    local test_config="/tmp/agentos_test_config_$$"
    cat > "$test_config" << 'EOF'
key1=value1
key2=value2
key3=value with spaces
EOF
    assert_equal "value1" "$(agentos_config_get "$test_config" "key1")" "config_get should return correct value"
    assert_equal "value with spaces" "$(agentos_config_get "$test_config" "key3")" "config_get should handle spaces"
    rm -f "$test_config"
}

test_config_set() {
    local test_config="/tmp/agentos_test_config2_$$"
    touch "$test_config"
    agentos_config_set "$test_config" "newkey" "newvalue"
    assert_equal "newvalue" "$(agentos_config_get "$test_config" "newkey")" "config_set should set value"
    rm -f "$test_config"
}

###############################################################################
# 测试：用户交�?
###############################################################################

test_confirm() {
    echo "y" | agentos_confirm "Test prompt"
    assert_equal "0" "$?" "confirm should return 0 for 'y'"
}

###############################################################################
# 运行所有测�?
###############################################################################

run_all_tests() {
    local tests=(
        "test_to_lower"
        "test_to_upper"
        "test_trim"
        "test_contains"
        "test_random_string"
        "test_mkdir"
        "test_safe_rm"
        "test_backup_file"
        "test_version_compare_equal"
        "test_version_compare_greater"
        "test_version_compare_less"
        "test_version_check"
        "test_in_array"
        "test_config_get"
        "test_config_set"
    )

    echo "=========================================="
    echo "  AgentOS commons Utils Tests"
    echo "=========================================="
    echo ""

    for test in "${tests[@]}"; do
        run_test "$test" "$test"
    done

    print_test_report
}

# 如果直接执行此脚本，运行测试
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    run_all_tests
fi