
# AgentRT 跨平台兼容性测试 (TeamC P2-C04)
# 测试代码豁免 BAN 严格合规：测试允许使用裸 malloc/free/memset 等函数
add_compile_definitions(AGENTRT_COMPLIANCE_IMPL)

if(NOT TARGET svc_common)
    agentrt_print_warn("  [platform] Skipping tests: svc_common not available (BUILD_DAEMON=OFF)")
    return()
endif()

add_executable(test_platform_compat test_platform_compat.c)

target_include_directories(test_platform_compat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentrt/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentrt/daemons/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentrt/daemons/common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentrt/atoms/syscall/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentrt/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentrt/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentrt/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentrt/commons/utils/string/include
)

target_link_libraries(test_platform_compat PRIVATE
    agentrt_core
    agentrt_syscall
    svc_common
    Threads::Threads
    agentrt_platform_libs
    m
)

if(AGENTRT_HAS_CJSON)
    target_link_libraries(test_platform_compat PRIVATE cjson)
endif()

target_compile_features(test_platform_compat PRIVATE c_std_11)
target_compile_options(test_platform_compat PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

agentrt_print_ok("  [platform] test_platform_compat: P2-C04 configured")

add_executable(test_mkdir_recursive test_mkdir_recursive.c)

target_include_directories(test_mkdir_recursive PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentrt/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../unit/commons/unit
)

target_compile_features(test_mkdir_recursive PRIVATE c_std_11)
target_compile_options(test_mkdir_recursive PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

target_link_libraries(test_mkdir_recursive PRIVATE
    Threads::Threads
    agentrt_platform_libs
)

add_test(
    NAME "platform_test_mkdir_recursive"
    COMMAND test_mkdir_recursive
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

agentrt_print_ok("  [platform] test_mkdir_recursive: R-09-87 configured")
