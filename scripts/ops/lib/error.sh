#!/usr/bin/env bash
# shellcheck shell=bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 错误码定义模块
# 遵循 AgentOS 架构设计原则：错误可追溯原则 (E-7)

###############################################################################
# 错误码定义（参考AgentOS 统一错误码体系）
###############################################################################

# 成功
declare -r AGENTRT_SUCCESS=0

# 通用错误 (1000-1999)
declare -r AGENTRT_ERR_GENERAL=1000
declare -r AGENTRT_ERR_INVALID_PARAM=1001
declare -r AGENTRT_ERR_OUT_OF_MEMORY=1002
declare -r AGENTRT_ERR_TIMEOUT=1003
declare -r AGENTRT_ERR_NOT_FOUND=1004
declare -r AGENTRT_ERR_PERMISSION_DENIED=1005
declare -r AGENTRT_ERR_NETWORK=1006
declare -r AGENTRT_ERR_IO=1007
declare -r AGENTRT_ERR_PARSE=1008
declare -r AGENTRT_ERR_ASSERTION=1009

# 构建相关错误 (2000-2999)
declare -r AGENTRT_ERR_BUILD_FAILED=2001
declare -r AGENTRT_ERR_BUILD_CLEAN=2002
declare -r AGENTRT_ERR_BUILD_CONFIG=2003
declare -r AGENTRT_ERR_BUILD_DEPENDENCY=2004
declare -r AGENTRT_ERR_BUILD_TIMEOUT=2005
declare -r AGENTRT_ERR_BUILD_PERMISSION=2006

# 安装相关错误 (3000-3999)
declare -r AGENTRT_ERR_INSTALL_FAILED=3001
declare -r AGENTRT_ERR_INSTALL_DEPENDENCY=3002
declare -r AGENTRT_ERR_INSTALL_PERMISSION=3003
declare -r AGENTRT_ERR_INSTALL_ALREADY=3004
declare -r AGENTRT_ERR_INSTALL_NOT_FOUND=3005

# Docker相关错误 (4000-4999)
declare -r AGENTRT_ERR_DOCKER_NOT_FOUND=4001
declare -r AGENTRT_ERR_DOCKER_NOT_RUNNING=4002
declare -r AGENTRT_ERR_DOCKER_BUILD=4003
declare -r AGENTRT_ERR_DOCKER_IMAGE=4004
declare -r AGENTRT_ERR_DOCKER_CONTAINER=4005
declare -r AGENTRT_ERR_DOCKER_NETWORK=4006
declare -r AGENTRT_ERR_DOCKER_VOLUME=4007
declare -r AGENTRT_ERR_DOCKER_COMPOSE=4008

# 配置相关错误 (5000-5999)
declare -r AGENTRT_ERR_CONFIG_NOT_FOUND=5001
declare -r AGENTRT_ERR_CONFIG_INVALID=5002
declare -r AGENTRT_ERR_CONFIG_PERMISSION=5003
declare -r AGENTRT_ERR_CONFIG_SYNTAX=5004

# 测试相关错误 (6000-6999)
declare -r AGENTRT_ERR_TEST_FAILED=6001
declare -r AGENTRT_ERR_TEST_TIMEOUT=6002
declare -r AGENTRT_ERR_TEST_SKIP=6003
declare -r AGENTRT_ERR_TEST_NOT_FOUND=6004

# 脚本执行环境错误 (7000-7999)
declare -r AGENTRT_ERR_ENV_PLATFORM=7001
declare -r AGENTRT_ERR_ENV_MISSING_DEP=7002
declare -r AGENTRT_ERR_ENV_INVALID_VERSION=7003

