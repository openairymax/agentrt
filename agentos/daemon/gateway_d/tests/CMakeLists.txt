# Gateway Daemon Tests (TeamC)
# Copyright (C) 2026 SPHARX. All Rights Reserved.



set(GATEWAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../gateway)
set(GATEWAY_INCLUDE_DIR ${GATEWAY_DIR}/include)
set(GATEWAY_SRC_DIR ${GATEWAY_DIR}/src)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons)

# libmicrohttpd/libwebsockets found by root CMakeLists.txt

set(PROTOCOL_ROUTER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src/protocol)

set(TEST_SOURCES
    test_service.c
    test_protocol_router.c
)

foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

    add_executable(${TEST_NAME}
        ${TEST_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/service.c
    )

    if(${TEST_NAME} STREQUAL "test_protocol_router")
        target_sources(${TEST_NAME} PRIVATE
            ${PROTOCOL_ROUTER_DIR}/gateway_protocol_router.c
            ${PROTOCOL_ROUTER_DIR}/gateway_mcp_server.c
            ${PROTOCOL_ROUTER_DIR}/gateway_a2a_handler.c
            ${PROTOCOL_ROUTER_DIR}/gateway_openai_compat.c
        )
        target_include_directories(${TEST_NAME} PRIVATE
            ${PROTOCOL_ROUTER_DIR}
        )
    endif()

    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
        ${GATEWAY_INCLUDE_DIR}
        ${GATEWAY_SRC_DIR}
        ${GATEWAY_SRC_DIR}/gateway
        ${GATEWAY_SRC_DIR}/utils
        ${SYSCALL_INCLUDE_DIR}
        ${COREKERN_INCLUDE_DIR}
        ${COMMONS_DIR}
        ${COMMONS_DIR}/platform/include
        ${COMMONS_DIR}/utils/error/include
        ${COMMONS_DIR}/utils/logging/include
        ${COMMONS_DIR}/utils/config_unified/include
        ${COMMONS_DIR}/utils/memory/include
        ${COMMONS_DIR}/utils/string/include
        ${COMMONS_DIR}/utils/sync/include
        ${COMMONS_DIR}/utils/cache/include
        ${COMMONS_DIR}/utils/compat/include
    )

    target_link_libraries(${TEST_NAME}
        PRIVATE
            svc_common
            Threads::Threads
    )

    if(AGENTOS_HAS_CJSON AND TARGET gateway_lib_obj AND AGENTOS_HAS_MICROHTTPD)
        target_link_libraries(${TEST_NAME} PRIVATE $<TARGET_OBJECTS:gateway_lib_obj>)
    endif()

    if(AGENTOS_HAS_MICROHTTPD)
        if(libmicrohttpd_FOUND)
            target_link_libraries(${TEST_NAME} PRIVATE microhttpd)
        elseif(MICROHTTPD_FOUND)
            target_include_directories(${TEST_NAME} PRIVATE ${MICROHTTPD_INCLUDE_DIRS})
            target_link_libraries(${TEST_NAME} PRIVATE ${MICROHTTPD_LIBRARIES})
        endif()
    endif()

    if(AGENTOS_HAS_LIBWEBSOCKETS)
        if(libwebsockets_FOUND)
            target_link_libraries(${TEST_NAME} PRIVATE websockets)
        elseif(LIBWEBSOCKETS_FOUND)
            target_include_directories(${TEST_NAME} PRIVATE ${LIBWEBSOCKETS_INCLUDE_DIRS})
            target_link_libraries(${TEST_NAME} PRIVATE ${LIBWEBSOCKETS_LIBRARIES})
        endif()
    endif()

    target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -g -Wno-unused-function)

    add_test(NAME gateway_d_${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
