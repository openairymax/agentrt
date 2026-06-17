
# AgentRT 端到端集成测试 (TeamC P2-C01)
# 覆盖: corekern+daemon+syscall 跨模块联动
# 场景: 初始化/配置/IPC/syscall/熔断/告警/多线程/追踪/定时器/压力/幂等

if(NOT TARGET svc_common)
    message(STATUS "  [integration] Skipping test_e2e_core: svc_common not available (BUILD_DAEMON=OFF)")
    return()
endif()

add_executable(test_e2e_core test_e2e_core.c)

target_include_directories(test_e2e_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../agentos/atoms/corekern/include
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
    svc_common
    Threads::Threads
    agentos_platform_libs
    m
)

if(AGENTOS_HAS_CJSON)
    target_link_libraries(test_e2e_core PRIVATE cjson)
endif()

target_compile_features(test_e2e_core PRIVATE c_std_11)
target_compile_options(test_e2e_core PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

add_test(NAME e2e_core COMMAND test_e2e_core
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "  [integration] test_e2e_core: P2-C01 E2E integration tests configured")

# ========================================================================
# LLM Routing Integration Test (INT-15)
# ========================================================================

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

if(AGENTOS_HAS_CJSON)
    target_link_libraries(test_llm_d_routing PRIVATE cjson)
endif()

if(AGENTOS_HAS_CURL)
    target_include_directories(test_llm_d_routing PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(test_llm_d_routing PRIVATE ${CURL_LIBRARIES})
endif()

target_compile_features(test_llm_d_routing PRIVATE c_std_11)
target_compile_options(test_llm_d_routing PRIVATE
    -Wall -Wextra -g -Wno-unused-function -Wno-unused-variable
)

add_test(NAME llm_d_routing COMMAND test_llm_d_routing
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "  [integration] test_llm_d_routing: INT-15 LLM routing integration tests configured")
