# 监控服务 CMakeLists.txt

# 添加可执行文件
add_executable(monit_d 
    src/main.c
)

# 添加头文件目录
target_include_directories(monit_d PRIVATE 
    include 
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# 添加测试目标
if(BUILD_TESTS)
    add_executable(test_monitor test/test_monitor.c)
    target_include_directories(test_monitor PRIVATE 
        include 
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    )
    target_link_libraries(test_monitor PRIVATE 
        monit_d
    )
    add_test(NAME test_monitor COMMAND test_monitor)
endif()
