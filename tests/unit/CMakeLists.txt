
# AgentOS 单元测试入口

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/atoms/coreloopthree/CMakeLists.txt")
    #     add_subdirectory(atoms/coreloopthree)  # FIXME: struct member changes
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/commons/unit/CMakeLists.txt")
    #     add_subdirectory(commons/unit)  # FIXME: missing headers (io.h, manager.h, cost.h)
endif()
