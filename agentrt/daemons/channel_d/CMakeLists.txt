

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(DAEMON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(DAEMON_COMMON_INCLUDE_DIR ${DAEMON_COMMON_DIR}/include)

set(COMMONS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../commons)
set(COMMONS_INCLUDE_DIR ${COMMONS_DIR})

set(CHANNEL_D_SOURCES
    src/main.c
    src/channel_service.c
)

add_executable(channel_d ${CHANNEL_D_SOURCES})

target_include_directories(channel_d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DAEMON_COMMON_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}
    ${COMMONS_INCLUDE_DIR}/platform/include
    ${COMMONS_INCLUDE_DIR}/utils/error/include
    ${COMMONS_INCLUDE_DIR}/utils/logging/include
    ${COMMONS_INCLUDE_DIR}/utils/memory/include
    ${COMMONS_INCLUDE_DIR}/utils/string/include
    ${COMMONS_INCLUDE_DIR}/utils/sync/include
    ${COMMONS_INCLUDE_DIR}/utils/compat/include
)

if(UNIX)
    target_link_libraries(channel_d PRIVATE
        svc_common
        agentrt_common
        Threads::Threads
        agentrt_platform_libs
    )
    target_compile_definitions(channel_d PRIVATE
        _GNU_SOURCE
        # R-09-119: use platform.h default
        AGENTRT_CONFIG_DIR="/etc/agentrt"
        AGENTRT_LOG_DIR="/var/log/agentrt"
    )
endif()

if(NOT MSVC)
target_compile_options(channel_d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=implicit-function-declaration
    -Werror=return-type
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)
endif()

install(TARGETS channel_d
    RUNTIME DESTINATION bin
)

install(FILES
    include/channel_service.h
    DESTINATION include/agentrt/daemons/channel_d
)

if(BUILD_TESTS)
if(AGENTRT_HAS_CMOCKA)
    enable_testing()
    add_executable(test_channel_e2e tests/test_channel_e2e.c src/channel_service.c)
    target_compile_definitions(test_channel_e2e PRIVATE AGENTRT_COMPLIANCE_IMPL)
    target_include_directories(test_channel_e2e PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMONS_INCLUDE_DIR}/utils/string/include
    )
    target_link_libraries(test_channel_e2e PRIVATE
        ${CMOCKA_LIBRARIES}
        svc_common
        agentrt_common
        Threads::Threads agentrt_platform_libs
    )
    target_compile_options(test_channel_e2e PRIVATE -Wall -Wextra)
    add_test(NAME channel_e2e COMMAND test_channel_e2e)
else()
    enable_testing()
    add_executable(test_channel_e2e tests/test_channel_e2e.c src/channel_service.c)
    target_compile_definitions(test_channel_e2e PRIVATE AGENTRT_COMPLIANCE_IMPL)
    target_include_directories(test_channel_e2e PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMONS_DIR}/tests/utils
        ${COMMONS_INCLUDE_DIR}/utils/string/include
    )
    target_link_libraries(test_channel_e2e PRIVATE Threads::Threads agentrt_platform_libs svc_common agentrt_common)
    target_compile_options(test_channel_e2e PRIVATE -Wall -Wextra)
    target_compile_definitions(test_channel_e2e PRIVATE
        USE_CMOCKA_STUB
        # R-09-119: use platform.h default
        AGENTRT_CONFIG_DIR="/etc/agentrt"
        AGENTRT_LOG_DIR="/var/log/agentrt"
    )
    add_test(NAME channel_e2e COMMAND test_channel_e2e)
endif()
endif() # BUILD_TESTS
