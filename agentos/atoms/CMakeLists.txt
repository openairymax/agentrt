# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# AgentOS 内核顶层 CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(agentos-kernel VERSION 1.0.0.6 LANGUAGES C CXX)

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# 库构建选项
option(BUILD_SHARED_LIBS "Build shared libraries instead of static libraries" ON)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# 编译选项
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 全局安全编译选项（基于AgentOS C编码风格指南附录A）
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
    add_compile_options(/utf-8)   # 强制UTF-8源文件编码（解决C4819中文注释警告，同时设置source-charset和execution-charset）
    # 禁用一些过于严格的编码相关警告（源文件可能有混合编码的注释字符）
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

# 调试符号（由构建类型控制）
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(MSVC)
        add_compile_options(/Zi)  # 调试信息
        add_link_options(/DEBUG)
    else()
        add_compile_options(-g)
    endif()
endif()

# 链接器安全选项
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

# 发布构建的优化选项
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)  # 链接时优化

# 全局定义
add_definitions(-D_GNU_SOURCE -DAGENTOS_VERSION="${PROJECT_VERSION}")

# CMake包配置支持
include(CMakePackageConfigHelpers)

# 代码覆盖率支持
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
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

# 查找依赖
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OpenSSL_VERSION}")
    add_definitions(-DAGENTOS_HAS_OPENSSL=1)
else()
    message(WARNING "OpenSSL not found, some security features may be limited")
endif()
# 查找线程库（Windows平台使用原生线程支持）
if(WIN32)
    message(STATUS "Using Windows built-in thread support (no external Threads package needed)")
    add_definitions(-DAGENTOS_HAS_THREADS=1)
else()
    find_package(Threads REQUIRED)
    message(STATUS "Threads found: ${CMAKE_THREAD_LIBS_INIT}")
endif()

# libevent 是可选依赖（用于事件驱动模块）
if(WIN32)
    # Windows平台：尝试通过CMake查找或vcpkg
    find_package(libevent CONFIG QUIET)
    if(libevent_FOUND)
        message(STATUS "libevent found via CMake config: ${libevent_VERSION}")
        add_definitions(-DAGENTOS_HAS_LIBEVENT=1)
    else()
        # 尝试通过pkg-config（如果使用MSYS2或Cygwin）
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBEVENT QUIET libevent)
            if(LIBEVENT_FOUND)
                message(STATUS "libevent found via pkg-config: ${LIBEVENT_VERSION}")
                add_definitions(-DAGENTOS_HAS_LIBEVENT=1)
            else()
                message(WARNING "libevent not found on Windows. Event-driven features may be limited")
            endif()
        else()
            message(WARNING "libevent not found on Windows. Event-driven features may be limited")
        endif()
    endif()
else()
    # Unix-like平台：使用pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBEVENT QUIET libevent)
        if(LIBEVENT_FOUND)
            message(STATUS "libevent found: ${LIBEVENT_VERSION}")
            add_definitions(-DAGENTOS_HAS_LIBEVENT=1)
        else()
            message(WARNING "libevent not found on Unix-like platforms. Event-driven features may be limited")
        endif()
    else()
        message(WARNING "PkgConfig not found, cannot check for libevent. Event-driven features may be limited")
    endif()
endif()

# 可选依赖
# SQLite3（用于向量存储）
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    message(STATUS "SQLite3 found: ${SQLite3_VERSION}")
    add_definitions(-DAGENTOS_HAS_SQLITE3=1)
else()
    # 尝试通过pkg-config查找（Unix-like平台）
    if(PkgConfig_FOUND OR NOT WIN32)
        if(NOT PkgConfig_FOUND)
            find_package(PkgConfig QUIET)
        endif()
        if(PkgConfig_FOUND)
            pkg_check_modules(SQLITE3 QUIET sqlite3)
            if(SQLITE3_FOUND)
                message(STATUS "SQLite3 found via pkg-config: ${SQLITE3_VERSION}")
                add_definitions(-DAGENTOS_HAS_SQLITE3=1)
            else()
                message(WARNING "SQLite3 not found, vector store functionality may be limited")
            endif()
        else()
            message(WARNING "SQLite3 not found, vector store functionality may be limited")
        endif()
    else()
        message(WARNING "SQLite3 not found, vector store functionality may be limited")
    endif()
endif()

# cJSON（用于JSON解析）
find_package(cJSON QUIET)
if(cJSON_FOUND)
    message(STATUS "cJSON found: ${cJSON_VERSION}")
    add_definitions(-DAGENTOS_HAS_CJSON=1)
