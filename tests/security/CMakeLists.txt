cmake_minimum_required(VERSION 3.20)

# AgentOS 安全审计测试 (TeamC P2-C03)

add_executable(test_security_audit test_security_audit.c)

target_include_directories(test_security_audit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/syscall/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/string/include
)

target_link_libraries(test_security_audit PRIVATE
    agentos_core
    agentos_syscall
    svc_common
    Threads::Threads
    rt
    m
)

find_package(cjson QUIET)
if(TARGET cjson OR cjson_FOUND)
    target_link_libraries(test_security_audit PRIVATE cjson)
endif()

if(UNIX AND NOT APPLE)
    target_link_options(test_security_audit PRIVATE -Wl,--allow-multiple-definition)
endif()

target_compile_features(test_security_audit PRIVATE c_std_11)
target_compile_options(test_security_audit PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

message(STATUS "  [security] test_security_audit: P2-C03 configured")

# AgentOS SEC-017 桩函数合规验证测试 (TeamC P2-C05)
add_executable(test_sec017_compliance test_sec017_compliance.c)

target_include_directories(test_sec017_compliance PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/syscall/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/syscall/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/string/include
)

target_link_libraries(test_sec017_compliance PRIVATE
    agentos_core
    agentos_syscall
    svc_common
    Threads::Threads
    rt
    m
)

find_package(cjson QUIET)
if(TARGET cjson OR cjson_FOUND)
    target_link_libraries(test_sec017_compliance PRIVATE cjson)
endif()

if(UNIX AND NOT APPLE)
    target_link_options(test_sec017_compliance PRIVATE -Wl,--allow-multiple-definition)
endif()

target_compile_features(test_sec017_compliance PRIVATE c_std_11)
target_compile_options(test_sec017_compliance PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

message(STATUS "  [security] test_sec017_compliance: P2-C05 configured")
