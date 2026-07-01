
# AgentRT 安全审计测试 (TeamC P2-C03)
# 测试代码豁免 BAN 严格合规：测试允许使用裸 malloc/free/memset 等函数
add_compile_definitions(AGENTOS_COMPLIANCE_IMPL)

if(NOT TARGET svc_common)
    agentos_print_warn("  [security] Skipping tests: svc_common not available (BUILD_DAEMON=OFF)")
    return()
endif()

add_executable(test_security_audit test_security_audit.c)

target_include_directories(test_security_audit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/syscall/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/string/include
)

target_link_libraries(test_security_audit PRIVATE
    agentos_core
    agentos_syscall
    svc_common
    Threads::Threads
    agentos_platform_libs
    m
)

if(AGENTOS_HAS_CJSON)
    target_link_libraries(test_security_audit PRIVATE cjson)
endif()

target_compile_features(test_security_audit PRIVATE c_std_11)
target_compile_options(test_security_audit PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

add_test(NAME security_audit COMMAND test_security_audit)

agentos_print_ok("  [security] test_security_audit: P2-C03 configured")

# AgentRT SEC-017 桩函数合规验证测试 (TeamC P2-C05)
add_executable(test_sec017_compliance test_sec017_compliance.c)

target_include_directories(test_sec017_compliance PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/syscall/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/syscall/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/string/include
)

target_link_libraries(test_sec017_compliance PRIVATE
    agentos_core
    agentos_syscall
    svc_common
    Threads::Threads
    agentos_platform_libs
    m
)

if(AGENTOS_HAS_CJSON)
    target_link_libraries(test_sec017_compliance PRIVATE cjson)
endif()

target_compile_features(test_sec017_compliance PRIVATE c_std_11)
target_compile_definitions(test_sec017_compliance PRIVATE
    AGENTOS_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)
target_compile_options(test_sec017_compliance PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

add_test(NAME sec017_compliance COMMAND test_sec017_compliance)

agentos_print_ok("  [security] test_sec017_compliance: P2-C05 configured")
