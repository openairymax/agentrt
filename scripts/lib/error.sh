#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 错误码定义模块
# 遵循 AgentOS 架构设计原则：错误可追溯原则 (E-7)

###############################################################################
# 错误码定义（参考 AgentOS 统一错误码体系）
###############################################################################

# 成功
declare -r AGENTOS_SUCCESS=0

# 通用错误 (1000-1999)
declare -r AGENTOS_ERR_GENERAL=1000
declare -r AGENTOS_ERR_INVALID_PARAM=1001
declare -r AGENTOS_ERR_OUT_OF_MEMORY=1002
declare -r AGENTOS_ERR_TIMEOUT=1003
declare -r AGENTOS_ERR_NOT_FOUND=1004
declare -r AGENTOS_ERR_PERMISSION_DENIED=1005
declare -r AGENTOS_ERR_NETWORK=1006
declare -r AGENTOS_ERR_IO=1007
declare -r AGENTOS_ERR_PARSE=1008
declare -r AGENTOS_ERR_ASSERTION=1009

# 构建相关错误 (2000-2999)
declare -r AGENTOS_ERR_BUILD_FAILED=2001
declare -r AGENTOS_ERR_BUILD_CLEAN=2002
declare -r AGENTOS_ERR_BUILD_CONFIG=2003
declare -r AGENTOS_ERR_BUILD_DEPENDENCY=2004
declare -r AGENTOS_ERR_BUILD_TIMEOUT=2005
declare -r AGENTOS_ERR_BUILD_PERMISSION=2006

# 安装相关错误 (3000-3999)
declare -r AGENTOS_ERR_INSTALL_FAILED=3001
declare -r AGENTOS_ERR_INSTALL_DEPENDENCY=3002
declare -r AGENTOS_ERR_INSTALL_PERMISSION=3003
declare -r AGENTOS_ERR_INSTALL_ALREADY=3004
declare -r AGENTOS_ERR_INSTALL_NOT_FOUND=3005

# Docker相关错误 (4000-4999)
declare -r AGENTOS_ERR_DOCKER_NOT_FOUND=4001
declare -r AGENTOS_ERR_DOCKER_NOT_RUNNING=4002
declare -r AGENTOS_ERR_DOCKER_BUILD=4003
declare -r AGENTOS_ERR_DOCKER_IMAGE=4004
declare -r AGENTOS_ERR_DOCKER_CONTAINER=4005
declare -r AGENTOS_ERR_DOCKER_NETWORK=4006
declare -r AGENTOS_ERR_DOCKER_VOLUME=4007
declare -r AGENTOS_ERR_DOCKER_COMPOSE=4008

# 配置相关错误 (5000-5999)
declare -r AGENTOS_ERR_CONFIG_NOT_FOUND=5001
declare -r AGENTOS_ERR_CONFIG_INVALID=5002
declare -r AGENTOS_ERR_CONFIG_PERMISSION=5003
declare -r AGENTOS_ERR_CONFIG_SYNTAX=5004

# 测试相关错误 (6000-6999)
declare -r AGENTOS_ERR_TEST_FAILED=6001
declare -r AGENTOS_ERR_TEST_TIMEOUT=6002
declare -r AGENTOS_ERR_TEST_SKIP=6003
declare -r AGENTOS_ERR_TEST_NOT_FOUND=6004

# 脚本执行环境错误 (7000-7999)
declare -r AGENTOS_ERR_ENV_PLATFORM=7001
declare -r AGENTOS_ERR_ENV_MISSING_DEP=7002
declare -r AGENTOS_ERR_ENV_INVALID_VERSION=7003

