# ========================================================================
# AgentRT Unified Protocols Module
#
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-FileCopyrightText: 2026 SPHARX.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# 本模块提供统一的协议栈基础框架，包括：
# 1. 统一协议接口与消息类型 (Common Layer)
# 2. 核心组件：路由器/转换器/扩展框架/注册中心 (Core Layer)
# 3. 标准协议适配器：MCP/A2A (Standards Layer)
# 4. 集成适配器：OpenAI/OpenJiuwen/OpenClaw/Claude (Integrations Layer)
# 5. 框架适配器：LangChain/AutoGen (Frameworks Layer)
# 6. 统一接口定义 (Interfaces Layer)
# ========================================================================


# 编译选项
option(PROTOCOLS_ENABLE_OPENCLAW "Enable OpenClaw integration" OFF)
option(PROTOCOLS_ENABLE_CLAUDE "Enable Claude API integration" ON)
option(PROTOCOLS_ENABLE_LANGCHAIN "Enable LangChain framework adapter" ON)
option(PROTOCOLS_ENABLE_AUTOGEN "Enable AutoGen framework adapter" ON)
option(PROTOCOLS_ENABLE_AGNTCY "Enable AGNTCY ACP protocol adapter" ON)
option(PROTOCOLS_ENABLE_CHINA_ECO "Enable China domestic ecosystem protocol adapter" ON)

if(WIN32)
    set(PROTOCOLS_ENABLE_OPENCLAW OFF CACHE BOOL "OpenClaw requires Unix sockets" FORCE)
    set(PROTOCOLS_ENABLE_CHINA_ECO OFF CACHE BOOL "China Eco requires Unix" FORCE)
endif()

agentrt_print_info("[PROTOCOLS] Configuring protocols module (cJSON: ${AGENTRT_HAS_CJSON})")
agentrt_print_info("[PROTOCOLS]   MCP: ${PROTOCOLS_ENABLE_MCP}, A2A: ON, Claude: ${PROTOCOLS_ENABLE_CLAUDE}")

option(PROTOCOLS_ENABLE_MCP "Enable MCP protocol adapter" ON)
if(WIN32)
    set(PROTOCOLS_ENABLE_MCP OFF CACHE BOOL "MCP requires cJSON/unistd.h" FORCE)
endif()

# ============================================================
# 包含目录 (Include Directories)
# ============================================================
set(PROTOCOLS_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/core/router/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/adapter/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transformers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/registry/include
    ${CMAKE_CURRENT_SOURCE_DIR}/standards/a2a/include
    ${CMAKE_CURRENT_SOURCE_DIR}/standards/mcp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/integrations/openai/include
    ${CMAKE_CURRENT_SOURCE_DIR}/integrations/openjiuwen/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/compat/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/string/include
    # R-09-124: migrated from root global include_directories
    ${CMAKE_CURRENT_SOURCE_DIR}/../atoms/corekern/include
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

if(PROTOCOLS_ENABLE_AGNTCY)
    list(APPEND PROTOCOLS_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/standards/agntcy/include)
endif()

if(PROTOCOLS_ENABLE_CHINA_ECO)
    list(APPEND PROTOCOLS_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/integrations/china_eco/include)
endif()

# R-09-124: all includes moved to target_include_directories below

# ============================================================
# 源文件 (Source Files) — Common Layer
# ============================================================
set(PROTOCOLS_COMMON_SOURCES
    common/src/unified_protocol.c
    common/src/protocols_impl.c
    src/agentrt_protocol_interface.c
    src/protocol_toplevel_impl.c
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
)
if(PROTOCOLS_ENABLE_MCP)
    list(APPEND PROTOCOLS_STANDARDS_SOURCES
        standards/mcp/src/mcp_v1_adapter.c
        standards/mcp/src/mcp_transport.c)
endif()

if(PROTOCOLS_ENABLE_AGNTCY)
    list(APPEND PROTOCOLS_STANDARDS_SOURCES
        standards/agntcy/src/agntcy_acp_adapter.c)
endif()

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

if(PROTOCOLS_ENABLE_CHINA_ECO)
    list(APPEND PROTOCOLS_INTEGRATIONS_SOURCES
        integrations/china_eco/src/china_eco_adapter.c)
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
    include/unified_protocol.h
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
    standards/mcp/include/mcp_transport.h
)

