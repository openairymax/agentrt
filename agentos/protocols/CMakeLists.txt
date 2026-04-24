# ========================================================================
# AgentOS Unified Protocols Module
#
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-FileCopyrightText: 2026 SPHARX.
# SPDX-License-Identifier: Apache-2.0
#
# 本模块提供统一的协议栈基础框架，包括：
# 1. 统一协议接口与消息类型 (Common Layer)
# 2. 核心组件：路由器/转换器/扩展框架/注册中心 (Core Layer)
# 3. 标准协议适配器：MCP/A2A (Standards Layer)
# 4. 集成适配器：OpenAI/OpenJiuwen/OpenClaw/Claude (Integrations Layer)
# 5. 框架适配器：LangChain/AutoGen (Frameworks Layer)
# 6. 统一接口定义 (Interfaces Layer)
# ========================================================================

cmake_minimum_required(VERSION 3.20)

project(protocols
    VERSION 2.1.0
    DESCRIPTION "AgentOS Unified Protocols Framework - Multi-Protocol Ecosystem"
    LANGUAGES C
)

# 编译选项
option(PROTOCOLS_ENABLE_OPENCLAW "Enable OpenClaw integration" ON)
option(PROTOCOLS_ENABLE_CLAUDE "Enable Claude API integration" ON)
option(PROTOCOLS_ENABLE_LANGCHAIN "Enable LangChain framework adapter" ON)
option(PROTOCOLS_ENABLE_AUTOGEN "Enable AutoGen framework adapter" ON)

