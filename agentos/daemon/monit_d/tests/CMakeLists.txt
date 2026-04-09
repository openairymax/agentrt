# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Monitor 服务测试构建文件

cmake_minimum_required(VERSION 3.20)

set(MONIT_TEST_SOURCES
    test_metrics.c
    test_alert.c
    test_tracing.c
)

add_executable(test_monit_d ${MONIT_TEST_SOURCES})

target_include_directories(test_monit_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/include
)

target_link_libraries(test_monit_d PRIVATE
    svc_common
)

target_compile_options(test_monit_d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

enable_testing()

add_test(NAME monit_d_all COMMAND test_monit_d)
