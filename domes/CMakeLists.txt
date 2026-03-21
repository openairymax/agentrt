# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# Domes 安全隔离层构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-domes VERSION 1.0.0.5 LANGUAGES C)

# 查找依赖
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# 必须依赖
pkg_check_modules(YAML REQUIRED yaml-0.1)
pkg_check_modules(CJSON REQUIRED libcjson)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)   # 容器模式需要
pkg_check_modules(LIBMICROHTTPD REQUIRED libmicrohttpd)   # 容器模式可能需要（实际未用，但保留）

# 可选依赖（用于容器模式，无则降级）
pkg_check_modules(RUNC runc)  # 仅用于检查，不强制

# 包含路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../apexcore/core/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../apexcore/utils/observability/include)
include_directories(${YAML_INCLUDE_DIRS})
include_directories(${CJSON_INCLUDE_DIRS})

# 源文件列表
set(SOURCES
    src/core.c
    src/workbench/workbench.c
    src/workbench/workbench_process_core.c
    src/workbench/workbench_process_exec.c
    src/workbench/workbench_process_child.c
    src/workbench/workbench_container.c
    src/permission/permission_engine.c
    src/permission/permission_rule.c
    src/permission/permission_cache.c
    src/audit/audit_logger.c
    src/audit/audit_queue.c
    src/audit/audit_rotator.c
    src/sanitizer/sanitizer_core.c
    src/sanitizer/sanitizer_rules.c
    src/sanitizer/sanitizer_cache.c
)

# 生成静态库
add_library(agentos_domes STATIC ${SOURCES})

# 链接库
target_link_libraries(agentos_domes
    agentos_core
    agentos_utils
    Threads::Threads
    ${YAML_LIBRARIES}
    ${CJSON_LIBRARIES}
)

# 安装
install(TARGETS agentos_domes DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/domes.h DESTINATION include/agentos)