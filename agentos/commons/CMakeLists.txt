# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# commons 模块构建文件 - 统一基础库

cmake_minimum_required(VERSION 3.20)
project(agentos-commons VERSION 1.0.0.7 LANGUAGES C)

# 导出编译命令数据库（供 clangd/IDE 使用）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    # ════════════════════════════════════════════════════════════════
    # 平台基础层 (Platform Abstraction Layer)
    # 位置: platform/
    # 职责: 底层系统原语抽象（线程、socket、时间等）
    # 特点: 接近操作系统内核接口，性能关键路径
    # ════════════════════════════════════════════════════════════════
    platform/platform.c
    
    # ════════════════════════════════════════════════════════════════
    # 平台适配器层 (Platform Adapter Layer)  
    # 位置: utils/platform/ (注意: 与顶层 platform/ 功能不同)
    # 职责: 高级平台工具（文件系统、环境变量、进程管理等）
    # 特点: 面向应用层的跨平台工具集，封装常见操作
    # 
    # ⚠️ 命名说明:
    #   - platform/        → 系统级抽象（线程同步、网络socket）
    #   - utils/platform/  → 应用级工具（文件操作、命令执行）
    #   两者互补但职责不同，保持此设计以符合分层架构原则
    # ════════════════════════════════════════════════════════════════
    utils/platform/platform_adapter.c
    # 统一类型定义模块（纯头文件库，无 .c 源文件）
    # 统一令牌管理模块
    utils/token/counter.c
    utils/token/budget.c
    # 统一成本控制模块
    utils/cost/estimator.c
    utils/cost/controller.c
    # 统一可观测性模块
    utils/observability/logger.c
    utils/observability/metrics.c
    utils/observability/trace.c
    # 统一错误处理模块
    utils/error/handler.c
    # 统一 I/O 工具模块
    utils/io/file_utils.c
    # 统一安全输入验证模块
    utils/security/input_validator.c
    # 统一资源守护模块
    utils/resource/resource_guard.c
    # 统一日志模块
    utils/logging/src/logging.c
    utils/logging/logging_common.c
    # 统一配置模块（暂时禁用，实现与头文件不匹配）
    # utils/config_unified/src/core_config.c
    # 统一内存管理模块
    utils/memory/src/memory.c
    utils/memory/memory_common.c
    # 统一字符串处理模块
    utils/string/string_common.c
    # 统一线程同步原语模块
    utils/sync/src/sync.c
    utils/sync/sync_common.c
    # 缓存模块
    utils/cache/cache_common.c
    # 兼容性模块
    utils/compat/src/compat.c
    # 认知模块
    utils/cognition/cognition_common.c
    # 执行模块
    utils/execution/execution_common.c
    # 策略模块
    utils/strategy/strategy_common.c
    # 网络通信模块
    utils/network/network_common.c
    # 进程间通信模块
    utils/ipc/ipc_common.c
)

add_library(agentos_common STATIC ${SOURCES})

# MSVC: 启用 C11 标准和 UTF-8 源文件编码
if(MSVC)
    target_compile_options(agentos_common PRIVATE /std:c11 /utf-8 /W3 /wd4819 /wd4005 /D_ENABLE_ATOMIC_ALIGNMENT_FIX)
endif()

target_include_directories(agentos_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/execution/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/string
)

target_include_directories(agentos_common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/token/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cost/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/observability/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/security/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/resource/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/string/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cache/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/compat/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/config_unified/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/cognition/include
    # 策略模块
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/strategy/include
    # 网络通信模块
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/network/include
    # 进程间通信模块
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/ipc/include
)

target_compile_definitions(agentos_common PRIVATE _CRT_SECURE_NO_WARNINGS)
if(NOT CJSON_FOUND)
    target_compile_definitions(agentos_common PRIVATE AGENTOS_NO_CJSON)
endif()

target_link_libraries(agentos_common
    PUBLIC Threads::Threads
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
    if(NOT CJSON_LIBRARIES)
        set(CJSON_LIBRARIES "cjson")
    endif()
    target_link_libraries(agentos_common PRIVATE ${CJSON_LIBRARIES})
    if(CJSON_INCLUDE_DIRS)
        target_include_directories(agentos_common PRIVATE ${CJSON_INCLUDE_DIRS})
    endif()
endif()

install(TARGETS agentos_common DESTINATION lib)
install(DIRECTORY platform/include/ DESTINATION include/agentos/platform)
install(DIRECTORY utils/token/include/ DESTINATION include/agentos/utils/token)
install(DIRECTORY utils/cost/include/ DESTINATION include/agentos/utils/cost)
install(DIRECTORY utils/observability/include/ DESTINATION include/agentos/utils/observability)
install(DIRECTORY utils/logging/include/ DESTINATION include/agentos/utils/logging)
install(DIRECTORY utils/error/include/ DESTINATION include/agentos/utils/error)
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
# 平台适配层安装目录 (应用级跨平台工具)
install(DIRECTORY utils/platform/include/ DESTINATION include/agentos/utils/platform)
# 认知模块安装目录
install(DIRECTORY utils/cognition/include/ DESTINATION include/agentos/utils/cognition)
# 执行模块安装目录
install(DIRECTORY utils/execution/include/ DESTINATION include/agentos/utils/execution)
# 策略模块安装目录
install(DIRECTORY utils/strategy/include/ DESTINATION include/agentos/utils/strategy)

# 测试配置
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
