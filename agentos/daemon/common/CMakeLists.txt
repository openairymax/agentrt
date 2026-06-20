# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# daemon 公共库构建文件 - 最小兼容层
#
# 本模块现在作为 commons 统一基础库的兼容层，
# 提供向后兼容的 API，实际实现来自 commons。
#
# 保留的 daemon 特有模块：
# - svc_common.h: 服务框架接口
# - ipc_client.c: IPC 客户端实现


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_COVERAGE "Build with code coverage" OFF)

set(COMMONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../commons")
if(NOT EXISTS "${COMMONS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "commons module not found at ${COMMONS_DIR}. Please ensure commons is available.")
endif()
if(NOT TARGET agentos_common)
    add_subdirectory(${COMMONS_DIR} ${CMAKE_BINARY_DIR}/commons)
endif()

set(ATOMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../atoms")

set(SVC_COMMON_SOURCES
    src/svc_common.c          # 服务框架核心实现
    src/ipc_client.c
    src/ipc_service_bus.c     # IPC服务总线
    src/ipc_bus_helper.c       # C-L09: daemon IPC Bus 自动注册便捷层
    src/service_discovery.c   # 跨进程服务发现
    src/service_discovery_helper.c  # C-L08: daemon 自动注册便捷层
    src/circuit_breaker.c     # 熔断器与自愈
    src/unified_metrics.c     # 统一指标收集
    src/alert_manager.c       # 智能告警管理
    src/config_manager.c      # 统一配置管理
    src/svc_auth.c
    src/daemon_security.c
    src/jsonrpc_helpers.c
    src/method_dispatcher.c
    src/safe_string_utils.c
    src/input_validator.c
    src/param_validator.c
    src/log_sanitizer.c
    src/platform_compat.c     # 平台兼容层实现
    src/agentos_event_loop.c  # 事件驱动循环
    src/daemon_event_driver.c # 统一事件驱动框架
    src/api_recovery.c        # API恢复系统
    src/orchestrator.c        # 流程编排器
    src/thread_pool.c         # 工作线程池
    src/parallel_dispatcher.c # 工具并行执行引擎
    src/checkpoint.c
    src/daemon_oom.c          # P1.22: Daemon OOM 降级回调注册
    src/ipc_backpressure.c    # P1.24: IPC Bus 背压控制
    src/daemon_bootstrap_sd.c    # P1.7 C-L08: daemon ServiceDiscovery 一键引导
    src/daemon_bootstrap_ipc.c   # P1.7 C-L09: daemon IPC Bus 一键引导
)

# 创建静态库
add_library(svc_common STATIC ${SVC_COMMON_SOURCES})

# 链接 commons 库
target_link_libraries(svc_common PUBLIC
    agentos_common
    agentos_compile_defs
)
if(AGENTOS_HAS_OPENSSL)
    target_link_libraries(svc_common PUBLIC ${OPENSSL_CRYPTO_LIBRARY})
endif()

# 平台特定配置
if(WIN32)
    target_compile_definitions(svc_common PRIVATE
        AGENTOS_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(svc_common PRIVATE
        ws2_32
        bcrypt
        advapi32
    )
elseif(APPLE)
    target_compile_definitions(svc_common PRIVATE
        AGENTOS_PLATFORM_MACOS=1
    )
elseif(UNIX)
    target_compile_definitions(svc_common PRIVATE
        AGENTOS_PLATFORM_LINUX=1
        _GNU_SOURCE
    )
    target_link_libraries(svc_common PRIVATE
        Threads::Threads
        agentos_platform_libs
    )
endif()

# 包含目录 - 兼容层头文件
target_include_directories(svc_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/agentos>
)

# 包含 commons 的头文件目录
target_include_directories(svc_common PUBLIC
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/compat/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}>
    $<BUILD_INTERFACE:${COMMONS_DIR}/platform/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/error/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/logging/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/config_unified/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/memory/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/string/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/sync/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/observability/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/io/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/types/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/cache/include>
    # 新增网络和IPC模块
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/network/include>
    $<BUILD_INTERFACE:${COMMONS_DIR}/utils/ipc/include>
)

target_include_directories(svc_common PUBLIC
    $<BUILD_INTERFACE:${ATOMS_DIR}/coreloopthree/include>
    $<BUILD_INTERFACE:${ATOMS_DIR}/coreloopthree/src/cognition>
    $<BUILD_INTERFACE:${ATOMS_DIR}/coreloopthree/src/cognition/critique>
    $<BUILD_INTERFACE:${ATOMS_DIR}/coreloopthree/src/cognition/foundation>
    $<BUILD_INTERFACE:${ATOMS_DIR}/memory>
    $<BUILD_INTERFACE:${ATOMS_DIR}/memory/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/llm_d/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/tool_d/include>
)
if(AGENTOS_HAS_CURL)
    target_compile_definitions(svc_common PUBLIC AGENTOS_HAS_CURL)
    if(CURL_FOUND)
        target_include_directories(svc_common PRIVATE ${CURL_INCLUDE_DIRS})
        target_link_libraries(svc_common PUBLIC ${CURL_LIBRARIES})
    elseif(LIBCURL_FOUND)
        target_include_directories(svc_common PRIVATE ${LIBCURL_INCLUDE_DIRS})
        target_link_libraries(svc_common PUBLIC ${LIBCURL_LIBRARIES})
    endif()
endif()
if(AGENTOS_HAS_YAML AND YAML_FOUND)
    target_include_directories(svc_common PRIVATE ${YAML_INCLUDE_DIRS})
    target_link_libraries(svc_common PUBLIC ${YAML_LIBRARIES})
endif()
if(AGENTOS_HAS_CJSON)
    if(cJSON_FOUND)
        target_link_libraries(svc_common PUBLIC cjson)
    elseif(CJSON_FOUND)
        target_include_directories(svc_common PRIVATE ${CJSON_INCLUDE_DIRS})
        target_link_libraries(svc_common PUBLIC ${CJSON_LIBRARIES})
    endif()
endif()

target_link_libraries(svc_common PUBLIC Threads::Threads)

# 编译选项
if(BUILD_COVERAGE)
    if(NOT MSVC)
    target_compile_options(svc_common PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fprofile-arcs
        -ftest-coverage
        -g
        -O0
    )
    target_link_options(svc_common PRIVATE
        --coverage
    )
    endif()
else()
    if(NOT MSVC)
    target_compile_options(svc_common PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=implicit-function-declaration
        -Werror=return-type
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
    endif()
endif()

# 导出符号
set_target_properties(svc_common PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    POSITION_INDEPENDENT_CODE ON
)

# 测试支持
if(BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    endif()
endif()

# 安装规则
install(TARGETS svc_common
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装兼容层头文件
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/platform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/error.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_logger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_cache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/compat.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_auth.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/daemon_security.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/jsonrpc_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/method_dispatcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/safe_string_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/input_validator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/param_validator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/log_sanitizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ipc_service_bus.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ipc_bus_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/service_discovery.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/service_discovery_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/circuit_breaker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/unified_metrics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/alert_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/api_recovery.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/thread_pool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/orchestrator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/daemon_defaults.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/parallel_dispatcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/agentos_event_loop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/daemon_oom.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ipc_backpressure.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/daemon_startup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/daemon_event_driver.h
    DESTINATION include/agentos
)
