cmake_minimum_required(VERSION 3.20)

# corekern模块单元测试
# 使用agentos_core.a + agentos_common.a
# 由于两个库有符号冲突，使用链接选项处理

add_executable(test_kernel_state test_kernel_state.c)

target_include_directories(test_kernel_state PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_features(test_kernel_state PRIVATE c_std_11)
target_compile_options(test_kernel_state PRIVATE
    -Wall -Wextra -Wpedantic -Werror=vla -Wno-address
)

# 链接agentos_core（它依赖agentos_common）
# 由于项目架构原因两个库有部分符号重复，测试使用--allow-multiple-definition处理
target_link_libraries(test_kernel_state
    PRIVATE -Wl,--allow-multiple-definition
    PRIVATE agentos_core
    PRIVATE pthread rt m
)

add_test(NAME kernel_state COMMAND test_kernel_state)
message(STATUS "corekern test: test_kernel_state (links agentos_core.a)")
