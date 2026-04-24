cmake_minimum_required(VERSION 3.20)

# AgentOS 端到端集成测试 (TeamC P2-C01)
# 覆盖: corekern+daemon+syscall 跨模块联动
# 场景: 初始化/配置/IPC/syscall/熔断/告警/多线程/追踪/定时器/压力/幂等

add_executable(test_e2e_core test_e2e_core.c)

target_include_directories(test_e2e_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/syscall/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/string/include
)

target_link_libraries(test_e2e_core PRIVATE
    agentos_core
    agentos_syscall
    svc_common
    Threads::Threads
    rt
    m
)

find_package(cjson QUIET)
if(TARGET cjson OR cjson_FOUND)
    target_link_libraries(test_e2e_core PRIVATE cjson)
endif()

if(UNIX AND NOT APPLE)
    target_link_options(test_e2e_core PRIVATE -Wl,--allow-multiple-definition)
endif()

target_compile_features(test_e2e_core PRIVATE c_std_11)
target_compile_options(test_e2e_core PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

add_test(NAME e2e_core COMMAND test_e2e_core
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "  [integration] test_e2e_core: P2-C01 E2E integration tests configured")
