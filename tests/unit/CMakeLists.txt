
# AgentRT 单元测试入口
# R-09-SEC: Re-enabled after BAN-181 error code centralization fix.
# All error codes now come from the centralized error.h, eliminating
# the struct member conflicts and missing header issues.

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/atoms/coreloopthree/CMakeLists.txt")
    add_subdirectory(atoms/coreloopthree)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/commons/unit/CMakeLists.txt")
    add_subdirectory(commons/unit)
endif()
