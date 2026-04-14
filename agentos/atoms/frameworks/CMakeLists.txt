cmake_minimum_required(VERSION 3.20)
project(agentos_frameworks VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(FRAMEWORK_SOURCES
    src/agentos_frameworks.c
)

add_library(agentos_frameworks STATIC ${FRAMEWORK_SOURCES})

target_include_directories(agentos_frameworks
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(agentos_frameworks
    PRIVATE
        agentos_common
)

install(TARGETS agentos_frameworks
    ARCHIVE DESTINATION lib
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/agentos_frameworks.h
    DESTINATION include/agentos
)
