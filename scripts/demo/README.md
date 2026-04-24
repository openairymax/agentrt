# 演示示例脚本

`scripts/demo/`

## 概述

`demo/` 目录包含快速展示 AgentOS 各项功能的示例脚本，涵盖 Agent 创建、技能调用、任务编排、记忆存储等核心功能，帮助开发者在短时间内了解系统能力。

## 演示列表

| 脚本 | 说明 |
|------|------|
| `quick_start.sh` | 快速启动演示，展示 AgentOS 最核心的工作流 |
| `agent_chat.sh` | Agent 对话演示，展示多 Agent 交互过程 |
| `skill_chain.sh` | 技能链演示，展示多个技能串联执行 |
| `memory_demo.sh` | 记忆系统演示，展示记忆的存储与检索 |
| `market_demo.sh` | 应用市场演示，展示技能和模板的安装使用 |

## 使用示例

```bash
# 快速启动演示
./demo/quick_start.sh

# Agent 对话演示
./demo/agent_chat.sh --interactive

# 技能链演示
./demo/skill_chain.sh --skills "search,translate,summarize"
```

## 演示流程

快速启动脚本的典型流程：

1. 启动 AgentOS 核心守护进程
2. 初始化内存和存储系统
3. 注册默认技能
4. 创建并启动 Agent 实例
5. 发送测试任务并展示结果
6. 清理运行环境

---

© 2026 SPHARX Ltd. All Rights Reserved.
