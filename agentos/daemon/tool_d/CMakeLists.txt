# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Tool 服务构建文件


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找依赖
# Threads found by root CMakeLists.txt
# PkgConfig found by root CMakeLists.txt
# cJSON found by root CMakeLists.txt (AGENTOS_HAS_CJSON)
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

# ==================== Tool 服务静态库 ====================
# 提取 service/registry/executor/validator/cache/tool_approval/safety_guard_bridge/
# tool_svc_adapter 为独立静态库，供 agentos-tool-d 可执行文件、atoms/coreloopthree
# 测试和集成测试共用，消除桩函数依赖。
# 排除 main.c（含 main()）、config.c、utils/tool_helpers.c（daemon 专用工具函数）。
set(TOOL_SERVICE_SOURCES
    src/service.c
    src/registry.c
    src/executor.c
    src/validator.c
    src/cache.c
    src/tool_approval.c
    src/safety_guard_bridge.c
    src/tool_svc_adapter.c
)

add_library(agentos_tool_service STATIC ${TOOL_SERVICE_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(agentos_tool_service PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
elseif(UNIX)
    target_compile_definitions(agentos_tool_service PRIVATE
        AGENTOS_PLATFORM_LINUX=1
        _GNU_SOURCE
    )
endif()

# 包含目录 - 兼容层和 commons
target_include_directories(agentos_tool_service PUBLIC
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
    # C-L05: Cupolas SafetyGuard 头文件路径
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cupolas/include
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
)

target_include_directories(agentos_tool_service PRIVATE
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 链接库：svc_common 提供 daemon 公共层，cupolas 提供 SafetyGuard
target_link_libraries(agentos_tool_service PUBLIC svc_common cupolas)

if(AGENTOS_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(agentos_tool_service PUBLIC cjson)
    elseif(CJSON_FOUND AND CJSON_LIBRARIES)
        target_link_libraries(agentos_tool_service PUBLIC ${CJSON_LIBRARIES})
    endif()
endif()
if(AGENTOS_HAS_YAML AND YAML_FOUND AND YAML_LIBRARIES)
    target_link_libraries(agentos_tool_service PUBLIC ${YAML_LIBRARIES})
endif()

if(UNIX)
    target_link_libraries(agentos_tool_service PUBLIC Threads::Threads agentos_platform_libs)
endif()

# 编译选项
target_compile_options(agentos_tool_service PRIVATE
    -Wall -Wextra -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# C-L05: 启用 Cupolas SafetyGuard V2 API
target_compile_definitions(agentos_tool_service PRIVATE AGENTOS_ENABLE_V2_API=1)

# ==================== Tool daemon 可执行文件 ====================
# main.c 仅属于 daemon 可执行文件；config.c 和 utils/tool_helpers.c 为 daemon 专用工具函数；
# tool_svc_adapter.c 已纳入 agentos_tool_service 库
add_executable(agentos-tool-d
    src/main.c
    src/config.c
    src/utils/tool_helpers.c
)

if(WIN32)
    target_compile_definitions(agentos-tool-d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(agentos-tool-d PRIVATE agentos_common ws2_32 bcrypt)
elseif(UNIX)
    target_link_libraries(agentos-tool-d PRIVATE agentos_common Threads::Threads agentos_platform_libs)
endif()

target_include_directories(agentos-tool-d PRIVATE
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cupolas/include
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

target_link_libraries(agentos-tool-d PRIVATE
    agentos_tool_service
    Threads::Threads
)

target_compile_options(agentos-tool-d PRIVATE
    -Wall -Wextra -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

target_compile_definitions(agentos-tool-d PRIVATE AGENTOS_ENABLE_V2_API=1)

# 测试支持
if(BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()

# 安装规则
install(TARGETS agentos-tool-d DESTINATION bin)
