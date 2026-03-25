# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Atoms Module Test Configuration
# Version: 1.0.0
#
# 本配置文件定义atoms模块的测试策略和覆盖率要求

cmake_minimum_required(VERSION 3.20)

# ==================== 测试框架配置 ====================

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_INTEGRATION_TESTS "Build integration tests" ON)
option(BUILD_BENCHMARK_TESTS "Build benchmark tests" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers (ASan, UBSan)" OFF)
option(ENABLE_FUZZING "Enable fuzzing tests" OFF)

# ==================== 测试框架选择 ====================

# 优先使用Google Test，其次Unity
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # 尝试下载Unity测试框架
    include(FetchContent)
    FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG v2.5.2
    )
    FetchContent_MakeAvailable(unity)
    set(TEST_FRAMEWORK "Unity")
else()
    set(TEST_FRAMEWORK "GTest")
endif()

# ==================== 覆盖率配置 ====================

if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
        
        # 查找gcovr
        find_program(GCOVR_EXECUTABLE gcovr)
        if(GCOVR_EXECUTABLE)
            # 添加覆盖率报告目标
            add_custom_target(coverage
                COMMAND ${GCOVR_EXECUTABLE}
                    -r ${CMAKE_SOURCE_DIR}
                    --xml -o ${CMAKE_BINARY_DIR}/coverage.xml
                    --html-details ${CMAKE_BINARY_DIR}/coverage.html
                    --exclude-unreachable-branches
                    --exclude-throw-branches
                    -j ${PROCESSOR_COUNT}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating coverage report..."
            )
            
            # 覆盖率质量门禁
            add_custom_target(coverage-check
                COMMAND ${GCOVR_EXECUTABLE}
                    -r ${CMAKE_SOURCE_DIR}
                    --fail-under-line 80
                    --fail-under-branch 70
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Checking coverage thresholds (80% line, 70% branch)..."
            )
        endif()
    elseif(MSVC)
        # MSVC使用OpenCppCoverage
        find_program(OPENCPPCOV_EXECUTABLE OpenCppCoverage)
        if(OPENCPPCOV_EXECUTABLE)
            add_custom_target(coverage
                COMMAND ${OPENCPPCOV_EXECUTABLE}
                    --sources ${CMAKE_SOURCE_DIR}
                    --export_type cobertura:${CMAKE_BINARY_DIR}/coverage.xml
                    --export_type html:${CMAKE_BINARY_DIR}/coverage_html
                    --cover_children
                    -- $<TARGET_FILE:test_runner>
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating coverage report with OpenCppCoverage..."
            )
        endif()
    endif()
endif()

# ==================== Sanitizer配置 ====================

if(ENABLE_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -fno-common
        )
        add_link_options(
            -fsanitize=address,undefined
        )
    elseif(MSVC)
        # MSVC使用运行时检查
        add_compile_options(/RTC1)
    endif()
endif()

# ==================== 模糊测试配置 ====================

if(ENABLE_FUZZING)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_compile_options(-fsanitize=fuzzer)
        add_link_options(-fsanitize=fuzzer)
        
        # 添加模糊测试目标
        add_custom_target(fuzz
                    COMMAND ${CMAKE_C_COMPILER}
                -fsanitize=fuzzer,address
                -o ${CMAKE_BINARY_DIR}/fuzz_runner
                ${CMAKE_SOURCE_DIR}/tests/fuzz/fuzz_main.c
            COMMAND ${CMAKE_BINARY_DIR}/fuzz_runner
                -max_total_time=60
                -max_len=4096
                -artifact_prefix=${CMAKE_BINARY_DIR}/fuzz_artifacts/
                ${CMAKE_BINARY_DIR}/fuzz_corpus
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running fuzzing tests..."
        )
    endif()
endif()

# ==================== 测试发现和注册 ====================

