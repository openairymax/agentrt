# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Monitor 服务构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-monit-d VERSION 3.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CJSON REQUIRED libcjson)

# 公共库路径
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common/include)

# Monitor 服务源文件
set(MONIT_D_SOURCES
    src/main.c
    src/metrics.c
    src/logging.c
    src/tracing.c
    src/alert.c
)

# 创建可执行文件
add_executable(agentos-monit-d ${MONIT_D_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(agentos-monit-d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(agentos-monit-d PRIVATE
        ws2_32
        bcrypt
    )
elseif(UNIX)
    target_link_libraries(agentos-monit-d PRIVATE
        pthread
        rt
    )
endif()

# 包含目录
target_include_directories(agentos-monit-d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${COMMON_INCLUDE_DIR}
    ${CJSON_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(agentos-monit-d PRIVATE
    Threads::Threads
    ${CJSON_LIBRARIES}
    svc_common
)

# 编译选项
target_compile_options(agentos-monit-d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# 测试目标
if(BUILD_TESTS)
    add_executable(test_monitor test/test_monitor.c)
    target_include_directories(test_monitor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${COMMON_INCLUDE_DIR}
    )
    target_link_libraries(test_monitor PRIVATE
        agentos-monit-d
        Threads::Threads
    )
    add_test(NAME test_monitor COMMAND test_monitor)
endif()

# 安装规则
install(TARGETS agentos-monit-d DESTINATION bin)
