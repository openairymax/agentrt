# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# syscall 模块构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-syscall VERSION 1.0.0.5 LANGUAGES C)

find_package(Threads REQUIRED)
find_package(CJSON REQUIRED)

# 符号导出管理
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# 定义构建共享库时的宏
if(BUILD_SHARED_LIBS)
    add_definitions(-DAGENTOS_BUILDING_SHARED)
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include
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

# 测试配置
enable_testing()

# 单元测试
add_executable(test_syscall_entry tests/unit/test_syscall_entry.c)
target_include_directories(test_syscall_entry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include
)
target_link_libraries(test_syscall_entry
    agentos_syscall
    agentos_core
    Threads::Threads
)
add_test(NAME test_syscall_entry COMMAND test_syscall_entry)

add_executable(test_syscall_table tests/unit/test_syscall_table.c)
target_include_directories(test_syscall_table PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include
)
target_link_libraries(test_syscall_table
    agentos_syscall
    agentos_core
    Threads::Threads
)
add_test(NAME test_syscall_table COMMAND test_syscall_table)

# 集成测试
add_executable(test_full_workflow tests/integration/test_full_workflow.c)
target_include_directories(test_full_workflow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include
)
target_link_libraries(test_full_workflow
    agentos_syscall
    agentos_core
    Threads::Threads
)
add_test(NAME test_full_workflow COMMAND test_full_workflow)