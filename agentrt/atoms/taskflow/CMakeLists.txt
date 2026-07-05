# ========================================================================
# AgentRT TaskFlow Module
#
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-FileCopyrightText: 2026 SPHARX.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# TaskFlow 图执行系统，基于 Pregel 超步模型的分布式计算框架
# 核心功能：
# 1. 有向无环图（DAG）执行引擎
# 2. Pregel 超步迭代计算模型
# 3. 任务依赖与数据流管理
# 4. 容错与状态检查点
# 5. 分布式图分区与通信
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

# 包含目录
set(TASKFLOW_COMMONS_MEMORY_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include)
set(TASKFLOW_COMMONS_PLATFORM_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include)

# include_directories(
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${TASKFLOW_COMMONS_MEMORY_INCLUDE}
#     ${TASKFLOW_COMMONS_PLATFORM_INCLUDE}
# )

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
add_library(agentrt_taskflow STATIC ${TASKFLOW_CORE_SOURCES})

if(BUILD_SHARED_LIBS)
    target_compile_definitions(agentrt_taskflow PRIVATE AGENTRT_BUILDING_SHARED TASKFLOW_BUILDING_SHARED)
endif()

if(WIN32)
    target_compile_definitions(agentrt_taskflow PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

target_include_directories(agentrt_taskflow PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TASKFLOW_COMMONS_MEMORY_INCLUDE}
    ${TASKFLOW_COMMONS_PLATFORM_INCLUDE}
)

# 头文件包含路径
target_include_directories(agentrt_taskflow PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${TASKFLOW_COMMONS_MEMORY_INCLUDE}>
    $<BUILD_INTERFACE:${TASKFLOW_COMMONS_PLATFORM_INCLUDE}>
    $<INSTALL_INTERFACE:include/agentrt/taskflow>
)

# 链接依赖
if(TARGET agentrt_common)
    target_link_libraries(agentrt_taskflow PUBLIC agentrt_common)
endif()

if(TARGET agentrt_coreloopthree)
    target_link_libraries(agentrt_taskflow PUBLIC agentrt_coreloopthree)
endif()

# IRON-2 (load_workflow_json 桩函数修复)：taskflow_advanced 是 B 类应用语义层，
# 可依赖 cJSON 实现 JSON 解析。cJSON 不可用时 load_workflow_json 返回 ENOTSUP
# （显式不支持，非桩函数），符合 IRON-2 铁律。
if(AGENTRT_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(agentrt_taskflow PRIVATE cjson)
        target_compile_definitions(agentrt_taskflow PRIVATE AGENTRT_TASKFLOW_HAVE_CJSON=1)
    elseif(CJSON_FOUND)
        target_link_libraries(agentrt_taskflow PRIVATE ${CJSON_LIBRARIES})
        target_include_directories(agentrt_taskflow PRIVATE ${CJSON_INCLUDE_DIRS})
        target_compile_definitions(agentrt_taskflow PRIVATE AGENTRT_TASKFLOW_HAVE_CJSON=1)
    endif()
endif()

# 安全编译选项（基于 AgentRT C 编码风格指南附录 A）
if(MSVC)
    target_compile_options(agentrt_taskflow PRIVATE /W4 /WX)
    target_compile_definitions(agentrt_taskflow PRIVATE _CRT_SECURE_NO_WARNINGS)
    # MSVC安全选项
    target_compile_options(agentrt_taskflow PRIVATE /GS)  # 缓冲区安全检查
    target_compile_options(agentrt_taskflow PRIVATE /sdl) # 启用额外安全检查
    target_compile_options(agentrt_taskflow PRIVATE /guard:cf) # 控制流防护
else()
    target_compile_options(agentrt_taskflow PRIVATE -Wall -Wextra -Wpedantic )
    # GCC/Clang安全选项
    target_compile_options(agentrt_taskflow PRIVATE -fstack-protector-strong)
    target_compile_options(agentrt_taskflow PRIVATE $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>)
    target_compile_options(agentrt_taskflow PRIVATE -fPIE)
    target_compile_options(agentrt_taskflow PRIVATE -Wformat -Wformat-security)
    target_compile_options(agentrt_taskflow PRIVATE -fno-omit-frame-pointer)
endif()

# 链接器安全选项
if(UNIX AND NOT APPLE)
    target_link_options(agentrt_taskflow PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    target_link_options(agentrt_taskflow PRIVATE -Wl,-dead_strip -pie)
endif()

# 私有头文件包含
target_include_directories(agentrt_taskflow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ========================================================================
# 测试目标（可选）
# ========================================================================

if(BUILD_TESTS)
    add_executable(test_taskflow_advanced
        tests/unit/test_taskflow_advanced.c
    )
    target_compile_definitions(test_taskflow_advanced PRIVATE AGENTRT_COMPLIANCE_IMPL)

    target_include_directories(test_taskflow_advanced PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../coreloopthree/tests/unit
        ${CMAKE_CURRENT_SOURCE_DIR}/../coreloopthree/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/llm_d/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/tool_d/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../cupolas/src
        ${TASKFLOW_COMMONS_MEMORY_INCLUDE}
    )

    target_compile_options(test_taskflow_advanced PRIVATE
        -Wall -Wextra -g -Wno-unused-function -Wno-deprecated-declarations
    )

    target_link_libraries(test_taskflow_advanced PRIVATE
        agentrt_taskflow
        agentrt_core
        agentrt_common
        svc_common
        agentrt_llm_service
        agentrt_tool_service
    )

    # IRON-2: test_taskflow_advanced 需要显式链接 cJSON（静态库 PRIVATE 依赖不传递）
    if(AGENTRT_HAS_CJSON)
        if(cJSON_FOUND)
            target_link_libraries(test_taskflow_advanced PRIVATE cjson)
        elseif(CJSON_FOUND)
            target_link_libraries(test_taskflow_advanced PRIVATE ${CJSON_LIBRARIES})
            target_include_directories(test_taskflow_advanced PRIVATE ${CJSON_INCLUDE_DIRS})
        endif()
    endif()

    if(UNIX AND NOT APPLE)
        target_link_libraries(test_taskflow_advanced PRIVATE
            -Wl,--whole-archive
            agentrt_coreloopthree
            -Wl,--no-whole-archive
            agentrt_platform_libs
        )
    elseif(WIN32)
        target_link_libraries(test_taskflow_advanced PRIVATE
            agentrt_coreloopthree
        )
    else()
        target_link_libraries(test_taskflow_advanced PRIVATE
            agentrt_coreloopthree
        )
    endif()
    # Threads found by root CMakeLists.txt
    target_link_libraries(test_taskflow_advanced PRIVATE Threads::Threads)

    target_link_libraries(test_taskflow_advanced PRIVATE agentrt_test_ldflags)

    add_test(NAME taskflow_advanced_test COMMAND test_taskflow_advanced)
endif()

# ========================================================================
# 安装配置
# ========================================================================

# 安装头文件
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/agentrt/taskflow
    FILES_MATCHING PATTERN "*.h"
)

# 安装库
install(TARGETS agentrt_taskflow
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