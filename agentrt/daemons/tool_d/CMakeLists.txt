# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Tool 服务构建文件


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找依赖
# Threads found by root CMakeLists.txt
# PkgConfig found by root CMakeLists.txt
# cJSON found by root CMakeLists.txt (AGENTRT_HAS_CJSON)
if(NOT AGENTRT_HAS_CJSON)
    agentrt_print_warn("libcjson not found, JSON features may be limited")
endif()

# ==================== 路径设置 ====================
# agentrt/daemons/common 兼容层路径
set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

# commons 统一基础库路径
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

# ==================== Tool 服务静态库 ====================
# 提取 service/registry/executor/validator/cache/tool_approval/safety_guard_bridge/
# tool_svc_adapter 为独立静态库，供 tool_d 可执行文件、atoms/coreloopthree
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

add_library(agentrt_tool_service STATIC ${TOOL_SERVICE_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(agentrt_tool_service PRIVATE
        AGENTRT_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
elseif(UNIX)
    target_compile_definitions(agentrt_tool_service PRIVATE
        AGENTRT_PLATFORM_LINUX=1
        _GNU_SOURCE
    )
endif()

# 包含目录 - 兼容层和 commons
target_include_directories(agentrt_tool_service PUBLIC
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
    # P3.18 (ACC-DT27): sandbox 公共 API（agentrt_sandbox.h）和 syscall 号（syscalls.h）
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms/syscall/include
)

target_include_directories(agentrt_tool_service PRIVATE
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 链接库：svc_common 提供 daemon 公共层，cupolas 提供 SafetyGuard
# P3.18 (ACC-DT27): agentrt_syscall 提供 sandbox 公共 API（agentrt_sandbox_invoke 等）
# 依赖链：agentrt_tool_service → agentrt_syscall → agentrt_coreloopthree → agentrt_core
# 无循环依赖（coreloopthree 不链接 tool_service，仅 include 头文件）
target_link_libraries(agentrt_tool_service PUBLIC svc_common cupolas agentrt_syscall)

if(AGENTRT_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(agentrt_tool_service PUBLIC cjson)
    elseif(CJSON_FOUND AND CJSON_LIBRARIES)
        target_link_libraries(agentrt_tool_service PUBLIC ${CJSON_LIBRARIES})
    endif()
endif()
if(AGENTRT_HAS_YAML AND YAML_FOUND AND YAML_LIBRARIES)
    target_link_libraries(agentrt_tool_service PUBLIC ${YAML_LIBRARIES})
endif()

if(UNIX)
    target_link_libraries(agentrt_tool_service PUBLIC Threads::Threads agentrt_platform_libs)
endif()

# 编译选项
target_compile_options(agentrt_tool_service PRIVATE
    -Wall -Wextra -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# C-L05: 启用 Cupolas SafetyGuard V2 API
target_compile_definitions(agentrt_tool_service PRIVATE AGENTRT_ENABLE_V2_API=1)

# ==================== Tool daemon 可执行文件 ====================
# main.c 仅属于 daemon 可执行文件；config.c 和 utils/tool_helpers.c 为 daemon 专用工具函数；
# tool_svc_adapter.c 已纳入 agentrt_tool_service 库
add_executable(tool_d
    src/main.c
    src/config.c
    src/utils/tool_helpers.c
)

if(WIN32)
    target_compile_definitions(tool_d PRIVATE
        AGENTRT_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(tool_d PRIVATE agentrt_common ws2_32 bcrypt)
elseif(UNIX)
    target_link_libraries(tool_d PRIVATE agentrt_common Threads::Threads agentrt_platform_libs)
endif()

target_include_directories(tool_d PRIVATE
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

target_link_libraries(tool_d PRIVATE
    agentrt_tool_service
    # P3.18 (ACC-DT27): agentrt_tool_service → agentrt_syscall → agentrt_coreloopthree
    # 传递依赖。coreloopthree 引用 cognition/llm/checkpoint/hook_d 等符号，
    # 需 whole-archive 强制包含 coreloopthree 全部 .o + 完整传递依赖库。
    -Wl,--whole-archive
    agentrt_coreloopthree
    -Wl,--no-whole-archive
    agentrt_cognition
    agentrt_execution
    agentrt_core
    agentrt_memory
    agentrt_atoms
    agentrt_common
    agentrt_llm_service
    Threads::Threads
    uuid
    m
)

target_compile_options(tool_d PRIVATE
    -Wall -Wextra -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

target_compile_definitions(tool_d PRIVATE AGENTRT_ENABLE_V2_API=1)

# 测试支持
if(BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()

# 安装规则
install(TARGETS tool_d DESTINATION bin)
