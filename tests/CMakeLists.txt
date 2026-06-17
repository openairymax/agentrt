
# AgentRT 顶层测试入口
# 包含各子模块测试和集成测试

# 单元测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/CMakeLists.txt")
    add_subdirectory(unit)
endif()

# 集成测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/c/CMakeLists.txt")
    add_subdirectory(integration/c)
endif()

# 性能基准测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/c/CMakeLists.txt")
    add_subdirectory(benchmarks/c)
endif()

# 安全审计测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/security/c/CMakeLists.txt")
    add_subdirectory(security/c)
endif()

# 跨平台兼容性测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/c/CMakeLists.txt")
    add_subdirectory(platform/c)
endif()
