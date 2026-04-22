# Gateway Daemon Tests (TeamC)
# Copyright (C) 2026 SPHARX. All Rights Reserved.

cmake_minimum_required(VERSION 3.16)

project(gateway_d_tests VERSION 1.0.0 LANGUAGES C)

set(GATEWAY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../gateway)
set(GATEWAY_INCLUDE_DIR ${GATEWAY_DIR}/include)
set(GATEWAY_SRC_DIR ${GATEWAY_DIR}/src)
set(ATOMIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../atoms)
set(SYSCALL_INCLUDE_DIR ${ATOMIC_DIR}/syscall/include)
set(COREKERN_INCLUDE_DIR ${ATOMIC_DIR}/corekern/include)
set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../commons)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(MICROHTTPD QUIET libmicrohttpd)
    pkg_check_modules(LIBWEBSOCKETS QUIET libwebsockets)
else()
    find_package(libmicrohttpd QUIET)
    find_package(libwebsockets QUIET)
endif()

set(TEST_SOURCES
    test_service.c
)

foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

    add_executable(${TEST_NAME}
        ${TEST_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/service.c
    )

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
            $<TARGET_OBJECTS:gateway_lib_obj>
            svc_common
            Threads::Threads
    )

    if(MICROHTTPD_FOUND)
        target_include_directories(${TEST_NAME} PRIVATE ${MICROHTTPD_INCLUDE_DIRS})
        target_link_libraries(${TEST_NAME} PRIVATE ${MICROHTTPD_LIBRARIES})
    endif()

    if(LIBWEBSOCKETS_FOUND)
        target_include_directories(${TEST_NAME} PRIVATE ${LIBWEBSOCKETS_INCLUDE_DIRS})
        target_link_libraries(${TEST_NAME} PRIVATE ${LIBWEBSOCKETS_LIBRARIES})
    endif()

    target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -g -Wno-unused-function)

    add_test(NAME gateway_d_${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
