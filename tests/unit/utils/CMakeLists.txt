# 测试配置

add_executable(utils_test
    core_test.c
    logger_test.c
    error_test.c
    token_test.c
    io_test.c
)

target_include_directories(utils_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../observability/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../token/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/include
)

target_link_libraries(utils_test PRIVATE
    agentos_utils_core
    agentos_utils_observability
    agentos_utils_error
    agentos_utils_token
    agentos_utils_io
    pthread
)

# 添加测试
add_test(NAME utils_test COMMAND utils_test)
