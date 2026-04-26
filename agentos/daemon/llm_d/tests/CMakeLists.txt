# Copyright (c) 2026 SPHARX. All Rights Reserved.
# LLM 服务单元测试构建文件 (TeamC)

cmake_minimum_required(VERSION 3.20)

set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)

set(LLM_IMPL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/token_counter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/response.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/cost_tracker.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/providers/registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/providers/provider.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/providers/openai.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/providers/anthropic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/providers/deepseek.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/providers/google.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/providers/local.c
)

set(LLM_TEST_SOURCES
    test_service.c
    test_cache.c
    test_token_counter.c
    test_response.c
)

set(LLM_COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/providers
    ${COMMONS_DIR}
    ${COMMONS_DIR}/platform/include
    ${COMMONS_DIR}/utils/error/include
    ${COMMONS_DIR}/utils/logging/include
    ${COMMONS_DIR}/utils/config_unified/include
    ${COMMONS_DIR}/utils/memory/include
    ${COMMONS_DIR}/utils/string/include
    ${COMMONS_DIR}/utils/sync/include
    ${COMMONS_DIR}/utils/cache/include
    ${COMMONS_DIR}/utils/compat/include
    ${SYSCALL_INCLUDE_DIR}
    ${COREKERN_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
)

foreach(TEST_SRC ${LLM_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

    add_executable(test_llm_${TEST_NAME} ${TEST_SRC} ${LLM_IMPL_SOURCES})

    target_include_directories(test_llm_${TEST_NAME} PRIVATE ${LLM_COMMON_INCLUDES})

    target_link_libraries(test_llm_${TEST_NAME} PRIVATE
        svc_common
        Threads::Threads
    )

    find_package(cjson QUIET)
    if(TARGET cjson OR cjson_FOUND)
        target_link_libraries(test_llm_${TEST_NAME} PRIVATE cjson)
    endif()

    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_include_directories(test_llm_${TEST_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
        target_link_libraries(test_llm_${TEST_NAME} PRIVATE ${CURL_LIBRARIES})
    endif()

    target_compile_options(test_llm_${TEST_NAME} PRIVATE -Wall -Wextra -g -Wno-unused-function)

    add_test(NAME llm_d_${TEST_NAME} COMMAND test_llm_${TEST_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

enable_testing()
