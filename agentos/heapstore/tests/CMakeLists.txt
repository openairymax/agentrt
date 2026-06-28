
# 测试代码豁免 BAN 严格合规：测试允许使用裸 malloc/free/memset 等函数
add_compile_definitions(AGENTOS_COMPLIANCE_IMPL)
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
        target_include_directories(${test} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../..
            ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include
        )
        target_link_libraries(${test}
            PRIVATE agentos_heapstore
            Threads::Threads
        )
        if(AGENTOS_HAS_SQLITE3)
            if(SQLite3_FOUND)
                target_link_libraries(${test} PRIVATE ${SQLite3_LIBRARIES})
            elseif(SQLITE3_FOUND)
                target_link_libraries(${test} PRIVATE ${SQLITE3_LIBRARIES})
            endif()
        endif()
        add_test(NAME ${test} COMMAND ${test})
    endif()
endforeach()

option(BUILD_BENCHMARK "Build performance benchmark" ON)

if(BUILD_BENCHMARK)
    add_executable(heapstore_benchmark benchmark_heapstore.c)
    target_include_directories(heapstore_benchmark PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include
    )
    target_link_libraries(heapstore_benchmark
        PRIVATE agentos_heapstore
        Threads::Threads
    )
    if(AGENTOS_HAS_SQLITE3)
        if(SQLite3_FOUND)
            target_link_libraries(heapstore_benchmark PRIVATE ${SQLite3_LIBRARIES})
        elseif(SQLITE3_FOUND)
            target_link_libraries(heapstore_benchmark PRIVATE ${SQLITE3_LIBRARIES})
        endif()
    endif()
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test_output)
