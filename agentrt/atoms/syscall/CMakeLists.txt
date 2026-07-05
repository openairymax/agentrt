# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# syscall 模块构建文件


# Threads found by root CMakeLists.txt
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

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
    src/sandbox_permission.c
    src/sandbox_utils.c
    src/sandbox_quota.c
    src/circuit_breaker.c
    src/rate_limiter.c
)

add_library(agentrt_syscall STATIC ${SOURCES})

if(BUILD_SHARED_LIBS)
    target_compile_definitions(agentrt_syscall PRIVATE AGENTRT_BUILDING_SHARED)
endif()

# P3.20: 启用 heapstore 持久化代码 — session.c/telemetry.c 中 #ifdef BUILD_HEAPSTORE 代码块
target_compile_definitions(agentrt_syscall PRIVATE BUILD_HEAPSTORE)

target_link_libraries(agentrt_syscall PUBLIC agentrt_atoms)

target_include_directories(agentrt_syscall PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/observability/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../heapstore/include>
    $<INSTALL_INTERFACE:include/agentrt/syscall>
)

target_include_directories(agentrt_syscall PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/llm_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory/src
    # P3.18 (ACC-DT27): syscall_entry.c 的 sys_tool_execute 调用 agentrt_process_run_capture
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include
)

# 安全编译选项（基于 AgentRT C 编码风格指南附录 A）
if(MSVC)
    target_compile_options(agentrt_syscall PRIVATE /W4 /WX)
    target_compile_definitions(agentrt_syscall PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(agentrt_syscall PRIVATE /GS)
    target_compile_options(agentrt_syscall PRIVATE /sdl)
    target_compile_options(agentrt_syscall PRIVATE /guard:cf)
else()
    target_compile_options(agentrt_syscall PRIVATE -Wall -Wextra -Wpedantic -Werror=implicit-function-declaration -Werror=return-type)
    target_compile_options(agentrt_syscall PRIVATE -fstack-protector-strong)
    target_compile_options(agentrt_syscall PRIVATE $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>)
    target_compile_options(agentrt_syscall PRIVATE -fPIE)
    target_compile_options(agentrt_syscall PRIVATE -Wformat -Wformat-security)
    target_compile_options(agentrt_syscall PRIVATE -fno-omit-frame-pointer)
endif()

# 链接器安全选项
if(UNIX AND NOT APPLE)
    target_link_options(agentrt_syscall PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    target_link_options(agentrt_syscall PRIVATE -Wl,-dead_strip -pie)
endif()

target_link_libraries(agentrt_syscall PUBLIC
    agentrt_core
    agentrt_coreloopthree
    agentrt_heapstore
    Threads::Threads
    $<$<BOOL:${CJSON_FOUND}>:${CJSON_LIBRARIES}>
)

# cJSON 用于 skill.c 的 data_transform/file_io/image_gen/audio_process JSON 解析
# 使用全局 AGENTRT_HAS_CJSON（根 CMakeLists.txt 检测）确保与项目一致
if(AGENTRT_HAS_CJSON)
    target_compile_definitions(agentrt_syscall PRIVATE AGENTRT_HAS_CJSON=1)
    if(CJSON_INCLUDE_DIRS)
        target_include_directories(agentrt_syscall PRIVATE ${CJSON_INCLUDE_DIRS})
    endif()
    agentrt_print_ok("  syscall: cJSON enabled for skill JSON features (data_transform/file_io/image_gen/audio_process)")
endif()

# libcurl 用于 skill.c 的 web_search/image_gen/audio_process HTTP API 调用
if(AGENTRT_HAS_CURL)
    target_compile_definitions(agentrt_syscall PRIVATE AGENTRT_HAS_CURL=1)
    if(CURL_FOUND)
        target_link_libraries(agentrt_syscall PRIVATE ${CURL_LIBRARIES})
        target_include_directories(agentrt_syscall PRIVATE ${CURL_INCLUDE_DIRS})
    elseif(LIBCURL_FOUND)
        target_link_libraries(agentrt_syscall PRIVATE ${LIBCURL_LIBRARIES})
        target_include_directories(agentrt_syscall PRIVATE ${LIBCURL_INCLUDE_DIRS})
    endif()
    agentrt_print_ok("  syscall: libcurl enabled for skill HTTP features (web_search/image_gen/audio_process)")
else()
    agentrt_print_warn("  syscall: libcurl not found - web_search/image_gen/audio_process HTTP features will be unavailable")
endif()

install(TARGETS agentrt_syscall DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/syscalls.h DESTINATION include/agentrt)
# P3.18 (ACC-DT27): sandbox 公共 API 头文件
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/agentrt_sandbox.h DESTINATION include/agentrt)

# 测试配置
if(BUILD_TESTS)
    enable_testing()

# 检查测试目录是否存在
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    # 单元测试
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_syscall_entry.c")
        add_executable(test_syscall_entry tests/unit/test_syscall_entry.c)
        target_compile_definitions(test_syscall_entry PRIVATE AGENTRT_COMPLIANCE_IMPL)
        target_include_directories(test_syscall_entry PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include
        )
        target_link_libraries(test_syscall_entry
            agentrt_syscall
            agentrt_core
            Threads::Threads
        )
        add_test(NAME test_syscall_entry COMMAND test_syscall_entry)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_syscall_table.c")
        add_executable(test_syscall_table tests/unit/test_syscall_table.c)
        target_compile_definitions(test_syscall_table PRIVATE AGENTRT_COMPLIANCE_IMPL)
        target_include_directories(test_syscall_table PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include
        )
        target_link_libraries(test_syscall_table
            agentrt_syscall
            agentrt_core
            Threads::Threads
        )
        add_test(NAME test_syscall_table COMMAND test_syscall_table)
    endif()

    # 集成测试
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_full_workflow.c")
        add_executable(test_full_workflow tests/integration/test_full_workflow.c)
        target_compile_definitions(test_full_workflow PRIVATE AGENTRT_COMPLIANCE_IMPL)
        target_include_directories(test_full_workflow PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../corekern/include
        )
        target_link_libraries(test_full_workflow
            agentrt_syscall
            agentrt_core
            Threads::Threads
        )
        add_test(NAME test_full_workflow COMMAND test_full_workflow)
    endif()

    # 扩展单元测试 (TeamC P1-C07 扩展, 覆盖率>50%)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_syscall_extended.c")
        add_executable(test_syscall_extended tests/test_syscall_extended.c)
        target_compile_definitions(test_syscall_extended PRIVATE AGENTRT_COMPLIANCE_IMPL)
        target_include_directories(test_syscall_extended PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../coreloopthree/tests/unit
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../../corekern/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/error/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/sync/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/compat/include
        )
        target_link_libraries(test_syscall_extended PRIVATE
            agentrt_syscall
            -Wl,--whole-archive
            agentrt_coreloopthree
            -Wl,--no-whole-archive
            agentrt_cognition
            agentrt_execution
            agentrt_core
            agentrt_memory
            agentrt_atoms
            agentrt_common
            svc_common
            agentrt_llm_service
            agentrt_tool_service
            Threads::Threads
            uuid
            agentrt_platform_libs
            m
        )
        target_link_libraries(test_syscall_extended PRIVATE agentrt_test_ldflags)
        target_compile_options(test_syscall_extended PRIVATE -g -O0 -fno-lto)
        # -O0 与 _FORTIFY_SOURCE=2 不兼容（需要 -O1+），取消定义以避免警告
        target_compile_options(test_syscall_extended PRIVATE -U_FORTIFY_SOURCE)
        add_test(NAME test_syscall_extended COMMAND test_syscall_extended)
        agentrt_print_ok("  syscall_tests: test_syscall_extended configured (P1-C07 extended)")
    endif()
else()
    agentrt_print_warn("Tests directory not found, skipping syscall tests")
endif()
endif()

# TeamC P1-C07: Syscall 核心单元测试
if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_syscalls.c")
    add_executable(test_syscalls tests/test_syscalls.c)
    target_compile_definitions(test_syscalls PRIVATE AGENTRT_COMPLIANCE_IMPL)
    target_include_directories(test_syscalls PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../coreloopthree/tests/unit
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/error/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include
    )
    target_link_libraries(test_syscalls PRIVATE
        agentrt_syscall
        -Wl,--whole-archive
        agentrt_coreloopthree
        -Wl,--no-whole-archive
        agentrt_cognition
        agentrt_execution
        agentrt_core
        agentrt_memory
        agentrt_atoms
        agentrt_common
        svc_common
        agentrt_llm_service
        agentrt_tool_service
        Threads::Threads
        uuid
    )
    
    # 直接链接id_utils.o以解决LTO符号丢失问题
    if(EXISTS "${CMAKE_BINARY_DIR}/agentrt/atoms/coreloopthree/CMakeFiles/agentrt_coreloopthree.dir/src/utils/id_utils.c.o")
        target_sources(test_syscalls PRIVATE
            ${CMAKE_BINARY_DIR}/agentrt/atoms/coreloopthree/CMakeFiles/agentrt_coreloopthree.dir/src/utils/id_utils.c.o
        )
    endif()
    target_link_libraries(test_syscalls PRIVATE agentrt_test_ldflags)
    add_test(NAME syscalls_test COMMAND test_syscalls)
    agentrt_print_ok("  syscall_tests: test_syscalls configured (P1-C07)")
endif()