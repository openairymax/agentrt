# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# AgentOS Lite 内核顶层 CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(agentos-kernel-lite VERSION 1.0.0 LANGUAGES C)

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# 编译选项
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 全局定义
add_definitions(-D_GNU_SOURCE -DAGENTOS_LITE_VERSION="${PROJECT_VERSION}")

# 查找依赖
find_package(Threads REQUIRED)

# 包含子目录
add_subdirectory(corekernlite)

# 安装配置
install(DIRECTORY corekernlite/include/ DESTINATION include/agentos/lite)