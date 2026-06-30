# AgentRT 集成测试 CMake 配置
# 测试代码豁免 BAN 严格合规：测试允许使用裸 malloc/free/memset 等函数
add_compile_definitions(AGENTOS_COMPLIANCE_IMPL)
# @owner: team-C
# P1.16m: 12 条连接线集成测试 + P2.8: Hook 系统集成测试
#
# Phase 1.7 重构：消除 test_stubs.c 桩文件依赖
# - 8 个 PURE_DUPLICATE 测试已迁移到真实库链接（无 test_stubs.c）
# - 5 个 BEHAVIOR_DIFF 测试暂保留 test_stubs.c（Phase 2-4 修复后迁移）

# ============================================================================
# 公共 include 路径
# ============================================================================
set(INTEGRATION_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/coreloopthree/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/syscall/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/memory/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/llm_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/tool_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/monit_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/gateway_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/hook_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/daemon/market_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/config_unified/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/string/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/cache/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/compat/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/network/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/commons/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/cupolas/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/cupolas/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================================
# 辅助函数：配置使用真实库的集成测试（无 test_stubs.c）
# ============================================================================
function(configure_real_lib_test test_name)
    cmake_parse_arguments(TEST "" "" "LIBS" ${ARGN})

    add_executable(${test_name} ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.c)

    target_include_directories(${test_name} PRIVATE ${INTEGRATION_INCLUDE_DIRS})
    target_compile_features(${test_name} PRIVATE c_std_11)
    target_compile_options(${test_name} PRIVATE
        -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
        -Wno-unused-parameter
    )

    if(NOT MSVC AND NOT APPLE)
        target_link_libraries(${test_name} PRIVATE
            "-Wl,--start-group"
            ${TEST_LIBS}
            "-Wl,--end-group"
            Threads::Threads
            agentos_platform_libs
            m
        )
    else()
        target_link_libraries(${test_name} PRIVATE
            ${TEST_LIBS}
            Threads::Threads
            m
        )
    endif()

    add_test(NAME integration_${test_name}
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    message(STATUS "  [integration] ${test_name}: configured (real libs, no stubs)")
endfunction()

# ============================================================================
# 辅助函数：配置使用 test_stubs.c 的集成测试（BEHAVIOR_DIFF，待迁移）
# ============================================================================
function(configure_stub_test test_name)
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

    message(STATUS "  [integration] ${test_name}: configured (stubs, pending migration)")
endfunction()

# ============================================================================
# PURE_DUPLICATE 测试（7个）— 已迁移到真实库，无 test_stubs.c
# ============================================================================

# C-L03: market_d → OpenLab（自包含 mock，仅需头文件）
configure_real_lib_test(test_c_link_03_market_openlab
    LIBS agentos_common
)

# C-L06: Orchestrator → CoreLoopThree（orchestrator 真实实现）
# 注意：agentos_coreloopthree 已 PUBLIC 链接 svc_common（含 orchestrator.c），
# 链接器自动处理重复符号（仅从首个库提取 .o）
# LTO 链接下 --start-group/--end-group 必须包裹所有传递依赖，
# 否则组外库引用组内库符号时无法回溯解析。
# 显式列出所有传递依赖：
#   - agentos_coreloopthree: 主库（含 cognition/engine.c, execution/tool_svc_adapter.c）
#   - agentos_llm_service:   提供 llm_service_complete[_stream]/llm_response_free
#   - agentos_tool_service:  提供 tool_service_execute/tool_approval_check/tool_result_free
#   - agentos_cognition:     传递依赖（引用 tool/llm 符号）
#   - agentos_execution:     传递依赖（引用 tool_approval_check）
#   - agentos_core:          atoms 内核
#   - agentos_memory:        记忆模块
#   - svc_common:            daemon 公共库（含 orchestrator.c, checkpoint.c）
configure_real_lib_test(test_c_link_06_orchestrator_loop
    LIBS agentos_coreloopthree agentos_llm_service agentos_tool_service
         agentos_cognition agentos_execution agentos_core agentos_memory svc_common
)

# C-L07: Checkpoint → CoreLoopThree（checkpoint_adapter 真实实现）
configure_real_lib_test(test_c_link_07_checkpoint_loop
    LIBS agentos_coreloopthree
)

# C-L08: ServiceDiscovery → All Daemons（service_discovery 真实实现）
configure_real_lib_test(test_c_link_08_service_discovery
    LIBS svc_common
)

# C-L09: IPC Bus → All Daemons（ipc_bus_helper 真实实现）
configure_real_lib_test(test_c_link_09_ipc_bus
    LIBS svc_common
)

# C-L10: monit_d → Prometheus（prometheus_exporter 真实实现）
configure_real_lib_test(test_c_link_10_monit_prometheus
    LIBS agentos_monit_service
)

# P2.8: Hook System Integration（hook_registry/executor/interceptor/timeout 真实实现）
configure_real_lib_test(test_hook_system
    LIBS agentos_hook_service
)

# ============================================================================
# BEHAVIOR_DIFF / FICTIONAL_API 测试（6个）— 暂保留 test_stubs.c，Phase 2-4 修复后迁移
# ============================================================================

# C-L01: Manager → CoreLoopThree（env var override 行为差异）
configure_stub_test(test_c_link_01_manager_loop)

# C-L02: llm_d → CoreLoopThree（FICTIONAL_API）
# 注意：测试代码使用 llm_svc_adapter_t* + llm_svc_adapter_* 虚构 API，
#       而真实头文件 llm_svc_adapter.h 提供 llm_service_adapter_* 框架式 API
#       （agentos_service_t 句柄 + agentos_error_t 返回值），需 Phase 3 重写。
configure_stub_test(test_c_link_02_llm_loop)

# C-L04: tool_d → CoreLoopThree（is_connected 桩返回 true，真实返回 false）
configure_stub_test(test_c_link_04_tool_loop)

# C-L05: Cupolas SafetyGuard → tool_d（RBAC 硬编码规则行为差异）
configure_stub_test(test_c_link_05_safety_tool)

# C-L11: gateway_d → Gateway（已迁移到真实库 agentos_gateway_service）
# v0.1.1 修复：detect_proto 真实实现用 strncmp 前缀匹配 + body 检测，
# 默认返回 JSONRPC（非桩的 COUNT）。测试 CHECK 用 `|| proto < COUNT` 宽泛条件，
# 真实实现下仍通过。forward 调用不检查 ret（无后端 daemon 时超时返回错误）。
configure_real_lib_test(test_c_link_11_gateway_forward
    LIBS agentos_gateway_service
)

# C-L12: CoreLoopThree → MemoryRovol（已迁移到真实库）
# v0.1.1 修复：真实 memoryrovol_bridge_create 依赖 builtin_init 成功，
# 测试环境无 MemoryRovol 库时优雅降级到 builtin（弱符号机制）。
# config_path 为 NULL 时使用默认路径，builtin_storage_create 会创建目录。
configure_real_lib_test(test_c_link_12_memoryrovol_bridge
    LIBS agentos_coreloopthree agentos_memory agentos_core svc_common
)

# ============================================================================
# 已有测试 (保留)
# ============================================================================

# 端到端核心测试 (P2-C01)
if(TARGET svc_common)
    add_executable(test_e2e_core test_e2e_core.c)

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
        -Wl,--start-group
        agentos_cognition
        agentos_execution
        agentos_memory
        agentos_atoms
        agentos_llm_service
        agentos_tool_service
        -Wl,--end-group
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
