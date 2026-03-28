# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Market 服务构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-market-d VERSION 3.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CJSON REQUIRED libcjson)
pkg_check_modules(YAML REQUIRED yaml-0.1)

# 公共库路�?
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bases/include)

# Market 服务源文�?
set(MARKET_D_SOURCES
    src/main.c
    src/agent_registry.c
    src/skill_registry.c
    src/installer.c
    src/publisher.c
)

# 创建可执行文�?
add_executable(agentos-market-d ${MARKET_D_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(agentos-market-d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(agentos-market-d PRIVATE
        ws2_32
        bcrypt
    )
elseif(UNIX)
    target_link_libraries(agentos-market-d PRIVATE
        pthread
        rt
    )
endif()

# 包含目录
target_include_directories(agentos-market-d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${COMMON_INCLUDE_DIR}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
)

# 链接�?
target_link_libraries(agentos-market-d PRIVATE
    Threads::Threads
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
    svc_common
)

# 编译选项
target_compile_options(agentos-market-d PRIVATE
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
    add_executable(test_market test/test_market.c)
    target_include_directories(test_market PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${COMMON_INCLUDE_DIR}
    )
    target_link_libraries(test_market PRIVATE
        agentos-market-d
        Threads::Threads
    )
    add_test(NAME test_market COMMAND test_market)
endif()

# 安装规则
install(TARGETS agentos-market-d DESTINATION bin)
