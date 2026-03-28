# atomsmini 顶层测试 CMakeLists.txt
# 包含集成测试和系统测试

enable_testing()

# 测试源文件
set(INTEGRATION_TEST_SOURCES
    test_atomsmini_integration.c
)

# 集成测试可执行文件
foreach(test_source ${INTEGRATION_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    
    add_executable(${test_name} ${test_source})
    
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/corekernlite/include
    )
    
    target_link_libraries(${test_name} PRIVATE
        agentos-kernel-lite
    )
    
    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

