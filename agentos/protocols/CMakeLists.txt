# ========================================================================
# AgentOS Unified Protocols Module
#
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-FileCopyrightText: 2026 SPHARX.
# SPDX-License-Identifier: Apache-2.0
#
# 本模块提供统一的协议栈基础框架，包括：
# 1. 统一协议接口 (UnifiedProtocol)
# 2. 基础传输层实现 (Transport Layer)
# 3. 协议适配器 (Protocol Adapters)
# ========================================================================

cmake_minimum_required(VERSION 3.20)

project(protocols
    VERSION 1.0.0
    DESCRIPTION "AgentOS Unified Protocols Framework"
    LANGUAGES C
)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${AGENTOS_COMMONS_INCLUDE_DIR}
)

# 源文件
set(PROTOCOLS_SOURCES
    src/unified_protocol.c
)

# 头文件
set(PROTOCOLS_HEADERS
    include/protocols.h
    include/unified_protocol.h
)

# 创建库
add_library(protocols ${PROTOCOLS_SOURCES})
target_include_directories(protocols PUBLIC include)

# 链接依赖
if(TARGET commons)
    target_link_libraries(protocols commons)
endif()

# 安装目标
install(TARGETS protocols
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${PROTOCOLS_HEADERS}
    DESTINATION include/agentos/protocols
)