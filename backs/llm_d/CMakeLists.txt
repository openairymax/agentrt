cmake_minimum_required(VERSION 3.20)
project(agentos-llm-d VERSION 1.0.0 LANGUAGES C)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(CJSON REQUIRED libcjson)
pkg_check_modules(YAML REQUIRED yaml-0.1)
pkg_check_modules(TIKTOKEN REQUIRED tiktoken)

# 公共库路径
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common/include)
set(COMMON_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/../common)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${COMMON_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${TIKTOKEN_INCLUDE_DIRS}
)

link_directories(${COMMON_LIB_DIR})

set(SOURCES
    src/main.c
    src/service.c
    src/providers/registry.c
    src/providers/openai.c
    src/providers/anthropic.c
    src/providers/deepseek.c
    src/providers/local.c
    src/cache.c
    src/cost_tracker.c
    src/token_counter.c
    src/response.c
)

add_executable(agentos-llm-d ${SOURCES})

target_link_libraries(agentos-llm-d
    Threads::Threads
    ${CURL_LIBRARIES}
    ${CJSON_LIBRARIES}
    ${YAML_LIBRARIES}
    ${TIKTOKEN_LIBRARIES}
    agentos-common
)

install(TARGETS agentos-llm-d DESTINATION bin)