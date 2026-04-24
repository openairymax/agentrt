cmake_minimum_required(VERSION 3.20)

# AgentOS 跨平台兼容性测试 (TeamC P2-C04)

add_executable(test_platform_compat test_platform_compat.c)

target_include_directories(test_platform_compat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/syscall/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/string/include
)

target_link_libraries(test_platform_compat PRIVATE
    agentos_core
    agentos_syscall
    svc_common
    Threads::Threads
    rt
    m
)

find_package(cjson QUIET)
if(TARGET cjson OR cjson_FOUND)
    target_link_libraries(test_platform_compat PRIVATE cjson)
endif()

if(UNIX AND NOT APPLE)
    target_link_options(test_platform_compat PRIVATE -Wl,--allow-multiple-definition)
endif()

target_compile_features(test_platform_compat PRIVATE c_std_11)
target_compile_options(test_platform_compat PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

message(STATUS "  [platform] test_platform_compat: P2-C04 configured")
