# Copyright (c) 2026 SPHARX. All Rights Reserved.
# CoreLoopThree 测试套件构建文件


set(CORELOOPTHREE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)
set(MEMORYROVOL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../memory)

set(CL3_TEST_SOURCES
    unit/test_loop.c
    unit/test_cognition.c
    unit/test_cognition_e2e.c
    unit/test_execution.c
    unit/test_memory.c
    unit/test_compensation.c
    unit/test_intent_parser.c
    unit/test_cognitive_evolution.c
    unit/test_context_processor.c
    unit/test_parallel_dispatcher.c
    unit/test_multi_agent.c
    unit/test_browser.c
    unit/test_triple_coordinator.c
)

set(CL3_BENCH_SOURCES
    unit/test_performance_benchmark.c
)

set(CL3_COMMON_INCLUDES
    ${CORELOOPTHREE_DIR}/include
    ${CORELOOPTHREE_DIR}/src
    ${CORELOOPTHREE_DIR}/src/cognition
    ${CORELOOPTHREE_DIR}/src/cognition/critique
    ${CORELOOPTHREE_DIR}/src/cognition/foundation
    ${CORELOOPTHREE_DIR}/../memory
    ${CORELOOPTHREE_DIR}/../memory/src
    ${CORELOOPTHREE_DIR}/../../daemon/common/include
    ${CORELOOPTHREE_DIR}/../../daemon/llm_d/include
    ${CORELOOPTHREE_DIR}/../../daemon/tool_d/include
    ${CORELOOPTHREE_DIR}/../../commons/utils/quality
    ${CORELOOPTHREE_DIR}/../../cupolas/src
    ${COREKERN_INCLUDE_DIR}
    ${COMMONS_DIR}/platform/include
    ${COMMONS_DIR}/utils/include
    ${COMMONS_DIR}/utils/error/include
    ${COMMONS_DIR}/utils/logging/include
    ${COMMONS_DIR}/utils/memory/include
    ${COMMONS_DIR}/utils/string/include
    ${COMMONS_DIR}/utils/sync/include
    ${COMMONS_DIR}/utils/cache/include
    ${COMMONS_DIR}/utils/compat/include
    ${MEMORYROVOL_SRC_DIR}
    ${MEMORYROVOL_INCLUDE_DIR}
)

foreach(TEST_SRC ${CL3_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    string(REPLACE "test_" "" TEST_SHORT_NAME ${TEST_NAME})

    add_executable(test_cl3_${TEST_SHORT_NAME} ${TEST_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_cl3_stubs.c
    )

    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/unit/test_cl3_stubs.c
        PROPERTIES COMPILE_FLAGS "-fno-lto"
    )

    target_include_directories(test_cl3_${TEST_SHORT_NAME} PRIVATE ${CL3_COMMON_INCLUDES})

    target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE
        -Wl,--whole-archive
        agentos_coreloopthree
        -Wl,--no-whole-archive
        agentos_cognition
        agentos_execution
        agentos_core
        agentos_memory
        agentos_atoms
        agentos_syscall
        svc_common
        pthread
        dl
        m
    )

    if(AGENTRT_HAS_CJSON AND CJSON_FOUND)
        target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE ${CJSON_LIBRARIES})
    elseif(AGENTOS_CJSON_FOUND AND AGENTOS_CJSON_LIBRARIES)
        target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE ${AGENTOS_CJSON_LIBRARIES})
    endif()

    if(AGENTRT_HAS_CURL)
        if(CURL_FOUND AND CURL_LIBRARIES)
            target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE ${CURL_LIBRARIES})
        elseif(LIBCURL_FOUND AND LIBCURL_LIBRARIES)
            target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE ${LIBCURL_LIBRARIES})
        elseif(PC_CURL_LIBRARIES)
            target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE ${PC_CURL_LIBRARIES})
        else()
            target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE curl)
        endif()
    endif()

    if(UNIX)
        target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE agentos_platform_libs)
    endif()

    # C-L12: Link MemoryRovol if available
    if(TARGET agentos_memoryrovol)
        target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE agentos_memoryrovol)
    endif()

    target_compile_options(test_cl3_${TEST_SHORT_NAME} PRIVATE
        -Wall -Wextra -g -Wno-unused-function -Wno-deprecated-declarations
        "SHELL:-include memory_compat.h"
    )

    target_link_libraries(test_cl3_${TEST_SHORT_NAME} PRIVATE agentos_test_ldflags)

    add_test(NAME cl3_${TEST_SHORT_NAME} COMMAND test_cl3_${TEST_SHORT_NAME})
