cmake_minimum_required(VERSION 3.20)

# AgentOS 顶层测试入口
# 包含各子模块测试和集成测试

# 集成测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/CMakeLists.txt")
    add_subdirectory(integration)
endif()

# 性能基准测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
    add_subdirectory(benchmarks)
endif()

# 安全审计测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/security/CMakeLists.txt")
    add_subdirectory(security)
endif()

# 跨平台兼容性测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/CMakeLists.txt")
    add_subdirectory(platform)
endif()
