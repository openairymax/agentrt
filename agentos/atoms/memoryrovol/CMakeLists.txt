# Copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
# MemoryRovol AgentOS 集成层构建文件
#
# 本文件仅负责：
#   1. 安装 MemoryRovol 公共头文件到 AgentOS
#   2. 通过 MEMORYROVOL_SOURCE 选项决定集成方式：
#      - ON  (默认): 从 MemoryRovol 独立仓库源码编译
#      - OFF:        链接预编译商业静态库 (.a/.lib)
#
# MemoryRovol 的所有源代码实现位于独立仓库：
#   ${MEMORYROVOL_DIR} (默认: ../../../../../MemoryRovol)
#
# BAN-35 合规：AgentOS 仓库中不含任何 MemoryRovol 实现源码

cmake_minimum_required(VERSION 3.20)

option(MEMORYROVOL_SOURCE "Build MemoryRovol from source (independent repo)" ON)

if(DEFINED MEMORYROVOL_PRO_LIB)
    if(MEMORYROVOL_SOURCE)
        message(WARNING
            "MEMORYROVOL_PRO_LIB is set, overriding MEMORYROVOL_SOURCE=ON.\n"
            "Will link against precompiled PRO library instead of building from source."
        )
        set(MEMORYROVOL_SOURCE OFF)
    endif()
    if(NOT EXISTS "${MEMORYROVOL_PRO_LIB}")
        message(FATAL_ERROR
            "MEMORYROVOL_PRO_LIB specified but file not found: ${MEMORYROVOL_PRO_LIB}"
        )
    endif()
    message(STATUS "MemoryRovol PRO mode: linking precompiled library ${MEMORYROVOL_PRO_LIB}")
    set(MEMORYROVOL_PRO_MODE ON)
else()
    set(MEMORYROVOL_PRO_MODE OFF)
endif()

if(MEMORYROVOL_SOURCE AND NOT MEMORYROVOL_PRO_MODE)
    if(NOT DEFINED MEMORYROVOL_DIR)
        set(MEMORYROVOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../MemoryRovol")
    endif()

    if(NOT EXISTS "${MEMORYROVOL_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "MemoryRovol source directory not found: ${MEMORYROVOL_DIR}\n"
            "Please set MEMORYROVOL_DIR to the MemoryRovol independent repository path,\n"
            "or set MEMORYROVOL_PRO_LIB to link a precompiled library."
        )
    endif()

    message(STATUS "MemoryRovol SOURCE mode: building from ${MEMORYROVOL_DIR}")

    set(AGENTOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." CACHE PATH "AgentOS root directory")

    set(BUILD_TESTS ON)

    add_subdirectory(${MEMORYROVOL_DIR} ${CMAKE_BINARY_DIR}/memoryrovol-build)

elseif(MEMORYROVOL_PRO_MODE)
    add_library(agentos_memoryrovol STATIC IMPORTED)
    set_target_properties(agentos_memoryrovol PROPERTIES
        IMPORTED_LOCATION "${MEMORYROVOL_PRO_LIB}"
    )
    target_include_directories(agentos_memoryrovol INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/agentos/memoryrovol>
    )
else()
    message(FATAL_ERROR
        "No MemoryRovol integration mode selected.\n"
        "Set either MEMORYROVOL_SOURCE=ON (default) or MEMORYROVOL_PRO_LIB=<path>."
    )
endif()

install(DIRECTORY include/ DESTINATION include/agentos/memoryrovol)
