# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Market 服务测试构建文件

cmake_minimum_required(VERSION 3.20)

set(MARKET_TEST_SOURCES
    test_agent_registry.c
    test_skill_registry.c
    test_installer.c
)

add_executable(test_market_d ${MARKET_TEST_SOURCES})

target_include_directories(test_market_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
)

target_link_libraries(test_market_d PRIVATE
    svc_common
    cjson
)

target_compile_options(test_market_d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

enable_testing()

add_test(NAME market_d_all COMMAND test_market_d)
