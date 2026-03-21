# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# syscall 模块构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-syscall VERSION 1.0.0.5 LANGUAGES C)

find_package(Threads REQUIRED)
find_package(CJSON REQUIRED)

set(SOURCES
    src/task.c
    src/memory.c
    src/session.c
    src/telemetry.c
    src/syscall_entry.c
    src/syscall_table.c
)

add_library(agentos_syscall STATIC ${SOURCES})

target_include_directories(agentos_syscall PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../coreloopthree/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../memoryrovol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/observability/include
    ${CJSON_INCLUDE_DIRS}
)

target_link_libraries(agentos_syscall
    agentos_core
    agentos_coreloopthree
    agentos_memoryrovol
    agentos_utils
    Threads::Threads
    ${CJSON_LIBRARIES}
)

install(TARGETS agentos_syscall DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/syscalls.h DESTINATION include/agentos)