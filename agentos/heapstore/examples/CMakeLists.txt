# heapstore examples CMakeLists.txt
#
# Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0



set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# R-09-124: converted to target_include_directories
# include_directories(
#     ${CMAKE_SOURCE_DIR}/include
#     ${CMAKE_BINARY_DIR}/include
# )

# Find required libraries
# Threads found by root CMakeLists.txt

# Example executables
add_executable(quick_start quick_start.c)
target_include_directories(quick_start PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)
target_link_libraries(quick_start
    agentos_heapstore
    Threads::Threads
)

add_executable(batch_write batch_write.c)
target_include_directories(batch_write PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)
target_link_libraries(batch_write
    agentos_heapstore
    Threads::Threads
)

# Installation
install(TARGETS quick_start batch_write
    RUNTIME DESTINATION bin/examples
)
