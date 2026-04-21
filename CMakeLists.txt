# AgentOS - 智能体操作系统
# 根目录构建配置文件
# Copyright (c) 2026 SPHARX. All Rights Reserved.

cmake_minimum_required(VERSION 3.20)
project(AgentOS VERSION 1.0.0 LANGUAGES C CXX)

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug|Release|RelWithDebInfo|MinSizeRel)" FORCE)
endif()

# 项目范围选项
option(BUILD_SHARED_LIBS "Build shared libraries instead of static libraries" ON)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)  # 暂时关闭，待解决现有警告后启用
option(BUILD_TESTS "Build unit tests" OFF)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(AGENTOS_BUILD_ALL "Build all AgentOS components" ON)

# 输出构建配置信息
message(STATUS "=========================================")
message(STATUS "  AgentOS Build Configuration")
message(STATUS "=========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "Build shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "=========================================")

# 导出编译命令数据库（供clangd/IDE使用）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 全局编译选项
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 平台特定的安全编译选项
if(MSVC)
    # MSVC安全选项
    add_compile_options(/W4 /WX- /permissive-)  # 最高警告级别，但警告不视为错误
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    add_compile_options(/GS /sdl- /guard:cf /utf-8)  # 安全选项

    # 禁用特定警告
    add_compile_options(/wd4828)  # 文件包含在当前源字符集中无效的字符
    add_compile_options(/wd4996)  # 不安全函数警告（通过_CRT_SECURE_NO_WARNINGS处理）
    add_compile_options(/wd4255)  # 无函数原型
    add_compile_options(/wd4668)  # 未定义的宏替换为0

    # 链接器安全选项
    add_link_options(/DYNAMICBASE /NXCOMPAT /GUARD:CF)
else()
    # GCC/Clang安全选项
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
    if(WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
    add_compile_options(-fstack-protector-strong -fPIE -fno-omit-frame-pointer)
    add_compile_definitions(-D_FORTIFY_SOURCE=2)

    # 链接器安全选项
    if(UNIX AND NOT APPLE)
        add_link_options(-Wl,-z,relro -Wl,-z,now -pie)
    elseif(APPLE)
        add_link_options(-Wl,-dead_strip -pie)
    endif()
endif()

# Sanitizer支持（仅GCC/Clang）
if(ENABLE_SANITIZERS AND NOT MSVC)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
        message(STATUS "Sanitizers enabled (AddressSanitizer, UndefinedBehaviorSanitizer)")
    else()
        message(WARNING "Sanitizers only supported for GCC/Clang compilers")
    endif()
endif()

# MSVC Release模式特殊处理：解决/O2与/RTC1冲突
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    # 移除默认的/RTC1标志（如果存在），因为与/O2不兼容
    string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    message(STATUS "Adjusted MSVC Release flags to avoid /O2 and /RTC1 conflict")
endif()

# 调试符号（由构建类型控制）
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(MSVC)
        add_compile_options(/Zi /Ob0 /Od /RTC1)  # 调试信息，禁用优化，运行时检查
        add_link_options(/DEBUG)
    else()
        add_compile_options(-g -O0 -fno-inline)  # 调试信息，无优化，禁用内联
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        add_compile_options(/O2 /Ob2 /Oi /Ot /GL)  # 优化选项
        add_link_options(/LTCG)  # 链接时代码生成
    else()
        add_compile_options(-O3 -flto -DNDEBUG)  # 优化，链接时优化，禁用断言
        add_link_options(-flto)
    endif()
endif()

# 代码覆盖率支持
if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs -ftest-coverage -lgcov)
        message(STATUS "Code coverage enabled for ${CMAKE_C_COMPILER_ID}")
    elseif(MSVC)
        message(WARNING "Code coverage for MSVC requires Visual Studio professional edition")
    else()
        message(WARNING "Code coverage only supported for GCC/Clang and MSVC compilers")
    endif()
endif()

# 查找必需的系统依赖
if(WIN32)
    # Windows平台线程支持
    message(STATUS "Using Windows built-in thread support")
else()
    find_package(Threads REQUIRED)
    message(STATUS "Threads found: ${CMAKE_THREAD_LIBS_INIT}")
endif()

# 包含子目录
if(AGENTOS_BUILD_ALL)
    message(STATUS "Building AgentOS core components...")

    # 全局include路径（确保所有模块都能找到commons的头文件）
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/commons
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/commons/utils/include
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/commons/utils/memory/include
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/commons/utils/string/include
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/commons/utils/logging/include
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/commons/utils/error/include
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/commons/platform/include
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/atoms/corekern/include
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/protocols/include
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/daemon/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/agentos/gateway/src
    )

    # 主要AgentOS组件
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/agentos")
        add_subdirectory(agentos)
    else()
        message(WARNING "agentos directory not found, core components will not be built")
    endif()

    # 测试组件（可选）
    if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()

# 安装配置
if(AGENTOS_BUILD_ALL)
    # 创建安装目标
    install(DIRECTORY docs/ DESTINATION share/doc/agentos
            PATTERN ".gitignore" EXCLUDE
            PATTERN "*.pyc" EXCLUDE)

    # 安装许可证文件
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
        install(FILES LICENSE DESTINATION share/licenses/agentos)
    endif()

    # 安装README文件
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
        install(FILES README.md DESTINATION share/doc/agentos)
    endif()
endif()

# 包生成支持
include(InstallRequiredSystemLibraries)
set(CPACK_SET_DESTDIR ON)
set(CPACK_PACKAGE_NAME "AgentOS")
set(CPACK_PACKAGE_VENDOR "SPHARX")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Intelligent Agent Operating System")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
endif()

include(CPack)

# 自定义目标：清理构建
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/_deps
    COMMENT "Cleaning all build artifacts"
)

# 构建后信息
message(STATUS "")
message(STATUS "AgentOS configured successfully!")
message(STATUS "  Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Next steps:")
message(STATUS "  cmake --build . --config Release   # Build the project")
message(STATUS "  cmake --install . --config Release # Install the project")
message(STATUS "  ctest --output-on-failure          # Run tests (if built)")
message(STATUS "")

# 检查常见问题
if(MSVC AND CMAKE_C_STANDARD EQUAL 11)
    message(STATUS "Note: MSVC may have limited C11 support. Consider using Visual Studio 2019 or later.")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/agentos/atoms/CMakeLists.txt")
    message(WARNING "agentos/atoms/CMakeLists.txt not found - atoms module may not build correctly")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/agentos/commons/CMakeLists.txt")
    message(WARNING "agentos/commons/CMakeLists.txt not found - commons module may not build correctly")
endif()
