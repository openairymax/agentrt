
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# 使用根级依赖检测结果 (BAN-12: 单一依赖检测点)
if(AGENTRT_HAS_CJSON)
    set(AGENTRT_CJSON_FOUND ON CACHE INTERNAL "cJSON available for coreloopthree")
    if(cJSON_FOUND)
        set(AGENTRT_CJSON_INCLUDE_DIRS "${cJSON_INCLUDE_DIRS}" CACHE INTERNAL "")
        set(AGENTRT_CJSON_LIBRARIES "${cJSON_LIBRARIES}" CACHE INTERNAL "")
    elseif(CJSON_FOUND)
        set(AGENTRT_CJSON_INCLUDE_DIRS "${CJSON_INCLUDE_DIRS}" CACHE INTERNAL "")
        set(AGENTRT_CJSON_LIBRARIES "${CJSON_LIBRARIES}" CACHE INTERNAL "")
    endif()
    agentrt_print_ok("coreloopthree: cJSON available")
else()
    agentrt_print_warn("coreloopthree: cJSON not found, some features disabled")
endif()

add_subdirectory(src/cognition)
add_subdirectory(src/execution)

if(BUILD_TESTS)
    option(BUILD_CORELOOPTHREE_TESTS "Build CoreLoopThree test suite" ON)
    if(BUILD_CORELOOPTHREE_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()

add_library(agentrt_coreloopthree STATIC
    src/loop.c
    src/config_loader.c
    src/yaml_loader.c
    src/prompt_loader.c
    src/orch_adapter.c
    src/checkpoint_adapter.c
    src/manager_adapter.c
    src/memory/memory_service.c
    src/memory/memoryrovol_bridge.c
    src/orchestrator.c        # SP02: 从 daemons/common/src/ 迁入（编排器与 coreloopthree 强耦合）
    # P3.18 (ACC-DT26): hook_d → CoreLoopThree — 关键事件 hook 触发
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/hook_d/src/service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/hook_d/src/hook_executor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/hook_d/src/hook_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/hook_d/src/hook_interceptor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/hook_d/src/hook_timeout.c
    # P0.20.1: 生产 Hook 处理器 — 审计/metrics/trace 内置注册
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/hook_d/src/hook_builtin_handlers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/hook_d/src/hook_audit_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/hook_d/src/hook_metrics_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/hook_d/src/hook_trace_handler.c
)

target_link_libraries(agentrt_coreloopthree PUBLIC agentrt_atoms)

# C-L12: MemoryRovol 构建模式
# 三种模式：
#   AGENTRT_MEMORY_BUILTIN    — 仅内置免费提供商
#   AGENTRT_MEMORY_MEMORYROVOL — 仅 MemoryRovol 商业提供商
#   AGENTRT_MEMORY_HYBRID     — 混合模式（默认，根据配置切换）
if(NOT DEFINED AGENTRT_MEMORY_MODE)
    set(AGENTRT_MEMORY_MODE "HYBRID" CACHE STRING "Memory provider mode: BUILTIN, MEMORYROVOL, HYBRID")
endif()
agentrt_print_info("coreloopthree: AGENTRT_MEMORY_MODE=${AGENTRT_MEMORY_MODE}")

if(AGENTRT_MEMORY_MODE STREQUAL "BUILTIN")
    target_compile_definitions(agentrt_coreloopthree PRIVATE
        AGENTRT_MEMORY_BUILTIN=1
    )
    agentrt_print_info("coreloopthree: Memory mode = BUILTIN (free provider only)")
elseif(AGENTRT_MEMORY_MODE STREQUAL "MEMORYROVOL")
    target_compile_definitions(agentrt_coreloopthree PRIVATE
        AGENTRT_MEMORY_MEMORYROVOL=1
    )
    if(TARGET agentrt_memoryrovol)
        target_link_libraries(agentrt_coreloopthree PRIVATE agentrt_memoryrovol)
        agentrt_print_info("coreloopthree: Memory mode = MEMORYROVOL (commercial provider)")
    else()
        agentrt_print_warn("coreloopthree: MEMORYROVOL mode but agentrt_memoryrovol target not found, falling back to BUILTIN")
        target_compile_definitions(agentrt_coreloopthree PRIVATE AGENTRT_MEMORY_BUILTIN=1)
    endif()
else()
    # 默认 HYBRID 模式
    target_compile_definitions(agentrt_coreloopthree PRIVATE
        AGENTRT_MEMORY_HYBRID=1
    )
    if(TARGET agentrt_memoryrovol)
        target_link_libraries(agentrt_coreloopthree PRIVATE agentrt_memoryrovol)
        agentrt_print_info("coreloopthree: Memory mode = HYBRID (builtin + MemoryRovol)")
    else()
        agentrt_print_warn("coreloopthree: Memory mode = HYBRID (MemoryRovol not found, builtin fallback available)")
    endif()
endif()

# 静态库 + 弱符号语义修复：memoryrovol_bridge.c 用 __attribute__((weak)) 引用
# memoryrovol_init 等 16 个符号。弱未定义引用不会触发归档成员抽取，因此即使
# agentrt_memoryrovol 被链接，memoryrovol_bridge_api.c（提供强符号）也不会被
# 从 libagentrt_memoryrovol.a 中抽取，弱符号恒为 NULL，bridge 永远降级 builtin。
# 修复：将 memoryrovol_bridge_api.c 直接编译进 agentrt_coreloopthree（条件式），
# 使强符号与弱引用同在一个静态库内，链接器在归档内部即可解析弱→强覆盖。
# 该文件内部转调 agentrt_memoryrov_* API（强引用），正常触发 memoryrovol.a 抽取。
if(TARGET agentrt_memoryrovol AND DEFINED MEMORYROVOL_DIR)
    target_sources(agentrt_coreloopthree PRIVATE
        "${MEMORYROVOL_DIR}/src/memoryrovol_bridge_api.c")
    target_include_directories(agentrt_coreloopthree PRIVATE
        "${MEMORYROVOL_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include")
    agentrt_print_ok("coreloopthree: memoryrovol_bridge_api.c compiled in (weak→strong symbol fix)")
endif()

target_include_directories(agentrt_coreloopthree PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../taskflow/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/llm_d/include>
    $<INSTALL_INTERFACE:include/agentrt/coreloopthree>
)

if(MSVC)
    target_compile_options(agentrt_coreloopthree PRIVATE /W4 /WX)
    target_compile_definitions(agentrt_coreloopthree PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(agentrt_coreloopthree PRIVATE /GS)
    target_compile_options(agentrt_coreloopthree PRIVATE /sdl)
    target_compile_options(agentrt_coreloopthree PRIVATE /guard:cf)
else()
    target_compile_options(agentrt_coreloopthree PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(agentrt_coreloopthree PRIVATE -fstack-protector-strong)
    target_compile_options(agentrt_coreloopthree PRIVATE $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>)
    target_compile_options(agentrt_coreloopthree PRIVATE -fPIE)
    target_compile_options(agentrt_coreloopthree PRIVATE -Wformat -Wformat-security)
    target_compile_options(agentrt_coreloopthree PRIVATE -fno-omit-frame-pointer)
endif()

if(UNIX AND NOT APPLE)
    target_link_options(agentrt_coreloopthree PRIVATE -Wl,-z,relro -Wl,-z,now -pie)
elseif(APPLE)
    target_link_options(agentrt_coreloopthree PRIVATE -Wl,-dead_strip -pie)
endif()

target_include_directories(agentrt_coreloopthree PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cognition
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cognition/critique
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cognition/foundation
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../taskflow/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/llm_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/tool_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemons/hook_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/quality
)

target_link_libraries(agentrt_coreloopthree PUBLIC
    agentrt_cognition
    agentrt_execution
    agentrt_core
    agentrt_memory
    agentrt_taskflow
    svc_common
)

if(AGENTRT_CJSON_FOUND)
    target_compile_definitions(agentrt_coreloopthree PRIVATE AGENTRT_HAS_CJSON=1)
    target_include_directories(agentrt_coreloopthree PRIVATE ${AGENTRT_CJSON_INCLUDE_DIRS})
    target_link_libraries(agentrt_coreloopthree PRIVATE ${AGENTRT_CJSON_LIBRARIES})
endif()

if(WIN32)
    target_link_libraries(agentrt_coreloopthree PRIVATE ws2_32)
elseif(UNIX)
    target_link_libraries(agentrt_coreloopthree PRIVATE uuid)
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/agentrt
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS agentrt_coreloopthree
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
