# Copyright (c) 2026 SPHARX. All Rights Reserved.
# LLM 服务构建文件


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 测试支持选项
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_COVERAGE "Build with code coverage" OFF)

# 查找依赖
# Threads found by root CMakeLists.txt
# PkgConfig found by root CMakeLists.txt
# cJSON found by root CMakeLists.txt (AGENTOS_HAS_CJSON)
# CURL found by root CMakeLists.txt (AGENTOS_HAS_CURL)
if(NOT AGENTOS_HAS_CURL)
    message(STATUS "libcurl not found, LLM remote API features may be limited")
endif()
if(NOT AGENTOS_HAS_CJSON)
    message(STATUS "libcjson not found, JSON features may be limited")
endif()

# ==================== 路径设置 ====================
# agentos/daemon/common 兼容层路径
set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

# commons 统一基础库路径
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

# LLM 服务源文件
set(LLM_D_SOURCES
    src/main.c
    src/service.c
    src/llm_svc_adapter.c
    src/providers/registry.c
    src/providers/openai.c
    src/providers/anthropic.c
    src/providers/deepseek.c
    src/providers/google.c
    src/providers/local.c
    src/providers/provider.c
    src/cache.c
    src/cost_tracker.c
    src/token_counter.c
    src/response.c
    src/router/llm_router.c
    src/router/cost_aware_router.c
    src/router/round_robin_router.c
    src/router/least_latency_router.c
    src/router/quality_first_router.c
)

# 创建可执行文件
add_executable(agentos-llm-d ${LLM_D_SOURCES})

# 平台特定配置
# 注意：agentos_common 不在此处直接链接，而是通过 svc_common 的 PUBLIC
# 传递依赖处理，避免在链接命令中重复出现导致 LTO 符号解析失败。
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
        Threads::Threads
        agentos_platform_libs
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
    ${COMMONS_INCLUDE_DIR}/utils/cache/include
    ${COMMONS_INCLUDE_DIR}/utils/compat/include
    # 新增网络和IPC模块
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 链接库
# 链接策略说明：
#   LTO + ASan 组合下，链接器扫描静态库时使用 LTO 符号摘要，可能漏掉符号。
#   根 CMakeLists.txt 已添加 -ffat-lto-objects，让 LTO 对象同时包含机器代码，
#   链接器可用传统符号表解析静态库。
#   agentos_common 通过 svc_common 的 PUBLIC 传递依赖引入，位于链接命令末尾，
#   单次扫描即可解析 svc_common 引用的符号。
if(NOT MSVC AND NOT APPLE)
    target_link_libraries(agentos-llm-d PRIVATE
        "-Wl,--start-group"
        svc_common
        "-Wl,--end-group"
        Threads::Threads
        ${CURL_LIBRARIES}
        ${CJSON_LIBRARIES}
        ${YAML_LIBRARIES}
    )
else()
    target_link_libraries(agentos-llm-d PRIVATE
        svc_common
        Threads::Threads
        ${CURL_LIBRARIES}
        ${CJSON_LIBRARIES}
        ${YAML_LIBRARIES}
    )
endif()

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
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()

# 安装规则
install(TARGETS agentos-llm-d DESTINATION bin)
