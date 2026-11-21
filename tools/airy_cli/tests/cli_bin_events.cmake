# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# 断言 airy_cli 在事件模式下确有帧输出。--events/--timeout 曾被解析层当成
# 终止码，进程零输出、rc=0 静默退出——发行流水线 R17 的首个端到端探针
# 因此判红，而单测只覆盖 cli_parse_args 的返回值，看不到真正拉起的进程。
# 无网关时预期出 run_end 失败帧；此处只判「有输出」，退出码不作约定。

if(NOT CLI)
    message(FATAL_ERROR "缺少 -DCLI=<airy_cli 路径>")
endif()

execute_process(
    COMMAND "${CLI}" --events ndjson --quiet -p "smoke probe"
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE rc
    TIMEOUT 60)

if(out STREQUAL "")
    message(FATAL_ERROR "事件模式零输出（rc=${rc}，stderr=${err}）")
endif()

message(STATUS "事件帧非空（rc=${rc}）：${out}")
