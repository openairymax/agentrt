# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Hook 守护进程构建文件
# SP04: hook 系统核心（9 个 .c + 7 个 .h）已迁移到 atoms/coreloopthree/src/hook/
#       和 atoms/coreloopthree/include/。hook_d 退化为仅含 main.c 的薄 daemon 壳，
#       hook 系统通过 link agentrt_coreloopthree 获取。

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_COVERAGE "Build with code coverage" OFF)

# ==================== 路径设置 ====================
set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

# ==================== Hook daemon 可执行文件 ====================
# SP04: hook 系统核心（hook_registry/executor/interceptor/timeout/handlers）
# 已迁移到 atoms/coreloopthree/src/hook/，通过 link agentrt_coreloopthree 获取。
# hook_d 仅保留 main.c（daemon 进程入口：socket + SD + IPC + cupolas 守护进程生命周期）。
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
    target_compile_definitions(hook_d PRIVATE
        AGENTRT_PLATFORM_LINUX=1
        _GNU_SOURCE
    )
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
# SP04: agentrt_hook_service 已删除，改为 link agentrt_coreloopthree（含 hook 系统核心）
if(NOT MSVC AND NOT APPLE)
    target_link_libraries(hook_d PRIVATE
        "-Wl,--start-group"
        agentrt_coreloopthree
        svc_common
        cupolas
        "-Wl,--end-group"
        Threads::Threads
    )
else()
    target_link_libraries(hook_d PRIVATE agentrt_coreloopthree svc_common cupolas Threads::Threads)
endif()

if(AGENTRT_HAS_YAML AND YAML_FOUND AND YAML_LIBRARIES)
    target_link_libraries(hook_d PRIVATE ${YAML_LIBRARIES})
endif()

if(AGENTRT_HAS_CURL)
    target_compile_definitions(hook_d PRIVATE AGENTRT_HAS_CURL=1)
    if(CURL_INCLUDE_DIRS)
        target_include_directories(hook_d PRIVATE ${CURL_INCLUDE_DIRS})
    endif()
    if(CURL_LIBRARIES)
        target_link_libraries(hook_d PRIVATE ${CURL_LIBRARIES})
    elseif(LIBCURL_LIBRARIES)
        target_link_libraries(hook_d PRIVATE ${LIBCURL_LIBRARIES})
    elseif(PC_CURL_LIBRARIES)
        target_link_libraries(hook_d PRIVATE ${PC_CURL_LIBRARIES})
    else()
        target_link_libraries(hook_d PRIVATE curl)
    endif()
endif()

# 编译选项
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
