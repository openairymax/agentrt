# AgentRT 核心组件构建文件
# agentrt/CMakeLists.txt
# Copyright (c) 2026 SPHARX. All Rights Reserved.


# CTest必须在此处启用，以便子目录测试能传播到根CTest
if(BUILD_TESTS)
    enable_testing()
    agentrt_print_ok("AgentRT: CTest enabled")
endif()

agentrt_print_info("Configuring AgentRT core components...")

# 从根 CMakeLists.txt 传播依赖可用性标志
if(AGENTRT_HAS_CJSON)
    set(AGENTRT_HAS_CJSON TRUE CACHE BOOL "cJSON available for agentrt")
endif()
if(AGENTRT_HAS_CURL)
    set(AGENTRT_HAS_CURL TRUE CACHE BOOL "CURL available for agentrt")
endif()
if(AGENTRT_HAS_YAML)
    set(AGENTRT_HAS_YAML TRUE CACHE BOOL "libyaml available for agentrt")
endif()
if(AGENTRT_HAS_OPENSSL)
    set(AGENTRT_HAS_OPENSSL TRUE CACHE BOOL "OpenSSL available for agentrt")
endif()
if(AGENTRT_HAS_MICROHTTPD)
    set(AGENTRT_HAS_MICROHTTPD TRUE CACHE BOOL "libmicrohttpd available for agentrt")
endif()
if(AGENTRT_HAS_LIBWEBSOCKETS)
    set(AGENTRT_HAS_LIBWEBSOCKETS TRUE CACHE BOOL "libwebsockets available for agentrt")
endif()

# 子模块启用状态汇总
agentrt_print_info("[MODULE] Submodule build status:")
agentrt_print_info("[MODULE]   atoms:    ${BUILD_ATOMS}")
agentrt_print_info("[MODULE]   commons:  ${BUILD_COMMONS}")
agentrt_print_info("[MODULE]   cupolas:  ${BUILD_CUPOLAS}")
agentrt_print_info("[MODULE]   daemon:   ${BUILD_DAEMON}")
agentrt_print_info("[MODULE]   gateway:  ${BUILD_GATEWAY}")
agentrt_print_info("[MODULE]   heapstore: ${BUILD_HEAPSTORE}")
agentrt_print_info("[MODULE]   toolkit:  ${BUILD_TOOLKIT}")
agentrt_print_info("[MODULE]   Memory backend: ${AGENTRT_MEMORY_BACKEND}")
agentrt_print_info("[MODULE]   With MemoryRovol: ${AGENTRT_WITH_MEMORYROVOL}")

# 全局特性测试宏定义 - 必须在所有其他配置之前
# 这些宏确保POSIX函数和类型在所有平台上一致可用
if(UNIX AND NOT APPLE)
    add_compile_definitions(
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
        _GNU_SOURCE
    )
    agentrt_print_ok("POSIX feature test macros enabled for Linux")
elseif(APPLE)
    add_compile_definitions(
        _POSIX_C_SOURCE=200112L
        _DARWIN_C_SOURCE
    )
    agentrt_print_ok("POSIX feature test macros enabled for macOS")
endif()

if(MSVC)
    if(EXISTS "${CMAKE_SOURCE_DIR}/scripts/dev/cmake/windows_preinclude.h")
        agentrt_print_ok("[MSVC] Force-include file found: scripts/dev/cmake/windows_preinclude.h")
    else()
        agentrt_print_warn("[MSVC] Force-include file NOT FOUND: scripts/dev/cmake/windows_preinclude.h")
    endif()
    add_compile_options(/FI"${CMAKE_SOURCE_DIR}/scripts/dev/cmake/windows_preinclude.h")
endif()

# 选项设置
option(BUILD_ATOMS "Build atoms module (core kernel components)" ON)
option(BUILD_COMMONS "Build commons module (common utilities)" ON)
option(BUILD_CUPOLAS "Build cupolas module (security framework)" ON)
option(BUILD_DAEMON "Build daemon modules (system services)" ON)  # R-09-124: 启用服务编译
option(BUILD_GATEWAY "Build gateway module (API gateway)" ON)  # R-09-124: 启用网关编译
option(BUILD_HEAPSTORE "Build heapstore module (persistent storage)" ON)
option(BUILD_TOOLKIT "Build toolkit modules (language SDKs)" OFF)  # 默认关闭，需要额外配置

