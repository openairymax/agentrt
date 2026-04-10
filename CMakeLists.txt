# ========================================================================
# AgentOS 完整生态系统 - 根构建配置文件
#
# Copyright (C) 2026 SPHARX. All Rights Reserved.
# SPDX-FileCopyrightText: 2026 SPHARX.
# SPDX-License-Identifier: Apache-2.0
#
# 本文件提供 AgentOS 整体项目的统一构建配置，支持：
# 1. 多模块依赖管理
# 2. 跨平台构建（Windows/Linux/macOS）
# 3. 统一版本管理和发布
# 4. 完整的安全编译选项
# 5. 集成测试和代码质量检查
# ========================================================================

cmake_minimum_required(VERSION 3.20)
project(AgentOS
    VERSION 1.0.0.7
    DESCRIPTION "AgentOS - 全球首个专为智能体协作设计的操作系统"
    HOMEPAGE_URL "https://atomgit.com/spharx/agentos"
    LANGUAGES C CXX
)

# ========================================================================
# 包管理器集成配置 (vcpkg/conan)
# ========================================================================

option(USE_VCPKG "Use vcpkg for dependency management" ON)
option(USE_CONAN "Use conan for dependency management" OFF)

# vcpkg 集成
if(USE_VCPKG)
    # 尝试自动检测 vcpkg 工具链文件
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE FILEPATH "Vcpkg toolchain file")
        message(STATUS "Using vcpkg from: $ENV{VCPKG_ROOT}")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg.json")
        # 如果存在 vcpkg.json，提示用户设置 VCPKG_ROOT
        message(STATUS "vcpkg.json found. To use vcpkg, set VCPKG_ROOT environment variable")
    endif()
endif()

# conan 集成
if(USE_CONAN)
    # 预留 conan 集成
    message(STATUS "Conan support is not yet implemented")
endif()

# ========================================================================
# 构建选项配置
# ========================================================================

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug|Release|RelWithDebInfo|MinSizeRel)" FORCE)
endif()

# 库构建选项
option(BUILD_SHARED_LIBS "Build shared libraries instead of static libraries" ON)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_EXAMPLES "Build example applications" ON)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

# 模块构建选项（可选择性构建）
option(BUILD_ATOMS "Build atoms core modules (required)" ON)
option(BUILD_COMMONS "Build commons utility library (required)" ON)
option(BUILD_CUPOLAS "Build Cupolas security framework" ON)
option(BUILD_HEAPSTORE "Build heapstore memory management" OFF)
option(BUILD_GATEWAY "Build gateway protocol translation layer" OFF)
option(BUILD_DAEMON "Build daemon services" ON)
option(BUILD_MANAGER "Build configuration manager" OFF)
option(BUILD_OPENLAB "Build openlab examples and demos" OFF)
option(BUILD_TOOLKIT "Build development toolkit" OFF)

# ========================================================================
# 编译器和语言标准配置
# ========================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ========================================================================
# 全局安全编译选项（基于AgentOS C编码风格指南附录A）
# ========================================================================

if(MSVC)
    # MSVC安全选项
    add_compile_options(/W4)  # 最高警告级别
    if(WARNINGS_AS_ERRORS)
        add_compile_options(/WX)  # 警告视为错误
    endif()
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/GS)      # 缓冲区安全检查
    add_compile_options(/sdl)     # 启用额外安全检查
    add_compile_options(/guard:cf) # 控制流防护
    add_compile_options(/utf-8)   # 强制UTF-8源文件编码
    # 禁用一些过于严格的编码相关警告
    add_compile_options(/wd4828)  # 禁用"文件包含在当前源字符集中无效的字符"警告
else()
    # GCC/Clang安全选项
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
    add_compile_options(-fstack-protector-strong)
    add_compile_definitions(-D_FORTIFY_SOURCE=2)
    add_compile_options(-fPIE)
    add_compile_options(-Wformat -Wformat-security)
    add_compile_options(-fno-omit-frame-pointer)
endif()

# ========================================================================
# 调试符号配置（由构建类型控制）
# ========================================================================

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(MSVC)
        add_compile_options(/Zi)  # 调试信息
        add_link_options(/DEBUG)
    else()
        add_compile_options(-g)
    endif()
