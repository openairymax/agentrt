# Copyright (c) 2026 SPHARX. All Rights Reserved.
# Plugin 守护进程构建文件

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_COVERAGE "Build with code coverage" OFF)

# ==================== 路径设置 ====================
set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})
set(CUPOLAS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../cupolas)

# Plugin 服务源文件
set(PLUGIN_D_SOURCES
    src/main.c
    src/service.c
    src/plugin_permission.c
    src/plugin_discovery.c
)

add_executable(plugin_d ${PLUGIN_D_SOURCES})

target_compile_definitions(plugin_d PRIVATE AGENTRT_ENABLE_V2_API=1)

if(WIN32)
    target_compile_definitions(plugin_d PRIVATE
        AGENTRT_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(plugin_d PRIVATE agentrt_common ws2_32 bcrypt)
elseif(UNIX)
    target_link_libraries(plugin_d PRIVATE
        agentrt_common Threads::Threads agentrt_platform_libs dl)
endif()

target_include_directories(plugin_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DAEMON_COMMON_INCLUDE_DIR}
    ${CUPOLAS_DIR}/include
    ${COMMONS_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}/platform/include
    ${COMMONS_INCLUDE_DIR}/utils/error/include
    ${COMMONS_INCLUDE_DIR}/utils/logging/include
    ${COMMONS_INCLUDE_DIR}/utils/config_unified/include
    ${COMMONS_INCLUDE_DIR}/utils/memory/include
    ${COMMONS_INCLUDE_DIR}/utils/sync/include
    ${YAML_INCLUDE_DIRS}
)

target_link_libraries(plugin_d PRIVATE
    svc_common cupolas Threads::Threads ${YAML_LIBRARIES}
)

if(BUILD_COVERAGE)
    target_compile_options(plugin_d PRIVATE
        -Wall -Wextra -Wpedantic -fprofile-arcs -ftest-coverage -g -O0)
    target_link_options(plugin_d PRIVATE --coverage)
else()
    target_compile_options(plugin_d PRIVATE
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

install(TARGETS plugin_d DESTINATION bin)
