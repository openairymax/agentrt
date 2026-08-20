// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_classify.c
 * @brief 简单对话意图分辨（task vs chat）启发式（纯字符串，无 LLM/全局依赖）。
 *
 * 两级路由的启发式第一级（2.5.x 意图分辨）：
 *   1. 强对话咨询词（consult_marks）——讲解/解释/咨询类输入即使含命令式
 *      动词（如"介绍一下如何构建项目"）也判 chat：用户要的是方法说明而非
 *      执行工程任务。先前置检查，避免被 task_marks 抢先短路；
 *   2. 明确任务词（task_marks）——命令式动词命中即 task（零 LLM 开销）；
 *   3. 明确对话词（chat_marks）——寒暄/检索/文件读改命中即 chat；
 *   4. 两表均未命中返回 -1，由调用方走 LLM 分类（cli_llm_classify）。
 *
 * fail-safe 语义：调用方在 LLM 失败时归 0（chat）——chat 误路由无害，
 * task 误路由会阻塞/执行（与 cli_classify_input 的决策一致）。
 *
 * 本模块为纯函数（无外部符号引用），独立于 cli_chat.c 可单测。
 */

#include "cli_internal.h"

#include <stddef.h>
#include <string.h>

int cli_classify_heuristic(const char *input)
{
    if (!input || input[0] == '\0')
        return -1;

    /* 强对话咨询词：讲解/解释/咨询类输入，即使含命令式动词也判 chat */
    static const char *const consult_marks[] = {
        "介绍一下", "解释", "为什么", "如何", "怎么", "讲讲", "聊聊",
        "什么是", "是什么", "怎么做", "怎样", "帮我看看", "帮我查一下",
        "what is", "what are", "why ", "explain ", "how to", "how do",
        "tell me", "what's ",
    };
    for (size_t i = 0; i < sizeof(consult_marks) / sizeof(consult_marks[0]); i++) {
        if (strstr(input, consult_marks[i]))
            return 0;
    }

    /* 明确任务词（中文/英文命令式，Claude Code/Codex 约定：命令式动词即任务） */
    static const char *const task_marks[] = {
        /* 中文命令式 */
        "实现", "开发", "构建", "创建", "编写", "修复", "重构",
        "部署", "设计", "添加", "支持", "优化", "迁移", "集成",
        "写一个", "写一", "做一个", "实现一个", "帮我实现", "开发一个",
        "生成", "运行", "测试", "检查",
        "启动", "停止", "安装", "配置", "删除", "更新", "下载",
        /* 英文命令式 */
        "create ", "create a", "create an", "write ", "write a", "write an",
        "implement ", "implement a", "implement an", "build ", "build a",
        "fix ", "fix a", "refactor ", "add ", "add a", "add an",
        "support ", "optimize ", "migrate ", "integrate ", "update ",
        "remove ", "delete ", "run ", "run a", "install ", "configure ",
        "test ", "generate ", "generate a",
        "deploy ", "start ", "stop ", "download ", "rename ", "move ",
        "make a", "make an", "make ", "change ", "modify ", "convert ",
    };
    for (size_t i = 0; i < sizeof(task_marks) / sizeof(task_marks[0]); i++) {
        if (strstr(input, task_marks[i]))
            return 1;
    }

    /* 明确对话词：寒暄 + 联网检索 + 本地文件读/查/改（超级智能体日常操作） */
    static const char *const chat_marks[] = {
        "你好", "谢谢", "再见", "你是谁", "帮助", "请问", "你好呀",
        "hello", "hi", "thanks", "thank you", "bye", "who are you",
        "help me",
        /* 联网检索/信息查询 → 聊天工具回路（web_search/web_fetch） */
        "搜索", "查询", "搜索一下", "查一下", "了解",
        "search ", "look up",
        /* 本地文件读/查/编辑 → 聊天工具回路（fs_read/fs_list/fs_edit 等）：
         * 查看、读取、修改单个文件属于日常操作，非工程任务
         * （2026-08-16：此前被误路由到任务管线导致提交失败） */
        "读取", "读一下", "读文件", "查看", "看看", "打开", "内容是什么",
        "修改一下", "改一下", "编辑一下", "写入", "文件",
    };
    for (size_t i = 0; i < sizeof(chat_marks) / sizeof(chat_marks[0]); i++) {
        if (strstr(input, chat_marks[i]))
            return 0;
    }

    /* 未命中任何词表：交 LLM 兜底 */
    return -1;
}
