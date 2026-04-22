# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Market 服务测试构建文件 (TeamC)

cmake_minimum_required(VERSION 3.20)

set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)

set(MARKET_IMPL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/market_service_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/agent_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/agent_registry_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/skill_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/installer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/publisher.c
)

set(MARKET_TEST_SOURCES
    test_agent_registry.c
    test_skill_registry.c
    test_installer.c
)

set(MARKET_COMMON_INCLUDES
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

foreach(TEST_SRC ${MARKET_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

    add_executable(test_market_${TEST_NAME} ${TEST_SRC} ${MARKET_IMPL_SOURCES})

    target_include_directories(test_market_${TEST_NAME} PRIVATE ${MARKET_COMMON_INCLUDES})

    target_link_libraries(test_market_${TEST_NAME} PRIVATE
        svc_common
        Threads::Threads
    )

    find_package(cjson QUIET)
    if(TARGET cjson OR cjson_FOUND)
        target_link_libraries(test_market_${TEST_NAME} PRIVATE cjson)
    endif()

    target_compile_options(test_market_${TEST_NAME} PRIVATE -Wall -Wextra -g -Wno-unused-function)

    add_test(NAME market_d_${TEST_NAME} COMMAND test_market_${TEST_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

enable_testing()