# 自动发现测试文件
function(discover_tests MODULE_NAME)
    file(GLOB_RECURSE TEST_SOURCES
        "${CMAKE_SOURCE_DIR}/tests/unit/${MODULE_NAME}/*.c"
        "${CMAKE_SOURCE_DIR}/tests/unit/${MODULE_NAME}/*.cpp"
    )
    
    if(TEST_SOURCES)
        if(TEST_FRAMEWORK STREQUAL "GTest")
            foreach(TEST_SOURCE ${TEST_SOURCES})
                get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
                
                add_executable(test_${MODULE_NAME}_${TEST_NAME}
                    ${TEST_SOURCE}
                )
                
                target_link_libraries(test_${MODULE_NAME}_${TEST_NAME}
                    PRIVATE
                    ${MODULE_NAME}
                    GTest::gtest
                    GTest::gtest_main
                )
                
                gtest_discover_tests(test_${MODULE_NAME}_${TEST_NAME}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    PROPERTIES
                        LABELS "${MODULE_NAME};unit"
                        TIMEOUT 300
                )
            endforeach()
        elseif(TEST_FRAMEWORK STREQUAL "Unity")
            # Unity测试框架集成
            add_executable(test_${MODULE_NAME}
                ${TEST_SOURCES}
                ${unity_SOURCE_DIR}/src/unity.c
            )
            
            target_link_libraries(test_${MODULE_NAME}
                PRIVATE
                ${MODULE_NAME}
            )
            
            target_include_directories(test_${MODULE_NAME}
                PRIVATE
                ${unity_SOURCE_DIR}/src
            )
            
            add_test(NAME ${MODULE_NAME}_tests
                COMMAND test_${MODULE_NAME}
            )
            
            set_tests_properties(${MODULE_NAME}_tests
                PROPERTIES
                    LABELS "${MODULE_NAME};unit"
                    TIMEOUT 300
            )
        endif()
    endif()
endfunction()

# ==================== 测试配置 ====================

# 设置测试属性
set(CTEST_TEST_TIMEOUT 300)
set(CTEST_MEMORYCHECK_TYPE AddressSanitizer)
set(CTEST_COVERAGE_TYPE gcov)

# 测试输出配置
set(CTEST_OUTPUT_ON_FAILURE ON)
set(CTEST_PROGRESS_OUTPUT ON)

# ==================== 内存检查配置 ====================

if(ENABLE_SANITIZERS OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_program(VALGRIND_EXECUTABLE valgrind)
    if(VALGRIND_EXECUTABLE)
        add_custom_target(memcheck
            COMMAND ${VALGRIND_EXECUTABLE}
                --tool=memcheck
                --leak-check=full
                --show-leak-kinds=all
                --track-origins=yes
                --error-exitcode=1
                --xml=yes
                --xml-file=${CMAKE_BINARY_DIR}/valgrind_report.xml
                $<TARGET_FILE:test_runner>
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running memory check with Valgrind..."
        )
    endif()
endif()

# ==================== 测试报告配置 ====================

# 生成JUnit XML报告
if(TEST_FRAMEWORK STREQUAL "GTest")
    set(GTEST_OUTPUT "xml:${CMAKE_BINARY_DIR}/test_results/")
endif()

# 生成HTML报告
find_program(PYTHON_EXECUTABLE python3 python)
if(PYTHON_EXECUTABLE)
    add_custom_target(test-report
        COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/scripts/generate_test_report.py
            --input ${CMAKE_BINARY_DIR}/test_results/
            --output ${CMAKE_BINARY_DIR}/test_report.html
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating test report..."
    )
endif()

# ==================== 测试覆盖率目标 ====================

# 定义覆盖率目标
set(COVERAGE_LINE_MIN 80)
set(COVERAGE_BRANCH_MIN 70)
set(COVERAGE_FUNCTION_MIN 85)

# 添加覆盖率检查到测试
if(ENABLE_COVERAGE AND GCOVR_EXECUTABLE)
    add_custom_target(test-coverage
        DEPENDS test coverage-check
        COMMENT "Running tests with coverage check..."
    )
endif()

# ==================== 消息输出 ====================

message(STATUS "=== Test Configuration ===")
message(STATUS "Test Framework: ${TEST_FRAMEWORK}")
message(STATUS "Build Tests: ${BUILD_TESTS}")
message(STATUS "Integration Tests: ${BUILD_INTEGRATION_TESTS}")
message(STATUS "Benchmark Tests: ${BUILD_BENCHMARK_TESTS}")
message(STATUS "Coverage: ${ENABLE_COVERAGE}")
message(STATUS "Sanitizers: ${ENABLE_SANITIZERS}")
message(STATUS "Fuzzing: ${ENABLE_FUZZING}")
if(ENABLE_COVERAGE)
    message(STATUS "Coverage Thresholds:")
    message(STATUS "  Line: ${COVERAGE_LINE_MIN}%")
    message(STATUS "  Branch: ${COVERAGE_BRANCH_MIN}%")
    message(STATUS "  Function: ${COVERAGE_FUNCTION_MIN}%")
endif()
