# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Scheduler 服务测试构建文件

cmake_minimum_required(VERSION 3.20)

set(SCHED_TEST_SOURCES
    test_scheduler.c
    test_strategies.c
)

add_executable(test_sched_d ${SCHED_TEST_SOURCES})

target_include_directories(test_sched_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../bases/include
)

target_link_libraries(test_sched_d PRIVATE
    svc_common
)

target_compile_options(test_sched_d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

enable_testing()

add_test(NAME sched_d_all COMMAND test_sched_d)
