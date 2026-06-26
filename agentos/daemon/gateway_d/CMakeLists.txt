# Gateway Daemon - AgentRT 网关守护进程
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#
# 架构说明：
#   gateway_d 是 gateway 库的用户空间守护进程包装器。
#   它依赖 gateway 库提供的公共 API (include/gateway.h)，
#   并通过 gateway 库间接调用 agentos/atoms/syscall 接口。
#
#   依赖链：gateway_d --> gateway (静态库) --> agentos/atoms/syscall + agentos/atoms/corekern



set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 使用根级依赖检测结果 (BAN-12: 单一依赖检测点)
# Threads, PkgConfig, OpenSSL, cJSON, libmicrohttpd, libwebsockets
# 已在根 CMakeLists.txt 中统一查找

# ==================== 路径设置 ====================

# Gateway 库路径（同层级的 gateway 模块）
set(GATEWAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../gateway)
set(GATEWAY_INCLUDE_DIR ${GATEWAY_DIR}/include)
set(GATEWAY_SRC_DIR ${GATEWAY_DIR}/src)

# Atom 模块路径（syscall 接口 + corekern 基础定义）
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)

# agentos/daemon/common 兼容层路径
set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

# commons 统一基础库路径
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

# protocols 协议栈路径
set(PROTOCOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../protocols)
set(PROTOCOLS_INCLUDE_DIR ${PROTOCOLS_DIR}/include)
set(PROTOCOLS_MCP_DIR ${PROTOCOLS_DIR}/standards/mcp)
set(PROTOCOLS_MCP_INCLUDE_DIR ${PROTOCOLS_MCP_DIR}/include)
set(PROTOCOLS_A2A_DIR ${PROTOCOLS_DIR}/standards/a2a)
set(PROTOCOLS_A2A_INCLUDE_DIR ${PROTOCOLS_A2A_DIR}/include)
set(PROTOCOLS_OPENAI_DIR ${PROTOCOLS_DIR}/integrations/openai)
set(PROTOCOLS_OPENAI_INCLUDE_DIR ${PROTOCOLS_OPENAI_DIR}/include)
set(PROTOCOLS_COMMON_INCLUDE_DIR ${PROTOCOLS_DIR}/common/include)

# ==================== 协议栈开关 ====================
option(AGENTOS_ENABLE_PROTOCOLS "Enable MCP/A2A/OpenAI protocol stack" ON)

# ==================== 源文件 ====================
set(GATEWAY_D_SOURCES
    src/main.c
    src/service.c
    src/gateway_svc_adapter.c
    src/gateway_forward.c
)

if(AGENTOS_ENABLE_PROTOCOLS)
    list(APPEND GATEWAY_D_SOURCES
        src/protocol/gateway_mcp_server.c
        src/protocol/gateway_a2a_handler.c
        src/protocol/gateway_openai_compat.c
        src/protocol/gateway_protocol_router.c
    )
    message(STATUS "[gateway_d] Protocol stack: ENABLED (MCP/A2A/OpenAI)")
else()
    message(STATUS "[gateway_d] Protocol stack: DISABLED")
endif()

# 可执行文件
add_executable(gateway_d ${GATEWAY_D_SOURCES})

if(AGENTOS_ENABLE_PROTOCOLS)
    target_compile_definitions(gateway_d PRIVATE AGENTOS_HAS_PROTOCOLS=1)
endif()

