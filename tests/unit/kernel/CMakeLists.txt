# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."

cmake_minimum_required(VERSION 3.20)
project(agentos-core-tests)

# 包含核心模块的头文件
include_directories(${CMAKE_SOURCE_DIR}/atoms/corekern/include)

# 链接核心模块库
link_directories(${CMAKE_BINARY_DIR}/atoms/corekern)

# 测试可执行文件
add_executable(test_ipc test_ipc.c)
target_link_libraries(test_ipc agentos_core Threads::Threads)

add_executable(test_mem test_mem.c)
target_link_libraries(test_mem agentos_core Threads::Threads)

add_executable(test_time test_time.c)
target_link_libraries(test_time agentos_core Threads::Threads)

add_executable(test_task test_task.c)
target_link_libraries(test_task agentos_core Threads::Threads)

# 运行测试
add_custom_target(run_tests
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_ipc
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_mem
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_time
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_task
    DEPENDS test_ipc test_mem test_time test_task
    COMMENT "Running core module tests"
)
