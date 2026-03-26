# Copyright (c) 2026 SPHARX. All Rights Reserved.
# CoreLoopThree 主模块构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-coreloopthree VERSION 1.0.1 LANGUAGES C)

# 设置输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 符号导出管理
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# 定义构建共享库时的宏
if(BUILD_SHARED_LIBS)
    add_definitions(-DAGENTOS_BUILDING_SHARED)
endif()

# 跨平台编译选项
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# 安全编译选项（基于AgentOS C编码风格指南附录A）
if(MSVC)
    target_compile_options(agentos_coreloopthree PRIVATE /W4 /WX)
    target_compile_definitions(agentos_coreloopthree PRIVATE _CRT_SECURE_NO_WARNINGS)
    # MSVC安全选项
    target_compile_options(agentos_coreloopthree PRIVATE /GS)  # 缓冲区安全检查
    target_compile_options(agentos_coreloopthree PRIVATE /sdl) # 启用额外安全检查
    target_compile_options(agentos_coreloopthree PRIVATE /guard:cf) # 控制流防护
else()
    target_compile_options(agentos_coreloopthree PRIVATE -Wall -Wextra -Wpedantic -Werror)
    # GCC/Clang安全选项
    target_compile_options(agentos_coreloopthree PRIVATE -fstack-protector-strong)
    target_compile_options(agentos_coreloopthree PRIVATE -D_FORTIFY_SOURCE=2)
    target_compile_options(agentos_coreloopthree PRIVATE -fPIE)
    target_compile_options(agentos_coreloopthree PRIVATE -Wformat -Wformat-security)
    target_compile_options(agentos_coreloopthree PRIVATE -fno-omit-frame-pointer)
endif()

# 链接器安全选项
if(UNIX AND NOT APPLE)
    target_link_options(agentos_coreloopthree PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    target_link_options(agentos_coreloopthree PRIVATE -Wl,-dead_strip -pie)
endif()

# 包含头文件路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../memoryrovol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# 添加子目录
add_subdirectory(src/cognition)
add_subdirectory(src/execution)
add_subdirectory(src/memory)

# 创建主库
add_library(agentos_coreloopthree STATIC
    src/loop.c
    src/config_loader.c
    src/utils/error_utils.c
    src/utils/id_utils.c
)

target_include_directories(agentos_coreloopthree PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(agentos_coreloopthree
    agentos_cognition
    agentos_execution
    agentos_memory
    agentos_core
)

# 跨平台链接库
if(WIN32)
    target_link_libraries(agentos_coreloopthree PRIVATE ws2_32)
endif()

# 安装头文件
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

# 安装库
install(TARGETS agentos_coreloopthree
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)