# 教程引导脚本

`scripts/tutorial/`

## 概述

`tutorial/` 目录包含 AgentOS 的交互式教程和引导脚本，帮助新开发者逐步学习和掌握 AgentOS 的各项功能，从基础概念到高级特性的渐进式学习路径。

## 教程列表

| 教程 | 说明 |
|------|------|
| `01_hello_agent.sh` | 创建并运行第一个 Agent |
| `02_skill_dev.sh` | Skill 开发入门 |
| `03_memory_usage.sh` | 记忆系统操作实践 |
| `04_task_flow.sh` | 任务编排与 DAG 使用 |
| `05_protocol_debug.sh` | 协议调试与抓包分析 |

## 使用示例

```bash
# 启动交互式教程
./tutorial/01_hello_agent.sh

# 技能开发教程
./tutorial/02_skill_dev.sh --interactive

# 查看教程说明
./tutorial/03_memory_usage.sh --help
```

## 学习路径

1. **基础入门** - 了解 AgentOS 架构，创建并运行第一个 Agent
2. **技能开发** - 学习技能的开发、注册和调用流程
3. **记忆系统** - 掌握四级记忆层次的使用和管理
4. **任务编排** - 学习 DAG 任务图的设计和执行
5. **协议通信** - 深入理解 Binder/JSON-RPC 协议和调试方法

---

© 2026 SPHARX Ltd. All Rights Reserved.
