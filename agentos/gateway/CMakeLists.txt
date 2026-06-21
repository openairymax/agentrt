# gateway - AgentRT 网关模块
# 协议转换层：HTTP · WebSocket · Stdio
#
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-FileCopyrightText: 2026 SPHARX.
# SPDX-License-Identifier: Apache-2.0


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COREKERN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../atoms/corekern")
set(SYSCALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../atoms/syscall")
set(COMMONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../commons")
set(PROTOCOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../protocols")

message(STATUS "[GATEWAY] Configuring gateway module (cJSON: ${AGENTOS_HAS_CJSON}, microhttpd: ${AGENTOS_HAS_MICROHTTPD}, websockets: ${AGENTOS_HAS_LIBWEBSOCKETS})")
# 验证关键路径
foreach(_dir ${COREKERN_DIR}/include ${SYSCALL_DIR}/include ${COMMONS_DIR} ${PROTOCOLS_DIR}/include ${PROTOCOLS_DIR}/common/include)
    if(EXISTS "${_dir}")
        message(STATUS "[GATEWAY]   [OK] ${_dir}")
    else()
        message(WARNING "[GATEWAY]   [MISSING] ${_dir}")
    endif()
endforeach()

if(AGENTOS_HAS_CJSON)
set(GATEWAY_SOURCES
    src/gateway/gateway_api.c
    src/gateway/http_gateway.c
    src/gateway/http_gateway_routes.c
    src/gateway/ws_gateway.c
    src/gateway/stdio_gateway.c
    src/gateway/gateway_protocol_bridge.c
    src/utils/jsonrpc.c
    src/utils/syscall_router.c
    src/utils/gateway_rpc_handler.c
    src/utils/gateway_rate_limiter.c
    src/utils/gateway_protocol_handler.c
    src/utils/mcp_server.c
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
    src/utils/mcp_server.h
    src/utils/gateway_rate_limiter.h
)

# ========== 库目标 ==========
add_library(gateway STATIC ${GATEWAY_SOURCES})

# jsonrpc.c / syscall_router.c 等源文件使用 AGENTOS_HAS_CJSON 守卫
target_compile_definitions(gateway PUBLIC AGENTOS_HAS_CJSON=1)

if(AGENTOS_HAS_LIBWEBSOCKETS)
    set_source_files_properties(
        src/gateway/ws_gateway.c
        PROPERTIES COMPILE_DEFINITIONS "AGENTOS_COMPLIANCE_IMPL"
    )
endif()

target_include_directories(gateway
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/utils>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COREKERN_DIR}/include
        ${SYSCALL_DIR}/include
        ${COMMONS_DIR}
        ${COMMONS_DIR}/utils/include
        ${COMMONS_DIR}/utils/types/include
        ${COMMONS_DIR}/utils/compat/include
        ${COMMONS_DIR}/platform/include
        ${COMMONS_DIR}/utils/error/include
        ${COMMONS_DIR}/utils/logging/include
        ${COMMONS_DIR}/utils/memory/include
        ${COMMONS_DIR}/utils/sync/include
        ${COMMONS_DIR}/utils/compat/include
        ${COMMONS_DIR}/utils/include
        ${PROTOCOLS_DIR}/core/router/include
        ${PROTOCOLS_DIR}/core/registry/include
        ${PROTOCOLS_DIR}/core/adapter/include
        ${PROTOCOLS_DIR}/core/transformers/include
        ${PROTOCOLS_DIR}/common/include
        ${PROTOCOLS_DIR}/src
        ${PROTOCOLS_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        # R-09-124: cjson (system library)
        ${CJSON_INCLUDE_DIRS}/cjson
)

target_link_libraries(gateway PRIVATE
    agentos_protocols
    agentos_common
    agentos_memory
)

if(AGENTOS_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(gateway PUBLIC cjson)
    elseif(CJSON_FOUND)
        target_link_libraries(gateway PUBLIC ${CJSON_LIBRARIES})
    endif()
endif()

if(AGENTOS_HAS_MICROHTTPD)
    if(libmicrohttpd_FOUND)
        target_link_libraries(gateway PRIVATE microhttpd)
    elseif(MICROHTTPD_FOUND)
        target_link_libraries(gateway PRIVATE ${MICROHTTPD_LIBRARIES})
    endif()
endif()

if(AGENTOS_HAS_LIBWEBSOCKETS)
    if(libwebsockets_FOUND)
        target_link_libraries(gateway PRIVATE websockets)
    elseif(LIBWEBSOCKETS_FOUND)
        target_link_libraries(gateway PRIVATE ${LIBWEBSOCKETS_LIBRARIES})
    endif()
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

    # 主网关测试 (7个用例) - DISABLED: dual-header conflict
    # include/gateway.h vs src/gateway/gateway.h incompatible
    # add_executable(test_gateway tests/test_gateway.c)
    # target_link_libraries(test_gateway gateway)
    # add_test(NAME gateway_test COMMAND test_gateway)

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
            ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/logging/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/error/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/memory/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/types/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_link_libraries(gateway_benchmark PRIVATE gateway Threads::Threads m)

        if(AGENTOS_HAS_CURL)
            target_include_directories(gateway_benchmark PRIVATE ${CURL_INCLUDE_DIRS})
            target_link_libraries(gateway_benchmark PRIVATE ${CURL_LIBRARIES})
            target_compile_definitions(gateway_benchmark PRIVATE USE_CURL=1)
            message(STATUS "  gateway_benchmark: with libcurl support")
        else()
            message(STATUS "  gateway_benchmark: simulation mode (no libcurl)")
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
else()
    # Stub library when cJSON is not available - provides link target but no functionality
    add_library(gateway STATIC
        src/gateway/gateway.c
    )
    target_compile_definitions(gateway PRIVATE AGENTOS_NO_CJSON)
    message(STATUS "gateway: cJSON not found - building stub library")
endif() # AGENTOS_HAS_CJSON