endif()

# ========================================================================
# 链接器安全选项
# ========================================================================

if(MSVC)
    # Windows链接器安全选项
    add_link_options(/DYNAMICBASE)      # ASLR支持
    add_link_options(/NXCOMPAT)         # 数据执行保护
    add_link_options(/GUARD:CF)         # 控制流防护
elseif(UNIX AND NOT APPLE)
    # Linux安全选项
    add_link_options(-Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    # macOS安全选项
    add_link_options(-Wl,-dead_strip -pie)
endif()

# ========================================================================
# 发布构建优化选项
# ========================================================================

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)  # 链接时优化

# ========================================================================
# 全局定义和版本信息
# ========================================================================

add_definitions(
    -D_GNU_SOURCE
    -DAGENTOS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    -DAGENTOS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    -DAGENTOS_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    -DAGENTOS_VERSION_TWEAK=${PROJECT_VERSION_TWEAK}
    -DAGENTOS_VERSION="${PROJECT_VERSION}"
)

# ========================================================================
# 代码覆盖率支持
# ========================================================================

if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # 添加覆盖率编译和链接标志
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs -ftest-coverage -lgcov)

        # 确保调试符号开启
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")

        message(STATUS "Code coverage enabled for GCC/Clang")
    elseif(MSVC)
        message(WARNING "Code coverage for MSVC requires Visual Studio professional edition")
        # MSVC覆盖率选项（需要专业版）
        add_compile_options(/DEBUG)
    else()
        message(WARNING "Code coverage only supported for GCC/Clang and MSVC compilers")
    endif()
endif()

# ========================================================================
# 系统依赖查找和配置
# ========================================================================

# CMake包配置支持
include(CMakePackageConfigHelpers)

# 线程库支持
if(WIN32)
    message(STATUS "Using Windows built-in thread support")
    add_definitions(-DAGENTOS_HAS_THREADS=1)
else()
    find_package(Threads REQUIRED)
    message(STATUS "Threads found: ${CMAKE_THREAD_LIBS_INIT}")
endif()

# OpenSSL（加密和安全）
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OpenSSL_VERSION}")
    add_definitions(-DAGENTOS_HAS_OPENSSL=1)
else()
    message(WARNING "OpenSSL not found, some security features may be limited")
endif()

# SQLite3（向量存储和持久化）
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    message(STATUS "SQLite3 found: ${SQLite3_VERSION}")
    add_definitions(-DAGENTOS_HAS_SQLITE3=1)
else()
    message(WARNING "SQLite3 not found, vector store functionality may be limited")
endif()

# cJSON（JSON解析）
find_package(cJSON QUIET)
if(cJSON_FOUND)
    message(STATUS "cJSON found: ${cJSON_VERSION}")
    add_definitions(-DAGENTOS_HAS_CJSON=1)
else()
    message(WARNING "cJSON not found, JSON functionality may be limited")
endif()

# libmicrohttpd（HTTP服务器）
find_package(libmicrohttpd QUIET)
if(libmicrohttpd_FOUND)
    message(STATUS "libmicrohttpd found")
    add_definitions(-DAGENTOS_HAS_LIBMICROHTTPD=1)
else()
    message(WARNING "libmicrohttpd not found, HTTP gateway may be limited")
endif()

# libwebsockets（WebSocket支持）
find_package(libwebsockets QUIET)
if(libwebsockets_FOUND)
    message(STATUS "libwebsockets found")
    add_definitions(-DAGENTOS_HAS_LIBWEBSOCKETS=1)
else()
    message(WARNING "libwebsockets not found, WebSocket gateway may be limited")
endif()

# ========================================================================
# 测试框架支持
# ========================================================================