# ==================== 平台特定配置 ====================
# 注意：agentos_common 不在此处直接链接，而是通过 svc_common 的 PUBLIC
# 传递依赖处理，避免在链接命令中重复出现导致 LTO 符号解析失败。
if(WIN32)
    target_compile_definitions(gateway_d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(gateway_d PRIVATE
        ws2_32
        bcrypt
    )
elseif(UNIX)
    target_link_libraries(gateway_d PRIVATE
        Threads::Threads
        agentos_platform_libs
    )
endif()

# ==================== 包含目录 ====================
target_include_directories(gateway_d PRIVATE
    # 本模块头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol
    # Gateway 公共 API（service.c 引用 "gateway.h"）
    ${GATEWAY_INCLUDE_DIR}
    # Gateway 内部头文件（gateway_api.c 引用 http_gateway.h 等）
    ${GATEWAY_SRC_DIR}
    ${GATEWAY_SRC_DIR}/gateway
    ${GATEWAY_SRC_DIR}/utils
    # Atom 模块（service.c 引用 "syscalls.h", "agentos.h"）
    ${SYSCALL_INCLUDE_DIR}
    ${COREKERN_INCLUDE_DIR}
    # Daemon 兼容层
    ${DAEMON_COMMON_INCLUDE_DIR}
    # Commons 统一基础库
    ${COMMONS_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}/platform/include
    ${COMMONS_INCLUDE_DIR}/utils/error/include
    ${COMMONS_INCLUDE_DIR}/utils/logging/include
    ${COMMONS_INCLUDE_DIR}/utils/config_unified/include
    ${COMMONS_INCLUDE_DIR}/utils/memory/include
    ${COMMONS_INCLUDE_DIR}/utils/string/include
    ${COMMONS_INCLUDE_DIR}/utils/sync/include
    ${COMMONS_INCLUDE_DIR}/utils/cache/include
    ${COMMONS_INCLUDE_DIR}/utils/compat/include
    # 网络和IPC模块
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
    # 协议栈
    ${PROTOCOLS_INCLUDE_DIR}
    ${PROTOCOLS_COMMON_INCLUDE_DIR}
    ${PROTOCOLS_MCP_DIR}/include
    ${PROTOCOLS_A2A_INCLUDE_DIR}
    ${PROTOCOLS_OPENAI_INCLUDE_DIR}
    # 外部依赖
    ${CJSON_INCLUDE_DIRS}
)

# ==================== 链接库 ====================
#
# 注意：gateway 库需要在构建时可用。
# 如果使用独立构建，需确保 gateway 已预先编译或通过 add_subdirectory 引入。
#
# 方式一（推荐 - 通过上级 CMake 引入）:
#   在 AgentRT 根 CMakeLists.txt 中同时 add_subdirectory(gateway) 和 add_subdirectory(daemon)
#   然后 gateway_d 可以直接链接 gateway target
#
# 方式二（独立构建 - 当前默认）:
#   gateway_d 自行编译 gateway 源码并链接（见下方）

# 将 gateway 库的源文件直接加入编译（确保符号可解析）
# gateway 库大量依赖 cJSON，不可用时整个库跳过
if(AGENTOS_HAS_CJSON)
set(GATEWAY_LIB_SOURCES
    ${GATEWAY_SRC_DIR}/gateway/gateway_api.c
    ${GATEWAY_SRC_DIR}/gateway/http_gateway.c
    ${GATEWAY_SRC_DIR}/gateway/http_gateway_routes.c
    ${GATEWAY_SRC_DIR}/gateway/ws_gateway.c
    ${GATEWAY_SRC_DIR}/gateway/stdio_gateway.c
    ${GATEWAY_SRC_DIR}/utils/jsonrpc.c
    ${GATEWAY_SRC_DIR}/utils/syscall_router.c
    ${GATEWAY_SRC_DIR}/utils/gateway_rpc_handler.c
    ${GATEWAY_SRC_DIR}/utils/gateway_protocol_handler.c
    ${GATEWAY_SRC_DIR}/utils/gateway_rate_limiter.c
)

# 创建 gateway 库对象（不安装，仅供 gateway_d 使用）
add_library(gateway_lib_obj OBJECT ${GATEWAY_LIB_SOURCES})

if(AGENTOS_HAS_LIBWEBSOCKETS)
    set_source_files_properties(
        ${GATEWAY_SRC_DIR}/gateway/ws_gateway.c
        PROPERTIES COMPILE_DEFINITIONS "AGENTOS_COMPLIANCE_IMPL"
    )
endif()
target_include_directories(gateway_lib_obj PUBLIC
    ${GATEWAY_INCLUDE_DIR}
    ${GATEWAY_SRC_DIR}
    ${GATEWAY_SRC_DIR}/gateway
    ${GATEWAY_SRC_DIR}/utils
    ${SYSCALL_INCLUDE_DIR}
    ${COREKERN_INCLUDE_DIR}
    ${CJSON_INCLUDE_DIRS}
    ${COMMONS_DIR}/platform/include
    ${COMMONS_DIR}/utils/logging/include
    ${COMMONS_DIR}/utils/error/include
    ${COMMONS_DIR}/utils/sync/include
    ${COMMONS_DIR}/utils/compat/include
    ${COMMONS_DIR}/utils/memory/include
    ${COMMONS_DIR}/utils/string/include
     ${COMMONS_DIR}/utils/include
     ${DAEMON_COMMON_INCLUDE_DIR}
     ${DAEMON_COMMON_DIR}/src
     ${PROTOCOLS_INCLUDE_DIR}
     ${PROTOCOLS_COMMON_INCLUDE_DIR}
)
endif()

# 条件包含外部库头文件和定义（使用根级缓存变量）
# 注意：gateway_lib_obj 仅在 AGENTOS_HAS_CJSON 时创建，此处必须检查目标存在性
if(AGENTOS_HAS_MICROHTTPD)
    if(TARGET gateway_lib_obj)
        if(libmicrohttpd_FOUND)
            target_include_directories(gateway_lib_obj PUBLIC ${libmicrohttpd_INCLUDE_DIRS})
        elseif(MICROHTTPD_FOUND)
            target_include_directories(gateway_lib_obj PUBLIC ${MICROHTTPD_INCLUDE_DIRS})
        endif()
        target_compile_definitions(gateway_lib_obj PUBLIC GATEWAY_HAS_HTTP=1 GATEWAY_HAS_LIBMICROHTTPD=1)
    endif()
    target_compile_definitions(gateway_d PRIVATE GATEWAY_HAS_HTTP=1 GATEWAY_HAS_LIBMICROHTTPD=1)
else()
    message(STATUS "libmicrohttpd not found, HTTP gateway may not compile.")
endif()

if(AGENTOS_HAS_LIBWEBSOCKETS)
    if(TARGET gateway_lib_obj)
        if(libwebsockets_FOUND)
            target_include_directories(gateway_lib_obj PUBLIC ${libwebsockets_INCLUDE_DIRS})
        elseif(LIBWEBSOCKETS_FOUND)
            target_include_directories(gateway_lib_obj PUBLIC ${LIBWEBSOCKETS_INCLUDE_DIRS})
        endif()
        target_compile_definitions(gateway_lib_obj PUBLIC GATEWAY_HAS_WS=1)
    endif()
    target_compile_definitions(gateway_d PRIVATE GATEWAY_HAS_WS=1)
else()
    message(STATUS "libwebsockets not found, WebSocket gateway may not compile.")
endif()

# 链接策略说明：
#   LTO + ASan 组合下，链接器扫描静态库时使用 LTO 符号摘要，可能漏掉符号
#   （表现为 undefined reference to memory_alloc 等，但符号确实存在于 .a 中）。
#   根 CMakeLists.txt 已添加 -ffat-lto-objects，让 LTO 对象同时包含机器代码，
#   链接器可用传统符号表解析静态库，无需 --whole-archive。
#   使用 --start-group/--end-group 处理 svc_common 与 agentos_common 之间的
#   传递依赖符号解析（agentos_common 通过 svc_common PUBLIC 传递，位于组外之后，
#   单次扫描即可解析）。
if(NOT MSVC AND NOT APPLE)
    target_link_libraries(gateway_d PRIVATE
        "-Wl,--start-group"
        svc_common
        agentos_protocols
        "-Wl,--end-group"
        Threads::Threads
    )
else()
    target_link_libraries(gateway_d PRIVATE
        svc_common
        agentos_protocols
        Threads::Threads
    )
endif()
if(AGENTOS_HAS_CJSON)
    target_link_libraries(gateway_d PRIVATE
        $<TARGET_OBJECTS:gateway_lib_obj>
    )
endif()

if(AGENTOS_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(gateway_d PRIVATE cjson)
    elseif(CJSON_FOUND)
        target_link_libraries(gateway_d PRIVATE ${CJSON_LIBRARIES})
    endif()
endif()

if(AGENTOS_HAS_MICROHTTPD)
    if(libmicrohttpd_FOUND)
        target_link_libraries(gateway_d PRIVATE microhttpd)
    elseif(MICROHTTPD_FOUND)
        target_link_libraries(gateway_d PRIVATE ${MICROHTTPD_LIBRARIES})
    endif()
endif()
if(AGENTOS_HAS_LIBWEBSOCKETS)
    if(libwebsockets_FOUND)
        target_link_libraries(gateway_d PRIVATE websockets)
    elseif(LIBWEBSOCKETS_FOUND)
        target_link_libraries(gateway_d PRIVATE ${LIBWEBSOCKETS_LIBRARIES})
    endif()
endif()

# ==================== 编译选项 ====================
if(NOT MSVC)
target_compile_options(gateway_d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)
endif()

# ==================== 安装 ====================
install(TARGETS gateway_d
    RUNTIME DESTINATION bin
)

install(FILES
    include/gateway_service.h
    include/gateway_svc_adapter.h
    DESTINATION include/agentos/daemon/gateway_d
)

# ==================== 测试 ====================
if(BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()
