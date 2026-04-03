# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# commons 模块构建文件 - 统一基础库

cmake_minimum_required(VERSION 3.20)
project(agentos-commons VERSION 1.0.0.7 LANGUAGES C)

find_package(Threads REQUIRED)

# 可选依赖：pkg-config 支持的库（Windows 上可能不可用）
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(TIKTOKEN QUIET tiktoken)
    pkg_check_modules(YAML QUIET yaml-0.1)
    pkg_check_modules(CJSON QUIET libcjson)
endif()

# 如果 pkg-config 未找到，尝试标准 find_package
if(NOT TIKTOKEN_FOUND)
    find_package(tiktoken QUIET)
endif()
if(NOT YAML_FOUND)
    find_package(YAML QUIET)
endif()
if(NOT CJSON_FOUND)
    find_package(cJSON QUIET)
endif()

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
    utils/manager/manager.c
    utils/io/file_utils.c
    utils/security/input_validator.c
    utils/resource/resource_guard.c
    # 统一日志模块
    utils/logging/src/logging_compat.c
    utils/logging/src/logging.c
    utils/logging/src/atomic_logging.c
    utils/logging/src/service_logging.c
    # 统一配置模块
    utils/config_unified/src/core_config.c
    utils/config_unified/src/config_source.c
    utils/config_unified/src/config_service.c
    utils/config_unified/src/config_compat.c
    # 统一内存管理模块
    utils/memory/src/memory.c
    utils/memory/src/memory_pool.c
    utils/memory/src/memory_debug.c
    # 统一字符串处理模块
    utils/string/src/string.c
    # 统一线程同步原语模块
    utils/sync/src/sync.c
    # 缓存模块
    utils/cache/cache_common.c
    # 兼容性模块
    utils/compat/src/compat.c
    # 网络通信模块
    utils/network/network_common.c
    # 进程间通信模块
    utils/ipc/ipc_common.c
)

add_library(agentos_common STATIC ${SOURCES})

target_include_directories(agentos_common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/token/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cost/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/observability/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/manager/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/config_unified/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/security/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/resource/include
    # 统一基础库包含目录
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/string/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/sync/include
    # 缓存和兼容性模块
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cache/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/compat/include
    # 网络通信模块
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/network/include
    # 进程间通信模块
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/ipc/include
)

target_link_libraries(agentos_common
    Threads::Threads
)

# 链接可选依赖（如果找到）
if(TIKTOKEN_FOUND)
    target_link_libraries(agentos_common PRIVATE ${TIKTOKEN_LIBRARIES})
    target_include_directories(agentos_common PRIVATE ${TIKTOKEN_INCLUDE_DIRS})
endif()
if(YAML_FOUND)
    target_link_libraries(agentos_common PRIVATE ${YAML_LIBRARIES})
    target_include_directories(agentos_common PRIVATE ${YAML_INCLUDE_DIRS})
endif()
if(CJSON_FOUND)
    target_link_libraries(agentos_common PRIVATE ${CJSON_LIBRARIES})
    target_include_directories(agentos_common PRIVATE ${CJSON_INCLUDE_DIRS})
endif()

install(TARGETS agentos_common DESTINATION lib)
install(DIRECTORY platform/include/ DESTINATION include/agentos/platform)
install(DIRECTORY utils/core/include/ DESTINATION include/agentos/utils/core)
install(DIRECTORY utils/token/include/ DESTINATION include/agentos/utils/token)
install(DIRECTORY utils/cost/include/ DESTINATION include/agentos/utils/cost)
install(DIRECTORY utils/observability/include/ DESTINATION include/agentos/utils/observability)
install(DIRECTORY utils/logging/include/ DESTINATION include/agentos/utils/logging)
install(DIRECTORY utils/error/include/ DESTINATION include/agentos/utils/error)
install(DIRECTORY utils/manager/include/ DESTINATION include/agentos/utils/manager)
install(DIRECTORY utils/config_unified/include/ DESTINATION include/agentos/utils/config_unified)
install(DIRECTORY utils/io/include/ DESTINATION include/agentos/utils/io)
install(DIRECTORY utils/types/include/ DESTINATION include/agentos/utils/types)
install(FILES utils/security/input_validator.h DESTINATION include/agentos/utils/security)
install(FILES utils/resource/resource_guard.h DESTINATION include/agentos/utils/resource)
# 统一基础库安装目录
install(DIRECTORY utils/memory/include/ DESTINATION include/agentos/utils/memory)
install(DIRECTORY utils/string/include/ DESTINATION include/agentos/utils/string)
install(DIRECTORY utils/sync/include/ DESTINATION include/agentos/utils/sync)
# 缓存和兼容性模块安装目录
install(DIRECTORY utils/cache/include/ DESTINATION include/agentos/utils/cache)
install(DIRECTORY utils/compat/include/ DESTINATION include/agentos/utils/compat)
# 网络通信模块安装目录
install(DIRECTORY utils/network/include/ DESTINATION include/agentos/utils/network)
# 进程间通信模块安装目录
install(DIRECTORY utils/ipc/include/ DESTINATION include/agentos/utils/ipc)

# 测试配置
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()