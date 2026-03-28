# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Tool 服务单元测试构建文件

cmake_minimum_required(VERSION 3.20)

# 测试源文�?
set(TOOL_TEST_SOURCES
    test_service.c
    test_cache.c
    test_executor.c
    test_validator.c
    test_registry.c
)

# 创建测试可执行文�?
add_executable(test_tool_d ${TOOL_TEST_SOURCES})

# 链接�?tool_d 和依�?
target_link_libraries(test_tool_d PRIVATE
    agentos-tool-d
    svc_common
    Threads::Threads
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
)

# 包含目录
target_include_directories(test_tool_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../bases/include
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 编译选项
target_compile_options(test_tool_d PRIVATE
    -Wall
    -Wextra
    -g
)

# 注册测试
add_test(NAME tool_d_all COMMAND test_tool_d)

# 可选：只有�?BUILD_COVERAGE 时才收集覆盖�?
if(BUILD_COVERAGE)
    target_compile_options(test_tool_d PRIVATE
        -fprofile-arcs
        -ftest-coverage
    )
    target_link_options(test_tool_d PRIVATE
        --coverage
    )
endif()
