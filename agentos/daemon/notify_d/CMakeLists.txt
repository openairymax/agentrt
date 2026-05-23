# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Notify 服务构建文件


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Threads found by root CMakeLists.txt

set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

set(NOTIFY_D_SOURCES
    src/main.c
)

add_executable(agentos-notify-d ${NOTIFY_D_SOURCES})

if(WIN32)
    target_compile_definitions(agentos-notify-d PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(agentos-notify-d PRIVATE
        agentos_common
        ws2_32
    )
elseif(UNIX)
    target_link_libraries(agentos-notify-d PRIVATE
        agentos_common
        Threads::Threads
        agentos_platform_libs
    )
endif()

target_include_directories(agentos-notify-d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DAEMON_COMMON_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}/platform/include
    ${COMMONS_INCLUDE_DIR}/utils/error/include
    ${COMMONS_INCLUDE_DIR}/utils/logging/include
    ${COMMONS_INCLUDE_DIR}/utils/memory/include
    ${COMMONS_INCLUDE_DIR}/utils/string/include
    ${COMMONS_INCLUDE_DIR}/utils/sync/include
    ${COMMONS_INCLUDE_DIR}/utils/network/include
    ${COMMONS_INCLUDE_DIR}/utils/ipc/include
    ${COMMONS_INCLUDE_DIR}/utils/compat/include
)

target_link_libraries(agentos-notify-d PRIVATE
    svc_common
    Threads::Threads
)

target_compile_options(agentos-notify-d PRIVATE
    -Wall
    -Wextra
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

install(TARGETS agentos-notify-d DESTINATION bin)
