
add_library(agentos_execution STATIC
    engine.c
    tool_svc_adapter.c
    core/registry.c
    core/trace.c
    transaction/compensation.c
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../commons/utils/quality
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../daemon/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../daemon/llm_d/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../daemon/tool_d/include
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

# libcurl 用于 api.c 的真实 HTTPS/HTTP 请求（消除伪 HTTPS 致命漏洞）
if(AGENTRT_HAS_CURL)
    target_compile_definitions(agentos_execution PRIVATE AGENTRT_HAS_CURL=1)
    if(CURL_FOUND AND CURL_LIBRARIES)
        target_link_libraries(agentos_execution PRIVATE ${CURL_LIBRARIES})
        target_include_directories(agentos_execution PRIVATE ${CURL_INCLUDE_DIRS})
    elseif(LIBCURL_FOUND AND LIBCURL_LIBRARIES)
        target_link_libraries(agentos_execution PRIVATE ${LIBCURL_LIBRARIES})
        target_include_directories(agentos_execution PRIVATE ${LIBCURL_INCLUDE_DIRS})
    elseif(PC_CURL_LIBRARIES)
        target_link_libraries(agentos_execution PRIVATE ${PC_CURL_LIBRARIES})
        target_include_directories(agentos_execution PRIVATE ${PC_CURL_INCLUDE_DIRS})
    else()
        target_link_libraries(agentos_execution PRIVATE curl)
    endif()
    message(STATUS "  execution: libcurl enabled for api.c HTTPS features (真实 TLS)")
else()
    message(WARNING "  execution: libcurl not found - api.c HTTPS features will return ENOTSUP")
endif()

if(WIN32)
    target_compile_options(agentos_execution PRIVATE /W3 /O2)
else()
    target_compile_options(agentos_execution PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS agentos_execution DESTINATION lib)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../include/execution.h DESTINATION include/agentos)
