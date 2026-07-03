
# MemoryRovol bridge: integrates external MemoryRovol repo into AgentRT atoms tree
# Per R-09-01-6, MemoryRovol migrated to independent repository.
# This CMakeLists.txt bridges the external repo into the AgentRT build.
# The AGENTOS_WITH_MEMORYROVOL option is defined in the root CMakeLists.txt.

agentos_print_info("[MEMORYROVOL] AGENTOS_WITH_MEMORYROVOL=${AGENTOS_WITH_MEMORYROVOL}")

if(NOT AGENTOS_WITH_MEMORYROVOL)
    agentos_print_warn("[MEMORYROVOL] Integration disabled (set -DAGENTOS_WITH_MEMORYROVOL=ON to enable)")
    return()
endif()

# Locate external MemoryRovol source directory
# Search order:
#   1. MEMORYROVOL_DIR cmake variable
#   2. MEMORYROVOL_ROOT environment variable
#   3. Sibling directory relative to AgentRT root (OpenAirymax/MemoryRovol)
agentos_print_info("[MEMORYROVOL] Searching for external MemoryRovol source...")
if(NOT MEMORYROVOL_DIR)
    if(DEFINED ENV{MEMORYROVOL_ROOT})
        set(MEMORYROVOL_DIR "$ENV{MEMORYROVOL_ROOT}" CACHE PATH "External MemoryRovol source directory")
        agentos_print_ok("[MEMORYROVOL]   Found via MEMORYROVOL_ROOT env: ${MEMORYROVOL_DIR}")
    else()
        get_filename_component(_agentos_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
        get_filename_component(_workspace_root "${_agentos_root}/.." ABSOLUTE)
        set(_mr_candidate "${_workspace_root}/MemoryRovol")
        agentos_print_info("[MEMORYROVOL]   Checking sibling: ${_mr_candidate}")
        if(EXISTS "${_mr_candidate}/CMakeLists.txt")
            # CACHE + FORCE：使 MEMORYROVOL_DIR 在父目录及兄弟目录（coreloopthree）
            # 作用域可见。coreloopthree 的 CMakeLists 据此将 memoryrovol_bridge_api.c
            # 直接编译进 agentos_coreloopthree，解决静态库+弱符号不抽取问题。
            set(MEMORYROVOL_DIR "${_mr_candidate}" CACHE PATH "External MemoryRovol source directory" FORCE)
            agentos_print_ok("[MEMORYROVOL]   Found at sibling directory")
        else()
            agentos_print_warn("[MEMORYROVOL]   Sibling directory not found")
        endif()
    endif()
endif()

if(NOT MEMORYROVOL_DIR OR NOT EXISTS "${MEMORYROVOL_DIR}/CMakeLists.txt")
    agentos_print_warn("[MEMORYROVOL] Source not found! "
        "Set MEMORYROVOL_DIR or MEMORYROVOL_ROOT environment variable. "
        "Falling back to builtin provider only.")
    set(AGENTOS_WITH_MEMORYROVOL OFF CACHE BOOL "Build with MemoryRovol" FORCE)
    return()
endif()

agentos_print_ok("[MEMORYROVOL] Source found: ${MEMORYROVOL_DIR}")

# Propagate AgentRT workspace root and include paths to MemoryRovol build
set(AGENTOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." CACHE PATH "AgentRT workspace root directory" FORCE)
set(AGENTOS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "AgentRT agentos/ directory (includes daemon, atoms, etc.)" FORCE)
set(AGENTOS_ATOMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "AgentRT atoms source directory" FORCE)

# Inherit AgentRT build settings — build full PRO from source (not OSS)
# 尊重外部传入的 MEMORYROVOL_OSS 值（CI 的 OSS job 会传 ON）
if(NOT DEFINED MEMORYROVOL_OSS)
    set(MEMORYROVOL_OSS OFF CACHE BOOL "Build MemoryRovol in OSS mode (L1+L2 only)")
endif()
set(MEMORYROVOL_EMBEDDED ON CACHE BOOL "MemoryRovol embedded in AgentRT tree" FORCE)

# Add MemoryRovol as subdirectory with binary dir under AgentRT build tree
add_subdirectory("${MEMORYROVOL_DIR}" "${CMAKE_BINARY_DIR}/memoryrovol")

# Verify the target was created
if(NOT TARGET agentos_memoryrovol)
    agentos_print_warn("MemoryRovol build did not produce 'agentos_memoryrovol' target")
    return()
endif()

# Propagate AgentRT include paths to MemoryRovol source files
# The MemoryRovol external repo needs AgentRT headers for:
#   - memory_compat.h  (AGENTOS_MALLOC/CALLOC/FREE)
#   - service_logging.h (service_log_output_record)
#   - platform.h        (agentos_sleep_ms, agentos_thread_*)
target_include_directories(agentos_memoryrovol PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/include
)

# Force-include AgentRT compat headers for MemoryRovol sources
# that lack explicit #include of these headers
target_compile_options(agentos_memoryrovol PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/agentos_mr_force_includes.h"
)

# Create interface library that links MemoryRovol into AgentRT atoms
add_library(agentos_memoryrovol_bridge INTERFACE)
target_link_libraries(agentos_memoryrovol_bridge INTERFACE agentos_memoryrovol)
target_include_directories(agentos_memoryrovol_bridge INTERFACE
    ${MEMORYROVOL_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/logging/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/common/include
)

# Link MemoryRovol into the atoms aggregate
if(TARGET agentos_atoms)
    target_link_libraries(agentos_atoms INTERFACE agentos_memoryrovol)
endif()

agentos_print_ok("MemoryRovol integrated into AgentRT build (PRO mode)")
