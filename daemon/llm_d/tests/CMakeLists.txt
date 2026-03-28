# Copyright (c) 2026 SPHARX. All Rights Reserved.
# LLM 服务单元测试构建文件

cmake_minimum_required(VERSION 3.20)

# 测试源文�?
set(LLM_TEST_SOURCES
    test_service.c
    test_cache.c
    test_token_counter.c
    test_response.c
)

# 创建测试可执行文�?
add_executable(test_llm_d ${LLM_TEST_SOURCES})

# 链接�?llm_d 和依�?
target_link_libraries(test_llm_d PRIVATE
    agentos-llm-d
    svc_common
    Threads::Threads
    ${CURL_LIBRARIES}
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
)

# 包含目录
target_include_directories(test_llm_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../bases/include
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 编译选项
target_compile_options(test_llm_d PRIVATE
    -Wall
    -Wextra
    -g
)

# 注册测试
foreach(test_file ${LLM_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_test(NAME llm_d_${test_name} COMMAND test_llm_d)
endforeach()

# 可选：只有�?BUILD_COVERAGE 时才收集覆盖�?
if(BUILD_COVERAGE)
    target_compile_options(test_llm_d PRIVATE
        -fprofile-arcs
        -ftest-coverage
    )
    target_link_options(test_llm_d PRIVATE
        --coverage
    )
endif()