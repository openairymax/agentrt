cmake_minimum_required(VERSION 3.20)

# MemoryRovol bridge: integrates external MemoryRovol repo into AgentOS atoms tree
# Per R-09-01-6, MemoryRovol migrated to independent repository.
# This CMakeLists.txt bridges the external repo into the AgentOS build.
# The AGENTOS_WITH_MEMORYROVOL option is defined in the root CMakeLists.txt.

if(NOT AGENTOS_WITH_MEMORYROVOL)
    message(STATUS "MemoryRovol integration disabled (set -DAGENTOS_WITH_MEMORYROVOL=ON to enable)")
    return()
endif()

# Locate external MemoryRovol source directory
# Search order:
#   1. MEMORYROVOL_DIR cmake variable
#   2. MEMORYROVOL_ROOT environment variable
#   3. Sibling directory relative to AgentOS root (OpenAirymax/MemoryRovol)
if(NOT MEMORYROVOL_DIR)
    if(DEFINED ENV{MEMORYROVOL_ROOT})
        set(MEMORYROVOL_DIR "$ENV{MEMORYROVOL_ROOT}")
    else()
        get_filename_component(_agentos_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
        get_filename_component(_workspace_root "${_agentos_root}/.." ABSOLUTE)
        set(_mr_candidate "${_workspace_root}/MemoryRovol")
        if(EXISTS "${_mr_candidate}/CMakeLists.txt")
            set(MEMORYROVOL_DIR "${_mr_candidate}")
        endif()
    endif()
endif()

if(NOT MEMORYROVOL_DIR OR NOT EXISTS "${MEMORYROVOL_DIR}/CMakeLists.txt")
    message(WARNING "MemoryRovol source not found. "
        "Set MEMORYROVOL_DIR or MEMORYROVOL_ROOT environment variable. "
        "Falling back to builtin provider only.")
    set(AGENTOS_WITH_MEMORYROVOL OFF CACHE BOOL "Build with MemoryRovol" FORCE)
    return()
endif()

message(STATUS "MemoryRovol source found: ${MEMORYROVOL_DIR}")

# Propagate AgentOS include paths to MemoryRovol build
set(AGENTOS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "AgentOS atoms include directory")
set(AGENTOS_ATOMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "AgentOS atoms source directory")

# Inherit AgentOS build settings
set(MEMORYROVOL_OSS OFF CACHE BOOL "Build MemoryRovol in OSS mode (L1+L2 only)" FORCE)

# Add MemoryRovol as subdirectory with binary dir under AgentOS build tree
add_subdirectory("${MEMORYROVOL_DIR}" "${CMAKE_BINARY_DIR}/memoryrovol")

# Verify the target was created
if(NOT TARGET agentos_memoryrovol)
    message(WARNING "MemoryRovol build did not produce 'agentos_memoryrovol' target")
    return()
endif()

# Create interface library that links MemoryRovol into AgentOS atoms
add_library(agentos_memoryrovol_bridge INTERFACE)
target_link_libraries(agentos_memoryrovol_bridge INTERFACE agentos_memoryrovol)
target_include_directories(agentos_memoryrovol_bridge INTERFACE
    ${MEMORYROVOL_DIR}/include
)

# Link MemoryRovol into the atoms aggregate
if(TARGET agentos_atoms)
    target_link_libraries(agentos_atoms INTERFACE agentos_memoryrovol)
endif()

message(STATUS "MemoryRovol integrated into AgentOS build (PRO mode)")
