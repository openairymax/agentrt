# commons 模块测试 CMakeLists.txt
# 包含单元测试和集成测试

# 测试配置
enable_testing()

# 测试源文件
set(UNIT_TEST_SOURCES
    unit/test_platform.c
    unit/test_error.c
    unit/test_logger.c
    unit/test_config.c
    unit/test_token.c
    unit/test_cost.c
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
        ${CMAKE_SOURCE_DIR}/platform/include
        ${CMAKE_SOURCE_DIR}/utils/core/include
        ${CMAKE_SOURCE_DIR}/utils/error/include
        ${CMAKE_SOURCE_DIR}/utils/observability/include
        ${CMAKE_SOURCE_DIR}/utils/manager/include
        ${CMAKE_SOURCE_DIR}/utils/token/include
        ${CMAKE_SOURCE_DIR}/utils/cost/include
    )
    
    target_link_libraries(${test_name} PRIVATE
        agentos_common
    )
    
    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

# 集成测试可执行文件
foreach(test_source ${INTEGRATION_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    
    add_executable(${test_name} ${test_source})
    
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/platform/include
        ${CMAKE_SOURCE_DIR}/utils/core/include
        ${CMAKE_SOURCE_DIR}/utils/error/include
        ${CMAKE_SOURCE_DIR}/utils/observability/include
        ${CMAKE_SOURCE_DIR}/utils/manager/include
        ${CMAKE_SOURCE_DIR}/utils/token/include
        ${CMAKE_SOURCE_DIR}/utils/cost/include
    )
    
    target_link_libraries(${test_name} PRIVATE
        agentos_common
    )
    
    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

# 测试覆盖率配置
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(agentos_common PRIVATE --coverage)
        target_link_options(agentos_common PRIVATE --coverage)
    endif()
endif()
