cmake_minimum_required(VERSION 3.20)

# MemoryRovol 模块单元测试 (TeamC)
# 覆盖: LRU缓存, 核心API, L1原始卷, 遗忘引擎
# 对齐: 看板v15.0 C-W1-003 / 规范手册v13.2 TEST-01~06

set(MEMORYROVOL_TEST_TARGETS
    test_lru_cache
    test_memoryrovol_core
    test_layer1_raw
    test_forgetting
)

foreach(test ${MEMORYROVOL_TEST_TARGETS})
    add_executable(${test} ${test}.c)
    target_include_directories(${test} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/string/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include
    )
    target_link_libraries(${test} PRIVATE 
        agentos_memoryrovol 
        agentos_common
    )
    
    if(UNIX AND NOT APPLE)
        target_link_options(${test} PRIVATE -Wl,--allow-multiple-definition)
    endif()
    
    add_test(NAME ${test} COMMAND ${test})
    message(STATUS "  memoryrovol test: ${test}")
endforeach()

message(STATUS "  memoryrovol_tests: configured (${MEMORYROVOL_TEST_TARGETS})")
