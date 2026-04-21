# Copyright (c) 2026 SPHARX. All Rights Reserved.
# agentos/gateway 单元测试 (TeamC)
# 对齐: 看板v14.0 C-W1-003 / 规范手册v13.0 TEST-01~06

cmake_minimum_required(VERSION 3.20)

project(gateway_tests VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 测试源文件
set(TEST_SOURCES
    test_gateway.c
    test_jsonrpc.c
    test_gateway_rpc_handler.c
    test_syscall_router.c
)

# 创建测试可执行文件
add_executable(test_gateway ${TEST_SOURCES})

target_include_directories(test_gateway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/include
)

# 核心依赖
target_link_libraries(test_gateway PRIVATE
    gateway_lib_obj
    svc_common
    Threads::Threads
)

# 可选依赖: cJSON (必需于JSON-RPC测试)
find_package(cjson QUIET)
if(TARGET cjson OR cjson_FOUND)
    target_link_libraries(test_gateway PRIVATE cjson)
    target_compile_definitions(test_gateway PRIVATE HAVE_CJSON=1)
endif()

# 可选依赖: libmicrohttpd (用于HTTP gateway测试)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(MICROHTTPD QUIET libmicrohttpd)
    if(MICROHTTPD_FOUND)
        target_include_directories(test_gateway PRIVATE ${MICROHTTPD_INCLUDE_DIRS})
        target_link_libraries(test_gateway PRIVATE ${MICROHTTPD_LIBRARIES})
    endif()
endif()

target_compile_options(test_gateway PRIVATE
    -Wall -Wextra -g -Wno-unused-function
)

enable_testing()
add_test(NAME gateway_tests COMMAND test_gateway
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "  gateway_tests: configured (${TEST_SOURCES})")