if(PROTOCOLS_ENABLE_AGNTCY)
    list(APPEND PROTOCOLS_STANDARDS_HEADERS
        standards/agntcy/include/agntcy_acp_adapter.h)
endif()

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

if(PROTOCOLS_ENABLE_CHINA_ECO)
    list(APPEND PROTOCOLS_INTEGRATIONS_HEADERS
        integrations/china_eco/include/china_eco_adapter.h)
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
add_library(agentrt_protocols ${PROTOCOLS_ALL_SOURCES})
target_include_directories(agentrt_protocols PUBLIC ${PROTOCOLS_INCLUDE_DIRS})

set_target_properties(agentrt_protocols PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    OUTPUT_NAME agentrt_protocols
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

if(TARGET agentrt_common)
    target_link_libraries(agentrt_protocols PRIVATE agentrt_common)
endif()
if(TARGET svc_common)
    target_link_libraries(agentrt_protocols PRIVATE svc_common)
endif()
target_link_libraries(agentrt_protocols PUBLIC agentrt_compile_defs)

if(AGENTRT_HAS_CURL)
    if(CURL_FOUND)
        target_link_libraries(agentrt_protocols PRIVATE ${CURL_LIBRARIES})
        target_include_directories(agentrt_protocols PRIVATE ${CURL_INCLUDE_DIRS})
    elseif(LIBCURL_FOUND)
        target_link_libraries(agentrt_protocols PRIVATE ${LIBCURL_LIBRARIES})
        target_include_directories(agentrt_protocols PRIVATE ${LIBCURL_INCLUDE_DIRS})
    endif()
endif()
if(AGENTRT_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(agentrt_protocols PRIVATE cjson)
    elseif(CJSON_FOUND)
        target_link_libraries(agentrt_protocols PRIVATE ${CJSON_LIBRARIES})
        target_include_directories(agentrt_protocols PRIVATE ${CJSON_INCLUDE_DIRS})
    endif()
endif()

target_compile_definitions(agentrt_protocols PRIVATE
    PROTOCOLS_VERSION="${PROJECT_VERSION}"
    PROTO_REGISTRY_MAX_ADAPTERS=32
)
if(MSVC)
    target_compile_options(agentrt_protocols PRIVATE /wd4100 /wd4189)
endif()

# 安装目标 — 库
install(TARGETS agentrt_protocols
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装目标 — 头文件
install(FILES ${PROTOCOLS_ALL_HEADERS}
    DESTINATION include/agentrt/protocols
)

# ============================================================
# 构建信息输出
# ============================================================
agentrt_print_info("")
agentrt_print_info("=== AgentRT Protocols Module v${PROJECT_VERSION} ===")
agentrt_print_info("  Common sources:   ${PROTOCOLS_COMMON_SOURCES}")
agentrt_print_info("  Core sources:     ${PROTOCOLS_CORE_SOURCES}")
agentrt_print_info("  Standard sources: ${PROTOCOLS_STANDARDS_SOURCES}")
agentrt_print_info("  Integration src:  ${PROTOCOLS_INTEGRATIONS_SOURCES}")
agentrt_print_info("  Framework sources:${PROTOCOLS_FRAMEWORKS_SOURCES}")
agentrt_print_info("  Total source files: ${PROTOCOLS_ALL_SOURCES}")
agentrt_print_info("  Total header files: ${PROTOCOLS_ALL_HEADERS}")
agentrt_print_info("  OpenClaw:         ${PROTOCOLS_ENABLE_OPENCLAW}")
agentrt_print_info("  Claude:           ${PROTOCOLS_ENABLE_CLAUDE}")
agentrt_print_info("  LangChain:        ${PROTOCOLS_ENABLE_LANGCHAIN}")
agentrt_print_info("  AutoGen:          ${PROTOCOLS_ENABLE_AUTOGEN}")
agentrt_print_info("  AGNTCY ACP:       ${PROTOCOLS_ENABLE_AGNTCY}")
agentrt_print_info("  China Eco:        ${PROTOCOLS_ENABLE_CHINA_ECO}")
agentrt_print_info("==============================================")
agentrt_print_info("")

# ============================================================
# Unit Tests
# ============================================================
if(BUILD_TESTS)
    set(PROTOCOL_TEST_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/integrations/openclaw/include
        ${CMAKE_CURRENT_SOURCE_DIR}/standards/agntcy/include
        ${CMAKE_CURRENT_SOURCE_DIR}/integrations/china_eco/include
        ${CMAKE_CURRENT_SOURCE_DIR}/core/transformers/include
        ${CMAKE_CURRENT_SOURCE_DIR}/core/router/include
        ${CMAKE_CURRENT_SOURCE_DIR}/core/adapter/include
        ${CMAKE_CURRENT_SOURCE_DIR}/core/registry/include
        ${CMAKE_CURRENT_SOURCE_DIR}/standards/a2a/include
        ${CMAKE_CURRENT_SOURCE_DIR}/standards/mcp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/integrations/openai/include
        ${CMAKE_CURRENT_SOURCE_DIR}/integrations/openjiuwen/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../commons/platform/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/types/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/error/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/memory/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/sync/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/compat/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/observability/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../atoms/corekern/include
    )

    if(PROTOCOLS_ENABLE_OPENCLAW)
        add_executable(test_openclaw_adapter
            tests/test_openclaw_adapter.c
            integrations/openclaw/src/openclaw_adapter.c
            common/src/unified_protocol.c
            core/transformers/src/protocol_transformers.c
        )
        target_compile_definitions(test_openclaw_adapter PRIVATE AGENTRT_COMPLIANCE_IMPL)
        target_include_directories(test_openclaw_adapter PRIVATE ${PROTOCOL_TEST_INCLUDE_DIRS})
        target_link_libraries(test_openclaw_adapter PRIVATE agentrt_common svc_common m)
        if(UNIX)
            target_link_libraries(test_openclaw_adapter PRIVATE pthread)
        endif()
        add_test(NAME test_openclaw_adapter COMMAND test_openclaw_adapter)
    endif()

    if(PROTOCOLS_ENABLE_AGNTCY)
        add_executable(test_agntcy_acp
            tests/test_agntcy_acp.c
            standards/agntcy/src/agntcy_acp_adapter.c
            common/src/unified_protocol.c
            core/transformers/src/protocol_transformers.c
        )
        target_compile_definitions(test_agntcy_acp PRIVATE AGENTRT_COMPLIANCE_IMPL)
        target_include_directories(test_agntcy_acp PRIVATE ${PROTOCOL_TEST_INCLUDE_DIRS})
        target_link_libraries(test_agntcy_acp PRIVATE agentrt_common svc_common m)
        if(UNIX)
            target_link_libraries(test_agntcy_acp PRIVATE pthread)
        endif()
        add_test(NAME test_agntcy_acp COMMAND test_agntcy_acp)
    endif()

    if(PROTOCOLS_ENABLE_CHINA_ECO)
        add_executable(test_china_eco_crypto
            tests/test_china_eco_crypto.c
            integrations/china_eco/src/china_eco_adapter.c
            common/src/unified_protocol.c
            core/transformers/src/protocol_transformers.c
        )
        target_compile_definitions(test_china_eco_crypto PRIVATE AGENTRT_COMPLIANCE_IMPL)
        target_include_directories(test_china_eco_crypto PRIVATE ${PROTOCOL_TEST_INCLUDE_DIRS})
        target_link_libraries(test_china_eco_crypto PRIVATE agentrt_common svc_common m)
        if(UNIX)
            target_link_libraries(test_china_eco_crypto PRIVATE pthread)
        endif()
        add_test(NAME test_china_eco_crypto COMMAND test_china_eco_crypto)
    endif()
endif()
