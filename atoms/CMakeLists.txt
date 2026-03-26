# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# AgentOS 内核顶层 CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(agentos-kernel VERSION 1.0.0.6 LANGUAGES C CXX)

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

# 代码覆盖率支持
if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # 添加覆盖率编译和链接标志
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs -ftest-coverage -lgcov)
        
        # 确保调试符号开启
        string(REPLACE "-O2" "-O0" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        string(REPLACE "-O3" "-O0" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        
        message(STATUS "Code coverage enabled for GCC/Clang")
    else()
        message(WARNING "Code coverage only supported for GCC/Clang compilers")
    endif()
endif()

# 查找依赖
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent)

# 包含子目录
# Kernel subdirectories
add_subdirectory(corekern)
add_subdirectory(coreloopthree)
add_subdirectory(memoryrovol)
add_subdirectory(syscall)

# 安装配置
install(DIRECTORY include/ DESTINATION include/agentos)
