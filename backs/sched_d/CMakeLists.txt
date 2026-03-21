# 调度服务 CMakeLists.txt

# 添加可执行文件
add_executable(sched_d 
    src/main.c 
    src/monitor.c 
    src/strategies/weighted.c 
    src/strategies/round_robin.c 
    src/strategies/ml_based.c
)

# 添加头文件目录
target_include_directories(sched_d PRIVATE 
    include 
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# 添加测试目标
if(BUILD_TESTS)
    add_executable(test_scheduler test/test_scheduler.c)
    target_include_directories(test_scheduler PRIVATE 
        include 
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    )
    target_link_libraries(test_scheduler PRIVATE 
        sched_d
    )
    add_test(NAME test_scheduler COMMAND test_scheduler)
endif()
