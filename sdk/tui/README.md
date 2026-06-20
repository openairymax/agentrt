# TUI — AgentRT 终端用户界面

**模块路径**: `sdk/tui/`
**版本**: v0.1.1

## 概述

AgentRT TUI 是基于 Rust 开发的终端用户界面（Terminal User Interface），提供可视化的 AgentRT 交互体验。采用 ratatui 和 crossterm 构建，支持多面板切换、实时对话、日志监控、记忆查看、配置管理和插件管理等功能。通过 Gateway HTTP API 与 AgentRT 核心通信。

## 目录结构

```
tui/
├── src/
│   ├── main.rs              # 入口与终端初始化
│   ├── app.rs               # 应用状态与事件循环
│   ├── client.rs            # Gateway HTTP 客户端
│   ├── ui.rs                # UI 渲染入口
│   └── panels/
│       ├── mod.rs           # 面板模块导出
│       ├── chat.rs          # 对话面板
│       ├── config.rs        # 配置面板
│       ├── help.rs          # 帮助面板
│       ├── logs.rs          # 日志面板
│       ├── memory.rs        # 记忆面板
│       └── plugins.rs       # 插件面板
├── Cargo.toml               # Rust 项目配置
└── README.md                # 本文件
```

## 核心功能

### 多面板界面

| 面板 | 快捷键 | 说明 |
|------|--------|------|
| **Chat** | `Esc` | 对话面板，输入与展示智能体回复 |
| **Help** | `F1` | 帮助面板，显示快捷键说明 |
| **Config** | `F2` | 配置面板，查看和编辑运行配置 |
| **Logs** | `F3` | 日志面板，实时查看系统日志 |
| **Memory** | `F4` | 记忆面板，查看智能体记忆内容 |
| **Plugins** | `F5` | 插件面板，管理已加载的插件 |

### 操作快捷键

| 快捷键 | 说明 |
|--------|------|
| `Ctrl+C` | 退出程序 |
| `Esc` | 返回对话面板 |
| `F1` - `F5` | 切换对应面板 |
| `Enter` | 提交输入 |
| `Backspace` | 删除字符 |
| `↑` / `↓` | 上下滚动 |
| `PageUp` / `PageDown` | 翻页滚动 |

## 使用说明

### 启动

```bash
# 启动 TUI（连接默认 Gateway）
agentrt-tui

# 指定 Gateway 地址和智能体配置
agentrt-tui --gateway-url http://localhost:8080 --agent-file agents/main.agent.yaml

# 设置环境变量
export AGENTRT_GATEWAY_URL=http://localhost:8080
agentrt-tui
```

### 对话操作

1. 启动后在 **Chat** 面板输入消息
2. 按 `Enter` 提交，智能体实时回复
3. 使用 `↑`/`↓` 翻阅历史消息
4. 按 `F1` 查看完整帮助信息

### 面板导航

- 按 `F3` 切换到日志面板，实时监控运行时日志
- 按 `F4` 切换到记忆面板，查看智能体已存储的记忆
- 按 `F5` 切换到插件面板，管理插件加载与卸载
- 按 `Esc` 快速回到对话面板

## 依赖关系

| 类别 | 依赖 |
|------|------|
| TUI 框架 | ratatui 0.28, crossterm 0.28 |
| HTTP 客户端 | reqwest 0.12 (json + rustls-tls + stream) |
| 异步运行时 | tokio 1 (full), tokio-stream 0.1, futures 0.3 |
| 序列化 | serde 1.0, serde_json 1.0 |
| CLI 解析 | clap 4.5 (derive + env) |
| 错误处理 | thiserror 2.0, anyhow 1.0 |
| 工具 | chrono 0.4, unicode-width 0.1 |

## 构建说明

```bash
# 构建
cargo build --release

# 运行
./target/release/agentrt-tui

# 确保 AgentRT Gateway 已在运行
```

---

© 2026 SPHARX Ltd. All Rights Reserved.