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

# ==================== LLM 服务静态库 ====================
# 提取 service/providers/cache/cost_tracker/token_counter/response 为独立静态库，
# 供 agentos-llm-d 可执行文件和 atoms/coreloopthree 测试共用，消除桩函数依赖。
# 排除 main.c（含 main()）和 llm_svc_adapter.c（daemon 侧适配器，非服务核心）。
set(LLM_SERVICE_SOURCES
    src/service.c
    src/cache.c
    src/cost_tracker.c
    src/token_counter.c
    src/response.c
    src/providers/registry.c
    src/providers/provider.c
    src/providers/openai.c
    src/providers/anthropic.c
    src/providers/deepseek.c
    src/providers/google.c
    src/providers/local.c
    src/router/llm_router.c
    src/router/cost_aware_router.c
    src/router/round_robin_router.c
    src/router/least_latency_router.c
    src/router/quality_first_router.c
)

add_library(agentos_llm_service STATIC ${LLM_SERVICE_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(agentos_llm_service PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
elseif(UNIX)
    target_compile_definitions(agentos_llm_service PRIVATE
        AGENTOS_PLATFORM_LINUX=1
        _GNU_SOURCE
    )
endif()

# 包含目录 - 兼容层和 commons
target_include_directories(agentos_llm_service PUBLIC
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
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
)

target_include_directories(agentos_llm_service PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 链接库：svc_common 提供 daemon 公共层 + commons 传递依赖
target_link_libraries(agentos_llm_service PUBLIC svc_common)

if(AGENTRT_HAS_CURL)
    if(CURL_FOUND AND CURL_LIBRARIES)
        target_link_libraries(agentos_llm_service PUBLIC ${CURL_LIBRARIES})
    elseif(LIBCURL_FOUND AND LIBCURL_LIBRARIES)
        target_link_libraries(agentos_llm_service PUBLIC ${LIBCURL_LIBRARIES})
    endif()
endif()
if(AGENTOS_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(agentos_llm_service PUBLIC cjson)
    elseif(CJSON_FOUND AND CJSON_LIBRARIES)
        target_link_libraries(agentos_llm_service PUBLIC ${CJSON_LIBRARIES})
    endif()
endif()
if(AGENTOS_HAS_YAML AND YAML_FOUND AND YAML_LIBRARIES)
    target_link_libraries(agentos_llm_service PUBLIC ${YAML_LIBRARIES})
endif()

if(UNIX)
    target_link_libraries(agentos_llm_service PUBLIC Threads::Threads agentos_platform_libs)
endif()

# 编译选项
if(BUILD_COVERAGE)
    target_compile_options(agentos_llm_service PRIVATE
        -Wall -Wextra -Wpedantic -fprofile-arcs -ftest-coverage -g -O0
    )
else()
    target_compile_options(agentos_llm_service PRIVATE
        -Wall -Wextra -Wpedantic
        -Werror=implicit-function-declaration
        -Werror=return-type
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

# ==================== LLM daemon 可执行文件 ====================
# main.c + llm_svc_adapter.c 仅属于 daemon 可执行文件，不入库
add_executable(agentos-llm-d
    src/main.c
    src/llm_svc_adapter.c
)

if(WIN32)
    target_compile_definitions(agentos-llm-d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(agentos-llm-d PRIVATE ws2_32 bcrypt)
elseif(UNIX)
    target_link_libraries(agentos-llm-d PRIVATE Threads::Threads agentos_platform_libs)
endif()

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
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 链接策略：LTO + ASan 下使用 --start-group/--end-group 避免符号扫描遗漏
if(NOT MSVC AND NOT APPLE)
    target_link_libraries(agentos-llm-d PRIVATE
        "-Wl,--start-group"
        agentos_llm_service
        "-Wl,--end-group"
        Threads::Threads
    )
else()
    target_link_libraries(agentos-llm-d PRIVATE agentos_llm_service Threads::Threads)
endif()

if(BUILD_COVERAGE)
    target_compile_options(agentos-llm-d PRIVATE
        -Wall -Wextra -Wpedantic -fprofile-arcs -ftest-coverage -g -O0
    )
    target_link_options(agentos-llm-d PRIVATE --coverage)
else()
    target_compile_options(agentos-llm-d PRIVATE
        -Wall -Wextra -Wpedantic
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
