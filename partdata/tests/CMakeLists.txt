cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -g -O0)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)

set(TEST_SOURCES
    test_partdata_core.c
    test_partdata_registry.c
    test_partdata_trace.c
    test_partdata_log.c
    test_partdata_ipc.c
    test_partdata_memory.c
    test_partdata_integration.c
)

add_executable(partdata_tests ${TEST_SOURCES})

target_link_libraries(partdata_tests
    agentos_partdata
    Threads::Threads
)

add_test(NAME PartdataTests COMMAND partdata_tests)

option(BUILD_BENCHMARK "Build performance benchmark" ON)

if(BUILD_BENCHMARK)
    add_executable(partdata_benchmark benchmark_partdata.c)
    target_link_libraries(partdata_benchmark
        agentos_partdata
        Threads::Threads
    )
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test_output)
