# Copyright (c) 2026 SPHARX. All Rights Reserved.
# agentos/daemon/commons 单元测试

cmake_minimum_required(VERSION 3.20)

# 测试源文件
set(TEST_SOURCES
    test_error.c
    test_platform.c
    test_logger.c
    test_config.c
    test_svc_auth.c
    test_jsonrpc_helpers.c
    test_safe_string_utils.c
    test_input_validator.c
)

# 创建测试可执行文件
add_executable(test_svc_common ${TEST_SOURCES})

# 链接到 svc_common 和依赖
target_link_libraries(test_svc_common PRIVATE
    svc_common
    Threads::Threads
    ${LIBCURL_LIBRARIES}
    ${YAML_LIBRARIES}
    ${CJSON_LIBRARIES}
)

# 包含目录
target_include_directories(test_svc_common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${LIBCURL_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

# 编译选项
target_compile_options(test_svc_common PRIVATE
    -Wall
    -Wextra
    -g
)

# 注册测试
add_test(NAME svc_common_tests COMMAND test_svc_common)

# 可选：只有当 BUILD_COVERAGE 时才收集覆盖率
if(BUILD_COVERAGE)
    target_compile_options(test_svc_common PRIVATE
        -fprofile-arcs
        -ftest-coverage
    )
    target_link_options(test_svc_common PRIVATE
        --coverage
    )
endif()