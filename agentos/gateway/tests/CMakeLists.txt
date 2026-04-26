cmake_minimum_required(VERSION 3.20)

project(gateway_tests VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TEST_SOURCES
    test_gateway.c
    test_jsonrpc.c
    test_gateway_rpc_handler.c
    test_syscall_router.c
)

add_executable(test_gateway ${TEST_SOURCES})

target_include_directories(test_gateway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/gateway
)

target_link_libraries(test_gateway PRIVATE gateway)

enable_testing()
add_test(NAME test_gateway COMMAND test_gateway)
