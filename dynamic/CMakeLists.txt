# AgentOS Dynamic Runtime - CMake 构建配置
# 
# Copyright (c) 2026 SPHARX. All Rights Reserved.

cmake_minimum_required(VERSION 3.16)

project(agentos_dynamic
    VERSION 1.1.0
    LANGUAGES C
    DESCRIPTION "AgentOS Dynamic Runtime - Gateway Layer")

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

# 源文件
set(DYNAMIC_SOURCES
    src/platform.c
    src/agentos.c
    src/main.c
    src/server.c
    src/session.c
    src/health.c
    src/telemetry.c
    src/auth.c
    src/ratelimit.c
    src/gateway/http_gateway.c
    src/gateway/ws_gateway.c
    src/gateway/stdio_gateway.c
)

# 头文件目录
set(DYNAMIC_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 可执行文件
add_executable(dynamic ${DYNAMIC_SOURCES})

target_include_directories(dynamic PRIVATE ${DYNAMIC_INCLUDE_DIRS})

target_link_libraries(dynamic
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# 编译定义
if(WIN32)
    target_compile_definitions(dynamic PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(dynamic PRIVATE ws2_32)
else()
    target_compile_definitions(dynamic PRIVATE
        _GNU_SOURCE
        _POSIX_C_SOURCE=200809L
    )
endif()

# 静态库（可选）
option(BUILD_SHARED_LIBS "Build shared library" OFF)

if(NOT BUILD_SHARED_LIBS)
    add_library(dynamic_lib STATIC
        src/platform.c
        src/agentos.c
        src/server.c
        src/session.c
        src/health.c
        src/telemetry.c
        src/auth.c
        src/ratelimit.c
        src/gateway/http_gateway.c
        src/gateway/ws_gateway.c
        src/gateway/stdio_gateway.c
    )
    
    target_include_directories(dynamic_lib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(dynamic_lib
        PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
    )
    
    if(WIN32)
        target_compile_definitions(dynamic_lib PRIVATE
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
        target_link_libraries(dynamic_lib PUBLIC ws2_32)
    else()
        target_compile_definitions(dynamic_lib PRIVATE
            _GNU_SOURCE
            _POSIX_C_SOURCE=200809L
        )
    endif()
    
    set_target_properties(dynamic_lib PROPERTIES
        OUTPUT_NAME "agentos_dynamic"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

# 测试（可选）
option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_dynamic tests/test_dynamic.c)
    target_link_libraries(test_dynamic dynamic_lib)
    target_include_directories(test_dynamic PRIVATE ${DYNAMIC_INCLUDE_DIRS})
    add_test(NAME test_dynamic COMMAND test_dynamic)
endif()

# 安装
install(TARGETS dynamic RUNTIME DESTINATION bin)
install(FILES include/dynamic.h DESTINATION include)

if(NOT BUILD_SHARED_LIBS)
    install(TARGETS dynamic_lib
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
    )
    install(FILES 
        src/agentos.h
        src/platform.h
        src/session.h
        src/health.h
        src/telemetry.h
        src/auth.h
        src/ratelimit.h
        DESTINATION include/agentos/dynamic
    )
endif()

# 打印配置摘要
message(STATUS "")
message(STATUS "AgentOS Dynamic Runtime v${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "")
