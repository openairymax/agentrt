# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# AgentOS 内核顶层 CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(agentos-kernel VERSION 1.0.0.5 LANGUAGES C CXX)

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# 编译选项
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 全局定义
add_definitions(-D_GNU_SOURCE -DAGENTOS_VERSION="${PROJECT_VERSION}")

# 查找依赖
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent)

# 包含子目录
add_subdirectory(core)
add_subdirectory(coreloopthree)
add_subdirectory(memoryrovol)
add_subdirectory(syscall)

# 安装配置
install(DIRECTORY include/ DESTINATION include/agentos)