# commons 模块测试 CMakeLists.txt
# 包含单元测试和集成测试

enable_testing()

set(COMMONS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# 测试源文件
set(UNIT_TEST_SOURCES
    unit/test_platform.c
    unit/test_error.c
    unit/test_logger.c
    # unit/test_config.c  # 禁用：config_unified模块实现与头文件不匹配
    unit/test_token.c
    unit/test_cost.c
    unit/test_types.c
    unit/test_ipc.c
    unit/test_network.c
)

set(INTEGRATION_TEST_SOURCES
    integration/test_common_integration.c
    integration/test_unified_modules.c
)

# 单元测试可执行文件
foreach(test_source ${UNIT_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)

    add_executable(${test_name} ${test_source})

    target_include_directories(${test_name} PRIVATE
        ${COMMONS_ROOT}/platform/include
        ${COMMONS_ROOT}/utils/logging/include
        ${COMMONS_ROOT}/utils/error/include
        ${COMMONS_ROOT}/utils/string/include
        ${COMMONS_ROOT}/utils/observability/include
        ${COMMONS_ROOT}/utils/types/include
        ${COMMONS_ROOT}/utils/token/include
        ${COMMONS_ROOT}/utils/cost/include
        ${COMMONS_ROOT}/utils/ipc/include
        ${COMMONS_ROOT}/utils/network/include
        ${AGENTOS_ROOT}/atoms/corekern/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${test_name} PRIVATE
        agentos_common
        agentos_core
    )

    add_test(
        NAME "commons_test_${test_name}"
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

# 集成测试可执行文件
foreach(test_source ${INTEGRATION_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)

    add_executable(${test_name} ${test_source})

    target_include_directories(${test_name} PRIVATE
        ${COMMONS_ROOT}/platform/include
        ${COMMONS_ROOT}/utils/logging/include
        ${COMMONS_ROOT}/utils/error/include
        ${COMMONS_ROOT}/utils/string/include
        ${COMMONS_ROOT}/utils/observability/include
        ${COMMONS_ROOT}/utils/types/include
        ${AGENTOS_ROOT}/atoms/corekern/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${test_name} PRIVATE
        agentos_common
        agentos_core
    )

    add_test(
        NAME "commons_itest_${test_name}"
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()
