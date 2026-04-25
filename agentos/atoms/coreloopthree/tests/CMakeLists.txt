# Copyright (c) 2026 SPHARX. All Rights Reserved.
# CoreLoopThree 测试套件构建文件

cmake_minimum_required(VERSION 3.20)

set(CORELOOPTHREE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)
set(MEMORYROVOL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../memoryrovol/src)

set(CL3_TEST_SOURCES
    unit/test_loop.c
    unit/test_cognition.c
    unit/test_execution.c
    unit/test_memory.c
    unit/test_compensation.c
    unit/test_intent_parser.c
)

set(CL3_COMMON_INCLUDES
    ${CORELOOPTHREE_DIR}/include
    ${CORELOOPTHREE_DIR}/src
    ${COREKERN_INCLUDE_DIR}
    ${COMMONS_DIR}/platform/include
    ${COMMONS_DIR}/utils/core/include
    ${COMMONS_DIR}/utils/error/include
    ${COMMONS_DIR}/utils/logging/include
    ${COMMONS_DIR}/utils/memory/include
    ${COMMONS_DIR}/utils/string/include
    ${COMMONS_DIR}/utils/sync/include
    ${COMMONS_DIR}/utils/cache/include
    ${COMMONS_DIR}/utils/compat/include
    ${MEMORYROVOL_SRC_DIR}
    ${MEMORYROVOL_INCLUDE_DIR}
)

foreach(TEST_SRC ${CL3_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    string(REPLACE "test_" "" TEST_SHORT_NAME ${TEST_NAME})

    add_executable(test_cl3_${TEST_SHORT_NAME} ${TEST_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_cl3_stubs.c
    )

    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/unit/test_cl3_stubs.c
        PROPERTIES COMPILE_FLAGS "-fno-lto"
    )

    target_include_directories(test_cl3_${TEST_SHORT_NAME} PRIVATE ${CL3_COMMON_INCLUDES})

    target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE
        -Wl,--whole-archive
        agentos_coreloopthree
        -Wl,--no-whole-archive
        agentos_cognition
        agentos_execution
        agentos_core
        agentos_memoryrovol
        agentos_atoms
        pthread
        dl
        m
    )

    if(UNIX)
        target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE rt)
    endif()

    target_compile_options(test_cl3_${TEST_SHORT_NAME} PRIVATE
        -Wall -Wextra -g -Wno-unused-function
    )

    if(UNIX)
        target_link_options(test_cl3_${TEST_SHORT_NAME} PRIVATE
            -Wl,--allow-multiple-definition
        )
    endif()

    add_test(NAME cl3_${TEST_SHORT_NAME} COMMAND test_cl3_${TEST_SHORT_NAME})
endforeach()

enable_testing()
