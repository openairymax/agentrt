# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# AgentOS 服务层 (Backs) 顶层构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-backs 
    VERSION 1.0.0.5 
    DESCRIPTION "AgentOS Service Layer - Core Backend Services"
    LANGUAGES C
)

message(STATUS "Building AgentOS Backs v${PROJECT_VERSION}")

# C 标准设置
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项
option(BUILD_TESTS "Build service tests" OFF)
option(ENABLE_TRACING "Enable OpenTelemetry tracing" OFF)

# 全局定义
add_definitions(-D_GNU_SOURCE)
add_definitions(-DAGENTOS_BACKS_VERSION="${PROJECT_VERSION}")

# 查找依赖
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# 包含公共库子目录
add_subdirectory(common)

# 包含各服务子目录
add_subdirectory(llm_d)
add_subdirectory(tool_d)
add_subdirectory(market_d)
add_subdirectory(sched_d)
add_subdirectory(monit_d)

# 安装配置
install(DIRECTORY common/include/ DESTINATION include/agentos/backs/common)
install(DIRECTORY llm_d/include/ DESTINATION include/agentos/backs/llm_d)
install(DIRECTORY tool_d/include/ DESTINATION include/agentos/backs/tool_d)
install(DIRECTORY market_d/include/ DESTINATION include/agentos/backs/market_d)
install(DIRECTORY sched_d/include/ DESTINATION include/agentos/backs/sched_d)
install(DIRECTORY monit_d/include/ DESTINATION include/agentos/backs/monit_d)
