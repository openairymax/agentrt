# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Tool 服务单元测试构建文件 (TeamC)

cmake_minimum_required(VERSION 3.20)

set(TOOL_TEST_SOURCES
    test_service.c
    test_cache.c
    test_executor.c
    test_validator.c
    test_registry.c
)

add_executable(test_tool_d ${TOOL_TEST_SOURCES})

target_include_directories(test_tool_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/include
)

target_link_libraries(test_tool_d PRIVATE
    svc_common
    Threads::Threads
)

find_package(cjson QUIET)
if(TARGET cjson OR cjson_FOUND)
    target_link_libraries(test_tool_d PRIVATE cjson)
endif()

target_compile_options(test_tool_d PRIVATE -Wall -Wextra -g -Wno-unused-function)

enable_testing()
add_test(NAME tool_d_tests COMMAND test_tool_d
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
