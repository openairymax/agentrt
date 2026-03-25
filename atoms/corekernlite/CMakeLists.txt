cmake_minimum_required(VERSION 3.10)

project(agentos_corekernlite 
    VERSION 1.0.0
    DESCRIPTION "AgentOS Lite Microkernel - Lightweight Core Kernel Implementation"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(AGENTOS_LITE_BUILD_SHARED "Build shared library" ON)
option(AGENTOS_LITE_BUILD_TESTS "Build test suite" ON)
option(AGENTOS_LITE_ENABLE_DEBUG "Enable debug logging" OFF)

if(AGENTOS_LITE_ENABLE_DEBUG)
    add_compile_definitions(AGENTOS_LITE_DEBUG)
endif()

set(COREKERNLITE_SOURCES
    src/error.c
    src/mem.c
    src/task.c
    src/time.c
    src/agentos_lite.c
)

set(COREKERNLITE_HEADERS
    include/export.h
    include/error.h
    include/mem.h
    include/task.h
    include/time.h
    include/agentos_lite.h
)

if(AGENTOS_LITE_BUILD_SHARED)
    add_library(agentos_corekernlite SHARED ${COREKERNLITE_SOURCES})
    target_compile_definitions(agentos_corekernlite PRIVATE AGENTOS_LITE_BUILD_DLL)
else()
    add_library(agentos_corekernlite STATIC ${COREKERNLITE_SOURCES})
endif()

target_include_directories(agentos_corekernlite
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(WIN32)
    target_compile_definitions(agentos_corekernlite PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
    )
    target_link_libraries(agentos_corekernlite PUBLIC kernel32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(agentos_corekernlite PUBLIC pthread rt)
elseif(APPLE)
    target_link_libraries(agentos_corekernlite PUBLIC pthread)
endif()

if(MSVC)
    target_compile_options(agentos_corekernlite PRIVATE
        /W4
        /WX-
        /MP
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(agentos_corekernlite PRIVATE /Od /Zi /RTC1)
        target_link_options(agentos_corekernlite PRIVATE /DEBUG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(agentos_corekernlite PRIVATE /O2 /GL)
        target_link_options(agentos_corekernlite PRIVATE /LTCG)
    endif()
else()
    target_compile_options(agentos_corekernlite PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fvisibility=hidden
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(agentos_corekernlite PRIVATE -O0 -g3 -fsanitize=address)
        target_link_options(agentos_corekernlite PRIVATE -fsanitize=address)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(agentos_corekernlite PRIVATE -O3 -DNDEBUG)
    endif()
endif()

set_target_properties(agentos_corekernlite PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${COREKERNLITE_HEADERS}"
    C_EXTENSIONS ON
)

if(AGENTOS_LITE_BUILD_TESTS)
    enable_testing()
    
    add_executable(test_corekernlite tests/test_main.c)
    target_link_libraries(test_corekernlite PRIVATE agentos_corekernlite)
    
    if(WIN32)
        target_compile_definitions(test_corekernlite PRIVATE
            _CRT_SECURE_NO_WARNINGS
        )
    endif()
    
    add_test(NAME CoreKernLiteTests COMMAND test_corekernlite)
endif()

install(TARGETS agentos_corekernlite
    EXPORT agentos_corekernlite-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/agentos/lite
)

install(FILES ${COREKERNLITE_HEADERS}
    DESTINATION include/agentos/lite
)

install(EXPORT agentos_corekernlite-targets
    FILE agentos_corekernlite-config.cmake
    NAMESPACE agentos::
    DESTINATION lib/cmake/agentos_corekernlite
)

message(STATUS "AgentOS CoreKernLite Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${AGENTOS_LITE_BUILD_SHARED}")
message(STATUS "  Tests: ${AGENTOS_LITE_BUILD_TESTS}")
message(STATUS "  Debug logging: ${AGENTOS_LITE_ENABLE_DEBUG}")
