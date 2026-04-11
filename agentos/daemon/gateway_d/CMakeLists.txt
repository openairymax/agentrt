# Gateway Daemon - AgentOS 网关守护进程
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#
# 架构说明：
#   gateway_d 是 gateway 库的用户空间守护进程包装器。
#   它依赖 gateway 库提供的公共 API (include/gateway.h)，
#   并通过 gateway 库间接调用 agentos/atoms/syscall 接口。
#
#   依赖链：gateway_d --> gateway (静态库) --> agentos/atoms/syscall + agentos/atoms/corekern

cmake_minimum_required(VERSION 3.20)

project(gateway_d VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ==================== 查找外部依赖 ====================
find_package(Threads REQUIRED)

# 使用与根项目一致的依赖查找方式
# 在Windows上，PkgConfig通常不可用，使用find_package替代
if(WIN32)
    find_package(PkgConfig QUIET)
else()
    find_package(PkgConfig)
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
        set(CJSON_FOUND FALSE)
        set(CJSON_LIBRARIES "")
        set(CJSON_INCLUDE_DIRS "")
    endif()
endif()

# ==================== 路径设置 ====================

# Gateway 库路径（同层级的 gateway 模块）
set(GATEWAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../gateway)
set(GATEWAY_INCLUDE_DIR ${GATEWAY_DIR}/include)
set(GATEWAY_SRC_DIR ${GATEWAY_DIR}/src)

# Atom 模块路径（syscall 接口 + corekern 基础定义）
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)

# agentos/daemon/common 兼容层路径
set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

# commons 统一基础库路径
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

# ==================== 源文件 ====================
set(GATEWAY_D_SOURCES
    src/main.c
    src/service.c
)

# 可执行文件
add_executable(gateway_d ${GATEWAY_D_SOURCES})

# ==================== 平台特定配置 ====================
if(WIN32)
    target_compile_definitions(gateway_d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(gateway_d PRIVATE
        ws2_32
        bcrypt
    )
elseif(UNIX)
    target_link_libraries(gateway_d PRIVATE
        pthread
        rt
    )
endif()

# ==================== 包含目录 ====================
target_include_directories(gateway_d PRIVATE
    # 本模块头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # Gateway 公共 API（service.c 引用 "gateway.h"）
    ${GATEWAY_INCLUDE_DIR}
    # Gateway 内部头文件（gateway_api.c 引用 http_gateway.h 等）
    ${GATEWAY_SRC_DIR}
    ${GATEWAY_SRC_DIR}/gateway
    ${GATEWAY_SRC_DIR}/utils
    # Atom 模块（service.c 引用 "syscalls.h", "agentos.h"）
    ${SYSCALL_INCLUDE_DIR}
    ${COREKERN_INCLUDE_DIR}
    # Daemon 兼容层
    ${DAEMON_COMMON_INCLUDE_DIR}
    # Commons 统一基础库
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
    # 网络和IPC模块
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
    # 外部依赖
    ${CJSON_INCLUDE_DIRS}
)

# ==================== 链接库 ====================
#
# 注意：gateway 库需要在构建时可用。
# 如果使用独立构建，需确保 gateway 已预先编译或通过 add_subdirectory 引入。
#
# 方式一（推荐 - 通过上级 CMake 引入）:
#   在 AgentOS 根 CMakeLists.txt 中同时 add_subdirectory(gateway) 和 add_subdirectory(daemon)
#   然后 gateway_d 可以直接链接 gateway target
#
# 方式二（独立构建 - 当前默认）:
#   gateway_d 自行编译 gateway 源码并链接（见下方）

# 将 gateway 库的源文件直接加入编译（确保符号可解析）
set(GATEWAY_LIB_SOURCES
    ${GATEWAY_SRC_DIR}/agentos/gateway/gateway_api.c
    ${GATEWAY_SRC_DIR}/agentos/gateway/http_gateway.c
    ${GATEWAY_SRC_DIR}/agentos/gateway/ws_gateway.c
    ${GATEWAY_SRC_DIR}/agentos/gateway/stdio_gateway.c
    ${GATEWAY_SRC_DIR}/utils/jsonrpc.c
    ${GATEWAY_SRC_DIR}/utils/syscall_router.c
)

# 创建 gateway 库对象（不安装，仅供 gateway_d 使用）
add_library(gateway_lib_obj OBJECT ${GATEWAY_LIB_SOURCES})
target_include_directories(gateway_lib_obj PUBLIC
    ${GATEWAY_INCLUDE_DIR}
    ${GATEWAY_SRC_DIR}
    ${GATEWAY_SRC_DIR}/gateway
    ${GATEWAY_SRC_DIR}/utils
    ${SYSCALL_INCLUDE_DIR}
    ${COREKERN_INCLUDE_DIR}
    ${CJSON_INCLUDE_DIRS}
)

# 查找 gateway 所需的外部库
# PkgConfig 已在前面查找过，使用 PKG_CONFIG_FOUND 变量
if(PKG_CONFIG_FOUND)
    pkg_check_modules(MICROHTTPD QUIET libmicrohttpd)
    pkg_check_modules(LIBWEBSOCKETS QUIET libwebsockets)
else()
    # 尝试使用 CMake 的 find_package 查找这些库
    find_package(libmicrohttpd QUIET)
    find_package(libwebsockets QUIET)
endif()

# 条件包含外部库头文件和定义
if(MICROHTTPD_FOUND)
    target_include_directories(gateway_lib_obj PUBLIC ${MICROHTTPD_INCLUDE_DIRS})
    target_compile_definitions(gateway_lib_obj PUBLIC GATEWAY_HAS_HTTP=1)
else()
    message(WARNING "libmicrohttpd not found, HTTP gateway may not compile.")
endif()

if(LIBWEBSOCKETS_FOUND)
    target_include_directories(gateway_lib_obj PUBLIC ${LIBWEBSOCKETS_INCLUDE_DIRS})
    target_compile_definitions(gateway_lib_obj PUBLIC GATEWAY_HAS_WS=1)
else()
    message(WARNING "libwebsockets not found, WebSocket gateway may not compile.")
endif()

target_link_libraries(gateway_d PRIVATE
    $<TARGET_OBJECTS:gateway_lib_obj>
    svc_common
    Threads::Threads
)

# 条件链接CJSON库
if(CJSON_FOUND)
    target_link_libraries(gateway_d PRIVATE ${CJSON_LIBRARIES})
endif()

# 如果找到了 HTTP/WS 库，也链接它们
if(MICROHTTPD_FOUND)
    target_link_libraries(gateway_d PRIVATE ${MICROHTTPD_LIBRARIES})
endif()
if(LIBWEBSOCKETS_FOUND)
    target_link_libraries(gateway_d PRIVATE ${LIBWEBSOCKETS_LIBRARIES})
endif()

# ==================== 编译选项 ====================
target_compile_options(gateway_d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# ==================== 安装 ====================
install(TARGETS gateway_d
    RUNTIME DESTINATION bin
)

install(FILES
    include/gateway_service.h
    DESTINATION include/agentos/daemon/gateway_d
)

# ==================== 测试 ====================
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
