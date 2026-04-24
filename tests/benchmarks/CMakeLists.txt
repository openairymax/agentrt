cmake_minimum_required(VERSION 3.20)

# AgentOS 性能基准测试 (TeamC P2-C02)

add_executable(test_performance_benchmarks test_performance_benchmarks.c)

target_include_directories(test_performance_benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/daemon/common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/atoms/syscall/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/utils/string/include
)

target_link_libraries(test_performance_benchmarks PRIVATE
    agentos_core
    agentos_syscall
    svc_common
    Threads::Threads
    rt
    m
)

find_package(cjson QUIET)
if(TARGET cjson OR cjson_FOUND)
    target_link_libraries(test_performance_benchmarks PRIVATE cjson)
endif()

if(UNIX AND NOT APPLE)
    target_link_options(test_performance_benchmarks PRIVATE -Wl,--allow-multiple-definition)
endif()

target_compile_features(test_performance_benchmarks PRIVATE c_std_11)
target_compile_options(test_performance_benchmarks PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

message(STATUS "  [benchmarks] test_performance_benchmarks: P2-C02 configured")
