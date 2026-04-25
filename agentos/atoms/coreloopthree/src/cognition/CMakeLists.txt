# Copyright (c) 2026 SPHARX. All Rights Reserved.
# 认知层构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-cognition)

file(GLOB COGNITION_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/planner/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/coordinator/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/dispatcher/*.c"
)

add_library(agentos_cognition STATIC ${COGNITION_SOURCES})

target_include_directories(agentos_cognition PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../memoryrovol/include
)

target_link_libraries(agentos_cognition
    agentos_core
    m
)

if(WIN32)
    target_compile_options(agentos_cognition PRIVATE /W3 /O2)
else()
    target_compile_options(agentos_cognition PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS agentos_cognition DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cognition.h DESTINATION include/agentos)
