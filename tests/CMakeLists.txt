# AgentRT 合规性测试
# Task #39: BAN-030/BAN-155/BAN-154/BAN-073 编译期验证测试
# @owner: team-C
#
# 此测试在 AIRY_COMPLIANCE_STRICT 模式下编译，不定义 AIRY_COMPLIANCE_IMPL，
# 以验证毒化机制生效。测试仅使用安全宏（AIRY_MEMSET/MEMCPY/STRNCPY_TERM），
# 这些宏基于 __builtin_* 实现，不被毒化。
#
# 此测试为自包含测试，不链接外部库（避免 ASan 符号依赖）。

add_executable(test_banned_functions test_banned_functions.c)

target_include_directories(test_banned_functions PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/compliance/include
)

target_compile_features(test_banned_functions PRIVATE c_std_11)
target_compile_options(test_banned_functions PRIVATE
    -Wall -Wextra -Wpedantic -Wno-type-limits
)

add_test(NAME test_banned_functions COMMAND test_banned_functions)

airy_print_ok("  [compliance] test_banned_functions: BAN-030/155/154/073 compile-time + runtime verification")
