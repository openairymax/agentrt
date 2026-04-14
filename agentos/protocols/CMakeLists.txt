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
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/router/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/adapter/include
    ${CMAKE_CURRENT_SOURCE_DIR}/standards/a2a/include
    ${CMAKE_CURRENT_SOURCE_DIR}/standards/mcp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/integrations/openai/include
    ${CMAKE_CURRENT_SOURCE_DIR}/integrations/openjiuwen/include
    ${AGENTOS_COMMONS_INCLUDE_DIR}
)

# 源文件
set(PROTOCOLS_SOURCES
    common/src/unified_protocol.c
    core/router/src/protocol_router.c
    core/adapter/src/protocol_extension_framework.c
    standards/a2a/src/a2a_v03_adapter.c
    standards/mcp/src/mcp_v1_adapter.c
    integrations/openai/src/openai_enterprise_adapter.c
    integrations/openjiuwen/src/openjiuwen_adapter.c
)

# 头文件
set(PROTOCOLS_HEADERS
    common/include/protocols.h
    common/include/unified_protocol.h
    core/router/include/protocol_router.h
    core/adapter/include/protocol_extension_framework.h
    standards/a2a/include/a2a_v03_adapter.h
    standards/mcp/include/mcp_v1_adapter.h
    integrations/openai/include/openai_enterprise_adapter.h
    integrations/openjiuwen/include/openjiuwen_adapter.h
)

# 创建库
add_library(protocols ${PROTOCOLS_SOURCES})
target_include_directories(protocols PUBLIC common/include)

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