###############################################################################
# 错误码到描述的映射
###############################################################################
declare -A AGENTOS_ERROR_MESSAGES=(
    [$AGENTOS_SUCCESS]="Success"
    [$AGENTOS_ERR_GENERAL]="General error"
    [$AGENTOS_ERR_INVALID_PARAM]="Invalid parameter"
    [$AGENTOS_ERR_OUT_OF_MEMORY]="Out of memory"
    [$AGENTOS_ERR_TIMEOUT]="Operation timed out"
    [$AGENTOS_ERR_NOT_FOUND]="Resource not found"
    [$AGENTOS_ERR_PERMISSION_DENIED]="Permission denied"
    [$AGENTOS_ERR_NETWORK]="Network error"
    [$AGENTOS_ERR_IO]="I/O error"
    [$AGENTOS_ERR_PARSE]="Parse error"
    [$AGENTOS_ERR_ASSERTION]="Assertion failed"
    [$AGENTOS_ERR_BUILD_FAILED]="Build failed"
    [$AGENTOS_ERR_BUILD_CLEAN]="Build clean failed"
    [$AGENTOS_ERR_BUILD_CONFIG]="Build configuration error"
    [$AGENTOS_ERR_BUILD_DEPENDENCY]="Build dependency error"
    [$AGENTOS_ERR_BUILD_TIMEOUT]="Build timeout"
    [$AGENTOS_ERR_BUILD_PERMISSION]="Build permission error"
    [$AGENTOS_ERR_INSTALL_FAILED]="Installation failed"
    [$AGENTOS_ERR_INSTALL_DEPENDENCY]="Installation dependency error"
    [$AGENTOS_ERR_INSTALL_PERMISSION]="Installation permission error"
    [$AGENTOS_ERR_INSTALL_ALREADY]="Already installed"
    [$AGENTOS_ERR_INSTALL_NOT_FOUND]="Installation not found"
    [$AGENTOS_ERR_DOCKER_NOT_FOUND]="Docker not found"
    [$AGENTOS_ERR_DOCKER_NOT_RUNNING]="Docker not running"
    [$AGENTOS_ERR_DOCKER_BUILD]="Docker build failed"
    [$AGENTOS_ERR_DOCKER_IMAGE]="Docker image error"
    [$AGENTOS_ERR_DOCKER_CONTAINER]="Docker container error"
    [$AGENTOS_ERR_DOCKER_NETWORK]="Docker network error"
    [$AGENTOS_ERR_DOCKER_VOLUME]="Docker volume error"
    [$AGENTOS_ERR_DOCKER_COMPOSE]="Docker compose error"
    [$AGENTOS_ERR_CONFIG_NOT_FOUND]="Configuration file not found"
    [$AGENTOS_ERR_CONFIG_INVALID]="Configuration invalid"
    [$AGENTOS_ERR_CONFIG_PERMISSION]="Configuration permission error"
    [$AGENTOS_ERR_CONFIG_SYNTAX]="Configuration syntax error"
    [$AGENTOS_ERR_TEST_FAILED]="Test failed"
    [$AGENTOS_ERR_TEST_TIMEOUT]="Test timeout"
    [$AGENTOS_ERR_TEST_SKIP]="Test skipped"
    [$AGENTOS_ERR_TEST_NOT_FOUND]="Test not found"
    [$AGENTOS_ERR_ENV_PLATFORM]="Unsupported platform"
    [$AGENTOS_ERR_ENV_MISSING_DEP]="Missing dependency"
    [$AGENTOS_ERR_ENV_INVALID_VERSION]="Invalid version"
)

###############################################################################
# 公共API：获取错误描述
###############################################################################
agentos_error_get_message() {
    local error_code=$1
    local msg="${AGENTOS_ERROR_MESSAGES[$error_code]:-Unknown error code: $error_code}"
    echo "$msg"
}

agentos_error_die() {
    local error_code=$1
    local message="${2:-}"
    local error_msg
    error_msg=$(agentos_error_get_message $error_code)
    if [[ -n "$message" ]]; then
        agentos_log_fatal "[$error_code] $error_msg: $message"
    else
        agentos_log_fatal "[$error_code] $error_msg"
    fi
}

agentos_error_log() {
    local error_code=$1
    local message="${2:-}"
    local error_msg
    error_msg=$(agentos_error_get_message $error_code)
    if [[ -n "$message" ]]; then
        agentos_log_error "[$error_code] $error_msg: $message"
    else
        agentos_log_error "[$error_code] $error_msg"
    fi
}

###############################################################################
# 导出公共API
###############################################################################
export -f agentos_error_get_message agentos_error_die agentos_error_log