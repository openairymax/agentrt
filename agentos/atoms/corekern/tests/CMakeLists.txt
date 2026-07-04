
# 测试代码豁免 BAN 严格合规：测试允许使用裸 malloc/free/memset 等函数
add_compile_definitions(AGENTOS_COMPLIANCE_IMPL)

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
agentos_print_info("corekern test: test_kernel_state (links agentos_core.a)")

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
agentos_print_info("corekern test: test_corekern (deep unit tests)")

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
agentos_print_info("corekern test: test_memory_engine (memory/engine/scheduler/strategy)")

add_executable(test_oom_handler test_oom_handler.c)

target_include_directories(test_oom_handler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../commons/utils/types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../commons/platform/include
)

target_compile_features(test_oom_handler PRIVATE c_std_11)
target_compile_options(test_oom_handler PRIVATE
    -Wall -Wextra -Wpedantic -Wno-address -Wno-unused-function
)

target_link_libraries(test_oom_handler
    PRIVATE agentos_test_ldflags
    PRIVATE agentos_core
    PRIVATE Threads::Threads agentos_platform_libs
)

add_test(NAME oom_handler COMMAND test_oom_handler)
agentos_print_info("corekern test: test_oom_handler (OOM trigger→degrade→restore)")

# P0.22.1: ARE L2 统一 IPC 消息头单元测试
# 验证 128 字节定长头 + CRC32 校验 + 完整消息生命周期 + trace_id echo 语义
add_executable(test_are_ipc test_are_ipc.c)

target_include_directories(test_are_ipc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_features(test_are_ipc PRIVATE c_std_11)
target_compile_options(test_are_ipc PRIVATE
    -Wall -Wextra -Wpedantic -Werror=vla -Wno-address -Wno-unused-function
)

target_link_libraries(test_are_ipc
    PRIVATE agentos_test_ldflags
    PRIVATE agentos_core
    PRIVATE Threads::Threads agentos_platform_libs
)

add_test(NAME are_ipc COMMAND test_are_ipc)
agentos_print_info("corekern test: test_are_ipc (ARE L2 unified IPC header)")