else()
    # 尝试通过pkg-config查找
    if(PkgConfig_FOUND OR NOT WIN32)
        if(NOT PkgConfig_FOUND)
            find_package(PkgConfig QUIET)
        endif()
        if(PkgConfig_FOUND)
            pkg_check_modules(CJSON QUIET libcjson)
            if(CJSON_FOUND)
                message(STATUS "cJSON found via pkg-config: ${CJSON_VERSION}")
                add_definitions(-DAGENTOS_HAS_CJSON=1)
            else()
                message(WARNING "cJSON not found, JSON functionality may be limited")
            endif()
        else()
            message(WARNING "cJSON not found, JSON functionality may be limited")
        endif()
    else()
        message(WARNING "cJSON not found, JSON functionality may be limited")
    endif()
endif()

# 测试支持
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    message(STATUS "Unit tests enabled")
    
    # 查找测试框架依赖
    if(WIN32)
        # Windows平台：尝试查找Google Test
        find_package(GTest QUIET)
        if(GTest_FOUND)
            message(STATUS "Google Test found: ${GTest_VERSION}")
            add_definitions(-DAGENTOS_HAS_GTEST=1)
        else()
            # 尝试查找CUnit（如果通过vcpkg安装）
            find_package(CUnit QUIET)
            if(CUNIT_FOUND)
                message(STATUS "CUnit found: ${CUNIT_VERSION}")
                add_definitions(-DAGENTOS_HAS_CUNIT=1)
            else()
                message(WARNING "No test framework found on Windows. Please install GTest via vcpkg: vcpkg install gtest")
                message(WARNING "Tests will be built but may not run properly without a test framework")
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
            # 尝试查找Check作为备选
            find_package(Check QUIET)
            if(Check_FOUND)
                message(STATUS "Check found: ${Check_VERSION}")
                add_definitions(-DAGENTOS_HAS_CHECK=1)
            endif()
        endif()
    endif()
endif()

# 统一的头文件路径管理
# 定义全局包含目录，避免使用相对路径
set(AGENTOS_INCLUDE_DIRS
    # atoms 内部模块头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/coreloopthree/include
    ${CMAKE_CURRENT_SOURCE_DIR}/memoryrovol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/syscall/include
    # commons 基础库头文件 (完整覆盖所有子模块)
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/config/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/config_unified/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/observability/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/cache/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/cost/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/string/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/token/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/network/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/security/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/resource/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/uuid/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/strategy/include
)

# 创建接口库来管理公共依赖
add_library(agentos_atoms INTERFACE)
target_include_directories(agentos_atoms INTERFACE
    $<BUILD_INTERFACE:${AGENTOS_INCLUDE_DIRS}>
    $<INSTALL_INTERFACE:include/agentos>
)

# 包含子目录
# Kernel subdirectories
add_subdirectory(corekern)
add_subdirectory(coreloopthree)
add_subdirectory(memoryrovol)
add_subdirectory(syscall)

# 安装配置
# 库文件安装
# install(TARGETS agentos_atoms agentos_core agentos_coreloopthree agentos_memoryrovol agentos_syscall agentos_cognition agentos_execution
#     EXPORT AgentOSTargets
#     ARCHIVE DESTINATION lib/agentos
#     LIBRARY DESTINATION lib/agentos
#     RUNTIME DESTINATION bin
# )

# 子模块头文件安装
install(DIRECTORY corekern/include/ DESTINATION include/agentos/corekern)
install(DIRECTORY coreloopthree/include/ DESTINATION include/agentos/coreloopthree)
install(DIRECTORY memoryrovol/include/ DESTINATION include/agentos/memoryrovol)
install(DIRECTORY syscall/include/ DESTINATION include/agentos/syscall)

# 创建目标导出集
# install(EXPORT AgentOSTargets
#     FILE AgentOSTargets.cmake
#     NAMESPACE AgentOS::
#     DESTINATION lib/cmake/AgentOS
# )

# 创建版本配置文件
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/AgentOSConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/AgentOSConfigVersion.cmake
    DESTINATION lib/cmake/AgentOS
)

# 文档安装（如果存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES README.md DESTINATION share/doc/agentos)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/BUILD.md")
    install(FILES BUILD.md DESTINATION share/doc/agentos)
endif()

# 许可证文件安装（如果存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    install(FILES LICENSE DESTINATION share/licenses/agentos)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
    install(FILES LICENSE.txt DESTINATION share/licenses/agentos)
endif()
