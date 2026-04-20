# AgentOS 核心组件构建文件
# agentos/CMakeLists.txt
# Copyright (c) 2026 SPHARX. All Rights Reserved.

cmake_minimum_required(VERSION 3.20)
project(agentos-core VERSION 1.0.0 LANGUAGES C CXX)

message(STATUS "Configuring AgentOS core components...")

# 全局特性测试宏定义 - 必须在所有其他配置之前
# 这些宏确保POSIX函数和类型在所有平台上一致可用
if(UNIX AND NOT APPLE)
    add_compile_definitions(
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
        _GNU_SOURCE
    )
    message(STATUS "POSIX feature test macros enabled for Linux")
elseif(APPLE)
    add_compile_definitions(
        _POSIX_C_SOURCE=200112L
        _DARWIN_C_SOURCE
    )
    message(STATUS "POSIX feature test macros enabled for macOS")
endif()

# 选项设置
option(BUILD_ATOMS "Build atoms module (core kernel components)" ON)
option(BUILD_COMMONS "Build commons module (common utilities)" ON)
option(BUILD_CUPOLAS "Build cupolas module (security framework)" ON)
option(BUILD_DAEMON "Build daemon modules (system services)" OFF)  # Windows上暂时禁用，需要PkgConfig
option(BUILD_GATEWAY "Build gateway module (API gateway)" OFF)  # Windows上暂时禁用，需要cJSON
option(BUILD_HEAPSTORE "Build heapstore module (persistent storage)" ON)
option(BUILD_TOOLKIT "Build toolkit modules (language SDKs)" OFF)  # 默认关闭，需要额外配置

# 全局包含目录
# 注意: 不包含 string/include 和 memory/include，因为它们包含与系统头文件同名的
# string.h 和 memory.h，会导致系统头文件被遮蔽（shadowing）
# 需要这些自定义头文件的模块应通过 target_include_directories PRIVATE 方式引入
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/include
)

# 包含子目录
if(BUILD_ATOMS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/atoms/CMakeLists.txt")
    add_subdirectory(atoms)
else()
    message(WARNING "atoms module not found or disabled")
endif()

if(BUILD_COMMONS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/commons/CMakeLists.txt")
    add_subdirectory(commons)
else()
    message(WARNING "commons module not found or disabled")
endif()

# protocols模块（gateway依赖）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/protocols/CMakeLists.txt")
    add_subdirectory(protocols)
endif()

if(BUILD_CUPOLAS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cupolas/CMakeLists.txt")
    add_subdirectory(cupolas)
else()
    message(WARNING "cupolas module not found or disabled")
endif()

if(BUILD_DAEMON AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/daemon/CMakeLists.txt")
    add_subdirectory(daemon)
else()
    message(WARNING "daemon module not found or disabled")
endif()

if(BUILD_GATEWAY AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gateway/CMakeLists.txt")
    add_subdirectory(gateway)
else()
    message(WARNING "gateway module not found or disabled")
endif()

if(BUILD_HEAPSTORE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/heapstore/CMakeLists.txt")
    add_subdirectory(heapstore)
else()
    message(WARNING "heapstore module not found or disabled")
endif()

# toolkit包含多种语言SDK，需要特殊处理
if(BUILD_TOOLKIT)
    message(STATUS "Toolkit modules require additional configuration for each language SDK")
    # Python SDK
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/toolkit/python/setup.py")
        message(STATUS "Python SDK found (requires Python build system)")
    endif()
    # Go SDK
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/toolkit/go/go.mod")
        message(STATUS "Go SDK found (requires Go build system)")
    endif()
    # TypeScript SDK
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/toolkit/typescript/package.json")
        message(STATUS "TypeScript SDK found (requires Node.js build system)")
    endif()
    # Rust SDK
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/toolkit/rust/Cargo.toml")
        message(STATUS "Rust SDK found (requires Cargo build system)")
    endif()
endif()

# 创建聚合目标
add_custom_target(agentos-all
    DEPENDS
        $<IF:$<TARGET_EXISTS:agentos_atoms>,agentos_atoms,>
        $<IF:$<TARGET_EXISTS:agentos_commons>,agentos_commons,>
        $<IF:$<TARGET_EXISTS:agentos_cupolas>,agentos_cupolas,>
    COMMENT "Build all AgentOS core components"
)

# 安装配置
install(DIRECTORY atoms/corekern/include/ DESTINATION include/agentos/corekern)
install(DIRECTORY atoms/coreloopthree/include/ DESTINATION include/agentos/coreloopthree)
install(DIRECTORY atoms/memoryrovol/include/ DESTINATION include/agentos/memoryrovol)
install(DIRECTORY atoms/syscall/include/ DESTINATION include/agentos/syscall)
install(DIRECTORY commons/utils/memory/include/ DESTINATION include/agentos/commons/memory)
install(DIRECTORY commons/utils/string/include/ DESTINATION include/agentos/commons/string)
install(DIRECTORY commons/utils/logging/include/ DESTINATION include/agentos/commons/logging)

message(STATUS "AgentOS core components configuration complete")