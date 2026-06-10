
add_library(agentos_cognition STATIC
    engine.c
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
    foundation/metacognition.c
    foundation/semantic_unit.c
    foundation/thinking_chain.c
    critique/stream_critic.c
    dispatch/parallel_dispatcher.c
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

target_include_directories(agentos_cognition PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory
)

target_link_libraries(agentos_cognition
    agentos_core
    agentos_memory
    m
)

if(AGENTOS_CJSON_FOUND)
    target_compile_definitions(agentos_cognition PRIVATE AGENTOS_HAS_CJSON=1)
    target_include_directories(agentos_cognition PRIVATE ${AGENTOS_CJSON_INCLUDE_DIRS})
    target_link_libraries(agentos_cognition ${AGENTOS_CJSON_LIBRARIES})
endif()

if(WIN32)
    target_compile_options(agentos_cognition PRIVATE /W3 /O2)
else()
    target_compile_options(agentos_cognition PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS agentos_cognition DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cognition.h DESTINATION include/agentos)
