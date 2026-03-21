# 市场服务 CMakeLists.txt

# 添加可执行文件
add_executable(market_d 
    src/main.c
    src/agent_registry.c
    src/skill_registry.c
    src/installer.c
    src/publisher.c
)

# 添加头文件目录
target_include_directories(market_d PRIVATE 
    include 
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# 添加测试目标
if(BUILD_TESTS)
    add_executable(test_market test/test_market.c)
    target_include_directories(test_market PRIVATE 
        include 
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    )
    target_link_libraries(test_market PRIVATE 
        market_d
    )
    add_test(NAME test_market COMMAND test_market)
endif()
