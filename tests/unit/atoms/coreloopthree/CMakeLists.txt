
# AgentRT CoreLoopThree 单元测试
# 测试代码豁免 BAN 严格合规：测试允许使用裸 malloc/free/memset 等函数
add_compile_definitions(AGENTRT_COMPLIANCE_IMPL)

set(STUBS_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/agentrt/atoms/coreloopthree/tests/unit)
set(DAEMON_COMMON_INCLUDE ${CMAKE_SOURCE_DIR}/agentrt/daemons/common/include)
set(DAEMON_LLM_INCLUDE ${CMAKE_SOURCE_DIR}/agentrt/daemons/llm_d/include)
set(DAEMON_TOOL_INCLUDE ${CMAKE_SOURCE_DIR}/agentrt/daemons/tool_d/include)
set(CUPOLAS_SRC ${CMAKE_SOURCE_DIR}/agentrt/cupolas/src)
set(MEMORY_SRC ${CMAKE_SOURCE_DIR}/agentrt/atoms/memory/src)
set(MEMORY_INCLUDE ${CMAKE_SOURCE_DIR}/agentrt/atoms/memory)
set(COMMONS_QUALITY ${CMAKE_SOURCE_DIR}/agentrt/commons/utils/quality)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../build/lib)

add_executable(test_cognition test_cognition.c)
    target_include_directories(test_cognition PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_include_directories(test_cognition PRIVATE ${CMAKE_SOURCE_DIR}/agentrt/daemons/llm_d/include)
    target_include_directories(test_cognition PRIVATE ${STUBS_INCLUDE_DIR} ${DAEMON_COMMON_INCLUDE} ${DAEMON_LLM_INCLUDE} ${DAEMON_TOOL_INCLUDE} ${CUPOLAS_SRC} ${MEMORY_SRC} ${MEMORY_INCLUDE} ${COMMONS_QUALITY})
target_link_libraries(test_cognition
    -Wl,--whole-archive agentrt_coreloopthree -Wl,--no-whole-archive
    agentrt_cognition agentrt_execution agentrt_core agentrt_memory agentrt_atoms svc_common agentrt_common agentrt_llm_service agentrt_tool_service pthread dl m)

add_executable(test_execution test_execution.c)
    target_include_directories(test_execution PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_include_directories(test_execution PRIVATE ${STUBS_INCLUDE_DIR} ${DAEMON_COMMON_INCLUDE} ${DAEMON_LLM_INCLUDE} ${DAEMON_TOOL_INCLUDE} ${CUPOLAS_SRC} ${MEMORY_SRC} ${MEMORY_INCLUDE} ${COMMONS_QUALITY})
target_link_libraries(test_execution
    -Wl,--whole-archive agentrt_coreloopthree -Wl,--no-whole-archive
    agentrt_cognition agentrt_execution agentrt_core agentrt_memory agentrt_atoms svc_common agentrt_common agentrt_llm_service agentrt_tool_service pthread dl m)

add_executable(test_memory test_memory.c)
    target_include_directories(test_memory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_include_directories(test_memory PRIVATE ${STUBS_INCLUDE_DIR} ${DAEMON_COMMON_INCLUDE} ${DAEMON_LLM_INCLUDE} ${DAEMON_TOOL_INCLUDE} ${CUPOLAS_SRC} ${MEMORY_SRC} ${MEMORY_INCLUDE} ${COMMONS_QUALITY})
target_link_libraries(test_memory
    -Wl,--whole-archive agentrt_coreloopthree -Wl,--no-whole-archive
    agentrt_cognition agentrt_execution agentrt_core agentrt_memory agentrt_atoms svc_common agentrt_common agentrt_llm_service agentrt_tool_service pthread dl m)

add_executable(test_loop test_loop.c)
    target_include_directories(test_loop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_include_directories(test_loop PRIVATE ${STUBS_INCLUDE_DIR} ${DAEMON_COMMON_INCLUDE} ${DAEMON_LLM_INCLUDE} ${DAEMON_TOOL_INCLUDE} ${CUPOLAS_SRC} ${MEMORY_SRC} ${MEMORY_INCLUDE} ${COMMONS_QUALITY})
target_link_libraries(test_loop
    -Wl,--whole-archive agentrt_coreloopthree -Wl,--no-whole-archive
    agentrt_cognition agentrt_execution agentrt_core agentrt_memory agentrt_atoms svc_common agentrt_common agentrt_llm_service agentrt_tool_service pthread dl m)

add_executable(benchmark benchmark.c)
    target_include_directories(benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_include_directories(benchmark PRIVATE ${STUBS_INCLUDE_DIR} ${DAEMON_COMMON_INCLUDE} ${DAEMON_LLM_INCLUDE} ${DAEMON_TOOL_INCLUDE} ${CUPOLAS_SRC} ${MEMORY_SRC} ${MEMORY_INCLUDE} ${COMMONS_QUALITY})
target_link_libraries(benchmark
    -Wl,--whole-archive agentrt_coreloopthree -Wl,--no-whole-archive
    agentrt_cognition agentrt_execution agentrt_core agentrt_memory agentrt_atoms svc_common agentrt_common agentrt_llm_service agentrt_tool_service pthread dl m)

enable_testing()
add_test(NAME cognition_test COMMAND test_cognition)
add_test(NAME execution_test COMMAND test_execution)
add_test(NAME memory_test COMMAND test_memory)
add_test(NAME loop_test COMMAND test_loop)
