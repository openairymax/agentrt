// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_classify.c
 * @brief 意图分辨启发式（cli_classify_heuristic）单测（2.5.x）。
 *
 * 覆盖三级优先（consult > task > chat）+ 未命中 -1 + 关键回归场景：
 *   - 咨询词与命令式动词并存 → chat（"介绍一下如何构建项目" 不误路由 task）；
 *   - 明确任务词 → task（零 LLM 开销快路径）；
 *   - 寒暄/检索/文件读改 → chat（2026-08-16 误路由回归）；
 *   - 两表未命中 → -1（交 LLM 兜底）。
 */

#define _POSIX_C_SOURCE 199309L

#include "cli_internal.h"

#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, name, msg)                                  \
    do {                                                        \
        tests_run++;                                            \
        if (cond) {                                             \
            printf("  [PASS] %s\n", name);                      \
            tests_passed++;                                     \
        } else {                                                \
            printf("  [FAIL] %s: %s\n", name, msg);             \
        }                                                       \
    } while (0)

#define CHECK_TASK(input) CHECK(cli_classify_heuristic(input) == 1, input, "expect task")
#define CHECK_CHAT(input) CHECK(cli_classify_heuristic(input) == 0, input, "expect chat")
#define CHECK_UNK(input)  CHECK(cli_classify_heuristic(input) == -1, input, "expect -1")

static void test_consult_priority(void)
{
    /* 咨询词 + 命令式动词 → chat（用户要方法说明而非执行） */
    CHECK_CHAT("介绍一下如何构建一个项目");
    CHECK_CHAT("解释一下为什么启动失败");
    CHECK_CHAT("如何优化这个系统的性能");
    CHECK_CHAT("怎么实现登录功能");
    CHECK_CHAT("what is the best way to build a system");
    CHECK_CHAT("explain how to fix the deployment");
    CHECK_CHAT("帮我查一下今天的天气");
}

static void test_task_fast_path(void)
{
    /* 明确任务 → task（不含咨询词） */
    CHECK_TASK("帮我实现一个搜索功能");
    CHECK_TASK("开发一个命令行工具");
    CHECK_TASK("修复登录接口的 bug");
    CHECK_TASK("部署到生产环境");
    CHECK_TASK("write a script to backup files");
    CHECK_TASK("create a new project");
    CHECK_TASK("重构支付模块");
}

static void test_chat_fast_path(void)
{
    /* 寒暄/信息检索/文件读改 → chat */
    CHECK_CHAT("你好");
    CHECK_CHAT("谢谢");
    CHECK_CHAT("你是谁");
    CHECK_CHAT("搜索一下 Linux kernel 最新版本");
    CHECK_CHAT("查一下北京天气");
    CHECK_CHAT("读取 /etc/os-release 的内容");
    CHECK_CHAT("看看这个文件");
    CHECK_CHAT("hello there");
    CHECK_CHAT("what's the weather in Shanghai");
}

static void test_task_lead_priority(void)
{
    /* 强任务引导词 + 咨询词并存 → task（2.3.4：此前 consult 词表优先，
     * "帮我实现一个如何…"被误路由到对话；命令式前缀说明用户要执行） */
    CHECK_TASK("帮我实现一个如何排序的功能");
    CHECK_TASK("帮我做一个能自动备份的脚本");
    CHECK_TASK("帮我写一个解析 JSON 的函数");
    CHECK_TASK("实现一个如何计算费用的模块");
    /* 无强引导的纯咨询仍判 chat（"写一篇"不命中强引导"写一个"） */
    CHECK_CHAT("写一篇关于如何优化的文章");
}

static void test_unknown_goes_llm(void)
{
    /* 未命中任何词表 → -1（交 LLM 兜底） */
    CHECK_UNK("天色不早了");
    CHECK_UNK("a b c d");
    CHECK(cli_classify_heuristic("") == -1, "empty input -> -1", "expect -1");
    CHECK(cli_classify_heuristic(NULL) == -1, "NULL input -> -1", "expect -1");
}

static void test_analysis_and_scenario(void)
{
    /* 分析/比较/评估类咨询（2026-08-22 回归：此前"运行"场景描述误判任务） */
    CHECK_CHAT("请帮我分析一下：一个 8GB 内存的树莓派上运行容器化数据库与原生数据库的性能差异，从内存管理、IO、隔离开销三方面比较");
    CHECK_CHAT("分析一下 Docker 和 Podman 的优缺点");
    CHECK_CHAT("对比两个方案的利弊");
    CHECK_CHAT("评估一下这个架构的优劣");
    CHECK_CHAT("总结一下最近的进展");
    /* 分析 + 产物变更强动词 → task（用户要动作而非说明） */
    CHECK_TASK("帮我分析并修复这个 bug");
    CHECK_TASK("分析这个日志并修复内存泄漏");
    /* 场景描述动词（"上运行"/"性能测试"/"安全检查"）→ 非任务 */
    CHECK_CHAT("如何检查系统安全性");
    CHECK_UNK("在树莓派上运行容器化数据库需要注意什么");
    /* 无描述语境的场景动词 → 仍是任务 */
    CHECK_TASK("运行这个脚本");
    CHECK_TASK("帮我检查一下配置文件");
    CHECK_TASK("测试这个模块");
}

int main(void)
{
    printf("=== airy_cli Intent Classification Heuristic Tests ===\n\n");

    test_consult_priority();
    test_task_fast_path();
    test_chat_fast_path();
    test_task_lead_priority();
    test_unknown_goes_llm();
    test_analysis_and_scenario();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "FAILED: %d/%d tests passed\n", tests_passed, tests_run);
        return 1;
    }
    return 0;
}
