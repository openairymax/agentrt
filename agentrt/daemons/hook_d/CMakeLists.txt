# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Hook 守护进程构建文件

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_COVERAGE "Build with code coverage" OFF)

# ==================== 路径设置 ====================
set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

# ==================== Hook 服务静态库 ====================
# 提取 service/hook_registry/hook_executor/hook_interceptor/hook_timeout
# 为独立静态库，供 hook_d 可执行文件和集成测试共用，消除桩函数依赖。
# 排除 main.c（含 main()）。
set(HOOK_SERVICE_SOURCES
    src/service.c
    src/hook_registry.c
    src/hook_executor.c
    src/hook_interceptor.c
    src/hook_timeout.c
    # P0.20.1: 生产 Hook 处理器 — 审计/metrics/trace 内置注册
    src/hook_builtin_handlers.c
    src/hook_audit_handler.c
    src/hook_metrics_handler.c
    src/hook_trace_handler.c
)

add_library(agentrt_hook_service STATIC ${HOOK_SERVICE_SOURCES})

target_compile_definitions(agentrt_hook_service PRIVATE AGENTRT_ENABLE_V2_API=1)

if(WIN32)
    target_compile_definitions(agentrt_hook_service PRIVATE
        AGENTRT_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
elseif(UNIX)
    target_compile_definitions(agentrt_hook_service PRIVATE
        AGENTRT_PLATFORM_LINUX=1
        _GNU_SOURCE
    )
endif()

target_include_directories(agentrt_hook_service PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DAEMON_COMMON_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}/platform/include
    ${COMMONS_INCLUDE_DIR}/utils/error/include
    ${COMMONS_INCLUDE_DIR}/utils/logging/include
    ${COMMONS_INCLUDE_DIR}/utils/config_unified/include
    ${COMMONS_INCLUDE_DIR}/utils/memory/include
    ${COMMONS_INCLUDE_DIR}/utils/sync/include
    ${COMMONS_INCLUDE_DIR}/utils/string/include
    ${COMMONS_INCLUDE_DIR}/utils/cache/include
    ${COMMONS_INCLUDE_DIR}/utils/compat/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cupolas/include
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
)

target_include_directories(agentrt_hook_service PRIVATE
    ${YAML_INCLUDE_DIRS}
)

# 链接库：svc_common 提供 daemon 公共层，cupolas 提供 SafetyGuard
target_link_libraries(agentrt_hook_service PUBLIC svc_common cupolas)

if(AGENTRT_HAS_YAML AND YAML_FOUND AND YAML_LIBRARIES)
    target_link_libraries(agentrt_hook_service PUBLIC ${YAML_LIBRARIES})
endif()

if(UNIX)
    target_link_libraries(agentrt_hook_service PUBLIC Threads::Threads agentrt_platform_libs)
endif()

# libcurl: 用于 hook_executor_run_webhook 的真实 HTTP POST（替代不安全的 system("curl ...")）
# 当 libcurl 可用时编译 AGENTRT_HAS_CURL=1，webhook 走进程内 libcurl；
# 不可用时回退到 fork+execvp("curl") 二进制（argv 数组传参，无 shell 注入风险）。
if(AGENTRT_HAS_CURL)
    target_compile_definitions(agentrt_hook_service PRIVATE AGENTRT_HAS_CURL=1)
    if(CURL_INCLUDE_DIRS)
        target_include_directories(agentrt_hook_service PRIVATE ${CURL_INCLUDE_DIRS})
    endif()
    if(CURL_LIBRARIES)
        target_link_libraries(agentrt_hook_service PUBLIC ${CURL_LIBRARIES})
    elseif(LIBCURL_LIBRARIES)
        target_link_libraries(agentrt_hook_service PUBLIC ${LIBCURL_LIBRARIES})
    elseif(PC_CURL_LIBRARIES)
        target_link_libraries(agentrt_hook_service PUBLIC ${PC_CURL_LIBRARIES})
    else()
        target_link_libraries(agentrt_hook_service PUBLIC curl)
    endif()
    agentrt_print_ok("  hook_d: libcurl enabled for webhook HTTP POST (BAN-211/235)")
else()
    agentrt_print_warn("  hook_d: libcurl not found, webhook falls back to curl binary via execvp")
endif()

# 编译选项
if(BUILD_COVERAGE)
    target_compile_options(agentrt_hook_service PRIVATE
        -Wall -Wextra -Wpedantic -fprofile-arcs -ftest-coverage -g -O0)
else()
    target_compile_options(agentrt_hook_service PRIVATE
        -Wall -Wextra -Wpedantic
        -Werror=implicit-function-declaration -Werror=return-type
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

# ==================== Hook daemon 可执行文件 ====================
# main.c 仅属于 daemon 可执行文件；服务代码已纳入 agentrt_hook_service 库
add_executable(hook_d
    src/main.c
)

target_compile_definitions(hook_d PRIVATE AGENTRT_ENABLE_V2_API=1)

if(WIN32)
    target_compile_definitions(hook_d PRIVATE
        AGENTRT_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(hook_d PRIVATE ws2_32 bcrypt)
elseif(UNIX)
    target_link_libraries(hook_d PRIVATE Threads::Threads agentrt_platform_libs)
endif()

target_include_directories(hook_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DAEMON_COMMON_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}/platform/include
    ${COMMONS_INCLUDE_DIR}/utils/error/include
    ${COMMONS_INCLUDE_DIR}/utils/logging/include
    ${COMMONS_INCLUDE_DIR}/utils/config_unified/include
    ${COMMONS_INCLUDE_DIR}/utils/memory/include
    ${COMMONS_INCLUDE_DIR}/utils/sync/include
    ${COMMONS_INCLUDE_DIR}/utils/string/include
    ${COMMONS_INCLUDE_DIR}/utils/cache/include
    ${COMMONS_INCLUDE_DIR}/utils/compat/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cupolas/include
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
    ${YAML_INCLUDE_DIRS}
)

# 链接策略：LTO + ASan 下使用 --start-group/--end-group 避免符号扫描遗漏
if(NOT MSVC AND NOT APPLE)
    target_link_libraries(hook_d PRIVATE
        "-Wl,--start-group"
        agentrt_hook_service
        "-Wl,--end-group"
        Threads::Threads
    )
else()
    target_link_libraries(hook_d PRIVATE agentrt_hook_service Threads::Threads)
endif()

if(BUILD_COVERAGE)
    target_compile_options(hook_d PRIVATE
        -Wall -Wextra -Wpedantic -fprofile-arcs -ftest-coverage -g -O0)
    target_link_options(hook_d PRIVATE --coverage)
else()
    target_compile_options(hook_d PRIVATE
        -Wall -Wextra -Wpedantic
        -Werror=implicit-function-declaration -Werror=return-type
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

if(BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()

install(TARGETS hook_d DESTINATION bin)
