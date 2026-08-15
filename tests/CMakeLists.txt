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
target_compile_definitions(test_banned_functions PRIVATE AIRY_COMPLIANCE_STRICT)
target_compile_options(test_banned_functions PRIVATE
    -Wall -Wextra -Wpedantic -Wno-type-limits
)

add_test(NAME test_banned_functions COMMAND test_banned_functions)

airy_print_ok("  [compliance] test_banned_functions: BAN-030/155/154/073 compile-time + runtime verification")

# Build smoke test: fast sanity check of the compliance build environment.
add_executable(test_smoke test_smoke.c)

target_include_directories(test_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/compliance/include
)

target_compile_features(test_smoke PRIVATE c_std_11)
target_compile_definitions(test_smoke PRIVATE AIRY_COMPLIANCE_STRICT)
target_compile_options(test_smoke PRIVATE
    -Wall -Wextra -Wpedantic -Wno-type-limits
)

add_test(NAME test_smoke COMMAND test_smoke)

airy_print_ok("  [smoke] test_smoke: compliance build environment sanity check")

# Generic utils unit test: lightweight, self-contained tool semantics checks.
add_executable(test_utils test_utils.c)

target_include_directories(test_utils PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/compliance/include
)

target_compile_features(test_utils PRIVATE c_std_11)
target_compile_definitions(test_utils PRIVATE AIRY_COMPLIANCE_STRICT)
target_compile_options(test_utils PRIVATE
    -Wall -Wextra -Wpedantic -Wno-type-limits
)

add_test(NAME test_utils COMMAND test_utils)

airy_print_ok("  [utils] test_utils: generic utility semantics unit test")

# String utility unit test: lightweight, self-contained safe string semantics checks.
add_executable(test_string_utils test_string_utils.c)

target_include_directories(test_string_utils PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/compliance/include
)

target_compile_features(test_string_utils PRIVATE c_std_11)
target_compile_definitions(test_string_utils PRIVATE AIRY_COMPLIANCE_STRICT)
target_compile_options(test_string_utils PRIVATE
    -Wall -Wextra -Wpedantic -Wno-type-limits
)

add_test(NAME test_string_utils COMMAND test_string_utils)

airy_print_ok("  [str-utils] test_string_utils: safe string utility semantics unit test")
