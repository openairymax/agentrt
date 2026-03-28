cmake_minimum_required(VERSION 3.10)

project(agentos_memoryrovollite 
    VERSION 1.0.0
    DESCRIPTION "AgentOS Lite MemoryRovol - Lightweight Memory Management and Persistence Implementation"
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

# 源文件列表 - 轻量化内存管理实现
set(MEMORYROVOLLITE_SOURCES
    src/memory_rovol_lite.c
    src/storage_lite.c
    src/vector_lite.c
    src/retrieval_lite.c
    src/persistence_lite.c
)

# 头文件列表
set(MEMORYROVOLLITE_HEADERS
    include/memory_rovol_lite.h
    include/storage_lite.h
    include/vector_lite.h
    include/retrieval_lite.h
    include/persistence_lite.h
    include/agentos_memoryrovollite.h
)

if(AGENTOS_LITE_BUILD_SHARED)
    add_library(agentos_memoryrovollite SHARED ${MEMORYROVOLLITE_SOURCES})
    target_compile_definitions(agentos_memoryrovollite PRIVATE AGENTOS_LITE_BUILD_DLL)
else()
    add_library(agentos_memoryrovollite STATIC ${MEMORYROVOLLITE_SOURCES})
endif()

target_include_directories(agentos_memoryrovollite
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖：corekernlite
target_link_libraries(agentos_memoryrovollite PUBLIC agentos_corekernlite)

# 链接外部库：SQLite3（用于轻量化存储）
find_package(SQLite3 REQUIRED)
target_link_libraries(agentos_memoryrovollite PUBLIC SQLite::SQLite3)

if(WIN32)
    target_compile_definitions(agentos_memoryrovollite PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
    )
    target_link_libraries(agentos_memoryrovollite PUBLIC kernel32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(agentos_memoryrovollite PUBLIC pthread rt)
elseif(APPLE)
    target_link_libraries(agentos_memoryrovollite PUBLIC pthread)
endif()

if(MSVC)
    target_compile_options(agentos_memoryrovollite PRIVATE
        /W4
        /WX-
        /MP
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(agentos_memoryrovollite PRIVATE /Od /Zi /RTC1)
        target_link_options(agentos_memoryrovollite PRIVATE /DEBUG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(agentos_memoryrovollite PRIVATE /O2 /GL)
        target_link_options(agentos_memoryrovollite PRIVATE /LTCG)
    endif()
else()
    target_compile_options(agentos_memoryrovollite PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fvisibility=hidden
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(agentos_memoryrovollite PRIVATE -O0 -g3 -fsanitize=address)
        target_link_options(agentos_memoryrovollite PRIVATE -fsanitize=address)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(agentos_memoryrovollite PRIVATE -O3 -DNDEBUG)
    endif()
endif()

set_target_properties(agentos_memoryrovollite PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${MEMORYROVOLLITE_HEADERS}"
    C_EXTENSIONS ON
)

if(AGENTOS_LITE_BUILD_TESTS)
    enable_testing()
    
    add_executable(test_memoryrovollite tests/test_memoryrovollite.c)
    target_link_libraries(test_memoryrovollite PRIVATE agentos_memoryrovollite agentos_corekernlite)
    
    if(WIN32)
        target_compile_definitions(test_memoryrovollite PRIVATE
            _CRT_SECURE_NO_WARNINGS
        )
    endif()
    
    add_test(NAME MemoryRovolLiteTests COMMAND test_memoryrovollite)
endif()

install(TARGETS agentos_memoryrovollite
    EXPORT agentos_memoryrovollite-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/agentos/lite
)

install(FILES ${MEMORYROVOLLITE_HEADERS}
    DESTINATION include/agentos/lite
)

install(EXPORT agentos_memoryrovollite-targets
    FILE agentos_memoryrovollite-manager.cmake
    NAMESPACE agentos::
    DESTINATION lib/cmake/agentos_memoryrovollite
)

message(STATUS "AgentOS MemoryRovolLite Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${AGENTOS_LITE_BUILD_SHARED}")
message(STATUS "  Tests: ${AGENTOS_LITE_BUILD_TESTS}")
message(STATUS "  Debug logging: ${AGENTOS_LITE_ENABLE_DEBUG}")
message(STATUS "  SQLite3 found: ${SQLite3_FOUND}")