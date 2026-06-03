# AgentOS Commons 单元测试

add_executable(commons_unit_test
    io_test.c
    test_config.c
    test_cost.c
    test_input_validator.c
    test_ipc.c
    test_network.c
    test_observability.c
    test_platform.c
    test_resource_guard.c
    test_string_utils.c
    test_types.c
)

target_include_directories(commons_unit_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../agentos/commons/utils/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../agentos/commons/utils/observability/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../agentos/commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../agentos/commons/utils/token/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../agentos/commons/utils/io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../agentos/commons/utils/string/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../agentos/commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../agentos/commons/platform/include
)

target_link_libraries(commons_unit_test PRIVATE
    agentos_utils_core
    agentos_utils_observability
    agentos_utils_error
    agentos_utils_token
    agentos_utils_io
    pthread
)

add_test(NAME commons_unit_test COMMAND commons_unit_test)
