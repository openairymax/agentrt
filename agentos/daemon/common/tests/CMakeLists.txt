# Copyright (c) 2026 SPHARX. All Rights Reserved.
# agentos/daemon/common 单元测试 (TeamC)
# 对齐: 看板v14.0 C-W1-003 / 规范手册v13.0 TEST-01~06

cmake_minimum_required(VERSION 3.20)

project(svc_common_tests VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

set(TEST_SOURCES
    test_error.c
    test_platform.c
    test_logger.c
    test_config.c
    test_svc_auth.c
    test_jsonrpc_helpers.c
    test_safe_string_utils.c
    test_input_validator.c
)

foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    set(TARGET_NAME "svc_${TEST_NAME}")
    
    add_executable(${TARGET_NAME} ${TEST_SRC})
    
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons/platform/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons/utils/error/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons/utils/types/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons/utils/memory/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../atoms/corekern/include
    )
    
    target_link_libraries(${TARGET_NAME} PRIVATE
        svc_common
        Threads::Threads
    )
    
    find_package(cjson QUIET)
    if(TARGET cjson OR cjson_FOUND)
        target_link_libraries(${TARGET_NAME} PRIVATE cjson)
    endif()
    
    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_include_directories(${TARGET_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
        target_link_libraries(${TARGET_NAME} PRIVATE ${CURL_LIBRARIES})
    endif()
    
    target_compile_options(${TARGET_NAME} PRIVATE
        -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
    )
    
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    
    message(STATUS "  [test] ${TARGET_NAME}: configured")
endforeach()

message(STATUS "  svc_common_tests: ${TEST_SOURCES}")

# ===== P1-C06 深度单元测试 (test_daemon_common.c) =====
add_executable(svc_test_daemon_common test_daemon_common.c)

target_include_directories(svc_test_daemon_common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/include
)

target_link_libraries(svc_test_daemon_common PRIVATE
    svc_common
    Threads::Threads
)

find_package(cjson QUIET)
if(TARGET cjson OR cjson_FOUND)
    target_link_libraries(svc_test_daemon_common PRIVATE cjson)
endif()

find_package(CURL QUIET)
if(CURL_FOUND)
    target_include_directories(svc_test_daemon_common PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(svc_test_daemon_common PRIVATE ${CURL_LIBRARIES})
endif()

target_compile_options(svc_test_daemon_common PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

add_test(NAME svc_test_daemon_common COMMAND svc_test_daemon_common
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "  [test] svc_test_daemon_common: P1-C06 deep unit tests configured")