if(BUILD_TESTS)
    enable_testing()
    message(STATUS "Unit tests enabled")

    # 查找测试框架
    if(WIN32)
        # Windows平台：尝试查找Google Test
        find_package(GTest QUIET)
        if(GTest_FOUND)
            message(STATUS "Google Test found: ${GTest_VERSION}")
            add_definitions(-DAGENTOS_HAS_GTEST=1)
        else()
            # 尝试查找CUnit
            find_package(CUnit QUIET)
            if(CUNIT_FOUND)
                message(STATUS "CUnit found: ${CUNIT_VERSION}")
                add_definitions(-DAGENTOS_HAS_CUNIT=1)
            else()
                message(WARNING "No test framework found on Windows. Please install GTest via vcpkg: vcpkg install gtest")
            endif()
        endif()
    else()
        # Unix-like平台：使用CUnit
        find_package(CUnit QUIET)
        if(CUNIT_FOUND)
            message(STATUS "CUnit found: ${CUNIT_VERSION}")
            add_definitions(-DAGENTOS_HAS_CUNIT=1)
        else()
            message(WARNING "CUnit not found, some tests may be disabled")
        endif()
    endif()
endif()

# ========================================================================
# 项目信息输出
# ========================================================================

message(STATUS "")
message(STATUS "=========================================================")
message(STATUS "AgentOS ${PROJECT_VERSION} - 智能体操作系统")
message(STATUS "=========================================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Source directory: ${CMAKE_SOURCE_DIR}")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "Build options:")
message(STATUS "  BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
message(STATUS "  BUILD_TESTS: ${BUILD_TESTS}")
message(STATUS "  BUILD_EXAMPLES: ${BUILD_EXAMPLES}")
message(STATUS "  BUILD_DOCS: ${BUILD_DOCS}")
message(STATUS "  ENABLE_COVERAGE: ${ENABLE_COVERAGE}")
message(STATUS "")
message(STATUS "Module options:")
message(STATUS "  BUILD_ATOMS: ${BUILD_ATOMS}")
message(STATUS "  BUILD_COMMONS: ${BUILD_COMMONS}")
message(STATUS "  BUILD_CUPOLAS: ${BUILD_CUPOLAS}")
message(STATUS "  BUILD_HEAPSTORE: ${BUILD_HEAPSTORE}")
message(STATUS "  BUILD_GATEWAY: ${BUILD_GATEWAY}")
message(STATUS "  BUILD_DAEMON: ${BUILD_DAEMON}")
message(STATUS "  BUILD_MANAGER: ${BUILD_MANAGER}")
message(STATUS "  BUILD_OPENLAB: ${BUILD_OPENLAB}")
message(STATUS "  BUILD_TOOLKIT: ${BUILD_TOOLKIT}")
message(STATUS "=========================================================")
message(STATUS "")

# ========================================================================
# 模块构建 - 按依赖顺序添加子目录
# ========================================================================

# 1. 基础工具库（最低依赖）
if(BUILD_COMMONS)
    message(STATUS "Building commons utility library...")
    add_subdirectory(agentos/commons)
else()
    message(STATUS "Skipping commons utility library (disabled)")
endif()

# 2. 原子核心模块（微内核、认知循环、记忆系统、系统调用）
if(BUILD_ATOMS)
    message(STATUS "Building atoms core modules...")
    add_subdirectory(agentos/atoms)
else()
    message(STATUS "Skipping atoms core modules (disabled)")
endif()

# 3. 安全穹顶框架
if(BUILD_CUPOLAS)
    message(STATUS "Building Cupolas security framework...")
    add_subdirectory(agentos/cupolas)
else()
    message(STATUS "Skipping Cupolas security framework (disabled)")
endif()

# 4. 堆存储内存管理
if(BUILD_HEAPSTORE)
    message(STATUS "Building heapstore memory management...")
    add_subdirectory(agentos/heapstore)
else()
    message(STATUS "Skipping heapstore memory management (disabled)")
endif()

# 5. 配置管理器
if(BUILD_MANAGER)
    message(STATUS "Building configuration manager...")
    add_subdirectory(agentos/manager)
else()
    message(STATUS "Skipping configuration manager (disabled)")
endif()

# 6. 网关协议转换层
if(BUILD_GATEWAY)
    message(STATUS "Building gateway protocol translation layer...")
    add_subdirectory(agentos/gateway)
else()
    message(STATUS "Skipping gateway protocol translation layer (disabled)")
endif()

# 7. 守护进程服务
if(BUILD_DAEMON)
    message(STATUS "Building daemon services...")
    add_subdirectory(agentos/daemon)
else()
    message(STATUS "Skipping daemon services (disabled)")
endif()

# 8. 开发工具包
if(BUILD_TOOLKIT)
    message(STATUS "Building development toolkit...")
    add_subdirectory(agentos/toolkit)
else()
    message(STATUS "Skipping development toolkit (disabled)")
endif()

# 9. 开放实验室示例
if(BUILD_OPENLAB)
    message(STATUS "Building openlab examples and demos...")
    add_subdirectory(agentos/openlab)
else()
    message(STATUS "Skipping openlab examples and demos (disabled)")
endif()

# ========================================================================
# 全局目标定义和配置
# ========================================================================

# 创建全局接口库，提供统一的包含路径
add_library(agentos_global INTERFACE)

# 设置全局包含目录
target_include_directories(agentos_global INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/agentos>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/agentos/commons>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/agentos/atoms>
)

# 设置全局编译定义
target_compile_definitions(agentos_global INTERFACE
    AGENTOS_VERSION="${PROJECT_VERSION}"
)

# ========================================================================
# 安装配置
# ========================================================================

# 安装全局头文件
install(DIRECTORY agentos/commons/ DESTINATION include/agentos/commons
    FILES_MATCHING PATTERN "*.h"
    PATTERN "CMake*" EXCLUDE
    PATTERN "*.cmake" EXCLUDE
)

install(DIRECTORY agentos/atoms/ DESTINATION include/agentos/atoms
    FILES_MATCHING PATTERN "*.h"
    PATTERN "CMake*" EXCLUDE
    PATTERN "*.cmake" EXCLUDE
)

# 创建包配置文件
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/AgentOSConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/AgentOSConfigVersion.cmake
    DESTINATION lib/cmake/AgentOS
)