# ============================================================
# 包含目录 (Include Directories)
# ============================================================
set(PROTOCOLS_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/router/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/adapter/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transformers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/registry/include
    ${CMAKE_CURRENT_SOURCE_DIR}/standards/a2a/include
    ${CMAKE_CURRENT_SOURCE_DIR}/standards/mcp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/integrations/openai/include
    ${CMAKE_CURRENT_SOURCE_DIR}/integrations/openjiuwen/include
    ${AGENTOS_COMMONS_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../daemon/common/include
)

if(PROTOCOLS_ENABLE_OPENCLAW)
    list(APPEND PROTOCOLS_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/integrations/openclaw/include)
endif()

if(PROTOCOLS_ENABLE_CLAUDE)
    list(APPEND PROTOCOLS_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/integrations/claude/include)
endif()

if(PROTOCOLS_ENABLE_LANGCHAIN)
    list(APPEND PROTOCOLS_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/frameworks/langchain/include)
endif()

if(PROTOCOLS_ENABLE_AUTOGEN)
    list(APPEND PROTOCOLS_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/frameworks/autogen/include)
endif()

include_directories(${PROTOCOLS_INCLUDE_DIRS})

# ============================================================
# 源文件 (Source Files) — Common Layer
# ============================================================
set(PROTOCOLS_COMMON_SOURCES
    common/src/unified_protocol.c
)

# ============================================================
# 源文件 (Source Files) — Core Layer
# ============================================================
set(PROTOCOLS_CORE_SOURCES
    core/router/src/protocol_router.c
    core/adapter/src/protocol_extension_framework.c
    core/transformers/src/protocol_transformers.c
    core/registry/src/protocol_registry.c
)

# ============================================================
# 源文件 (Source Files) — Standards Layer
# ============================================================
set(PROTOCOLS_STANDARDS_SOURCES
    standards/a2a/src/a2a_v03_adapter.c
    standards/mcp/src/mcp_v1_adapter.c
)

# ============================================================
# 源文件 (Source Files) — Integrations Layer
# ============================================================
set(PROTOCOLS_INTEGRATIONS_SOURCES
    integrations/openai/src/openai_enterprise_adapter.c
    integrations/openjiuwen/src/openjiuwen_adapter.c
)

if(PROTOCOLS_ENABLE_OPENCLAW)
    list(APPEND PROTOCOLS_INTEGRATIONS_SOURCES
        integrations/openclaw/src/openclaw_adapter.c)
endif()

if(PROTOCOLS_ENABLE_CLAUDE)
    list(APPEND PROTOCOLS_INTEGRATIONS_SOURCES
        integrations/claude/src/claude_adapter.c)
endif()

# ============================================================
# 源文件 (Source Files) — Frameworks Layer
# ============================================================
set(PROTOCOLS_FRAMEWORKS_SOURCES
)

if(PROTOCOLS_ENABLE_LANGCHAIN)
    list(APPEND PROTOCOLS_FRAMEWORKS_SOURCES
        frameworks/langchain/src/langchain_adapter.c)
endif()

if(PROTOCOLS_ENABLE_AUTOGEN)
    list(APPEND PROTOCOLS_FRAMEWORKS_SOURCES
        frameworks/autogen/src/autogen_adapter.c)
endif()

# ============================================================
# 合并所有源文件
# ============================================================
set(PROTOCOLS_ALL_SOURCES
    ${PROTOCOLS_COMMON_SOURCES}
    ${PROTOCOLS_CORE_SOURCES}
    ${PROTOCOLS_STANDARDS_SOURCES}
    ${PROTOCOLS_INTEGRATIONS_SOURCES}
    ${PROTOCOLS_FRAMEWORKS_SOURCES}
)

# ============================================================
# 头文件 (Header Files) — Common Layer
# ============================================================
set(PROTOCOLS_COMMON_HEADERS
    common/include/protocols.h
    common/include/unified_protocol.h
)

# ============================================================
# 头文件 (Header Files) — Core Layer
# ============================================================
set(PROTOCOLS_CORE_HEADERS
    core/router/include/protocol_router.h
    core/adapter/include/protocol_extension_framework.h
    core/transformers/include/protocol_transformers.h
    core/registry/include/protocol_registry.h
)

# ============================================================
# 头文件 (Header Files) — Standards Layer
# ============================================================
set(PROTOCOLS_STANDARDS_HEADERS
    standards/a2a/include/a2a_v03_adapter.h
    standards/mcp/include/mcp_v1_adapter.h
)

# ============================================================
# 头文件 (Header Files) — Integrations Layer
# ============================================================
set(PROTOCOLS_INTEGRATIONS_HEADERS
    integrations/openai/include/openai_enterprise_adapter.h
    integrations/openjiuwen/include/openjiuwen_adapter.h
)

if(PROTOCOLS_ENABLE_OPENCLAW)
    list(APPEND PROTOCOLS_INTEGRATIONS_HEADERS
        integrations/openclaw/include/openclaw_adapter.h)
endif()

if(PROTOCOLS_ENABLE_CLAUDE)
    list(APPEND PROTOCOLS_INTEGRATIONS_HEADERS
        integrations/claude/include/claude_adapter.h)
endif()

# ============================================================
# 头文件 (Header Files) — Frameworks Layer
# ============================================================
set(PROTOCOLS_FRAMEWORKS_HEADERS
)

if(PROTOCOLS_ENABLE_LANGCHAIN)
    list(APPEND PROTOCOLS_FRAMEWORKS_HEADERS
        frameworks/langchain/include/langchain_adapter.h)
endif()

if(PROTOCOLS_ENABLE_AUTOGEN)
    list(APPEND PROTOCOLS_FRAMEWORKS_HEADERS
        frameworks/autogen/include/autogen_adapter.h)
endif()

# ============================================================
# 合并所有头文件
# ============================================================
set(PROTOCOLS_ALL_HEADERS
    ${PROTOCOLS_COMMON_HEADERS}
    ${PROTOCOLS_CORE_HEADERS}
    ${PROTOCOLS_STANDARDS_HEADERS}
    ${PROTOCOLS_INTEGRATIONS_HEADERS}
    ${PROTOCOLS_FRAMEWORKS_HEADERS}
)

# ============================================================
# 创建库目标
# ============================================================
add_library(agentos_protocols ${PROTOCOLS_ALL_SOURCES})
target_include_directories(agentos_protocols PUBLIC common/include)

# 库属性
set_target_properties(agentos_protocols PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 2
    OUTPUT_NAME agentos_protocols
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# 链接依赖
if(TARGET commons)
    target_link_libraries(agentos_protocols commons)
endif()

# 编译定义
target_compile_definitions(agentos_protocols PRIVATE
    PROTOCOLS_VERSION="${PROJECT_VERSION}"
    PROTO_REGISTRY_MAX_ADAPTERS=32
)

# 安装目标 — 库
install(TARGETS agentos_protocols
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装目标 — 头文件
install(FILES ${PROTOCOLS_ALL_HEADERS}
    DESTINATION include/agentos/protocols
)

# ============================================================
# 构建信息输出
# ============================================================
message(STATUS "")
message(STATUS "=== AgentOS Protocols Module v${PROJECT_VERSION} ===")
message(STATUS "  Common sources:   ${PROTOCOLS_COMMON_SOURCES}")
message(STATUS "  Core sources:     ${PROTOCOLS_CORE_SOURCES}")
message(STATUS "  Standard sources: ${PROTOCOLS_STANDARDS_SOURCES}")
message(STATUS "  Integration src:  ${PROTOCOLS_INTEGRATIONS_SOURCES}")
message(STATUS "  Framework sources:${PROTOCOLS_FRAMEWORKS_SOURCES}")
message(STATUS "  Total source files: ${PROTOCOLS_ALL_SOURCES}")
message(STATUS "  Total header files: ${PROTOCOLS_ALL_HEADERS}")
message(STATUS "  OpenClaw:         ${PROTOCOLS_ENABLE_OPENCLAW}")
message(STATUS "  Claude:           ${PROTOCOLS_ENABLE_CLAUDE}")
message(STATUS "  LangChain:        ${PROTOCOLS_ENABLE_LANGCHAIN}")
message(STATUS "  AutoGen:          ${PROTOCOLS_ENABLE_AUTOGEN}")
message(STATUS "==============================================")
message(STATUS "")
