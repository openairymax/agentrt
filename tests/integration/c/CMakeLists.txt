# AgentRT 集成测试 CMake 配置
# 测试代码豁免 BAN 严格合规：测试允许使用裸 malloc/free/memset 等函数
add_compile_definitions(AGENTOS_COMPLIANCE_IMPL)
# @owner: team-C
# P1.16m: 12 条连接线集成测试 + P2.8: Hook 系统集成测试
#
# 测试文件使用 stub 库提供 mock 实现，无需依赖完整的 daemon 二进制构建。
# 每个测试可独立编译运行，验证 API 合约正确性。

# ============================================================================
# 公共 include 路径
# ============================================================================
set(INTEGRATION_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/coreloopthree/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/llm_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/tool_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/monit_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/gateway_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/hook_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/config_unified/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/cupolas/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================================
# 连接线测试列表
# ============================================================================
set(CONNECTION_TESTS
    test_c_link_01_manager_loop       # C-L01: Manager → CoreLoopThree
    test_c_link_02_llm_loop           # C-L02: llm_d → CoreLoopThree
    test_c_link_03_market_openlab     # C-L03: market_d → OpenLab
    test_c_link_04_tool_loop          # C-L04: tool_d → CoreLoopThree
    test_c_link_05_safety_tool        # C-L05: Cupolas SafetyGuard → tool_d
    test_c_link_06_orchestrator_loop  # C-L06: Orchestrator → CoreLoopThree
    test_c_link_07_checkpoint_loop    # C-L07: Checkpoint → CoreLoopThree
    test_c_link_08_service_discovery  # C-L08: ServiceDiscovery → All Daemons
    test_c_link_09_ipc_bus            # C-L09: IPC Bus → All Daemons
    test_c_link_10_monit_prometheus   # C-L10: monit_d → Prometheus
    test_c_link_11_gateway_forward    # C-L11: gateway_d → Gateway
    test_c_link_12_memoryrovol_bridge # C-L12: CoreLoopThree → MemoryRovol
    test_hook_system                  # P2.8: Hook System Integration
)

# ============================================================================
# 为每个测试创建可执行文件
# ============================================================================
foreach(test_name ${CONNECTION_TESTS})
    add_executable(${test_name}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_stubs.c
    )

    target_include_directories(${test_name} PRIVATE ${INTEGRATION_INCLUDE_DIRS})
    target_compile_features(${test_name} PRIVATE c_std_11)
    target_compile_options(${test_name} PRIVATE
        -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
        -Wno-unused-parameter
    )
    target_link_libraries(${test_name} PRIVATE
        Threads::Threads
        m
    )

    add_test(NAME integration_${test_name}
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    message(STATUS "  [integration] ${test_name}: configured")
endforeach()

# ============================================================================
# 已有测试 (保留)
# ============================================================================

# 端到端核心测试 (P2-C01)
if(TARGET svc_common)
    add_executable(test_e2e_core test_e2e_core.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/coreloopthree/tests/unit/test_cl3_stubs.c
    )

    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/coreloopthree/tests/unit/test_cl3_stubs.c
        PROPERTIES COMPILE_FLAGS "-fno-lto"
    )

    target_include_directories(test_e2e_core PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/corekern/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/coreloopthree/tests/unit
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/coreloopthree/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/syscall/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/error/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/memory/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/platform/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/string/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/cache/include
    )

    target_link_libraries(test_e2e_core PRIVATE
        agentos_core
        agentos_syscall
        -Wl,--whole-archive
        agentos_coreloopthree
        -Wl,--no-whole-archive
        svc_common
        Threads::Threads
        agentos_platform_libs
        m
    )

    if(AGENTRT_HAS_CJSON OR CJSON_FOUND)
        target_link_libraries(test_e2e_core PRIVATE cjson)
    endif()

    target_compile_features(test_e2e_core PRIVATE c_std_11)
    target_compile_options(test_e2e_core PRIVATE
        -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
    )

    add_test(NAME e2e_core COMMAND test_e2e_core
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    message(STATUS "  [integration] test_e2e_core: P2-C01 E2E integration tests configured")
else()
    message(STATUS "  [integration] Skipping test_e2e_core: svc_common not available (BUILD_DAEMON=OFF)")
endif()

# LLM Routing Integration Test (INT-15)
set(LLM_D_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/llm_d)
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)

set(LLM_IMPL_SOURCES
    ${LLM_D_DIR}/src/service.c
    ${LLM_D_DIR}/src/cache.c
    ${LLM_D_DIR}/src/token_counter.c
    ${LLM_D_DIR}/src/response.c
    ${LLM_D_DIR}/src/cost_tracker.c
    ${LLM_D_DIR}/src/providers/registry.c
    ${LLM_D_DIR}/src/providers/provider.c
    ${LLM_D_DIR}/src/providers/openai.c
    ${LLM_D_DIR}/src/providers/anthropic.c
    ${LLM_D_DIR}/src/providers/deepseek.c
    ${LLM_D_DIR}/src/providers/google.c
    ${LLM_D_DIR}/src/providers/local.c
)

add_executable(test_llm_d_routing test_llm_d_routing.c ${LLM_IMPL_SOURCES})

target_include_directories(test_llm_d_routing PRIVATE
    ${LLM_D_DIR}/include
    ${LLM_D_DIR}/src
    ${LLM_D_DIR}/src/providers
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/include
)

target_link_libraries(test_llm_d_routing PRIVATE
    svc_common
    Threads::Threads
)

if(AGENTRT_HAS_CJSON OR CJSON_FOUND)
    target_link_libraries(test_llm_d_routing PRIVATE cjson)
endif()

if(AGENTRT_HAS_CURL)
    if(CURL_INCLUDE_DIRS)
        target_include_directories(test_llm_d_routing PRIVATE ${CURL_INCLUDE_DIRS})
    endif()
    if(CURL_LIBRARIES)
        target_link_libraries(test_llm_d_routing PRIVATE ${CURL_LIBRARIES})
    elseif(LIBCURL_LIBRARIES)
        target_link_libraries(test_llm_d_routing PRIVATE ${LIBCURL_LIBRARIES})
    elseif(PC_CURL_LIBRARIES)
        target_link_libraries(test_llm_d_routing PRIVATE ${PC_CURL_LIBRARIES})
    else()
        target_link_libraries(test_llm_d_routing PRIVATE curl)
    endif()
endif()

target_compile_features(test_llm_d_routing PRIVATE c_std_11)
target_compile_options(test_llm_d_routing PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

add_test(NAME llm_d_routing COMMAND test_llm_d_routing
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "  [integration] test_llm_d_routing: INT-15 LLM routing integration tests configured")