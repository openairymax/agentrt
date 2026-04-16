# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# syscall 模块构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-syscall VERSION 1.0.0.5 LANGUAGES C)

find_package(Threads REQUIRED)
find_package(CJSON QUIET)
if(CJSON_FOUND)
    message(STATUS "CJSON found: ${CJSON_VERSION}")
    add_definitions(-DAGENTOS_HAS_CJSON=1)
else()
    message(WARNING "CJSON not found, JSON functionality may be limited")
endif()

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
    src/agent.c
    src/skill.c
    src/sandbox.c
    src/sandbox_quota.c
    src/circuit_breaker.c
)

add_library(agentos_syscall STATIC ${SOURCES})

# 使用统一的头文件路径管理
target_link_libraries(agentos_syscall PUBLIC agentos_atoms)

target_include_directories(agentos_syscall PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../memoryrovol/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/observability/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../heapstore/include>
    $<INSTALL_INTERFACE:include/agentos/syscall>
)

# 安全编译选项（基于 AgentOS C 编码风格指南附录 A）
if(MSVC)
    target_compile_options(agentos_syscall PRIVATE /W4 /WX)
    target_compile_definitions(agentos_syscall PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(agentos_syscall PRIVATE /GS)
    target_compile_options(agentos_syscall PRIVATE /sdl)
    target_compile_options(agentos_syscall PRIVATE /guard:cf)
else()
    target_compile_options(agentos_syscall PRIVATE -Wall -Wextra -Wpedantic )
    target_compile_options(agentos_syscall PRIVATE -fstack-protector-strong)
    target_compile_options(agentos_syscall PRIVATE -D_FORTIFY_SOURCE=2)
    target_compile_options(agentos_syscall PRIVATE -fPIE)
    target_compile_options(agentos_syscall PRIVATE -Wformat -Wformat-security)
    target_compile_options(agentos_syscall PRIVATE -fno-omit-frame-pointer)
endif()

# 链接器安全选项
if(UNIX AND NOT APPLE)
    target_link_options(agentos_syscall PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    target_link_options(agentos_syscall PRIVATE -Wl,-dead_strip -pie)
endif()

target_link_libraries(agentos_syscall PUBLIC
    agentos_core
    agentos_coreloopthree
    agentos_memoryrovol
    Threads::Threads
    $<$<BOOL:${CJSON_FOUND}>:${CJSON_LIBRARIES}>
)

install(TARGETS agentos_syscall DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/syscalls.h DESTINATION include/agentos)

# 测试配置
enable_testing()

# 检查测试目录是否存在
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    # 单元测试
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_syscall_entry.c")
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
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_syscall_table.c")
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
    endif()

    # 集成测试
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_full_workflow.c")
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
    endif()

    # 扩展单元测试
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_syscall_extended.c")
        add_executable(test_syscall_extended tests/unit/test_syscall_extended.c)
        target_include_directories(test_syscall_extended PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include
        )
        target_link_libraries(test_syscall_extended
            agentos_syscall
            agentos_core
            Threads::Threads
        )
        add_test(NAME test_syscall_extended COMMAND test_syscall_extended)
    endif()
else()
    message(WARNING "Tests directory not found, skipping syscall tests")
endif()