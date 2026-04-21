# Copyright (c) 2026 SPHARX. All Rights Reserved.
# LLM 服务单元测试构建文件 (TeamC)

cmake_minimum_required(VERSION 3.20)

set(LLM_TEST_SOURCES
    test_service.c
    test_cache.c
    test_token_counter.c
    test_response.c
)

add_executable(test_llm_d ${LLM_TEST_SOURCES})

target_include_directories(test_llm_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../agentos/commons/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/include
)

target_link_libraries(test_llm_d PRIVATE
    svc_common
    Threads::Threads
)

find_package(cjson QUIET)
if(TARGET cjson OR cjson_FOUND)
    target_link_libraries(test_llm_d PRIVATE cjson)
endif()

find_package(CURL QUIET)
if(CURL_FOUND)
    target_include_directories(test_llm_d PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(test_llm_d PRIVATE ${CURL_LIBRARIES})
endif()

target_compile_options(test_llm_d PRIVATE -Wall -Wextra -g -Wno-unused-function)

enable_testing()
add_test(NAME llm_d_tests COMMAND test_llm_d
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
