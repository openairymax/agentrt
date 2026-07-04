# ========================================================================
# AgentRT Frameworks Module — 外部 AI 框架适配器桥接层
#
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-FileCopyrightText: 2026 SPHARX.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# Frameworks 层是 Airymax 作为"行业底座"连接外部 AI 框架的桥梁。
# 核心功能：
# 1. 外部框架适配器注册表（线程安全）
# 2. 框架实例生命周期管理（init/start/stop/destroy）
# 3. 请求路由与响应处理
# 4. 健康检查与能力查询
# 5. 兼容驱动 LangChain/AutoGPT/CrewAI/Semantic Kernel/LlamaIndex 等
# ========================================================================


# 设置输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 符号导出管理
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if(WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# ========================================================================
# 核心源文件
# ========================================================================

set(FRAMEWORKS_CORE_SOURCES
    src/agentos_frameworks.c
)

set(FRAMEWORKS_HEADERS
    include/agentos_frameworks.h
)

# ========================================================================
# 创建 Frameworks 库
# ========================================================================

add_library(agentos_frameworks STATIC ${FRAMEWORKS_CORE_SOURCES})

if(BUILD_SHARED_LIBS)
    target_compile_definitions(agentos_frameworks PRIVATE AGENTOS_BUILDING_SHARED FRAMEWORKS_BUILDING_SHARED)
endif()

if(WIN32)
    target_compile_definitions(agentos_frameworks PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

target_include_directories(agentos_frameworks PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/agentos/frameworks>
    ${COMMONS_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}/utils/memory/include
    ${COMMONS_INCLUDE_DIR}/utils/error/include
    ${COMMONS_INCLUDE_DIR}/utils/sync/include
    ${COMMONS_INCLUDE_DIR}/platform/include
)

# 链接依赖：Frameworks 是 atoms 层薄模块，仅依赖基础平台与线程库
if(TARGET agentos_atoms)
    target_link_libraries(agentos_frameworks PUBLIC agentos_atoms)
endif()

# 安全编译选项（基于 AgentRT C 编码风格指南附录 A）
if(MSVC)
    target_compile_options(agentos_frameworks PRIVATE /W4 /WX)
    target_compile_definitions(agentos_frameworks PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(agentos_frameworks PRIVATE /GS)
    target_compile_options(agentos_frameworks PRIVATE /sdl)
    target_compile_options(agentos_frameworks PRIVATE /guard:cf)
else()
    target_compile_options(agentos_frameworks PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(agentos_frameworks PRIVATE -fstack-protector-strong)
    target_compile_options(agentos_frameworks PRIVATE $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>)
    target_compile_options(agentos_frameworks PRIVATE -fPIE)
    target_compile_options(agentos_frameworks PRIVATE -Wformat -Wformat-security)
    target_compile_options(agentos_frameworks PRIVATE -fno-omit-frame-pointer)
endif()

# 链接器安全选项
if(UNIX AND NOT APPLE)
    target_link_options(agentos_frameworks PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    target_link_options(agentos_frameworks PRIVATE -Wl,-dead_strip -pie)
endif()

# 线程库（pthread 用于注册表互斥锁）
find_package(Threads REQUIRED)
target_link_libraries(agentos_frameworks PUBLIC Threads::Threads)

# ========================================================================
# 安装配置
# ========================================================================

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/agentos/frameworks
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS agentos_frameworks
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
