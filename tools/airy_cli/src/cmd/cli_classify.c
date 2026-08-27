// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_classify.c
 * @brief 简单对话意图分辨（task vs chat）启发式（纯字符串，无 LLM/全局依赖）。
 *
 * 两级路由的启发式第一级（2.5.x 意图分辨）：
 *   1. 强任务引导词（task_lead_marks）——"帮我实现/开发一个"等命令式
 *      工程动词前缀，即使后文含咨询词（"帮我实现一个如何构建的功能"）
 *      也判 task（2.3.4：此前 consult 词表优先，这类输入被误路由到对话）；
 *   2. 强对话咨询词（consult_marks）——讲解/解释/咨询类输入，即使含
 *      命令式动词（如"介绍一下如何构建项目"）也判 chat：用户要的是方法
 *      说明而非执行工程任务；
 *   3. 分析/评估/比较类咨询词（analysis_marks）——"分析/比较/评估"等
 *      咨询语义（2026-08-22 新增）：未含产物变更强动词时判 chat；含强
 *      动词（"帮我分析并修复这个 bug"）则交 task_marks 继续判定。此前
 *      分析/比较类输入常被后文场景动词（"在树莓派上运行数据库"的"运行"）
 *      误判为任务，误入任务管线；
 *   4. 明确任务词（task_marks）——命令式动词命中即 task（零 LLM 开销）。
 *      其中场景描述动词（运行/测试/检查/启动/停止）需无描述性语境
 *      （"上运行"/"性能测试"/"安全检查"）才计 task：同一动词可能是命令
 *      也可能是场景修饰；
 *   5. 明确对话词（chat_marks）——寒暄/检索/文件读改命中即 chat；
 *   6. 均未命中返回 -1，由调用方走 LLM 分类（cli_llm_classify）。
 *
 * fail-safe 语义：调用方在 LLM 失败时归 0（chat）——chat 误路由无害，
 * task 误路由会阻塞/执行（与 cli_classify_input 的决策一致）。
 *
 * 本模块为纯函数（无外部符号引用），独立于 cli_chat.c 可单测。
 */

#include "cli_internal.h"

#include <stddef.h>
#include <string.h>

/* 产物变更型强任务动词：与 analysis_marks 同现时（"分析并修复"）用户要
 * 的是动作而非说明，跳过分析层直接按任务判定。 */
static const char *const artifact_verbs[] = {
    "实现", "修复", "开发", "构建", "创建", "部署", "重构", "迁移",
    "安装", "配置", "删除", "更新", "下载", "编写", "生成", "集成",
    "改造", "写一个", "做一个", "构建一个", "实现一个", "开发一个",
    "please implement ", "please create ", "please build ", "please fix ",
};

/* 场景描述性动词：同一动词可能是命令（"运行这个脚本"）也可能是场景修饰
 * （"在树莓派上运行容器化数据库"/"性能测试"/"安全检查"）。 */
static const char *const scenario_verbs[] = {
    "运行", "测试", "检查", "启动", "停止",
};

/* 描述性语境：语境内嵌对应动词，命中即该动词是场景描述而非命令。 */
static const char *const scenario_contexts[] = {
    "上运行", "运行在", "运行于", "中运行", "的运行", "运行中",
    "运行性能", "运行开销", "运行时间", "运行时", "运行状态",
    "运行平台", "运行环境", "运行内存", "运行容器", "运行数据库",
    "运行服务", "运行进程", "运行程序",
    "性能测试", "压力测试", "单元测试", "集成测试", "测试报告", "测试用例",
    "安全检查", "健康检查", "例行检查", "检查项",
    "启动过程", "启动流程", "启动阶段", "启动逻辑", "启动时间", "启动时",
    "停止状态", "停止过程",
};

static int scenario_verb_descriptive(const char *input, const char *verb)
{
    for (size_t i = 0;
         i < sizeof(scenario_contexts) / sizeof(scenario_contexts[0]); i++) {
        if (strstr(scenario_contexts[i], verb) && strstr(input, scenario_contexts[i]))
            return 1;
    }
    return 0;
}

static int has_any(const char *input, const char *const *list, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (strstr(input, list[i]))
            return 1;
    }
    return 0;
}

