# Gateway Daemon Tests (TeamC)
# Copyright (C) 2026 SPHARX. All Rights Reserved.

cmake_minimum_required(VERSION 3.16)

project(gateway_d_tests VERSION 1.0.0 LANGUAGES C)

set(TEST_SOURCES
    test_service.c
)

foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    
    add_executable(${TEST_NAME} ${TEST_SRC})
    
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/include
    )
    
    target_link_libraries(${TEST_NAME}
        PRIVATE
            svc_common
            Threads::Threads
    )
    
    target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -g -Wno-unused-function)
    
    add_test(NAME gateway_d_${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
