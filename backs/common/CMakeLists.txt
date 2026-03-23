# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# backs 公共库构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-svc-common VERSION 3.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(YAML REQUIRED yaml-0.1)
pkg_check_modules(CJSON REQUIRED libcjson)

# 公共库源文件
set(SVC_COMMON_SOURCES
    src/ipc_client.c
    src/config.c
    src/logger.c
    src/platform.c
    src/error.c
)

# 创建静态库
add_library(svc_common STATIC ${SVC_COMMON_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(svc_common PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(svc_common PRIVATE
        ws2_32
        bcrypt
        advapi32
    )
elseif(APPLE)
    target_compile_definitions(svc_common PRIVATE
        AGENTOS_PLATFORM_MACOS=1
    )
elseif(UNIX)
    target_compile_definitions(svc_common PRIVATE
        AGENTOS_PLATFORM_LINUX=1
    )
    target_link_libraries(svc_common PRIVATE
        pthread
        rt
    )
endif()

# 包含目录
target_include_directories(svc_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/agentos>
)

target_include_directories(svc_common PRIVATE
    ${LIBCURL_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(svc_common PUBLIC
    ${LIBCURL_LIBRARIES}
    ${YAML_LIBRARIES}
    ${CJSON_LIBRARIES}
)

# 编译选项
target_compile_options(svc_common PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# 导出符号
set_target_properties(svc_common PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 3
    POSITION_INDEPENDENT_CODE ON
)

# 安装规则
install(TARGETS svc_common
    EXPORT agentos-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/platform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/error.h
    DESTINATION include/agentos
)

# 导出配置
install(EXPORT agentos-targets
    FILE agentos-targets.cmake
    NAMESPACE agentos::
    DESTINATION lib/cmake/agentos
)
