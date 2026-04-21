# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Market 服务测试构建文件 (TeamC)

cmake_minimum_required(VERSION 3.20)

set(MARKET_TEST_SOURCES
    test_agent_registry.c
    test_skill_registry.c
    test_installer.c
)

add_executable(test_market_d ${MARKET_TEST_SOURCES})

target_include_directories(test_market_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/include
)

target_link_libraries(test_market_d PRIVATE
    svc_common
    Threads::Threads
)

find_package(cjson QUIET)
if(TARGET cjson OR cjson_FOUND)
    target_link_libraries(test_market_d PRIVATE cjson)
endif()

target_compile_options(test_market_d PRIVATE
    -Wall -Wextra -Wpedantic -g -Wno-unused-function
)

enable_testing()
add_test(NAME market_d_tests COMMAND test_market_d
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
