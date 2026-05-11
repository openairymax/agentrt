cmake_minimum_required(VERSION 3.20)

add_library(agentos_execution STATIC
    engine.c
    registry.c
    compensation.c
    trace.c
    units/api.c
    units/browser.c
    units/code.c
    units/db.c
    units/file.c
    units/shell.c
    units/tool.c
    units/weighted.c
    ../utils/error_utils.c
    ../utils/id_utils.c
)

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

if(AGENTOS_CJSON_FOUND)
    target_compile_definitions(agentos_execution PRIVATE AGENTOS_HAS_CJSON=1)
    target_include_directories(agentos_execution PRIVATE ${AGENTOS_CJSON_INCLUDE_DIRS})
    target_link_libraries(agentos_execution ${AGENTOS_CJSON_LIBRARIES})
endif()

if(WIN32)
    target_compile_options(agentos_execution PRIVATE /W3 /O2)
else()
    target_compile_options(agentos_execution PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS agentos_execution DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/execution.h DESTINATION include/agentos)
