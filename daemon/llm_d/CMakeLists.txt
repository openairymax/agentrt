# Copyright (c) 2026 SPHARX. All Rights Reserved.
# LLM 服务构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-llm-d VERSION 3.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 测试支持选项
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_COVERAGE "Build with code coverage" OFF)

# 查找依赖
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(CJSON REQUIRED libcjson)
pkg_check_modules(YAML REQUIRED yaml-0.1)

# ==================== 路径设置 ====================
# daemon/common 兼容层路径
set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

# commons 统一基础库路径
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

# LLM 服务源文件
set(LLM_D_SOURCES
    src/main.c
    src/service.c
    src/providers/registry.c
    src/providers/openai.c
    src/providers/anthropic.c
    src/providers/deepseek.c
    src/providers/local.c
    src/providers/provider.c
    src/cache.c
    src/cost_tracker.c
    src/token_counter.c
    src/response.c
)

# 创建可执行文件
add_executable(agentos-llm-d ${LLM_D_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(agentos-llm-d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(agentos-llm-d PRIVATE
        ws2_32
        bcrypt
    )
elseif(UNIX)
    target_link_libraries(agentos-llm-d PRIVATE
        pthread
        rt
    )
endif()

# 包含目录 - 兼容层和 commons
target_include_directories(agentos-llm-d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DAEMON_COMMON_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}/platform/include
    ${COMMONS_INCLUDE_DIR}/utils/error/include
    ${COMMONS_INCLUDE_DIR}/utils/logging/include
    ${COMMONS_INCLUDE_DIR}/utils/config_unified/include
    ${COMMONS_INCLUDE_DIR}/utils/memory/include
    ${COMMONS_INCLUDE_DIR}/utils/string/include
    ${COMMONS_INCLUDE_DIR}/utils/sync/include
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(agentos-llm-d PRIVATE
    svc_common
    Threads::Threads
    ${CURL_LIBRARIES}
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
)

# 编译选项
if(BUILD_COVERAGE)
    target_compile_options(agentos-llm-d PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fprofile-arcs
        -ftest-coverage
        -g
        -O0
    )
    target_link_options(agentos-llm-d PRIVATE
        --coverage
    )
else()
    target_compile_options(agentos-llm-d PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=implicit-function-declaration
        -Werror=return-type
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

# 测试支持
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 安装规则
install(TARGETS agentos-llm-d DESTINATION bin)
