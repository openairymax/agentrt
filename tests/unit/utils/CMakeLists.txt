# 测试配置

add_executable(utils_test
    core_test.c
    logger_test.c
    error_test.c
    token_test.c
    io_test.c
)

target_include_directories(utils_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../common/utils/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../common/utils/observability/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../common/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../common/utils/token/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../common/utils/io/include
)

target_link_libraries(utils_test PRIVATE
    agentos_common
    pthread
)

# 添加测试
add_test(NAME utils_test COMMAND utils_test)
