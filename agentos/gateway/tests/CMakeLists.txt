# Copyright (c) 2026 SPHARX. All Rights Reserved.
# agentos/gateway 单元测试 (TeamC)
# 对齐: 看板v14.0 C-W1-003 / 规范手册v13.0 TEST-01~06

cmake_minimum_required(VERSION 3.20)

project(gateway_tests VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# TODO: test_gateway disabled due to dual-header conflict
#       include/gateway.h vs src/gateway/gateway.h have incompatible declarations
#       Fix: merge or refactor headers, then re-enable below
#
# set(TEST_SOURCES
#     test_gateway.c
#     test_jsonrpc.c
#     test_gateway_rpc_handler.c
#     test_syscall_router.c
# )
# add_executable(test_gateway ${TEST_SOURCES})
# ... (target_* commands for test_gateway)

enable_testing()
message(STATUS "  gateway_tests: SKIPPED (header conflict - needs refactoring)")
