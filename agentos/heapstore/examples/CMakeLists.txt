# heapstore examples CMakeLists.txt
#
# Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.15)

project(heapstore_examples C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include heapstore headers
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

# Find required libraries
find_package(Threads REQUIRED)

# Example executables
add_executable(quick_start quick_start.c)
target_link_libraries(quick_start
    agentos_heapstore
    Threads::Threads
)

add_executable(batch_write batch_write.c)
target_link_libraries(batch_write
    agentos_heapstore
    Threads::Threads
)

# Installation
install(TARGETS quick_start batch_write
    RUNTIME DESTINATION bin/examples
)
