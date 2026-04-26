# ========================================================================
# AgentOS TaskFlow Module
#
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-FileCopyrightText: 2026 SPHARX.
# SPDX-License-Identifier: Apache-2.0
#
# TaskFlow 图执行系统，基于 Pregel 超步模型的分布式计算框架
# 核心功能：
# 1. 有向无环图（DAG）执行引擎
# 2. Pregel 超步迭代计算模型
# 3. 任务依赖与数据流管理
# 4. 容错与状态检查点
# 5. 分布式图分区与通信
# ========================================================================

cmake_minimum_required(VERSION 3.20)

project(agentos-taskflow
    VERSION 1.0.0
    DESCRIPTION "AgentOS TaskFlow Graph Execution System"
    LANGUAGES C
)

# 设置输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 符号导出管理
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# 定义构建共享库时的宏
if(BUILD_SHARED_LIBS)
    add_definitions(-DAGENTOS_BUILDING_SHARED -DTASKFLOW_BUILDING_SHARED)
endif()

# 跨平台编译选项
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# 包含目录
set(TASKFLOW_COMMONS_MEMORY_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TASKFLOW_COMMONS_MEMORY_INCLUDE}
)

# ========================================================================
# 核心源文件
# ========================================================================

set(TASKFLOW_CORE_SOURCES
    src/taskflow_core.c
    src/graph_engine.c
    src/pregel_engine.c
    src/taskflow_execution_unit.c
    src/workflow_patterns.c
    src/taskflow_advanced.c
)

set(TASKFLOW_HEADERS
    include/taskflow.h
    include/graph_engine.h
    include/pregel_engine.h
    include/taskflow_integration.h
    include/workflow_patterns.h
    include/taskflow_types.h
    include/taskflow_advanced.h
)

# ========================================================================
# 创建 TaskFlow 库
# ========================================================================

# 主库
add_library(agentos_taskflow STATIC ${TASKFLOW_CORE_SOURCES})

# 头文件包含路径
target_include_directories(agentos_taskflow PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${TASKFLOW_COMMONS_MEMORY_INCLUDE}>
    $<INSTALL_INTERFACE:include/agentos/taskflow>
)

# 链接依赖
if(TARGET commons)
    target_link_libraries(agentos_taskflow PUBLIC commons)
endif()

if(TARGET atoms)
    target_link_libraries(agentos_taskflow PUBLIC atoms)
endif()

# 安全编译选项（基于 AgentOS C 编码风格指南附录 A）
if(MSVC)
    target_compile_options(agentos_taskflow PRIVATE /W4 /WX)
    target_compile_definitions(agentos_taskflow PRIVATE _CRT_SECURE_NO_WARNINGS)
    # MSVC安全选项
    target_compile_options(agentos_taskflow PRIVATE /GS)  # 缓冲区安全检查
    target_compile_options(agentos_taskflow PRIVATE /sdl) # 启用额外安全检查
    target_compile_options(agentos_taskflow PRIVATE /guard:cf) # 控制流防护
else()
    target_compile_options(agentos_taskflow PRIVATE -Wall -Wextra -Wpedantic )
    # GCC/Clang安全选项
    target_compile_options(agentos_taskflow PRIVATE -fstack-protector-strong)
    target_compile_options(agentos_taskflow PRIVATE -D_FORTIFY_SOURCE=2)
    target_compile_options(agentos_taskflow PRIVATE -fPIE)
    target_compile_options(agentos_taskflow PRIVATE -Wformat -Wformat-security)
    target_compile_options(agentos_taskflow PRIVATE -fno-omit-frame-pointer)
endif()

# 链接器安全选项
if(UNIX AND NOT APPLE)
    target_link_options(agentos_taskflow PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    target_link_options(agentos_taskflow PRIVATE -Wl,-dead_strip -pie)
endif()

# 私有头文件包含
target_include_directories(agentos_taskflow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ========================================================================
# 测试目标（可选）
# ========================================================================

if(BUILD_TESTS)
    add_executable(test_taskflow_advanced
        tests/unit/test_taskflow_advanced.c
    )

    target_include_directories(test_taskflow_advanced PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${TASKFLOW_COMMONS_MEMORY_INCLUDE}
    )

    target_link_libraries(test_taskflow_advanced
        agentos_taskflow
        -Wl,--whole-archive
        agentos_coreloopthree
        -Wl,--no-whole-archive
        agentos_core
        agentos_common
        Threads::Threads
        dl
        m
    )

    if(UNIX)
        target_link_options(test_taskflow_advanced PRIVATE
            -Wl,--allow-multiple-definition)
    endif()

    add_test(NAME taskflow_advanced_test COMMAND test_taskflow_advanced)
endif()

# ========================================================================
# 安装配置
# ========================================================================

# 安装头文件
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/agentos/taskflow
    FILES_MATCHING PATTERN "*.h"
)

# 安装库
install(TARGETS agentos_taskflow
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# ========================================================================
# 版本信息
# ========================================================================

# 生成版本头文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/taskflow_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/taskflow_version.h
    @ONLY
)