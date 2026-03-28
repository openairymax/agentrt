cmake_minimum_required(VERSION 3.15)
project(AgentOSlodges VERSION 1.0.0.6 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build lodges tests" ON)
option(BUILD_SHARED_LIBS "Build shared library" OFF)

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(lodges_SOURCES
    src/lodges_core.c
    src/lodges_registry.c
    src/lodges_trace.c
    src/lodges_log.c
    src/lodges_ipc.c
    src/lodges_memory.c
)

set(lodges_HEADERS
    include/lodges.h
    include/lodges_registry.h
    include/lodges_trace.h
    include/lodges_log.h
    include/lodges_ipc.h
    include/lodges_memory.h
    src/private.h
)

add_library(agentos_lodges STATIC ${lodges_SOURCES} ${lodges_HEADERS})

target_include_directories(agentos_lodges PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(agentos_lodges PRIVATE
    -DAGENTOS_lodges_VERSION_MAJOR=1
    -DAGENTOS_lodges_VERSION_MINOR=0
    -DAGENTOS_lodges_VERSION_PATCH=6
)

find_package(Threads REQUIRED)
target_link_libraries(agentos_lodges PRIVATE Threads::Threads)

if(NOT DEFINED SQLITE3_FOUND)
    find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
    find_library(SQLITE3_LIBRARY sqlite3)
    if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
        target_include_directories(agentos_lodges PRIVATE ${SQLITE3_INCLUDE_DIR})
        target_link_libraries(agentos_lodges PRIVATE ${SQLITE3_LIBRARY})
        target_compile_definitions(agentos_lodges -Dlodges_HAS_SQLITE3)
    endif()
endif()

install(TARGETS agentos_lodges
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${lodges_HEADERS}
    DESTINATION include/agentos/lodges
)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "AgentOS lodges v${PROJECT_VERSION}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build shared libs: ${BUILD_SHARED_LIBS}")

