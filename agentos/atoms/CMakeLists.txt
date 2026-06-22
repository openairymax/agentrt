add_library(agentos_atoms INTERFACE)

target_link_libraries(agentos_atoms INTERFACE agentos_compile_defs)

target_compile_definitions(agentos_atoms INTERFACE AGENTOS_VERSION="${PROJECT_VERSION}")

# 验证 atoms include 目录
message(STATUS "[ATOMS] Verifying include directories...")
set(_atoms_inc_dirs
    corekern/include
    coreloopthree/include
    syscall/include
    ../commons/utils/logging/include
    ../commons/utils/sync/include
    ../commons/utils/platform/include
    ../commons/utils/error/include
    ../commons/utils/types/include
    ../commons/utils/config_unified/include
    ../commons/utils/observability/include
    ../commons/utils/ipc/include
    ../commons/utils/io/include
    ../commons/utils/cache/include
    ../commons/utils/cost/include
    ../commons/utils/memory/include
    ../commons/utils/string/include
    ../commons/utils/token/include
    ../commons/utils/network/include
    ../commons/utils/security/src
    ../commons/utils/resource/src
    ../commons/utils/uuid/include
)
set(_atoms_inc_missing 0)
foreach(_dir ${_atoms_inc_dirs})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}")
        message(STATUS "[ATOMS]   [OK] ${_dir}")
    else()
        message(WARNING "[ATOMS]   [MISSING] ${_dir}")
        math(EXPR _atoms_inc_missing "${_atoms_inc_missing} + 1")
    endif()
endforeach()
if(_atoms_inc_missing GREATER 0)
    message(WARNING "[ATOMS]   ${_atoms_inc_missing} include directory(ies) MISSING!")
endif()

target_include_directories(agentos_atoms INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/corekern/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/coreloopthree/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/syscall/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/logging/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/sync/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/platform/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/error/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/types/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/config_unified/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/observability/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/ipc/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/io/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/cache/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/cost/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/memory/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/string/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/token/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/network/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/security/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/resource/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../commons/utils/uuid/include>
    $<INSTALL_INTERFACE:include/agentos>
)

add_subdirectory(corekern)
add_subdirectory(coreloopthree)
add_subdirectory(memory)
if(AGENTOS_WITH_MEMORYROVOL OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/memoryrovol/CMakeLists.txt")
    add_subdirectory(memoryrovol)
endif()
add_subdirectory(syscall)
add_subdirectory(taskflow)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/frameworks/CMakeLists.txt")
    add_subdirectory(frameworks)
endif()

install(DIRECTORY corekern/include/ DESTINATION include/agentos/corekern)
install(DIRECTORY coreloopthree/include/ DESTINATION include/agentos/coreloopthree)
install(DIRECTORY syscall/include/ DESTINATION include/agentos/syscall)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/AgentRTConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AgentRTConfigVersion.cmake
    DESTINATION lib/cmake/AgentRT
)
