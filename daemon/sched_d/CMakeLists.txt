# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Scheduler 服务构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-sched-d VERSION 3.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CJSON REQUIRED libcjson)

# 公共库路�?
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../commons/include)

# Scheduler 服务源文�?
set(SCHED_D_SOURCES
    src/main.c
    src/monitor.c
    src/strategies/weighted.c
    src/strategies/round_robin.c
    src/strategies/ml_based.c
)

# 创建可执行文�?
add_executable(agentos-sched-d ${SCHED_D_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(agentos-sched-d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(agentos-sched-d PRIVATE
        ws2_32
        bcrypt
    )
elseif(UNIX)
    target_link_libraries(agentos-sched-d PRIVATE
        pthread
        rt
    )
endif()

# 包含目录
target_include_directories(agentos-sched-d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${COMMON_INCLUDE_DIR}
    ${CJSON_INCLUDE_DIRS}
)

# 链接�?
target_link_libraries(agentos-sched-d PRIVATE
    Threads::Threads
    ${CJSON_LIBRARIES}
    svc_common
)

# 编译选项
target_compile_options(agentos-sched-d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<manager:Debug>:-g -O0>
    $<$<manager:Release>:-O3 -DNDEBUG>
)

# 测试目标
if(BUILD_TESTS)
    add_executable(test_scheduler test/test_scheduler.c)
    target_include_directories(test_scheduler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${COMMON_INCLUDE_DIR}
    )
    target_link_libraries(test_scheduler PRIVATE
        agentos-sched-d
        Threads::Threads
    )
    add_test(NAME test_scheduler COMMAND test_scheduler)
endif()

# 安装规则
install(TARGETS agentos-sched-d DESTINATION bin)
