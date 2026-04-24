cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -g -O0)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)

# 查找SQLite3依赖
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 sqlite3)
endif()
if(NOT SQLITE3_FOUND)
    find_library(SQLITE3_LIB sqlite3)
    if(SQLITE3_LIB)
        set(SQLITE3_LIBRARIES ${SQLITE3_LIB})
        set(SQLITE3_FOUND TRUE)
    endif()
endif()

# 单独编译每个测试文件为独立可执行文件
set(TEST_FILES
    test_heapstore_core
    test_heapstore_registry
    test_heapstore_trace
    test_heapstore_log
    test_heapstore_ipc
    test_heapstore_memory
    test_heapstore_integration
)

foreach(test ${TEST_FILES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test}.c")
        add_executable(${test} ${test}.c)
        target_link_libraries(${test}
            PRIVATE agentos_heapstore
            Threads::Threads
            ${SQLITE3_LIBRARIES}
        )
        add_test(NAME ${test} COMMAND ${test})
    endif()
endforeach()

option(BUILD_BENCHMARK "Build performance benchmark" ON)

if(BUILD_BENCHMARK)
    add_executable(heapstore_benchmark benchmark_heapstore.c)
    target_link_libraries(heapstore_benchmark
        PRIVATE agentos_heapstore
        Threads::Threads
        ${SQLITE3_LIBRARIES}
    )
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test_output)
