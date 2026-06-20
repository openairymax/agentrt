# CLI — AgentRT 命令行工具

**模块路径**: `sdk/cli/`
**版本**: v0.1.1

## 概述

AgentRT CLI 是基于 Rust 开发的命令行工具，提供与 AgentRT 智能体运行时交互的终端接口。支持项目管理、智能体创建与运行、LLM 提供商管理、Prompt 模板管理、市场搜索安装、部署运维和数据库迁移等全生命周期操作。所有运行时命令通过 Gateway HTTP API 与 AgentRT 核心通信。

## 目录结构

```
cli/
├── src/
│   ├── main.rs              # 入口与 CLI 路由
│   ├── client.rs            # Gateway HTTP 客户端
│   ├── templates.rs         # 项目模板生成
│   └── commands/
│       ├── mod.rs           # 命令模块导出
│       ├── init.rs          # agentrt init — 项目初始化
│       ├── create.rs        # agentrt create — 创建 agent/tool/plugin/prompt/skill
│       ├── run.rs           # agentrt run — 运行智能体（交互/单次）
│       ├── config_cmd.rs    # agentrt config — 配置管理
│       ├── llm.rs           # agentrt llm — LLM 提供商管理
│       ├── prompt.rs        # agentrt prompt — Prompt 模板管理
│       ├── market.rs        # agentrt market — 市场搜索与安装
│       ├── deploy.rs        # agentrt deploy — 部署与状态查询
│       └── db.rs            # agentrt db — 数据库迁移管理
├── Cargo.toml               # Rust 项目配置
└── README.md                # 本文件
```

## 命令列表

| 命令 | 说明 | 需 Gateway |
|------|------|:----------:|
| `agentrt init <name>` | 初始化新的 AgentRT 项目 | ✗ |
| `agentrt create agent\|tool\|plugin\|prompt\|skill <name>` | 创建组件模板 | ✗ |
| `agentrt run [prompt]` | 运行智能体（交互/单次） | ✓ |
| `agentrt config show\|set\|validate\|reload` | 配置管理 | △ |
| `agentrt llm list\|test\|cost` | LLM 提供商管理 | ✓ |
| `agentrt prompt list\|show\|tune\|ab-test` | Prompt 模板管理 | ✗ |
| `agentrt market search\|install\|publish` | 市场搜索与安装 | ✓ |
| `agentrt deploy deploy\|status\|logs` | 部署与运维 | ✓ |
| `agentrt db status\|migrate\|rollback\|new` | 数据库迁移管理 | ✗ |
| `agentrt completion <shell>` | 生成 Shell 补全脚本 | ✗ |

## 核心功能

### 项目初始化

```bash
# 创建新项目
agentrt init my-agent-project
cd my-agent-project

# 创建智能体
agentrt create agent my-agent

# 创建工具/插件
agentrt create tool web-scraper
agentrt create plugin logging-plugin
```

### 运行智能体

```bash
# 交互模式
agentrt run

# 单次执行
agentrt run "分析这份销售数据"

# 指定智能体配置和模型
agentrt run --agent-file agents/custom.agent.yaml --model gpt-4
```

### 配置管理

```bash
# 查看当前配置
agentrt config show

# 设置 LLM API Key
agentrt config set llm.providers.openai.api_key sk-xxxx

# 验证配置
agentrt config validate

# 重新加载 Gateway 配置
agentrt config reload
```

### Prompt 模板管理

```bash
# 列出所有模板
agentrt prompt list

# 查看模板详情
agentrt prompt show intent_classify

# 调优模板
agentrt prompt tune intent_classify --dataset ./data.jsonl

# A/B 测试
agentrt prompt ab-test intent_classify --baseline v1 --candidate v2
```

### 部署运维

```bash
# 部署到 Docker
agentrt deploy deploy --target docker

# 查看运行时状态
agentrt deploy status

# 查看运行时日志
agentrt deploy logs --lines 100
```

## 依赖关系

| 类别 | 依赖 |
|------|------|
| CLI 框架 | clap 4.5 (derive + env), clap_complete 4.5 |
| HTTP 客户端 | reqwest 0.12 (json + rustls-tls + stream + socks) |
| 异步运行时 | tokio 1 (full) |
| 序列化 | serde 1.0, serde_json, serde_yaml 0.9 |
| 错误处理 | thiserror 2.0, anyhow 1.0 |
| 终端输出 | colored 2, indicatif 0.17 |
| 工具 | chrono 0.4, dirs 5.0, url 2.5, urlencoding 2.1 |

## 构建说明

```bash
# 构建
cargo build --release

# 运行
./target/release/agentrt --help

# 生成 Shell 补全
source <(agentrt completion bash)
```

---

© 2026 SPHARX Ltd. All Rights Reserved.