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

# Hook 服务源文件（Phase 2 完整版）
set(HOOK_D_SOURCES
    src/main.c
    src/service.c
    src/hook_registry.c
    src/hook_executor.c
    src/hook_interceptor.c
    src/hook_timeout.c
)

add_executable(agentos-hook-d ${HOOK_D_SOURCES})

target_compile_definitions(agentos-hook-d PRIVATE AGENTOS_ENABLE_V2_API=1)

if(WIN32)
    target_compile_definitions(agentos-hook-d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(agentos-hook-d PRIVATE agentos_common ws2_32 bcrypt)
elseif(UNIX)
    target_link_libraries(agentos-hook-d PRIVATE
        agentos_common Threads::Threads agentos_platform_libs)
endif()

target_include_directories(agentos-hook-d PRIVATE
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cupolas/include
    ${YAML_INCLUDE_DIRS}
)

target_link_libraries(agentos-hook-d PRIVATE
    svc_common cupolas ${YAML_LIBRARIES}
)
if(NOT WIN32)
    target_link_libraries(agentos-hook-d PRIVATE Threads::Threads)
endif()

# libcurl: 用于 hook_executor_run_webhook 的真实 HTTP POST（替代不安全的 system("curl ...")）
# 当 libcurl 可用时编译 AGENTOS_HAS_CURL=1，webhook 走进程内 libcurl；
# 不可用时回退到 fork+execvp("curl") 二进制（argv 数组传参，无 shell 注入风险）。
if(AGENTRT_HAS_CURL)
    target_compile_definitions(agentos-hook-d PRIVATE AGENTOS_HAS_CURL=1)
    if(CURL_INCLUDE_DIRS)
        target_include_directories(agentos-hook-d PRIVATE ${CURL_INCLUDE_DIRS})
    endif()
    if(CURL_LIBRARIES)
        target_link_libraries(agentos-hook-d PRIVATE ${CURL_LIBRARIES})
    elseif(LIBCURL_LIBRARIES)
        target_link_libraries(agentos-hook-d PRIVATE ${LIBCURL_LIBRARIES})
    elseif(PC_CURL_LIBRARIES)
        target_link_libraries(agentos-hook-d PRIVATE ${PC_CURL_LIBRARIES})
    else()
        target_link_libraries(agentos-hook-d PRIVATE curl)
    endif()
    message(STATUS "  hook_d: libcurl enabled for webhook HTTP POST (BAN-211/235)")
else()
    message(STATUS "  hook_d: libcurl not found, webhook falls back to curl binary via execvp")
endif()

if(BUILD_COVERAGE)
    target_compile_options(agentos-hook-d PRIVATE
        -Wall -Wextra -Wpedantic -fprofile-arcs -ftest-coverage -g -O0)
    target_link_options(agentos-hook-d PRIVATE --coverage)
else()
    target_compile_options(agentos-hook-d PRIVATE
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

install(TARGETS agentos-hook-d DESTINATION bin)
