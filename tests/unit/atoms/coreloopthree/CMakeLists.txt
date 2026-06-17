
# AgentRT CoreLoopThree 单元测试

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../build/lib)

add_executable(test_cognition test_cognition.c)
    target_include_directories(test_cognition PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(test_cognition agentos_coreloopthree)

add_executable(test_execution test_execution.c)
    target_include_directories(test_execution PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(test_execution agentos_coreloopthree)

add_executable(test_memory test_memory.c)
    target_include_directories(test_memory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(test_memory agentos_coreloopthree)

add_executable(test_loop test_loop.c)
    target_include_directories(test_loop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(test_loop agentos_coreloopthree)

add_executable(benchmark benchmark.c)
    target_include_directories(benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(benchmark agentos_coreloopthree)

enable_testing()
add_test(NAME cognition_test COMMAND test_cognition)
add_test(NAME execution_test COMMAND test_execution)
add_test(NAME memory_test COMMAND test_memory)
add_test(NAME loop_test COMMAND test_loop)
