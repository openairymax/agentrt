# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Monitor 服务测试构建文件

cmake_minimum_required(VERSION 3.20)

set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)

set(MONIT_IMPL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/metrics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/alert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/tracing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/logging.c
)

set(MONIT_TEST_SOURCES
    test_metrics.c
    test_alert.c
    test_tracing.c
)

set(MONIT_COMMON_INCLUDES
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

foreach(TEST_SRC ${MONIT_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

    add_executable(test_monit_${TEST_NAME} ${TEST_SRC} ${MONIT_IMPL_SOURCES})

    target_include_directories(test_monit_${TEST_NAME} PRIVATE ${MONIT_COMMON_INCLUDES})

    target_link_libraries(test_monit_${TEST_NAME} PRIVATE
        svc_common
        Threads::Threads
    )

    target_compile_options(test_monit_${TEST_NAME} PRIVATE
        -Wall
        -Wextra
        -Wno-unused-function
    )

    add_test(NAME monit_d_${TEST_NAME} COMMAND test_monit_${TEST_NAME})
endforeach()

enable_testing()
