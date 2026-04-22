# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Scheduler 服务测试构建文件

cmake_minimum_required(VERSION 3.20)

set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)

set(SCHED_IMPL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sched_service_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/strategies/round_robin.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/strategies/weighted.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/strategies/ml_based.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/strategies/priority_based.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/monitor.c
)

set(SCHED_TEST_SOURCES
    test_scheduler.c
    test_strategies.c
)

set(SCHED_COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${COMMONS_DIR}
    ${COMMONS_DIR}/platform/include
    ${COMMONS_DIR}/utils/error/include
    ${COMMONS_DIR}/utils/logging/include
    ${COMMONS_DIR}/utils/config_unified/include
    ${COMMONS_DIR}/utils/memory/include
    ${COMMONS_DIR}/utils/string/include
    ${COMMONS_DIR}/utils/sync/include
    ${COMMONS_DIR}/utils/cache/include
    ${COMMONS_DIR}/utils/compat/include
    ${SYSCALL_INCLUDE_DIR}
    ${COREKERN_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
)

foreach(TEST_SRC ${SCHED_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

    add_executable(test_sched_${TEST_NAME} ${TEST_SRC} ${SCHED_IMPL_SOURCES})

    target_include_directories(test_sched_${TEST_NAME} PRIVATE ${SCHED_COMMON_INCLUDES})

    target_link_libraries(test_sched_${TEST_NAME} PRIVATE
        svc_common
        Threads::Threads
        m
    )

    target_compile_options(test_sched_${TEST_NAME} PRIVATE
        -Wall
        -Wextra
        -Wno-unused-function
    )

    add_test(NAME sched_d_${TEST_NAME} COMMAND test_sched_${TEST_NAME})
endforeach()

enable_testing()
