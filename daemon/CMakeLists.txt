# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# AgentOS Daemon 模块顶层构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-daemon VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 构建选项
option(BUILD_TESTS "Build all unit tests" ON)
option(BUILD_COVERAGE "Build with code coverage support" OFF)
option(BUILD_ALL_PLATFORMS "Build for all platforms (cross-compile)" OFF)

# 版本信息
set(DAEMON_VERSION_MAJOR 1)
set(DAEMON_VERSION_MINOR 0)
set(DAEMON_VERSION_PATCH 0)
set(DAEMON_VERSION "${DAEMON_VERSION_MAJOR}.${DAEMON_VERSION_MINOR}.${DAEMON_VERSION_PATCH}")

# 打印构建配置
message(STATUS "========================================")
message(STATUS "  AgentOS Daemon Build Configuration")
message(STATUS "========================================")
message(STATUS "  Version:     ${DAEMON_VERSION}")
message(STATUS "  Build Type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:  ${CMAKE_C_COMPILER}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "  Coverage:    ${BUILD_COVERAGE}")
message(STATUS "========================================")

# 包含 bases 模块
add_subdirectory(bases)

# 包含各个服务模块
add_subdirectory(llm_d)
add_subdirectory(tool_d)
add_subdirectory(monit_d)
add_subdirectory(sched_d)
add_subdirectory(market_d)

# 全局测试支持
if(BUILD_TESTS)
    enable_testing()
    message(STATUS "  Unit tests:  ENABLED")
    
    # 收集所有测�?
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "  GTest:       NOT FOUND (tests may use custom framework)")
    endif()
else()
    message(STATUS "  Unit tests:  DISABLED")
endif()

# 打印覆盖率和分析信息
if(BUILD_COVERAGE)
    message(STATUS "  Coverage:    ENABLED")
    message(STATUS "  Coverage Target: ${COVERAGE_TARGET}%")
    message(STATUS "  Note: Use 'make coverage' to generate coverage reports")
    
    # 查找覆盖率工�?
    find_program(LCOV NAMES lcov)
    find_program(GENHTML NAMES genhtml)
    
    if(LCOV AND GENHTML)
        # 添加覆盖率报告目�?
        add_custom_target(coverage
            COMMAND ${LCOV} --capture --directory ${CMAKE_BINARY_DIR} --output-file coverage.info
            COMMAND ${LCOV} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${GENHTML} coverage.info --output-directory ${CMAKE_BINARY_DIR}/coverage_report
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report..."
        )
        
        # 添加覆盖率清理目�?
        add_custom_target(coverage-clean
            COMMAND ${CMAKE_COMMAND} -E remove -f *.gcda *.gcno coverage.info
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/coverage_report
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Cleaning coverage data..."
        )
    else()
        message(STATUS "  Warning: lcov/genhtml not found - coverage report disabled")
    endif()
endif()

message(STATUS "========================================")

# 打印所有子项目状�?
message(STATUS "")
message(STATUS "Sub-modules:")
message(STATUS "  - bases    : ${CMAKE_CURRENT_SOURCE_DIR}/bases")
message(STATUS "  - llm_d     : ${CMAKE_CURRENT_SOURCE_DIR}/llm_d")
message(STATUS "  - tool_d    : ${CMAKE_CURRENT_SOURCE_DIR}/tool_d")
message(STATUS "  - monit_d   : ${CMAKE_CURRENT_SOURCE_DIR}/monit_d")
message(STATUS "  - sched_d   : ${CMAKE_CURRENT_SOURCE_DIR}/sched_d")
message(STATUS "  - market_d  : ${CMAKE_CURRENT_SOURCE_DIR}/market_d")
message(STATUS "")

# 安装支持
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bases/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/llm_d/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tool_d/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/monit_d/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sched_d/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/market_d/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

# 导出项目配置
export(TARGETS svc_common agentos-llm-d agentos-tool-d 
              agentos-monit-d agentos-sched-d agentos-market-d
    FILE "${CMAKE_CURRENT_BINARY_DIR}/agentos-daemon-targets.cmake"
)

export(PACKAGE agentos-daemon)