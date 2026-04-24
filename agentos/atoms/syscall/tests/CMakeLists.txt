cmake_minimum_required(VERSION 3.20)

# Syscall 模块单元测试 (TeamC P1-C07)
# 覆盖: init/cleanup, invoke分发, 参数验证, 边界条件
# 对齐: 看板v09 P1-C07 / 规范手册v09 TEST-03

add_executable(test_syscalls test_syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../coreloopthree/tests/unit/test_cl3_stubs.c
)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../../coreloopthree/tests/unit/test_cl3_stubs.c
    PROPERTIES COMPILE_FLAGS "-fno-lto"
)

target_include_directories(test_syscalls PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/error/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/memory/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../coreloopthree/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../coreloopthree/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../corekern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../commons/utils/compat/include
)

target_link_libraries(test_syscalls PRIVATE agentos_syscall)

if(UNIX AND NOT APPLE)
    target_link_options(test_syscalls PRIVATE -Wl,--allow-multiple-definition)
endif()

add_test(NAME syscalls_test COMMAND test_syscalls)
message(STATUS "  syscall_tests: configured (test_syscalls)")
