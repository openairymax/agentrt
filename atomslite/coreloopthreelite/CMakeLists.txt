cmake_minimum_required(VERSION 3.10)

project(agentos_coreloopthreelite 
    VERSION 1.0.0
    DESCRIPTION "AgentOS Lite CoreLoopThree - Lightweight Three-Layer Runtime Implementation"
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

# 源文件列表 - 轻量化核心循环实现
set(CORELOOPTHREELITE_SOURCES
    src/core_loop_lite.c
    src/cognition_lite.c
    src/execution_lite.c
    src/memory_lite.c
)

# 头文件列表
set(CORELOOPTHREELITE_HEADERS
    include/core_loop_lite.h
    include/cognition_lite.h
    include/execution_lite.h
    include/memory_lite.h
    include/agentos_coreloopthreelite.h
)

if(AGENTOS_LITE_BUILD_SHARED)
    add_library(agentos_coreloopthreelite SHARED ${CORELOOPTHREELITE_SOURCES})
    target_compile_definitions(agentos_coreloopthreelite PRIVATE AGENTOS_LITE_BUILD_DLL)
else()
    add_library(agentos_coreloopthreelite STATIC ${CORELOOPTHREELITE_SOURCES})
endif()

target_include_directories(agentos_coreloopthreelite
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖：corekernlite
target_link_libraries(agentos_coreloopthreelite PUBLIC agentos_corekernlite)

if(WIN32)
    target_compile_definitions(agentos_coreloopthreelite PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
    )
    target_link_libraries(agentos_coreloopthreelite PUBLIC kernel32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(agentos_coreloopthreelite PUBLIC pthread rt)
elseif(APPLE)
    target_link_libraries(agentos_coreloopthreelite PUBLIC pthread)
endif()

if(MSVC)
    target_compile_options(agentos_coreloopthreelite PRIVATE
        /W4
        /WX-
        /MP
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(agentos_coreloopthreelite PRIVATE /Od /Zi /RTC1)
        target_link_options(agentos_coreloopthreelite PRIVATE /DEBUG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(agentos_coreloopthreelite PRIVATE /O2 /GL)
        target_link_options(agentos_coreloopthreelite PRIVATE /LTCG)
    endif()
else()
    target_compile_options(agentos_coreloopthreelite PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fvisibility=hidden
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(agentos_coreloopthreelite PRIVATE -O0 -g3 -fsanitize=address)
        target_link_options(agentos_coreloopthreelite PRIVATE -fsanitize=address)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(agentos_coreloopthreelite PRIVATE -O3 -DNDEBUG)
    endif()
endif()

set_target_properties(agentos_coreloopthreelite PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${CORELOOPTHREELITE_HEADERS}"
    C_EXTENSIONS ON
)

if(AGENTOS_LITE_BUILD_TESTS)
    enable_testing()
    
    add_executable(test_coreloopthreelite tests/test_coreloopthreelite.c)
    target_link_libraries(test_coreloopthreelite PRIVATE agentos_coreloopthreelite agentos_corekernlite)
    
    if(WIN32)
        target_compile_definitions(test_coreloopthreelite PRIVATE
            _CRT_SECURE_NO_WARNINGS
        )
    endif()
    
    add_test(NAME CoreLoopThreeLiteTests COMMAND test_coreloopthreelite)
endif()

install(TARGETS agentos_coreloopthreelite
    EXPORT agentos_coreloopthreelite-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/agentos/lite
)

install(FILES ${CORELOOPTHREELITE_HEADERS}
    DESTINATION include/agentos/lite
)

install(EXPORT agentos_coreloopthreelite-targets
    FILE agentos_coreloopthreelite-config.cmake
    NAMESPACE agentos::
    DESTINATION lib/cmake/agentos_coreloopthreelite
)

message(STATUS "AgentOS CoreLoopThreeLite Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${AGENTOS_LITE_BUILD_SHARED}")
message(STATUS "  Tests: ${AGENTOS_LITE_BUILD_TESTS}")
message(STATUS "  Debug logging: ${AGENTOS_LITE_ENABLE_DEBUG}")