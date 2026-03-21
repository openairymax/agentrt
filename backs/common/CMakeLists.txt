# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# backs 公共库构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-svc-common VERSION 3.0.0 LANGUAGES C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(YAML REQUIRED yaml-0.1)
pkg_check_modules(CJSON REQUIRED libcjson)

add_library(svc_common STATIC
    src/ipc_client.c
    src/config.c
)

target_include_directories(svc_common PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atoms/utils/observability/include
    ${LIBCURL_INCLUDE_DIRS}
    ${YAML_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

target_link_libraries(svc_common
    ${LIBCURL_LIBRARIES}
    ${YAML_LIBRARIES}
    ${CJSON_LIBRARIES}
)

# 安装（如果需要）
install(TARGETS svc_common DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/svc_common.h DESTINATION include/agentos/svc)