endforeach()

foreach(BENCH_SRC ${CL3_BENCH_SOURCES})
    get_filename_component(BENCH_NAME ${BENCH_SRC} NAME_WE)
    string(REPLACE "test_" "" BENCH_SHORT_NAME ${BENCH_NAME})

    add_executable(test_cl3_${BENCH_SHORT_NAME} ${BENCH_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_cl3_stubs.c
    )

    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/unit/test_cl3_stubs.c
        PROPERTIES COMPILE_FLAGS "-fno-lto"
    )

    target_include_directories(test_cl3_${BENCH_SHORT_NAME} PRIVATE ${CL3_COMMON_INCLUDES})

    target_link_libraries(test_cl3_${BENCH_SHORT_NAME} PRIVATE
        -Wl,--whole-archive
        agentos_coreloopthree
        -Wl,--no-whole-archive
        agentos_cognition
        agentos_execution
        agentos_core
        agentos_memory
        agentos_atoms
        agentos_syscall
        svc_common
        pthread
        dl
        m
    )

    if(AGENTRT_HAS_CJSON AND CJSON_FOUND)
        target_link_libraries(test_cl3_${BENCH_SHORT_NAME} PRIVATE ${CJSON_LIBRARIES})
    elseif(AGENTOS_CJSON_FOUND AND AGENTOS_CJSON_LIBRARIES)
        target_link_libraries(test_cl3_${BENCH_SHORT_NAME} PRIVATE ${AGENTOS_CJSON_LIBRARIES})
    endif()

    if(AGENTRT_HAS_CURL)
        if(CURL_FOUND AND CURL_LIBRARIES)
            target_link_libraries(test_cl3_${BENCH_SHORT_NAME} PRIVATE ${CURL_LIBRARIES})
        elseif(LIBCURL_FOUND AND LIBCURL_LIBRARIES)
            target_link_libraries(test_cl3_${BENCH_SHORT_NAME} PRIVATE ${LIBCURL_LIBRARIES})
        elseif(PC_CURL_LIBRARIES)
            target_link_libraries(test_cl3_${BENCH_SHORT_NAME} PRIVATE ${PC_CURL_LIBRARIES})
        else()
            target_link_libraries(test_cl3_${BENCH_SHORT_NAME} PRIVATE curl)
        endif()
    endif()

    if(UNIX)
        target_link_libraries(test_cl3_${BENCH_SHORT_NAME} PRIVATE agentos_platform_libs)
    endif()

    # C-L12: Link MemoryRovol if available
    if(TARGET agentos_memoryrovol)
        target_link_libraries(test_cl3_${BENCH_SHORT_NAME} PRIVATE agentos_memoryrovol)
    endif()

    target_compile_options(test_cl3_${BENCH_SHORT_NAME} PRIVATE
        -Wall -Wextra -g -Wno-unused-function -Wno-deprecated-declarations
        "SHELL:-include memory_compat.h"
    )

    target_link_libraries(test_cl3_${BENCH_SHORT_NAME} PRIVATE agentos_test_ldflags)

    add_test(NAME cl3_${BENCH_SHORT_NAME} COMMAND test_cl3_${BENCH_SHORT_NAME})
endforeach()

enable_testing()
