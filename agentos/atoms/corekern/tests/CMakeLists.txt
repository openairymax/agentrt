
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

target_link_libraries(test_kernel_state
    PRIVATE agentos_test_ldflags
    PRIVATE agentos_core
    PRIVATE Threads::Threads agentos_platform_libs
)

add_test(NAME kernel_state COMMAND test_kernel_state)
message(STATUS "corekern test: test_kernel_state (links agentos_core.a)")

add_executable(test_corekern test_corekern.c)

target_include_directories(test_corekern PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_features(test_corekern PRIVATE c_std_11)
target_compile_options(test_corekern PRIVATE
    -Wall -Wextra -Wpedantic -Wno-address -Wno-unused-function
)

target_link_libraries(test_corekern
    PRIVATE agentos_test_ldflags
    PRIVATE agentos_core
    PRIVATE Threads::Threads agentos_platform_libs
)

add_test(NAME corekern_deep COMMAND test_corekern)
message(STATUS "corekern test: test_corekern (deep unit tests)")

add_executable(test_memory_engine test_memory_engine.c)

target_include_directories(test_memory_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons/utils/types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons/platform/include
)

target_compile_features(test_memory_engine PRIVATE c_std_11)
target_compile_options(test_memory_engine PRIVATE
    -Wall -Wextra -Wpedantic -Wno-address -Wno-unused-function
)

target_link_libraries(test_memory_engine
    PRIVATE agentos_test_ldflags
    PRIVATE agentos_core
    PRIVATE Threads::Threads agentos_platform_libs
)

add_test(NAME memory_engine COMMAND test_memory_engine)
message(STATUS "corekern test: test_memory_engine (memory/engine/scheduler/strategy)")
