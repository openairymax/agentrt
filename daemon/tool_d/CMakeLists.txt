# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Tool 服务构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-tool-d VERSION 3.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CJSON REQUIRED libcjson)
pkg_check_modules(YAML REQUIRED yaml-0.1)

# ==================== 路径设置 ====================
# daemon/common 兼容层路径
set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

# commons 统一基础库路径
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

# Tool 服务源文件
set(TOOL_D_SOURCES
    src/main.c
    src/service.c
    src/registry.c
    src/executor.c
    src/validator.c
    src/cache.c
    src/utils/tool_helpers.c
)

# 创建可执行文件
add_executable(agentos-tool-d ${TOOL_D_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(agentos-tool-d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(agentos-tool-d PRIVATE
        ws2_32
        bcrypt
    )
elseif(UNIX)
    target_link_libraries(agentos-tool-d PRIVATE
        pthread
        rt
    )
endif()

# 包含目录 - 兼容层和 commons
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
    # 新增网络和IPC模块
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(agentos-tool-d PRIVATE
    svc_common
    Threads::Threads
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
)

# 编译选项
target_compile_options(agentos-tool-d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# 测试支持
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 安装规则
install(TARGETS agentos-tool-d DESTINATION bin)
