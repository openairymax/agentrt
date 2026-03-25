# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# utils 模块构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-utils VERSION 1.0.0.5 LANGUAGES C)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(TIKTOKEN REQUIRED tiktoken)
pkg_check_modules(YAML REQUIRED yaml-0.1)
pkg_check_modules(CJSON REQUIRED libcjson)

set(SOURCES
    core/platform.c
    core/version.c
    token/counter.c
    token/budget.c
    cost/estimator.c
    cost/controller.c
    observability/logger.c
    observability/metrics.c
    observability/trace.c
    error/handler.c
    error/types.c
    io/file_utils.c
)

add_library(agentos_utils STATIC ${SOURCES})

target_include_directories(agentos_utils PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/token/include
    ${CMAKE_CURRENT_SOURCE_DIR}/cost/include
    ${CMAKE_CURRENT_SOURCE_DIR}/observability/include
    ${CMAKE_CURRENT_SOURCE_DIR}/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/types/include
    ${TIKTOKEN_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

target_link_libraries(agentos_utils
    ${TIKTOKEN_LIBRARIES}
    ${YAML_LIBRARIES}
    ${CJSON_LIBRARIES}
    Threads::Threads
)

install(TARGETS agentos_utils DESTINATION lib)
install(DIRECTORY core/include/ DESTINATION include/agentos/utils/core)
install(DIRECTORY token/include/ DESTINATION include/agentos/utils/token)
install(DIRECTORY cost/include/ DESTINATION include/agentos/utils/cost)
install(DIRECTORY observability/include/ DESTINATION include/agentos/utils/observability)
install(DIRECTORY error/include/ DESTINATION include/agentos/utils/error)
install(DIRECTORY io/include/ DESTINATION include/agentos/utils/io)
install(DIRECTORY types/include/ DESTINATION include/agentos/utils/types)

# 测试配置
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()