###############################################################################
# 错误码到描述的映�?
###############################################################################
declare -A AGENTRT_ERROR_MESSAGES=(
    [$AGENTRT_SUCCESS]="Success"
    [$AGENTRT_ERR_GENERAL]="General error"
    [$AGENTRT_ERR_INVALID_PARAM]="Invalid parameter"
    [$AGENTRT_ERR_OUT_OF_MEMORY]="Out of memory"
    [$AGENTRT_ERR_TIMEOUT]="Operation timed out"
    [$AGENTRT_ERR_NOT_FOUND]="Resource not found"
    [$AGENTRT_ERR_PERMISSION_DENIED]="Permission denied"
    [$AGENTRT_ERR_NETWORK]="Network error"
    [$AGENTRT_ERR_IO]="I/O error"
    [$AGENTRT_ERR_PARSE]="Parse error"
    [$AGENTRT_ERR_ASSERTION]="Assertion failed"
    [$AGENTRT_ERR_BUILD_FAILED]="Build failed"
    [$AGENTRT_ERR_BUILD_CLEAN]="Build clean failed"
    [$AGENTRT_ERR_BUILD_CONFIG]="Build configuration error"
    [$AGENTRT_ERR_BUILD_DEPENDENCY]="Build dependency error"
    [$AGENTRT_ERR_BUILD_TIMEOUT]="Build timeout"
    [$AGENTRT_ERR_BUILD_PERMISSION]="Build permission error"
    [$AGENTRT_ERR_INSTALL_FAILED]="Installation failed"
    [$AGENTRT_ERR_INSTALL_DEPENDENCY]="Installation dependency error"
    [$AGENTRT_ERR_INSTALL_PERMISSION]="Installation permission error"
    [$AGENTRT_ERR_INSTALL_ALREADY]="Already installed"
    [$AGENTRT_ERR_INSTALL_NOT_FOUND]="Installation not found"
    [$AGENTRT_ERR_DOCKER_NOT_FOUND]="Docker not found"
    [$AGENTRT_ERR_DOCKER_NOT_RUNNING]="Docker not running"
    [$AGENTRT_ERR_DOCKER_BUILD]="Docker build failed"
    [$AGENTRT_ERR_DOCKER_IMAGE]="Docker image error"
    [$AGENTRT_ERR_DOCKER_CONTAINER]="Docker container error"
    [$AGENTRT_ERR_DOCKER_NETWORK]="Docker network error"
    [$AGENTRT_ERR_DOCKER_VOLUME]="Docker volume error"
    [$AGENTRT_ERR_DOCKER_COMPOSE]="Docker compose error"
    [$AGENTRT_ERR_CONFIG_NOT_FOUND]="Configuration file not found"
    [$AGENTRT_ERR_CONFIG_INVALID]="Configuration invalid"
    [$AGENTRT_ERR_CONFIG_PERMISSION]="Configuration permission error"
    [$AGENTRT_ERR_CONFIG_SYNTAX]="Configuration syntax error"
    [$AGENTRT_ERR_TEST_FAILED]="Test failed"
    [$AGENTRT_ERR_TEST_TIMEOUT]="Test timeout"
    [$AGENTRT_ERR_TEST_SKIP]="Test skipped"
    [$AGENTRT_ERR_TEST_NOT_FOUND]="Test not found"
    [$AGENTRT_ERR_ENV_PLATFORM]="Unsupported platform"
    [$AGENTRT_ERR_ENV_MISSING_DEP]="Missing dependency"
    [$AGENTRT_ERR_ENV_INVALID_VERSION]="Invalid version"
)

###############################################################################
# 公共API：获取错误描述
###############################################################################
agentrt_error_get_message() {
    local error_code=$1
    local msg="${AGENTRT_ERROR_MESSAGES[$error_code]:-Unknown error code: $error_code}"
    echo "$msg"
}

agentrt_error_die() {
    local error_code=$1
    local message="${2:-}"
    local error_msg
    error_msg=$(agentrt_error_get_message $error_code)
    if [[ -n "$message" ]]; then
        agentrt_log_fatal "[$error_code] $error_msg: $message"
    else
        agentrt_log_fatal "[$error_code] $error_msg"
    fi
}

agentrt_error_log() {
    local error_code=$1
    local message="${2:-}"
    local error_msg
    error_msg=$(agentrt_error_get_message $error_code)
    if [[ -n "$message" ]]; then
        agentrt_log_error "[$error_code] $error_msg: $message"
    else
        agentrt_log_error "[$error_code] $error_msg"
    fi
}

###############################################################################
# 导出公共API
###############################################################################
export -f agentrt_error_get_message agentrt_error_die agentrt_error_log