int cli_classify_heuristic(const char *input)
{
    if (!input || input[0] == '\0')
        return -1;

    /* 强任务引导词：工程命令式前缀。与 consult 词冲突时（如"帮我实现一个
     * 如何排序的功能"）判 task——用户要的是执行，不是方法说明。置于
     * consult_marks 之前，避免被咨询词抢先短路。
     * 例外：输入以 consult 引导开头（"如何构建一个项目"/"介绍一下如何
     * 构建"）时是方法咨询而非命令执行——裸"构建一个"在咨询语境下是
     * 描述对象，不是命令。 */
    static const char *const task_lead_marks[] = {
        "帮我实现", "帮我开发", "帮我构建", "帮我创建", "帮我部署",
        "帮我修复", "帮我重构", "帮我做", "帮我写", "帮我设计",
        "实现一个", "开发一个", "构建一个", "创建一个", "部署一个",
        "修复一个", "重构一个", "做一个", "写一个", "设计一个",
        "please implement ", "please create ", "please build ",
        "please fix ", "please write ",
    };
    static const char *const consult_prefixes[] = {
        "如何", "怎么", "怎样", "介绍一下", "解释", "为什么", "什么是",
        "what is", "what are", "why ", "how to", "how do", "explain ",
    };
    for (size_t i = 0; i < sizeof(task_lead_marks) / sizeof(task_lead_marks[0]); i++) {
        if (strstr(input, task_lead_marks[i])) {
            int consult_lead = 0;
            for (size_t j = 0;
                 j < sizeof(consult_prefixes) / sizeof(consult_prefixes[0]); j++) {
                size_t plen = strlen(consult_prefixes[j]);
                if (strncmp(input, consult_prefixes[j], plen) == 0) {
                    consult_lead = 1;
                    break;
                }
            }
            return consult_lead ? 0 : 1;
        }
    }

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

    /* 分析/评估/比较类咨询词（2026-08-22）：咨询语义判 chat，除非同现
     * 产物变更强动词（"分析并修复"）。修复了"请帮我分析一下…在树莓派上
     * 运行容器化数据库…比较…"被"运行"误判为任务的缺陷。 */
    static const char *const analysis_marks[] = {
        "分析", "比较", "对比", "评估", "总结", "讲解", "探讨", "研讨",
        "剖析", "评测", "评价", "解析", "区别", "差异", "优缺点",
        "利弊", "优劣",
    };
    if (has_any(input, analysis_marks,
                sizeof(analysis_marks) / sizeof(analysis_marks[0]))) {
        if (!has_any(input, artifact_verbs,
                     sizeof(artifact_verbs) / sizeof(artifact_verbs[0])))
            return 0;
        /* 含产物变更强动词：落入 task_marks 继续判定 */
    }

    /* 明确任务词（中文/英文命令式，Claude Code/Codex 约定：命令式动词即任务）。
     * 场景描述动词（运行/测试/检查/启动/停止）单独处理：无描述性语境才计
     * 任务（2026-08-22，见 scenario_verb_descriptive）。 */
    static const char *const task_marks[] = {
        /* 中文命令式 */
        "实现", "开发", "构建", "创建", "编写", "修复", "重构",
        "部署", "设计", "添加", "支持", "优化", "迁移", "集成",
        "写一个", "写一", "做一个", "实现一个", "帮我实现", "开发一个",
        "生成", "安装", "配置", "删除", "更新", "下载",
        /* 英文命令式 */
        "create ", "create a", "create an", "write ", "write a", "write an",
        "implement ", "implement a", "implement an", "build ", "build a",
        "fix ", "fix a", "refactor ", "add ", "add a", "add an",
        "support ", "optimize ", "migrate ", "integrate ", "update ",
        "remove ", "delete ", "install ", "configure ",
        "generate ", "generate a",
        "deploy ", "start ", "stop ", "download ", "rename ", "move ",
        "make a", "make an", "make ", "change ", "modify ", "convert ",
    };
    for (size_t i = 0; i < sizeof(task_marks) / sizeof(task_marks[0]); i++) {
        if (strstr(input, task_marks[i]))
            return 1;
    }
    for (size_t i = 0; i < sizeof(scenario_verbs) / sizeof(scenario_verbs[0]); i++) {
        if (strstr(input, scenario_verbs[i]) &&
            !scenario_verb_descriptive(input, scenario_verbs[i]))
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
