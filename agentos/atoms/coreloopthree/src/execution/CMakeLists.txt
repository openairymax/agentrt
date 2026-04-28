# Copyright (c) 2026 SPHARX. All Rights Reserved.
# 行动层构建文件

cmake_minimum_required(VERSION 3.20)
project(agentos-execution)

file(GLOB EXECUTION_SOURCES
    "engine.c"
    "registry.c"
    "compensation.c"
    "trace.c"
    "units/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../utils/error_utils.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../utils/id_utils.c"
)

add_library(agentos_execution STATIC ${EXECUTION_SOURCES})

target_include_directories(agentos_execution PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../commons/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../commons/utils/strategy/include
)

target_link_libraries(agentos_execution
    PUBLIC agentos_core
)

if(UNIX AND NOT APPLE)
    target_link_libraries(agentos_execution PUBLIC uuid)
endif()

if(WIN32)
    target_compile_options(agentos_execution PRIVATE /W3 /O2)
else()
    target_compile_options(agentos_execution PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS agentos_execution DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/execution.h DESTINATION include/agentos)
