cmake_minimum_required(VERSION 3.16)

project(coreloopthree_tests)

# 包含核心模块的头文件
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

# 链接核心模块的库
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../build/lib)

# 测试可执行文件
add_executable(test_cognition test_cognition.c)
target_link_libraries(test_cognition agentos_coreloopthree)

add_executable(test_execution test_execution.c)
target_link_libraries(test_execution agentos_coreloopthree)

add_executable(test_memory test_memory.c)
target_link_libraries(test_memory agentos_coreloopthree)

add_executable(test_loop test_loop.c)
target_link_libraries(test_loop agentos_coreloopthree)

# 基准测试
add_executable(benchmark benchmark.c)
target_link_libraries(benchmark agentos_coreloopthree)

# 添加测试
enable_testing()
add_test(NAME cognition_test COMMAND test_cognition)
add_test(NAME execution_test COMMAND test_execution)
add_test(NAME memory_test COMMAND test_memory)
add_test(NAME loop_test COMMAND test_loop)
