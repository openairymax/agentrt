# gateway - AgentOS 网关模块
# 协议转换层：HTTP · WebSocket · Stdio
#
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-FileCopyrightText: 2026 SPHARX.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

project(gateway
    VERSION 1.0.0
    DESCRIPTION "AgentOS Gateway Module - Protocol Translation Layer"
    LANGUAGES C
)

# ========== 编译选项 ==========
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /WX- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ========== 依赖 ==========
# 使用与根项目一致的依赖查找方式
find_package(cJSON QUIET)
find_package(libmicrohttpd QUIET)
find_package(libwebsockets QUIET)

# 条件定义和消息
if(cJSON_FOUND)
    message(STATUS "cJSON found: ${cJSON_VERSION}")
    add_definitions(-DGATEWAY_HAS_CJSON=1)
    # cJSON头文件在 /usr/include/cjson/cJSON.h
    if(CJSON_INCLUDE_DIRS)
        include_directories(${CJSON_INCLUDE_DIRS}/cjson)
    endif()
else()
    message(WARNING "cJSON not found, JSON functionality may be limited")
endif()

if(libmicrohttpd_FOUND)
    message(STATUS "libmicrohttpd found")
    add_definitions(-DGATEWAY_HAS_LIBMICROHTTPD=1)
else()
    message(WARNING "libmicrohttpd not found, HTTP gateway may be limited")
endif()

if(libwebsockets_FOUND)
    message(STATUS "libwebsockets found")
    add_definitions(-DGATEWAY_HAS_LIBWEBSOCKETS=1)
else()
    message(WARNING "libwebsockets not found, WebSocket gateway may be limited")
endif()

# 核心头文件路径
set(COREKERN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../atoms/corekern")
include_directories(${COREKERN_DIR}/include)

# syscall 模块
set(SYSCALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../atoms/syscall")
include_directories(${SYSCALL_DIR}/include)

# commons 模块（基础工具库）
set(COMMONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../commons")
include_directories(
    ${COMMONS_DIR}
    ${COMMONS_DIR}/utils/error/include
    ${COMMONS_DIR}/utils/logging/include
    ${COMMONS_DIR}/utils/memory/include
    ${COMMONS_DIR}/utils/sync/include
)

# protocols 模块（统一协议栈）
set(PROTOCOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../protocols")
include_directories(
    ${PROTOCOLS_DIR}/include
    ${PROTOCOLS_DIR}/protocol/include
)

# gateway内部工具头文件
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

# ========== 源文件 ==========
set(GATEWAY_SOURCES
    src/gateway/gateway_api.c
    src/gateway/http_gateway.c
    src/gateway/http_gateway_routes.c
    src/gateway/ws_gateway.c
    src/gateway/stdio_gateway.c
    src/utils/jsonrpc.c
    src/utils/syscall_router.c
    src/utils/gateway_rpc_handler.c
    src/utils/gateway_protocol_handler.c
    src/utils/gateway_rate_limiter.c
)

# ========== 头文件 ==========
set(GATEWAY_HEADERS
    src/gateway/gateway.h
    src/gateway/http_gateway.h
    src/gateway/ws_gateway.h
    src/gateway/stdio_gateway.h
    src/utils/jsonrpc.h
    src/utils/syscall_router.h
    src/utils/gateway_utils.h
    src/utils/gateway_protocol_handler.h
    src/utils/gateway_rate_limiter.h
)

# ========== 库目标 ==========
add_library(gateway STATIC ${GATEWAY_SOURCES})

target_include_directories(gateway
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(gateway PRIVATE
    # agentos_protocols  # TODO: protocols模块有编译错误(TeamB修复后启用)
)

# 条件链接依赖库
if(cJSON_FOUND)
    target_link_libraries(gateway PUBLIC cjson)
endif()

if(libmicrohttpd_FOUND)
    target_link_libraries(gateway PRIVATE microhttpd)
endif()

if(libwebsockets_FOUND)
    target_link_libraries(gateway PRIVATE websockets)
endif()

# ========== 安装 ==========
install(TARGETS gateway
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    include/gateway.h
    DESTINATION include/agentos/gateway
)

install(FILES
    src/gateway/gateway.h
    src/gateway/http_gateway.h
    src/gateway/ws_gateway.h
    src/gateway/stdio_gateway.h
    src/utils/jsonrpc.h
    src/utils/syscall_router.h
    DESTINATION include/agentos/gateway
)

# ========== 测试 ==========
option(BUILD_TESTS "Build tests" ON)
option(BUILD_BENCHMARK "Build performance benchmark tool (PERF-001~003)" ON)
if(BUILD_TESTS)
    enable_testing()

    # 主网关测试 (7个用例)
    add_executable(test_gateway tests/test_gateway.c)
    target_link_libraries(test_gateway gateway)
    add_test(NAME gateway_test COMMAND test_gateway)

    # syscall_router 测试 (8个用例)
    add_executable(test_syscall_router tests/test_syscall_router.c)
    target_link_libraries(test_syscall_router gateway)
    add_test(NAME syscall_router_test COMMAND test_syscall_router)

    # jsonrpc 协议测试 (17个用例)
    add_executable(test_jsonrpc tests/test_jsonrpc.c)
    target_link_libraries(test_jsonrpc gateway)
    add_test(NAME jsonrpc_test COMMAND test_jsonrpc)

    # gateway_rpc_handler 统一RPC处理模块测试 (14个用例)
    add_executable(test_rpc_handler tests/test_gateway_rpc_handler.c)
    target_link_libraries(test_rpc_handler gateway)
    add_test(NAME rpc_handler_test COMMAND test_rpc_handler)

    # 性能基准测试工具 (C-W1-004 / PERF-001~003)
    if(BUILD_BENCHMARK)
        add_executable(gateway_benchmark tests/gateway_benchmark.c)
        target_include_directories(gateway_benchmark PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../commons/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_link_libraries(gateway_benchmark PRIVATE gateway Threads::Threads m)

        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(LIBCURL QUIET libcurl)
            if(LIBCURL_FOUND)
                target_include_directories(gateway_benchmark PRIVATE ${LIBCURL_INCLUDE_DIRS})
                target_link_libraries(gateway_benchmark PRIVATE ${LIBCURL_LIBRARIES})
                target_compile_definitions(gateway_benchmark PRIVATE USE_CURL=1)
                message(STATUS "  gateway_benchmark: with libcurl support")
            else()
                message(STATUS "  gateway_benchmark: simulation mode (no libcurl)")
            endif()
        endif()

        message(STATUS "  gateway_benchmark: configured (TeamC C-W1-004)")
    endif()
endif()

# ========== 格式化 ==========
find_program(CLANG_FORMAT "clang-format")
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=file ${GATEWAY_SOURCES} ${GATEWAY_HEADERS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting code with clang-format"
    )
endif()

# ========== 静态分析 ==========
find_program(CPPCHECK "cppcheck")
if(CPPCHECK)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK} --enable=all --std=c11
            --suppress=missingIncludeSystem
            --config-file=${CMAKE_CURRENT_SOURCE_DIR}/config/cppcheck.cfg
            ${GATEWAY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis"
    )
endif()
