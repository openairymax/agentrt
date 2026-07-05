

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Source files
set(UUID_SOURCES
    src/uuid_generator.c
)

# Create library
add_library(agentrt_uuid STATIC ${UUID_SOURCES})

# Include directories
target_include_directories(agentrt_uuid PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../string/include
)

# Platform-specific sources
if(WIN32)
    target_compile_definitions(agentrt_uuid PRIVATE _WIN32_WINNT=0x0600)
endif()

# Security compile options
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(agentrt_uuid PRIVATE
        -Wall -Wextra -Wpedantic
        -fstack-protector-strong
        $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>
    )
endif()

# Export
export(TARGETS agentrt_uuid FILE "${CMAKE_BINARY_DIR}/agentrt_uuid_targets.cmake")
