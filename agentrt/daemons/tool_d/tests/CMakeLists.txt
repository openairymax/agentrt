# Copyright (c) 2026 SPHARX. All Rights Reserved.

# 测试代码豁免 BAN 严格合规：测试允许使用裸 malloc/free/memset 等函数
add_compile_definitions(AGENTRT_COMPLIANCE_IMPL)
# Tool 服务单元测试构建文件 (TeamC)


set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)

set(TOOL_IMPL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/executor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/validator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/tool_approval.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/safety_guard_bridge.c
)

# YAML found by root CMakeLists.txt (AGENTRT_HAS_YAML)
if(AGENTRT_HAS_YAML)
    list(APPEND TOOL_IMPL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/config.c)
endif()

set(TOOL_TEST_SOURCES
    test_service.c
    test_cache.c
    test_executor.c
    test_validator.c
    test_registry.c
    test_sandbox_integration.c
)

set(TOOL_COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${COMMONS_DIR}
    ${COMMONS_DIR}/platform/include
    ${COMMONS_DIR}/utils/error/include
    ${COMMONS_DIR}/utils/logging/include
    ${COMMONS_DIR}/utils/config_unified/include
    ${COMMONS_DIR}/utils/memory/include
    ${COMMONS_DIR}/utils/string/include
    ${COMMONS_DIR}/utils/sync/include
    ${COMMONS_DIR}/utils/cache/include
    ${COMMONS_DIR}/utils/compat/include
    ${SYSCALL_INCLUDE_DIR}
    ${COREKERN_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../cupolas/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../cupolas/src/guards
)

foreach(TEST_SRC ${TOOL_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

    add_executable(test_tool_${TEST_NAME} ${TEST_SRC} ${TOOL_IMPL_SOURCES})

    target_include_directories(test_tool_${TEST_NAME} PRIVATE ${TOOL_COMMON_INCLUDES})

    # P3.18 (ACC-DT27): executor.c 调用 sandbox API（agentrt_sandbox_*），
    # 需链接 agentrt_syscall。agentrt_syscall PUBLIC 依赖 agentrt_coreloopthree，
    # 后者引用 cognition/llm_service/checkpoint/hook_d 等符号，需完整传递依赖。
    # 仿照 test_syscall_extended 配置，但不链接 agentrt_tool_service（避免与
    # TOOL_IMPL_SOURCES 中的 executor.c/service.c 等重复符号）。
    target_link_libraries(test_tool_${TEST_NAME} PRIVATE
        svc_common
        cupolas
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
        agentrt_llm_service
        Threads::Threads
        uuid
        agentrt_platform_libs
        m
    )

    # cJSON found by root CMakeLists.txt (AGENTRT_HAS_CJSON)
    if(AGENTRT_HAS_CJSON)
        target_link_libraries(test_tool_${TEST_NAME} PRIVATE cjson)
    endif()

    if(AGENTRT_HAS_YAML)
        target_link_libraries(test_tool_${TEST_NAME} PRIVATE yaml)
    endif()

    target_compile_options(test_tool_${TEST_NAME} PRIVATE -Wall -Wextra -g -Wno-unused-function)

    # C-L05: 启用 Cupolas SafetyGuard V2 API
    target_compile_definitions(test_tool_${TEST_NAME} PRIVATE
        AGENTRT_ENABLE_V2_API=1
    )

    add_test(NAME tool_d_${TEST_NAME} COMMAND test_tool_${TEST_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

enable_testing()
