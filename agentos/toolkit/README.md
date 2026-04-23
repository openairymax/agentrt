# Toolkit — 多语言 SDK 工具包

`agentos/toolkit/` 是 AgentOS 的多语言软件开发工具包，提供 Python、Go、Rust 和 TypeScript 四种语言的 SDK，方便开发者在不同语言环境中集成 AgentOS 功能。

## 设计目标

- **多语言支持**：覆盖主流编程语言，满足不同技术栈的需求
- **API 一致性**：各语言 SDK 提供一致的接口设计，降低学习成本
- **协议兼容**：JSON-RPC 2.0 协议在所有语言中实现一致
- **功能完整**：各语言 SDK 覆盖 AgentOS 核心功能

## 支持语言

| 语言 | 目录 | 版本 |
|------|------|------|
| **Python** | `python/` | v1.0.0.9 |
| **Go** | `go/` | v1.0.0.9 |
| **Rust** | `rust/` | v1.0.0.9 |
| **TypeScript** | `typescript/` | v1.0.0.9 |

## 核心功能

| 功能 | Python | Go | Rust | TypeScript |
|------|--------|----|------|------------|
| Agent 管理 | ✅ | ✅ | ✅ | ✅ |
| 任务管理 | ✅ | ✅ | ✅ | ✅ |
| 会话管理 | ✅ | ✅ | ✅ | ✅ |
| 内存管理 | ✅ | ✅ | ✅ | ✅ |
| 技能管理 | ✅ | ✅ | ✅ | ✅ |
| 系统调用 | ✅ | ✅ | ✅ | ✅ |
| 遥测 | ✅ | ✅ | ✅ | ✅ |

## 快速开始

### Python

```bash
cd toolkit/python
pip install -e .
```

### Go

```bash
cd toolkit/go
go build ./...
```

### Rust

```bash
cd toolkit/rust
cargo build
```

### TypeScript

```bash
cd toolkit/typescript
npm install
npm run build
```

## 协议兼容

所有语言 SDK 均遵循 AgentOS JSON-RPC 2.0 协议规范：

| 组件 | 说明 |
|------|------|
| Client | HTTP/WebSocket 客户端，支持连接池和重试 |
| Protocol | 请求/响应序列化与反序列化 |
| Types | 共享类型定义 |
| Errors | 统一错误处理 |

---

*AgentOS Toolkit*
