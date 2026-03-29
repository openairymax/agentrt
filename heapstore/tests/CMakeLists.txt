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
    test_heapstore_core.c
    test_heapstore_registry.c
    test_heapstore_trace.c
    test_heapstore_log.c
    test_heapstore_ipc.c
    test_heapstore_memory.c
    test_heapstore_integration.c
)

add_executable(heapstore_tests ${TEST_SOURCES})

target_link_libraries(heapstore_tests
    agentos_heapstore
    Threads::Threads
)

add_test(NAME heapstoreTests COMMAND heapstore_tests)

option(BUILD_BENCHMARK "Build performance benchmark" ON)

if(BUILD_BENCHMARK)
    add_executable(heapstore_benchmark benchmark_heapstore.c)
    target_link_libraries(heapstore_benchmark
        agentos_heapstore
        Threads::Threads
    )
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test_output)

