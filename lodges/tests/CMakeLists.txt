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
    test_lodges_core.c
    test_lodges_registry.c
    test_lodges_trace.c
    test_lodges_log.c
    test_lodges_ipc.c
    test_lodges_memory.c
    test_lodges_integration.c
)

add_executable(lodges_tests ${TEST_SOURCES})

target_link_libraries(lodges_tests
    agentos_lodges
    Threads::Threads
)

add_test(NAME lodgesTests COMMAND lodges_tests)

option(BUILD_BENCHMARK "Build performance benchmark" ON)

if(BUILD_BENCHMARK)
    add_executable(lodges_benchmark benchmark_lodges.c)
    target_link_libraries(lodges_benchmark
        agentos_lodges
        Threads::Threads
    )
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test_output)

