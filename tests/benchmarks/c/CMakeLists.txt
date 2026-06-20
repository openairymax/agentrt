
# AgentRT 性能基准测试 (TeamC P2-C02)

if(NOT TARGET svc_common)
    message(STATUS "  [benchmarks] Skipping tests: svc_common not available (BUILD_DAEMON=OFF)")
    return()
endif()

add_executable(test_performance_benchmarks test_performance_benchmarks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/coreloopthree/tests/unit/test_cl3_stubs.c
)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/coreloopthree/tests/unit/test_cl3_stubs.c
    PROPERTIES COMPILE_FLAGS "-fno-lto"
)

target_include_directories(test_performance_benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/coreloopthree/tests/unit
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/coreloopthree/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/syscall/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/string/include
)

target_link_libraries(test_performance_benchmarks PRIVATE
    agentos_core
    agentos_syscall
    -Wl,--whole-archive
    agentos_coreloopthree
    -Wl,--no-whole-archive
    svc_common
    Threads::Threads
    agentos_platform_libs
    m
)

if(AGENTOS_HAS_CJSON)
    target_link_libraries(test_performance_benchmarks PRIVATE cjson)
endif()

target_compile_features(test_performance_benchmarks PRIVATE c_std_11)
target_compile_options(test_performance_benchmarks PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

add_test(NAME performance_benchmarks COMMAND test_performance_benchmarks)

message(STATUS "  [benchmarks] test_performance_benchmarks: P2-C02 configured")
