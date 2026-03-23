# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Tool 服务构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-tool-d VERSION 3.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CJSON REQUIRED libcjson)
pkg_check_modules(YAML REQUIRED yaml-0.1)

# 公共库路径
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common/include)

# Tool 服务源文件
set(TOOL_D_SOURCES
    src/main.c
    src/service.c
    src/registry.c
    src/executor.c
    src/validator.c
    src/cache.c
    src/config.c
    src/utils/tool_helpers.c
)

# 创建可执行文件
add_executable(agentos-tool-d ${TOOL_D_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(agentos-tool-d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(agentos-tool-d PRIVATE
        ws2_32
        bcrypt
    )
elseif(UNIX)
    target_link_libraries(agentos-tool-d PRIVATE
        pthread
        rt
    )
endif()

# 包含目录
target_include_directories(agentos-tool-d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${COMMON_INCLUDE_DIR}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(agentos-tool-d PRIVATE
    Threads::Threads
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
    svc_common
)

# 编译选项
target_compile_options(agentos-tool-d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# 安装规则
install(TARGETS agentos-tool-d DESTINATION bin)
