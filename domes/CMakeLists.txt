cmake_minimum_required(VERSION 3.10)

project(domes C)

set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_C_FLAGS "/W4 /WX- /D_CRT_SECURE_NO_DEPRECATE /D_SCL_SECURE_NO_WARNINGS")
    set(CMAKE_C_FLAGS_DEBUG "/Zi /Od /RTC1")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /GL /DNDEBUG")
    add_definitions(-DDOMES_PLATFORM_WINDOWS=1)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)
else()
    set(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused-parameter -fPIC")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    add_definitions(-DDOMES_PLATFORM_POSIX=1)
    if(APPLE)
        add_definitions(-DDOMES_PLATFORM_64BIT=1)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-DDOMES_PLATFORM_64BIT=1)
    endif()
endif()

set(DOMES_SOURCES
    src/platform/platform.c
    src/core.c
    src/permission/permission_engine.c
    src/permission/permission_rule.c
    src/permission/permission_cache.c
    src/audit/audit_logger.c
    src/audit/audit_queue.c
    src/audit/audit_rotator.c
    src/sanitizer/sanitizer_core.c
    src/sanitizer/sanitizer_rules.c
    src/sanitizer/sanitizer_cache.c
    src/workbench/workbench.c
    src/workbench/workbench_process_core.c
)

set(DOMES_HEADERS
    include/domes.h
    src/core.h
    src/platform/platform.h
    src/permission/permission.h
    src/permission/permission_engine.h
    src/permission/permission_rule.h
    src/permission/permission_cache.h
    src/audit/audit.h
    src/audit/audit_queue.h
    src/audit/audit_rotator.h
    src/sanitizer/sanitizer.h
    src/sanitizer/sanitizer_rules.h
    src/sanitizer/sanitizer_cache.h
    src/workbench/workbench.h
    src/workbench/workbench_process.h
)

add_library(domes STATIC ${DOMES_SOURCES})

target_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include PUBLIC)
target_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src PRIVATE)

if(MSVC)
    target_link_libraries(domes PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(domes PRIVATE ${CMAKE_THREAD_LIBS_LIBRARY})
endif()

option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_domes_core tests/unit/test_domes_core.c)
    target_link_libraries(test_domes_core domes)
    
    add_executable(test_domes_integration tests/integration/test_domes_integration.c)
    target_link_libraries(test_domes_integration domes)
    
    if(MSVC)
        add_custom_command(TARGET test_domes_core POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE> $<TARGET_FILE_DIR>)
    endif()
endif()

install(TARGETS domes
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
)

install(FILES ${DOMES_HEADERS}
    DESTINATION include/domes
)

install(FILES include/domes.h
    DESTINATION include
)
