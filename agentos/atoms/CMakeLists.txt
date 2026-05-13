add_library(agentos_atoms INTERFACE)

target_link_libraries(agentos_atoms INTERFACE agentos_compile_defs)

target_compile_definitions(agentos_atoms INTERFACE AGENTOS_VERSION="${PROJECT_VERSION}")

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
    ${CMAKE_CURRENT_BINARY_DIR}/AgentOSConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AgentOSConfigVersion.cmake
    DESTINATION lib/cmake/AgentOS
)
