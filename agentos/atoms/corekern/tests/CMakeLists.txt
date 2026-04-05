# corekern 单元测试
cmake_minimum_required(VERSION 3.15)

# 添加测试可执行文件
add_executable(corekern_tests
    test_error.c
    test_main.c
    test_ipc.c
    test_mem.c
    test_task.c
    test_time.c
    test_corekern_extended.c
)

# 链接核心库
target_link_libraries(corekern_tests
    agentos_core
)

# 包含头文件路径
target_include_directories(corekern_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 添加测试
add_test(NAME corekern_error_tests COMMAND corekern_tests)
add_test(NAME corekern_basic_tests COMMAND corekern_tests)
add_test(NAME corekern_ipc_tests COMMAND corekern_tests)
add_test(NAME corekern_mem_tests COMMAND corekern_tests)
add_test(NAME corekern_task_tests COMMAND corekern_tests)
add_test(NAME corekern_time_tests COMMAND corekern_tests)