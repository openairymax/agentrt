# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Monitor 服务构建文件


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找依赖
# Threads found by root CMakeLists.txt
# PkgConfig found by root CMakeLists.txt
# cJSON found by root CMakeLists.txt (AGENTRT_HAS_CJSON)
if(NOT AGENTRT_HAS_CJSON)
    agentrt_print_warn("libcjson not found, JSON features may be limited")
endif()

# ==================== 路径设置 ====================
# agentrt/daemons/common 兼容层路径
set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

# commons 统一基础库路径
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

# ==================== Monitor 服务静态库 ====================
# 提取 service/metrics/prometheus_exporter/logging/tracing/alert/monit_svc_adapter
# 为独立静态库，供 monit_d 可执行文件和集成测试共用，消除桩函数依赖。
# 排除 main.c（含 main()）。
set(MONIT_SERVICE_SOURCES
    src/service.c
    src/metrics.c
    src/prometheus_exporter.c
    src/logging.c
    src/tracing.c
    src/alert.c
    src/monit_svc_adapter.c
)

add_library(agentrt_monit_service STATIC ${MONIT_SERVICE_SOURCES})

# 平台特定配置
if(WIN32)
    target_compile_definitions(agentrt_monit_service PRIVATE
        AGENTRT_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
elseif(UNIX)
    target_compile_definitions(agentrt_monit_service PRIVATE
        AGENTRT_PLATFORM_LINUX=1
        _GNU_SOURCE
    )
endif()

# 包含目录 - 兼容层和 commons
target_include_directories(agentrt_monit_service PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DAEMON_COMMON_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}/platform/include
    ${COMMONS_INCLUDE_DIR}/utils/error/include
    ${COMMONS_INCLUDE_DIR}/utils/logging/include
    ${COMMONS_INCLUDE_DIR}/utils/config_unified/include
    ${COMMONS_INCLUDE_DIR}/utils/memory/include
    ${COMMONS_INCLUDE_DIR}/utils/string/include
    ${COMMONS_INCLUDE_DIR}/utils/sync/include
    ${COMMONS_INCLUDE_DIR}/utils/cache/include
    ${COMMONS_INCLUDE_DIR}/utils/compat/include
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
)

target_include_directories(agentrt_monit_service PRIVATE
    ${CJSON_INCLUDE_DIRS}
)

# 链接库：svc_common 提供 daemon 公共层 + commons 传递依赖
target_link_libraries(agentrt_monit_service PUBLIC svc_common)

if(AGENTRT_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(agentrt_monit_service PUBLIC cjson)
    elseif(CJSON_FOUND AND CJSON_LIBRARIES)
        target_link_libraries(agentrt_monit_service PUBLIC ${CJSON_LIBRARIES})
    endif()
endif()

if(UNIX)
    target_link_libraries(agentrt_monit_service PUBLIC Threads::Threads agentrt_platform_libs)
endif()

# 编译选项
target_compile_options(agentrt_monit_service PRIVATE
    -Wall -Wextra -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# ==================== Monitor daemon 可执行文件 ====================
# main.c 仅属于 daemon 可执行文件；服务代码已纳入 agentrt_monit_service 库
add_executable(monit_d
    src/main.c
)

if(WIN32)
    target_compile_definitions(monit_d PRIVATE
        AGENTRT_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(monit_d PRIVATE ws2_32 bcrypt)
elseif(UNIX)
    target_link_libraries(monit_d PRIVATE Threads::Threads agentrt_platform_libs)
endif()

target_include_directories(monit_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DAEMON_COMMON_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}/platform/include
    ${COMMONS_INCLUDE_DIR}/utils/error/include
    ${COMMONS_INCLUDE_DIR}/utils/logging/include
    ${COMMONS_INCLUDE_DIR}/utils/config_unified/include
    ${COMMONS_INCLUDE_DIR}/utils/memory/include
    ${COMMONS_INCLUDE_DIR}/utils/string/include
    ${COMMONS_INCLUDE_DIR}/utils/sync/include
    ${COMMONS_INCLUDE_DIR}/utils/cache/include
    ${COMMONS_INCLUDE_DIR}/utils/compat/include
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
    ${CJSON_INCLUDE_DIRS}
)

# 链接策略：LTO + ASan 下使用 --start-group/--end-group 避免符号扫描遗漏
if(NOT MSVC AND NOT APPLE)
    target_link_libraries(monit_d PRIVATE
        "-Wl,--start-group"
        agentrt_monit_service
        "-Wl,--end-group"
        Threads::Threads
    )
else()
    target_link_libraries(monit_d PRIVATE agentrt_monit_service Threads::Threads)
endif()

target_compile_options(monit_d PRIVATE
    -Wall -Wextra -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# 测试支持
if(BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()

# 安装规则
install(TARGETS monit_d DESTINATION bin)
