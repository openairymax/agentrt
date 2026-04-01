# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# daemon 公共库构建文件 - 最小兼容层
#
# 本模块现在作为 commons 统一基础库的兼容层，
# 提供向后兼容的 API，实际实现来自 commons。
#
# 保留的 daemon 特有模块：
# - svc_common.h: 服务框架接口
# - ipc_client.c: IPC 客户端实现

cmake_minimum_required(VERSION 3.20)
project(agentos-svc-commons VERSION 3.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 测试支持选项
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_COVERAGE "Build with code coverage" OFF)

# 查找依赖
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(YAML REQUIRED yaml-0.1)
pkg_check_modules(CJSON REQUIRED libcjson)

# ==================== 重要: 添加 commons 依赖 ====================
# 设置 commons 路径
set(COMMONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../commons")

# 检查 commons 是否存在
if(NOT EXISTS "${COMMONS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "commons module not found at ${COMMONS_DIR}. Please ensure commons is available.")
endif()

# 包含 commons 目录（如果作为子项目构建）
if(NOT TARGET agentos_common)
    add_subdirectory(${COMMONS_DIR} ${CMAKE_BINARY_DIR}/commons)
endif()

# ==================== daemon 特有源文件 ====================
# 只保留 daemon 特有的实现文件
# 已迁移到 commons:
# - error.c -> commons/utils/error
# - platform.c -> commons/platform
# - logger.c -> commons/utils/logging
# - config.c -> commons/utils/config_unified
# - svc_cache.c -> commons/utils/cache

set(SVC_COMMON_SOURCES
    src/ipc_client.c
)

# 创建静态库
add_library(svc_common STATIC ${SVC_COMMON_SOURCES})

# 链接 commons 库
target_link_libraries(svc_common PUBLIC
    agentos_common
)

# 平台特定配置
if(WIN32)
    target_compile_definitions(svc_common PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(svc_common PRIVATE
        ws2_32
        bcrypt
        advapi32
    )
elseif(APPLE)
    target_compile_definitions(svc_common PRIVATE
        AGENTOS_PLATFORM_MACOS=1
    )
elseif(UNIX)
    target_compile_definitions(svc_common PRIVATE
        AGENTOS_PLATFORM_LINUX=1
    )
    target_link_libraries(svc_common PRIVATE
        pthread
        rt
    )
endif()

# 包含目录 - 兼容层头文件
target_include_directories(svc_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/agentos>
)

# 包含 commons 的头文件目录
target_include_directories(svc_common PUBLIC
    ${COMMONS_DIR}
    ${COMMONS_DIR}/platform/include
    ${COMMONS_DIR}/utils/error/include
    ${COMMONS_DIR}/utils/logging/include
    ${COMMONS_DIR}/utils/config_unified/include
    ${COMMONS_DIR}/utils/memory/include
    ${COMMONS_DIR}/utils/string/include
    ${COMMONS_DIR}/utils/sync/include
    ${COMMONS_DIR}/utils/observability/include
    ${COMMONS_DIR}/utils/io/include
    ${COMMONS_DIR}/utils/types/include
    ${COMMONS_DIR}/utils/cache/include
    ${COMMONS_DIR}/utils/compat/include
)

target_include_directories(svc_common PRIVATE
    ${LIBCURL_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

# 链接外部库
target_link_libraries(svc_common PUBLIC
    ${LIBCURL_LIBRARIES}
    ${YAML_LIBRARIES}
    ${CJSON_LIBRARIES}
    Threads::Threads
)

# 编译选项
if(BUILD_COVERAGE)
    target_compile_options(svc_common PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fprofile-arcs
        -ftest-coverage
        -g
        -O0
    )
    target_link_options(svc_common PRIVATE
        --coverage
    )
else()
    target_compile_options(svc_common PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=implicit-function-declaration
        -Werror=return-type
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

# 导出符号
set_target_properties(svc_common PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 3
    POSITION_INDEPENDENT_CODE ON
)

# 测试支持
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 安装规则
install(TARGETS svc_common
    EXPORT agentos-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装兼容层头文件
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/platform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/error.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_logger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_cache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/compat.h
    DESTINATION include/agentos
)

# 导出配置
install(EXPORT agentos-targets
    FILE agentos-targets.cmake
    NAMESPACE agentos::
    DESTINATION lib/cmake/agentos
)
