cmake_minimum_required(VERSION 3.20)

project(agentos_uuid C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Source files
set(UUID_SOURCES
    src/uuid_generator.c
)

# Create library
add_library(agentos_uuid STATIC ${UUID_SOURCES})

# Include directories
target_include_directories(agentos_uuid PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../string/include
)

# Platform-specific sources
if(WIN32)
    target_compile_definitions(agentos_uuid PRIVATE _WIN32_WINNT=0x0600)
endif()

# Security compile options
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(agentos_uuid PRIVATE
        -Wall -Wextra -Wpedantic
        -fstack-protector-strong
        -D_FORTIFY_SOURCE=2
    )
endif()

# Export
export(TARGETS agentos_uuid FILE "${CMAKE_BINARY_DIR}/agentos_uuid_targets.cmake")
