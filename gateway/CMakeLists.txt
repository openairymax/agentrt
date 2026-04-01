# AgentOS gateway Runtime - CMake 构建配置
# 
# Copyright (c) 2026 SPHARX. All Rights Reserved.

cmake_minimum_required(VERSION 3.16)

project(agentos_gateway
    VERSION 1.1.0
    LANGUAGES C
    DESCRIPTION "AgentOS gateway Runtime - Gateway Layer")

# C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译选项
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /D_CRT_SECURE_NO_WARNINGS")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# 查找依赖
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# 查找libmicrohttpd（HTTP网关）
find_library(MHD_LIBRARY NAMES microhttpd)
if(NOT MHD_LIBRARY)
    message(FATAL_ERROR "libmicrohttpd not found. Please install libmicrohttpd-dev package.")
endif()

# 查找libwebsockets（WebSocket网关）
find_library(WEBSOCKETS_LIBRARY NAMES websockets)
if(NOT WEBSOCKETS_LIBRARY)
    message(FATAL_ERROR "libwebsockets not found. Please install libwebsockets-dev package.")
endif()

# 查找cJSON（JSON解析）
find_library(CJSON_LIBRARY NAMES cjson cJSON)
if(NOT CJSON_LIBRARY)
    message(WARNING "cJSON library not found. Some features may not work.")
endif()

# 源文件（不包含 agentos.c，使用 commons 模块）
set(GATEWAY_SOURCES
    src/platform.c
    src/main.c
    src/server.c
    src/session.c
    src/session_stats.c
    src/health.c
    src/telemetry.c
    src/auth.c
    src/ratelimit.c
    src/config_runtime.c
    src/connection_pool.c
    src/utils/jsonrpc.c
    src/utils/hash.c
    src/utils/secure_mem.c
    src/gateway/http_gateway.c
    src/gateway/ws_gateway.c
    src/gateway/stdio_gateway.c
)

# 头文件目录
set(GATEWAY_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# commons 模块路径
set(COMMON_MODULE_DIR ${CMAKE_SOURCE_DIR}/../commons)

# commons 模块的 include 目录（agentos_common 的 include 是 PRIVATE，需要显式添加）
set(COMMON_INCLUDE_DIRS
    ${COMMON_MODULE_DIR}/platform/include
    ${COMMON_MODULE_DIR}/utils/core/include
    ${COMMON_MODULE_DIR}/utils/error/include
    ${COMMON_MODULE_DIR}/utils/observability/include
)

# 可执行文件
add_executable(gateway ${GATEWAY_SOURCES})

target_include_directories(gateway PRIVATE
    ${GATEWAY_INCLUDE_DIRS}
    ${COMMON_INCLUDE_DIRS}
)

target_link_libraries(gateway
    PRIVATE
    agentos_common
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    ${MHD_LIBRARY}
    ${WEBSOCKETS_LIBRARY}
    ${CJSON_LIBRARY}
)

# 编译定义
if(WIN32)
    target_compile_definitions(gateway PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(gateway PRIVATE ws2_32)
else()
    target_compile_definitions(gateway PRIVATE
        _GNU_SOURCE
        _POSIX_C_SOURCE=200809L
    )
endif()

# 静态库（可选）
option(BUILD_SHARED_LIBS "Build shared library" OFF)

if(NOT BUILD_SHARED_LIBS)
    add_library(gateway_lib STATIC
        src/platform.c
        src/server.c
        src/session.c
        src/session_stats.c
        src/health.c
        src/telemetry.c
        src/auth.c
        src/ratelimit.c
        src/config_runtime.c
        src/connection_pool.c
        src/utils/jsonrpc.c
        src/utils/hash.c
        src/utils/secure_mem.c
        src/gateway/http_gateway.c
        src/gateway/ws_gateway.c
        src/gateway/stdio_gateway.c
    )
    
    target_include_directories(gateway_lib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_INCLUDE_DIRS}
    )
    
    target_link_libraries(gateway_lib
        PUBLIC
        agentos_common
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
        ${MHD_LIBRARY}
        ${WEBSOCKETS_LIBRARY}
    )
    
    if(WIN32)
        target_compile_definitions(gateway_lib PRIVATE
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
        target_link_libraries(gateway_lib PUBLIC ws2_32)
    else()
        target_compile_definitions(gateway_lib PRIVATE
            _GNU_SOURCE
            _POSIX_C_SOURCE=200809L
        )
    endif()
    
    set_target_properties(gateway_lib PROPERTIES
        OUTPUT_NAME "agentos_gateway"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

# 测试（可选）
option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_gateway tests/test_gateway.c)
    target_link_libraries(test_gateway gateway_lib)
    target_include_directories(test_gateway PRIVATE ${GATEWAY_INCLUDE_DIRS})
    add_test(NAME test_gateway COMMAND test_gateway)
endif()

# 安装
install(TARGETS gateway RUNTIME DESTINATION bin)
install(FILES include/gateway.h DESTINATION include)

if(NOT BUILD_SHARED_LIBS)
    install(TARGETS gateway_lib
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
    )
    install(FILES 
        src/platform.h
        src/session.h
        src/health.h
        src/telemetry.h
        src/auth.h
        src/ratelimit.h
        src/connection_pool.h
        src/utils/jsonrpc.h
        src/utils/hash.h
        src/utils/secure_mem.h
        DESTINATION include/agentos/gateway
    )
endif()

# 打印配置摘要
message(STATUS "")
message(STATUS "AgentOS gateway Runtime v${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "")