# ========================================================================
# 后构建配置和工具
# ========================================================================

# 代码格式化目标
find_program(CLANG_FORMAT "clang-format")
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=file
            ${CMAKE_SOURCE_DIR}/agentos/**/*.c
            ${CMAKE_SOURCE_DIR}/agentos/**/*.h
            ${CMAKE_SOURCE_DIR}/agentos/**/*.cpp
            ${CMAKE_SOURCE_DIR}/agentos/**/*.hpp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting all AgentOS source code with clang-format"
    )
endif()

# 静态分析目标
find_program(CPPCHECK "cppcheck")
if(CPPCHECK)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK} --enable=all --std=c11
            --suppress=missingIncludeSystem
            --language=c
            --output-file=${CMAKE_BINARY_DIR}/cppcheck-report.txt
            ${CMAKE_SOURCE_DIR}/agentos
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis on AgentOS code"
    )
endif()

# 生成编译数据库（用于IDE和工具）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ========================================================================
# 打包配置
# ========================================================================

set(CPACK_PACKAGE_NAME "AgentOS")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "AgentOS - 全球首个专为智能体协作设计的操作系统")
set(CPACK_PACKAGE_VENDOR "SPHARX")
set(CPACK_PACKAGE_CONTACT "contact@spharx.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
endif()

include(CPack)

# ========================================================================
# 构建完成消息
# ========================================================================

message(STATUS "")
message(STATUS "=========================================================")
message(STATUS "AgentOS ${PROJECT_VERSION} 配置完成！")
message(STATUS "=========================================================")
message(STATUS "")
message(STATUS "构建命令:")
message(STATUS "  cmake --build ${CMAKE_BINARY_DIR} --config ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "安装命令:")
message(STATUS "  cmake --install ${CMAKE_BINARY_DIR} --config ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "测试命令:")
if(BUILD_TESTS)
    message(STATUS "  ctest --test-dir ${CMAKE_BINARY_DIR} -C ${CMAKE_BUILD_TYPE} -V")
else()
    message(STATUS "  (测试未启用，请设置 BUILD_TESTS=ON)")
endif()
message(STATUS "")
message(STATUS "格式化和静态分析:")
if(CLANG_FORMAT)
    message(STATUS "  cmake --build ${CMAKE_BINARY_DIR} --target format")
else()
    message(STATUS "  (clang-format 未找到)")
endif()
if(CPPCHECK)
    message(STATUS "  cmake --build ${CMAKE_BINARY_DIR} --target cppcheck")
else()
    message(STATUS "  (cppcheck 未找到)")
endif()
message(STATUS "=========================================================")
