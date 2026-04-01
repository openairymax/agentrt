cmake_minimum_required(VERSION 3.15)
project(AgentOSheapstore VERSION 1.0.0.6 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build heapstore tests" ON)
option(BUILD_SHARED_LIBS "Build shared library" OFF)

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(heapstore_SOURCES
    src/heapstore_core.c
    src/heapstore_registry.c
    src/heapstore_trace.c
    src/heapstore_log.c
    src/heapstore_ipc.c
    src/heapstore_memory.c
    src/heapstore_integration.c
    src/utils.c
)

set(heapstore_HEADERS
    include/heapstore.h
    include/heapstore_registry.h
    include/heapstore_trace.h
    include/heapstore_log.h
    include/heapstore_ipc.h
    include/heapstore_memory.h
    include/heapstore_integration.h
    include/utils.h
    src/private.h
)

add_library(agentos_heapstore STATIC ${heapstore_SOURCES} ${heapstore_HEADERS})

target_include_directories(agentos_heapstore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(agentos_heapstore PRIVATE
    -DAGENTOS_heapstore_VERSION_MAJOR=1
    -DAGENTOS_heapstore_VERSION_MINOR=0
    -DAGENTOS_heapstore_VERSION_PATCH=6
)

find_package(Threads REQUIRED)
target_link_libraries(agentos_heapstore PRIVATE Threads::Threads)

if(NOT DEFINED SQLITE3_FOUND)
    find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
    find_library(SQLITE3_LIBRARY sqlite3)
    if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
        target_include_directories(agentos_heapstore PRIVATE ${SQLITE3_INCLUDE_DIR})
        target_link_libraries(agentos_heapstore PRIVATE ${SQLITE3_LIBRARY})
        target_compile_definitions(agentos_heapstore -Dheapstore_HAS_SQLITE3)
    endif()
endif()

install(TARGETS agentos_heapstore
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${heapstore_HEADERS}
    DESTINATION include/agentos/heapstore
)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "AgentOS heapstore v${PROJECT_VERSION}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build shared libs: ${BUILD_SHARED_LIBS}")