# 全局包含目录
# 注意: 不包含 string/include 和 memory/include，因为它们包含与系统头文件同名的
# string.h 和 memory.h，会导致系统头文件被遮蔽（shadowing）
# 需要这些自定义头文件的模块应通过 target_include_directories PRIVATE 方式引入
# R-09-124: include_directories(
# R-09-124:     ${CMAKE_CURRENT_SOURCE_DIR}
# R-09-124:     ${CMAKE_CURRENT_SOURCE_DIR}/include
# R-09-124:     ${CMAKE_CURRENT_SOURCE_DIR}/atoms/corekern/include
# R-09-124:     ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/logging/include
# R-09-124:     ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/sync/include
# R-09-124:     ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/platform/include
# R-09-124:     ${CMAKE_CURRENT_SOURCE_DIR}/commons/utils/include
# R-09-124: )

# 包含子目录
# 构建顺序：commons（基础库）优先于 atoms（核心组件）
if(BUILD_COMMONS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/commons/CMakeLists.txt")
    add_subdirectory(commons)
else()
    agentrt_print_warn("commons module not found or disabled")
endif()

if(BUILD_ATOMS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/atoms/CMakeLists.txt")
    add_subdirectory(atoms)
else()
    agentrt_print_warn("atoms module not found or disabled")
endif()

# protocols模块（gateway依赖）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/protocols/CMakeLists.txt")
    add_subdirectory(protocols)
endif()

if(BUILD_CUPOLAS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cupolas/CMakeLists.txt")
    add_subdirectory(cupolas)
else()
    agentrt_print_warn("cupolas module not found or disabled")
endif()

if(BUILD_DAEMON AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/daemons/CMakeLists.txt")
    add_subdirectory(daemons)
else()
    agentrt_print_warn("daemons module not found or disabled")
endif()

if(BUILD_GATEWAY AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gateway/CMakeLists.txt")
    add_subdirectory(gateway)
else()
    agentrt_print_warn("gateway module not found or disabled")
endif()

if(BUILD_HEAPSTORE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/heapstore/CMakeLists.txt")
    add_subdirectory(heapstore)
else()
    agentrt_print_warn("heapstore module not found or disabled")
endif()

# toolkit包含多种语言SDK，需要特殊处理
if(BUILD_TOOLKIT)
    agentrt_print_info("Toolkit modules require additional configuration for each language SDK")
    # Python SDK
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/toolkit/python/setup.py")
        agentrt_print_ok("Python SDK found (requires Python build system)")
    endif()
    # Go SDK
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/toolkit/go/go.mod")
        agentrt_print_ok("Go SDK found (requires Go build system)")
    endif()
    # TypeScript SDK
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/toolkit/typescript/package.json")
        agentrt_print_ok("TypeScript SDK found (requires Node.js build system)")
    endif()
    # Rust SDK
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/toolkit/rust/Cargo.toml")
        agentrt_print_ok("Rust SDK found (requires Cargo build system)")
    endif()
endif()

# 创建聚合目标
add_custom_target(agentrt-all
    DEPENDS
        $<IF:$<TARGET_EXISTS:agentrt_atoms>,agentrt_atoms,>
        $<IF:$<TARGET_EXISTS:agentrt_commons>,agentrt_commons,>
        $<IF:$<TARGET_EXISTS:agentrt_cupolas>,agentrt_cupolas,>
    COMMENT "Build all AgentRT core components"
)

# 安装配置
install(DIRECTORY atoms/corekern/include/ DESTINATION include/agentrt/corekern)
install(DIRECTORY atoms/coreloopthree/include/ DESTINATION include/agentrt/coreloopthree)
install(DIRECTORY atoms/syscall/include/ DESTINATION include/agentrt/syscall)
install(DIRECTORY commons/utils/memory/include/ DESTINATION include/agentrt/commons/memory)
install(DIRECTORY commons/utils/string/include/ DESTINATION include/agentrt/commons/string)
install(DIRECTORY commons/utils/logging/include/ DESTINATION include/agentrt/commons/logging)

agentrt_print_ok("AgentRT core components configuration complete")