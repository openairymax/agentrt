# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Tool 服务单元测试构建文件 (TeamC)

cmake_minimum_required(VERSION 3.20)

set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)

set(TOOL_IMPL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/executor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/validator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/registry.c
)

find_package(yaml QUIET)
if(YAML_FOUND OR yaml_FOUND)
    list(APPEND TOOL_IMPL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/config.c)
endif()

set(TOOL_TEST_SOURCES
    test_service.c
    test_cache.c
    test_executor.c
    test_validator.c
    test_registry.c
)

set(TOOL_COMMON_INCLUDES
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

foreach(TEST_SRC ${TOOL_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

    add_executable(test_tool_${TEST_NAME} ${TEST_SRC} ${TOOL_IMPL_SOURCES})

    target_include_directories(test_tool_${TEST_NAME} PRIVATE ${TOOL_COMMON_INCLUDES})

    target_link_libraries(test_tool_${TEST_NAME} PRIVATE
        svc_common
        Threads::Threads
    )

    find_package(cjson QUIET)
    if(TARGET cjson OR cjson_FOUND)
        target_link_libraries(test_tool_${TEST_NAME} PRIVATE cjson)
    endif()

    if(YAML_FOUND OR yaml_FOUND)
        target_link_libraries(test_tool_${TEST_NAME} PRIVATE yaml)
    endif()

    target_compile_options(test_tool_${TEST_NAME} PRIVATE -Wall -Wextra -g -Wno-unused-function)

    add_test(NAME tool_d_${TEST_NAME} COMMAND test_tool_${TEST_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

enable_testing()
