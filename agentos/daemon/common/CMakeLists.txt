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
find_package(Threads REQUIRED)

# 使用与根项目一致的依赖查找方式
# 在Windows上，PkgConfig通常不可用，使用find_package替代
if(WIN32)
    find_package(PkgConfig QUIET)
else()
    find_package(PkgConfig)
endif()

# libcurl查找
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBCURL QUIET libcurl)
endif()
if(NOT LIBCURL_FOUND)
    # 尝试使用CMake的find_package
    find_package(CURL QUIET)
    if(CURL_FOUND)
        set(LIBCURL_FOUND TRUE)
        set(LIBCURL_LIBRARIES ${CURL_LIBRARIES})
        set(LIBCURL_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
        message(STATUS "libcurl found via find_package")
    else()
        message(WARNING "libcurl not found, some network features may be limited")
    endif()
endif()

# libyaml查找
if(PKG_CONFIG_FOUND)
    pkg_check_modules(YAML QUIET yaml-0.1)
endif()
if(NOT YAML_FOUND)
    find_package(YAML QUIET)
    if(YAML_FOUND)
        message(STATUS "libyaml found via find_package")
    else()
        message(WARNING "libyaml not found, YAML functionality may be limited")
    endif()
endif()

# cJSON查找
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CJSON QUIET libcjson)
endif()
if(NOT CJSON_FOUND)
    find_package(cJSON QUIET)
    if(cJSON_FOUND)
        set(CJSON_FOUND TRUE)
        set(CJSON_LIBRARIES cjson)
        set(CJSON_INCLUDE_DIRS ${cJSON_INCLUDE_DIRS})
        message(STATUS "cJSON found via find_package")
    else()
        message(WARNING "cJSON not found, JSON functionality may be limited")
    endif()
endif()

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
# - error.c -> agentos/commons/utils/error
# - platform.c -> agentos/commons/platform
# - logger.c -> agentos/commons/utils/logging
# - config.c -> agentos/commons/utils/config_unified
# - svc_cache.c -> agentos/commons/utils/cache

set(SVC_COMMON_SOURCES
    src/svc_common.c          # 服务框架核心实现
    src/ipc_client.c
    src/ipc_service_bus.c     # IPC服务总线
    src/service_discovery.c   # 跨进程服务发现
    src/circuit_breaker.c     # 熔断器与自愈
    src/unified_metrics.c     # 统一指标收集
    src/alert_manager.c       # 智能告警管理
    src/config_manager.c      # 统一配置管理
    src/svc_auth.c
    src/daemon_security.c
    src/jsonrpc_helpers.c
    src/method_dispatcher.c
    src/safe_string_utils.c
    src/input_validator.c
    src/param_validator.c
    src/log_sanitizer.c
    src/platform_compat.c     # 平台兼容层实现
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
    $<BUILD_INTERFACE:${COMMONS_DIR}>
    $<BUILD_INTERFACE:${COMMONS_DIR}/platform/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/error/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/logging/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/config_unified/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/memory/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/string/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/sync/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/observability/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/io/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/types/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/cache/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/compat/include>
    # 新增网络和IPC模块
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/network/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/ipc/include>
)

# 条件包含外部库头文件
if(LIBCURL_FOUND)
    target_include_directories(svc_common PRIVATE ${LIBCURL_INCLUDE_DIRS})
endif()
if(YAML_FOUND)
    target_include_directories(svc_common PRIVATE ${YAML_INCLUDE_DIRS})
endif()
if(CJSON_FOUND)
    target_include_directories(svc_common PRIVATE ${CJSON_INCLUDE_DIRS})
endif()

# 链接外部库
target_link_libraries(svc_common PUBLIC Threads::Threads)

if(LIBCURL_FOUND)
    target_link_libraries(svc_common PUBLIC ${LIBCURL_LIBRARIES})
endif()
if(YAML_FOUND)
    target_link_libraries(svc_common PUBLIC ${YAML_LIBRARIES})
endif()
if(CJSON_FOUND)
    target_link_libraries(svc_common PUBLIC ${CJSON_LIBRARIES})
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_auth.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/daemon_security.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/jsonrpc_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/method_dispatcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/safe_string_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/input_validator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/param_validator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/log_sanitizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ipc_service_bus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/service_discovery.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/circuit_breaker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/unified_metrics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/alert_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config_manager.h
    DESTINATION include/agentos
)
