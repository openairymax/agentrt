cmake_minimum_required(VERSION 3.20)
project(agentos-tool-d VERSION 1.0.0 LANGUAGES C)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CJSON REQUIRED libcjson)
pkg_check_modules(YAML REQUIRED yaml-0.1)
pkg_check_modules(LIBUV REQUIRED libuv)  # 用于异步执行和流式

# 公共库路径
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common/include)
set(COMMON_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/../common)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${COMMON_INCLUDE_DIR}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${LIBUV_INCLUDE_DIRS}
)

link_directories(${COMMON_LIB_DIR})

set(SOURCES
    src/main.c
    src/service.c
    src/registry.c
    src/executor.c
    src/validator.c
    src/cache.c
    src/config.c
    src/utils/tool_helpers.c
)

add_executable(agentos-tool-d ${SOURCES})

target_link_libraries(agentos-tool-d
    Threads::Threads
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
    ${LIBUV_LIBRARIES}
    agentos-common
)

install(TARGETS agentos-tool-d DESTINATION bin)