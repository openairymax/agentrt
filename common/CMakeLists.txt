# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# common 模块构建文件 - 从 atoms/utils 迁移而来

cmake_minimum_required(VERSION 3.20)
project(agentos-common VERSION 1.0.0.5 LANGUAGES C)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(TIKTOKEN REQUIRED tiktoken)
pkg_check_modules(YAML REQUIRED yaml-0.1)
pkg_check_modules(CJSON REQUIRED libcjson)

set(SOURCES
    platform/platform.c
    utils/core/platform.c
    utils/core/version.c
    utils/token/counter.c
    utils/token/budget.c
    utils/cost/estimator.c
    utils/cost/controller.c
    utils/observability/logger.c
    utils/observability/metrics.c
    utils/observability/trace.c
    utils/error/handler.c
    utils/config/config.c
    utils/io/file_utils.c
    utils/security/input_validator.c
    utils/resource/resource_guard.c
)

add_library(agentos_common STATIC ${SOURCES})

target_include_directories(agentos_common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/token/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cost/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/observability/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/config/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/security/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/resource/include
    ${TIKTOKEN_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

target_link_libraries(agentos_common
    ${TIKTOKEN_LIBRARIES}
    ${YAML_LIBRARIES}
    ${CJSON_LIBRARIES}
    Threads::Threads
)

install(TARGETS agentos_common DESTINATION lib)
install(DIRECTORY platform/include/ DESTINATION include/agentos/platform)
install(DIRECTORY utils/core/include/ DESTINATION include/agentos/utils/core)
install(DIRECTORY utils/token/include/ DESTINATION include/agentos/utils/token)
install(DIRECTORY utils/cost/include/ DESTINATION include/agentos/utils/cost)
install(DIRECTORY utils/observability/include/ DESTINATION include/agentos/utils/observability)
install(DIRECTORY utils/error/include/ DESTINATION include/agentos/utils/error)
install(DIRECTORY utils/config/include/ DESTINATION include/agentos/utils/config)
install(DIRECTORY utils/io/include/ DESTINATION include/agentos/utils/io)
install(DIRECTORY utils/types/include/ DESTINATION include/agentos/utils/types)
install(FILES utils/security/input_validator.h DESTINATION include/agentos/utils/security)
install(FILES utils/resource/resource_guard.h DESTINATION include/agentos/utils/resource)

# 测试配置
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()