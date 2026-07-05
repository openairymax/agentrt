# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# AgentRT Daemon 模块顶层构建文件

agentrt_print_info("[DAEMON] Configuring daemon module")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 统一 daemon 二进制输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 构建选项
option(BUILD_TESTS "Build all unit tests" ON)
option(BUILD_COVERAGE "Build with code coverage support" OFF)
option(BUILD_ALL_PLATFORMS "Build for all platforms (cross-compile)" OFF)

# 版本信息
set(DAEMON_VERSION "${PROJECT_VERSION}")

# 打印构建配置
agentrt_print_info("========================================")
agentrt_print_info("  AgentRT Daemon Build Configuration")
agentrt_print_info("========================================")
agentrt_print_info("  Version:     ${DAEMON_VERSION}")
agentrt_print_info("  Build Type:  ${CMAKE_BUILD_TYPE}")
agentrt_print_info("  C Compiler:  ${CMAKE_C_COMPILER}")
agentrt_print_info("  Build Tests: ${BUILD_TESTS}")
agentrt_print_info("  Coverage:    ${BUILD_COVERAGE}")
agentrt_print_info("========================================")

# 包含 common 模块（兼容层）
add_subdirectory(common)

# 包含各个服务模块
add_subdirectory(gateway_d)
add_subdirectory(llm_d)
add_subdirectory(tool_d)
add_subdirectory(monit_d)
add_subdirectory(sched_d)
add_subdirectory(market_d)
add_subdirectory(channel_d)
add_subdirectory(info_d)
add_subdirectory(notify_d)
add_subdirectory(observe_d)
add_subdirectory(hook_d)
add_subdirectory(plugin_d)

# 全局测试支持
if(BUILD_TESTS)
    enable_testing()
    agentrt_print_ok("  Unit tests:  ENABLED")
    
    # 收集所有测试（GTest 已在根 CMakeLists.txt 中查找）
    if(AGENTRT_HAS_GTEST)
        agentrt_print_ok("  GTest:       FOUND")
    else()
        agentrt_print_warn("  GTest:       NOT FOUND (tests may use custom framework)")
    endif()
else()
    agentrt_print_warn("  Unit tests:  DISABLED")
endif()

# 打印覆盖率和分析信息
if(BUILD_COVERAGE)
    agentrt_print_ok("  Coverage:    ENABLED")
    agentrt_print_info("  Coverage Target: ${COVERAGE_TARGET}%")
    agentrt_print_info("  Note: Use 'make coverage' to generate coverage reports")
    
    # 查找覆盖率工具
    find_program(LCOV NAMES lcov)
    find_program(GENHTML NAMES genhtml)
    
    if(LCOV AND GENHTML)
        # 添加覆盖率报告目标
        add_custom_target(coverage
            COMMAND ${LCOV} --capture --directory ${CMAKE_BINARY_DIR} --output-file coverage.info
            COMMAND ${LCOV} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${GENHTML} coverage.info --output-directory ${CMAKE_BINARY_DIR}/coverage_report
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report..."
        )
        
        # 添加覆盖率清理目标
        add_custom_target(coverage-clean
            COMMAND ${CMAKE_COMMAND} -E remove -f *.gcda *.gcno coverage.info
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/coverage_report
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Cleaning coverage data..."
        )
    else()
        agentrt_print_warn("  Warning: lcov/genhtml not found - coverage report disabled")
    endif()
endif()

agentrt_print_info("========================================")

# 打印所有子项目状态
agentrt_print_info("")
agentrt_print_info("Sub-modules:")
agentrt_print_info("  - gateway_d : ${CMAKE_CURRENT_SOURCE_DIR}/gateway_d")
agentrt_print_info("  - llm_d     : ${CMAKE_CURRENT_SOURCE_DIR}/llm_d")
agentrt_print_info("  - tool_d    : ${CMAKE_CURRENT_SOURCE_DIR}/tool_d")
agentrt_print_info("  - monit_d   : ${CMAKE_CURRENT_SOURCE_DIR}/monit_d")
agentrt_print_info("  - sched_d   : ${CMAKE_CURRENT_SOURCE_DIR}/sched_d")
agentrt_print_info("  - market_d  : ${CMAKE_CURRENT_SOURCE_DIR}/market_d")
agentrt_print_info("  - channel_d : ${CMAKE_CURRENT_SOURCE_DIR}/channel_d")
agentrt_print_info("  - info_d    : ${CMAKE_CURRENT_SOURCE_DIR}/info_d")
agentrt_print_info("  - notify_d  : ${CMAKE_CURRENT_SOURCE_DIR}/notify_d")
agentrt_print_info("  - observe_d : ${CMAKE_CURRENT_SOURCE_DIR}/observe_d")
agentrt_print_info("  - hook_d    : ${CMAKE_CURRENT_SOURCE_DIR}/hook_d")
agentrt_print_info("  - plugin_d  : ${CMAKE_CURRENT_SOURCE_DIR}/plugin_d")
agentrt_print_info("")

# 安装支持
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/common/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gateway_d/include/
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

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/hook_d/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/plugin_d/include/
    DESTINATION include/agentos
    FILES_MATCHING PATTERN "*.h"
)

# 导出项目配置
if(TARGET svc_common AND TARGET gateway_d AND TARGET llm_d AND TARGET tool_d
   AND TARGET monit_d AND TARGET sched_d AND TARGET market_d
   AND TARGET channel_d AND TARGET info_d AND TARGET notify_d
   AND TARGET observe_d AND TARGET hook_d AND TARGET plugin_d)
    export(TARGETS svc_common gateway_d llm_d tool_d
                  monit_d sched_d market_d channel_d
                  info_d notify_d observe_d
                  hook_d plugin_d
        FILE "${CMAKE_CURRENT_BINARY_DIR}/agentrt-daemon-targets.cmake"
    )
    export(PACKAGE agentrt-daemon)
endif()