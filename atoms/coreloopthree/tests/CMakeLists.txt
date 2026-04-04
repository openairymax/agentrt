# coreloopthree 单元测试
cmake_minimum_required(VERSION 3.15)

# 添加测试可执行文件
add_executable(coreloopthree_tests
    test_main.c
    test_majority.c
    test_coordinator.c
    test_coordinator_extended.c
    test_cognition.c
    test_execution.c
    test_loop.c
    test_memory.c
    benchmark.c
)

# 链接核心库
target_link_libraries(coreloopthree_tests
    agentos_coreloopthree
    agentos_cognition
    agentos_execution
    agentos_memory
    agentos_core
)

# 包含头文件路径
target_include_directories(coreloopthree_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../memoryrovol/include
)

# 安全编译选项
if(MSVC)
    target_compile_options(coreloopthree_tests PRIVATE /W4 /WX)
    target_compile_definitions(coreloopthree_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(coreloopthree_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(coreloopthree_tests PRIVATE -fstack-protector-strong)
    target_compile_options(coreloopthree_tests PRIVATE -D_FORTIFY_SOURCE=2)
    target_compile_options(coreloopthree_tests PRIVATE -fPIE)
endif()

# 添加测试
add_test(NAME coreloopthree_majority_tests COMMAND coreloopthree_tests)
add_test(NAME coreloopthree_coordinator_tests COMMAND coreloopthree_tests)
add_test(NAME coreloopthree_cognition_tests COMMAND coreloopthree_tests)
add_test(NAME coreloopthree_execution_tests COMMAND coreloopthree_tests)
add_test(NAME coreloopthree_loop_tests COMMAND coreloopthree_tests)
add_test(NAME coreloopthree_memory_tests COMMAND coreloopthree_tests)
add_test(NAME coreloopthree_benchmark COMMAND coreloopthree_tests)