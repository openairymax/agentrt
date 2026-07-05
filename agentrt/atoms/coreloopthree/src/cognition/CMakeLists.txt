
add_library(agentrt_cognition STATIC
    engine.c
    llm_svc_adapter.c
    intent/intent_parser.c
    intent/intent_classifier.c
    intent/intent_utils.c
    intent/entity_extractor.c
    context/context_processor.c
    dispatch/delegate.c
    ext/multi_agent_collaboration.c
    critique/confidence_calibrator.c
    ext/cognitive_evolution.c
    critique/triple_coordinator.c
    critique/tc3_llm_callbacks.c
    foundation/metacognition.c
    foundation/semantic_unit.c
    foundation/thinking_chain.c
    critique/stream_critic.c
    dispatch/parallel_dispatcher.c
    planner/llm_client.c
    planner/reactive.c
    planner/hierarchical.c
    planner/reflective.c
    planner/ml_planner.c
    coordinator/arbiter.c
    coordinator/majority.c
    coordinator/weighted.c
    coordinator/dual_model.c
    coordinator/coordinator_adapter.c
    dispatcher/round_robin.c
    dispatcher/priority.c
    dispatcher/weighted.c
    dispatcher/ml_based.c
    ../memory/engine.c
)

target_include_directories(agentrt_cognition PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../memory/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../daemons/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../daemons/llm_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../daemons/tool_d/include
    # P3.18 (ACC-DT26): hook_d → CoreLoopThree — engine.c 引用 agentrt_hook.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../daemons/hook_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../commons/utils/quality
)

target_link_libraries(agentrt_cognition
    agentrt_core
    agentrt_memory
    m
)

if(AGENTRT_CJSON_FOUND)
    target_compile_definitions(agentrt_cognition PRIVATE AGENTRT_HAS_CJSON=1)
    target_include_directories(agentrt_cognition PRIVATE ${AGENTRT_CJSON_INCLUDE_DIRS})
    target_link_libraries(agentrt_cognition ${AGENTRT_CJSON_LIBRARIES})
endif()

if(WIN32)
    target_compile_options(agentrt_cognition PRIVATE /W3 /O2)
else()
    target_compile_options(agentrt_cognition PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS agentrt_cognition DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cognition.h DESTINATION include/agentrt)
