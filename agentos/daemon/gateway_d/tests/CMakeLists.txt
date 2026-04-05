# Gateway Daemon Tests
# Copyright (C) 2026 SPHARX. All Rights Reserved.

cmake_minimum_required(VERSION 3.16)

project(gateway_d_tests VERSION 1.0.0 LANGUAGES C)

# 测试源文件
set(TEST_SOURCES
    test_service.c
)

# 创建测试
foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    
    add_executable(${TEST_NAME} ${TEST_SRC})
    
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    )
    
    target_link_libraries(${TEST_NAME}
        PRIVATE
            gateway_d_lib
            svc_common
    )
    
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
