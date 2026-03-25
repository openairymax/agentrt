cmake_minimum_required(VERSION 3.15)
project(AgentOSPartdata VERSION 1.0.0.6 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build partdata tests" ON)
option(BUILD_SHARED_LIBS "Build shared library" OFF)

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(PARTDATA_SOURCES
    src/partdata_core.c
    src/partdata_registry.c
    src/partdata_trace.c
    src/partdata_log.c
    src/partdata_ipc.c
    src/partdata_memory.c
)

set(PARTDATA_HEADERS
    include/partdata.h
    include/partdata_registry.h
    include/partdata_trace.h
    include/partdata_log.h
    include/partdata_ipc.h
    include/partdata_memory.h
    src/private.h
)

add_library(agentos_partdata STATIC ${PARTDATA_SOURCES} ${PARTDATA_HEADERS})

target_include_directories(agentos_partdata PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(agentos_partdata PRIVATE
    -DAGENTOS_PARTDATA_VERSION_MAJOR=1
    -DAGENTOS_PARTDATA_VERSION_MINOR=0
    -DAGENTOS_PARTDATA_VERSION_PATCH=6
)

find_package(Threads REQUIRED)
target_link_libraries(agentos_partdata PRIVATE Threads::Threads)

if(NOT DEFINED SQLITE3_FOUND)
    find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
    find_library(SQLITE3_LIBRARY sqlite3)
    if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
        target_include_directories(agentos_partdata PRIVATE ${SQLITE3_INCLUDE_DIR})
        target_link_libraries(agentos_partdata PRIVATE ${SQLITE3_LIBRARY})
        target_compile_definitions(agentos_partdata -DPARTDATA_HAS_SQLITE3)
    endif()
endif()

install(TARGETS agentos_partdata
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${PARTDATA_HEADERS}
    DESTINATION include/agentos/partdata
)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "AgentOS Partdata v${PROJECT_VERSION}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build shared libs: ${BUILD_SHARED_LIBS}")
