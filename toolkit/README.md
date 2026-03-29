# AgentOS Toolkit

**统一的多语言 SDK 集合 | 版本 3.0.0 | MIT License**

AgentOS Toolkit 是 AgentOS 微内核架构的官方多语言 SDK 集合，提供 Python、Go、Rust、TypeScript 四种语言的完整实现。通过统一的 API 接口，开发者可以便捷地与 AgentOS 核心服务进行交互，实现任务提交、记忆管理、会话控制和技能加载等功能。

## 📖 目录

- [核心特性](#-核心特性)
- [架构设计](#-架构设计)
- [快速开始](#-快速开始)
  - [Python SDK](#python-sdk)
  - [Go SDK](#go-sdk)
  - [Rust SDK](#rust-sdk)
  - [TypeScript SDK](#typescript-sdk)
- [通用功能](#-通用功能)
- [开发指南](#-开发指南)
- [版本信息](#-版本信息)

## ✨ 核心特性

### 🌍 多语言支持

- **Python SDK** (`v3.0.0`): 异步支持 (asyncio)、上下文管理、pytest 测试覆盖
- **Go SDK** (`v3.0.0`): 依赖倒转设计、函数式选项模式、87.2% 测试覆盖率
- **Rust SDK** (`v3.0.0`): 内存安全、零成本抽象、Tokio 异步运行时
- **TypeScript SDK** (`v3.0.0`): Promise/Async Await、完整类型定义、浏览器兼容

### 🎯 统一 API

所有语言 SDK 提供一致的核心接口：

```
submit_task()      → 提交任务到 AgentOS 认知循环
write_memory()     → 写入记忆到四层记忆系统
search_memory()    → 搜索记忆内容
create_session()   → 创建/恢复会话上下文
load_skill()       → 加载技能模块
```

### 🏗️ 模块化架构

```
toolkit/
├── python/          # Python SDK
├── go/              # Go SDK
├── rust/            # Rust SDK
├── typescript/      # TypeScript SDK
├── DESIGN.md        # 整体设计文档
└── README.md        # 本文档
```

每个 SDK 遵循统一的目录结构：

```
<language>/
├── client/          # HTTP 客户端实现
├── modules/         # 功能模块 (Task, Memory, Session, Skill)
├── types/           # 类型定义
├── utils/           # 工具函数
├── errors/          # 错误处理
└── config/          # 配置管理
```

### 🔒 生产就绪

- **统一错误码体系**: 0x0000-0x6xxx 十六进制错误码，跨语言一致
- **完整测试覆盖**: 单元测试 + 集成测试 + 边界测试
- **类型安全**: 静态类型检查 (Go/Rust/TypeScript) 或类型提示 (Python)
- **并发安全**: 线程/协程安全的客户端实现

## 🏛️ 架构设计

### 分层架构

```
┌─────────────────────────────────────────┐
│          应用层 (Application)           │
│  用户业务逻辑、Agent 实现、工作流编排    │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│        SDK 接口层 (Toolkit SDK)         │
│  TaskManager | MemoryManager | ...      │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│      系统调用封装层 (System Call)       │
│  APIClient 接口抽象 + HTTP 客户端实现    │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│        网络传输层 (Network Layer)       │
│  HTTP/HTTPS + RESTful API + JSON/RPC    │
└─────────────────────────────────────────┘
```

### 核心模块

- **TaskManager**: 任务提交、状态查询、结果获取
- **MemoryManager**: 记忆写入、搜索、删除（支持四层记忆系统）
- **SessionManager**: 会话创建、恢复、关闭
- **SkillManager**: 技能加载、卸载、查询

### 设计原则

遵循 [AgentOS 架构设计原则](../manuals/architecture/ARCHITECTURAL_PRINCIPLES.md)：

- ✅ **依赖倒转**: Manager 依赖 APIClient 接口而非具体实现
- ✅ **单一职责**: 每个 Manager 只负责一个功能域
- ✅ **接口隔离**: 细粒度的接口设计，避免胖接口
- ✅ **五维正交**: 功能、性能、安全、可维护性、可扩展性正交设计

## 🚀 快速开始

### Python SDK

#### 安装

```bash
pip install agentos-toolkit
```

#### 快速开始

```python
import asyncio
from agentos import TaskManager, MemoryManager, Config

async def main():
    # 初始化配置
    config = Config(
        base_url="http://localhost:8080",
        api_key="your-api-key",
        timeout=30
    )
    
    # 创建管理器
    task_mgr = TaskManager(config)
    memory_mgr = MemoryManager(config)
    
    # 提交任务
    task = await task_mgr.submit_task(
        content="分析用户输入的情感倾向",
        priority="high"
    )
    
    # 写入记忆
    await memory_mgr.write(
        content="用户表达了对产品的满意",
        level="L2",
        tags=["情感分析", "正面反馈"]
    )
    
    # 搜索记忆
    results = await memory_mgr.search(
        query="产品反馈",
        level="L2",
        limit=10
    )

if __name__ == "__main__":
    asyncio.run(main())
```

#### 核心 API

```python
# TaskManager
task = await task_mgr.submit_task(content, priority?, tags?)
status = await task_mgr.get_status(task_id)
result = await task_mgr.get_result(task_id)

# MemoryManager
await memory_mgr.write(content, level, tags?, metadata?)
results = await memory_mgr.search(query, level?, limit?)
await memory_mgr.delete(memory_id)

# SessionManager
session = await session_mgr.create(user_id, metadata?)
session = await session_mgr.restore(session_id)
await session_mgr.close(session_id)

# SkillManager
skill = await skill_mgr.load(skill_name, version?)
await skill_mgr.unload(skill_name)
skills = await skill_mgr.list()
```

**详细文档**: [python/README.md](./python/README.md)

---

### Go SDK

#### 安装

```bash
go get github.com/spharxworks/agentos-toolkit-go
```

#### 快速开始

```go
package main

import (
    "context"
    "github.com/spharxworks/agentos-toolkit-go"
    "github.com/spharxworks/agentos-toolkit-go/config"
    "github.com/spharxworks/agentos-toolkit-go/modules"
)

func main() {
    ctx := context.Background()
    
    // 初始化配置（函数式选项模式）
    cfg := config.New(
        config.WithBaseURL("http://localhost:8080"),
        config.WithAPIKey("your-api-key"),
        config.WithTimeout(30),
    )
    
    // 创建管理器
    taskMgr := modules.NewTaskManager(cfg)
    memoryMgr := modules.NewMemoryManager(cfg)
    
    // 提交任务
    task, err := taskMgr.SubmitTask(ctx, 
        "分析用户输入的情感倾向",
        modules.WithPriority("high"),
        modules.WithTags([]string{"情感分析"}),
    )
    
    // 写入记忆
    err = memoryMgr.Write(ctx,
        "用户表达了对产品的满意",
        modules.WithLevel("L2"),
        modules.WithTags([]string{"情感分析", "正面反馈"}),
    )
    
    // 搜索记忆
    results, err := memoryMgr.Search(ctx,
        "产品反馈",
        modules.WithLevel("L2"),
        modules.WithLimit(10),
    )
}
```

#### 核心 API

```go
// TaskManager
task, err := taskMgr.SubmitTask(ctx, content, options...)
status, err := taskMgr.GetStatus(ctx, taskID)
result, err := taskMgr.GetResult(ctx, taskID)

// MemoryManager
err := memoryMgr.Write(ctx, content, options...)
results, err := memoryMgr.Search(ctx, query, options...)
err := memoryMgr.Delete(ctx, memoryID)

// SessionManager
session, err := sessionMgr.Create(ctx, userID, options...)
session, err := sessionMgr.Restore(ctx, sessionID)
err := sessionMgr.Close(ctx, sessionID)

// SkillManager
skill, err := skillMgr.Load(ctx, skillName, options...)
err := skillMgr.Unload(ctx, skillName)
skills, err := skillMgr.List(ctx)
```

**详细文档**: [go/README.md](./go/README.md)

---

### Rust SDK

#### 安装

```toml
[dependencies]
agentos-toolkit = "3.0.0"
tokio = { version = "1", features = ["full"] }
serde = { version = "1", features = ["derive"] }
```

#### 快速开始

```rust
use agentos_toolkit::{Config, TaskManager, MemoryManager};
use tokio;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 初始化配置
    let config = Config::builder()
        .base_url("http://localhost:8080")
        .api_key("your-api-key")
        .timeout(30)
        .build()?;
    
    // 创建管理器
    let task_mgr = TaskManager::new(config.clone());
    let memory_mgr = MemoryManager::new(config.clone());
    
    // 提交任务
    let task = task_mgr.submit_task(
        "分析用户输入的情感倾向",
        Some("high"),
        Some(vec!["情感分析".to_string()]),
    ).await?;
    
    // 写入记忆
    memory_mgr.write(
        "用户表达了对产品的满意",
        "L2",
        Some(vec!["情感分析".to_string(), "正面反馈".to_string()]),
        None,
    ).await?;
    
    // 搜索记忆
    let results = memory_mgr.search(
        "产品反馈",
        Some("L2"),
        Some(10),
    ).await?;
    
    Ok(())
}
```

#### 核心 API

```rust
// TaskManager
let task = task_mgr.submit_task(content, priority, tags).await?;
let status = task_mgr.get_status(task_id).await?;
let result = task_mgr.get_result(task_id).await?;

// MemoryManager
memory_mgr.write(content, level, tags, metadata).await?;
let results = memory_mgr.search(query, level, limit).await?;
memory_mgr.delete(memory_id).await?;

// SessionManager
let session = session_mgr.create(user_id, metadata).await?;
let session = session_mgr.restore(session_id).await?;
session_mgr.close(session_id).await?;

// SkillManager
let skill = skill_mgr.load(skill_name, version).await?;
skill_mgr.unload(skill_name).await?;
let skills = skill_mgr.list().await?;
```

**详细文档**: [rust/README.md](./rust/README.md)

---

### TypeScript SDK

#### 安装

```bash
npm install @agentos/toolkit
# 或
yarn add @agentos/toolkit
```

#### 快速开始

```typescript
import { TaskManager, MemoryManager, Config } from '@agentos/toolkit';

async function main() {
    // 初始化配置
    const config = new Config({
        baseURL: 'http://localhost:8080',
        apiKey: 'your-api-key',
        timeout: 30000,
    });
    
    // 创建管理器
    const taskMgr = new TaskManager(config);
    const memoryMgr = new MemoryManager(config);
    
    // 提交任务
    const task = await taskMgr.submitTask({
        content: '分析用户输入的情感倾向',
        priority: 'high',
        tags: ['情感分析'],
    });
    
    // 写入记忆
    await memoryMgr.write({
        content: '用户表达了对产品的满意',
        level: 'L2',
        tags: ['情感分析', '正面反馈'],
    });
    
    // 搜索记忆
    const results = await memoryMgr.search({
        query: '产品反馈',
        level: 'L2',
        limit: 10,
    });
}

main().catch(console.error);
```

#### 核心 API

```typescript
// TaskManager
const task = await taskMgr.submitTask(options);
const status = await taskMgr.getStatus(taskId);
const result = await taskMgr.getResult(taskId);

// MemoryManager
await memoryMgr.write(options);
const results = await memoryMgr.search(options);
await memoryMgr.delete(memoryId);

// SessionManager
const session = await sessionMgr.create(options);
const session = await sessionMgr.restore(sessionId);
await sessionMgr.close(sessionId);

// SkillManager
const skill = await skillMgr.load(skillName, version);
await skillMgr.unload(skillName);
const skills = await skillMgr.list();
```

**详细文档**: [typescript/README.md](./typescript/README.md)

## 🔧 通用功能

### 四层记忆系统

所有 SDK 支持统一的四层记忆模型：

| 层级 | 名称 | 描述 | 示例 |
|------|------|------|------|
| **L1** | 原始卷 | 原始感官数据 | 图像像素、音频波形 |
| **L2** | 特征层 | 提取的特征向量 | SIFT 特征、MFCC |
| **L3** | 结构层 | 结构化知识 | 概念图、关系网 |
| **L4** | 模式层 | 抽象模式 | 因果链、数学结构 |

```python
# Python 示例
await memory_mgr.write(content, level="L1")  # 原始数据
await memory_mgr.write(content, level="L2")  # 特征向量
await memory_mgr.write(content, level="L3")  # 结构化知识
await memory_mgr.write(content, level="L4")  # 抽象模式
```

### 统一错误码体系

所有 SDK 使用统一的十六进制错误码：

| 错误码范围 | 类别 | 示例 |
|-----------|------|------|
| `0x0000-0x0FFF` | 成功 | `0x0000` 成功 |
| `0x1000-0x1FFF` | 客户端错误 | `0x1004` 参数无效 |
| `0x2000-0x2FFF` | 服务端错误 | `0x2001` 内部错误 |
| `0x3000-0x3FFF` | 网络错误 | `0x3001` 连接超时 |
| `0x4000-0x4FFF` | 认证错误 | `0x4001` 未授权 |
| `0x5000-0x5FFF` | 业务逻辑错误 | `0x5001` 任务不存在 |
| `0x6000-0x6FFF` | 系统保留 | - |

```go
// Go 示例
if err != nil {
    if err.Error().Code == 0x1004 {
        // 处理参数无效错误
    }
}
```

### 并发支持

- **Python**: `asyncio` + `async/await`
- **Go**: `goroutines` + `channels`
- **Rust**: `Tokio` + `async/await`
- **TypeScript**: `Promise` + `async/await`

## 🛠️ 开发指南

### 添加新语言 SDK

1. **创建目录结构**

```bash
mkdir -p <language>/{client,modules,types,utils,errors,config}
```

2. **实现核心接口**

- `APIClient`: HTTP 客户端接口
- `TaskManager`: 任务管理接口
- `MemoryManager`: 记忆管理接口
- `SessionManager`: 会话管理接口
- `SkillManager`: 技能管理接口

3. **遵循统一错误码**

参考 [DESIGN.md](./DESIGN.md) 中的错误码定义

4. **编写测试**

- 单元测试覆盖率 ≥ 85%
- 集成测试覆盖所有核心 API
- 边界测试覆盖错误场景

5. **编写文档**

- README.md 包含安装、快速开始、API 参考
- 代码注释完整
- 示例代码可运行

### 代码规范

- **命名**: 语义化命名，遵循语言惯例
- **类型**: 完整的类型定义/提示
- **错误**: 使用统一错误码体系
- **测试**: 测试文件与源代码同目录
- **文档**: 所有公共 API 必须有文档注释

## 📊 版本信息

| SDK | 版本 | 状态 | 测试覆盖率 |
|-----|------|------|-----------|
| Python | `3.0.0` | ✅ 稳定 | 92.3% |
| Go | `3.0.0` | ✅ 稳定 | 87.2% |
| Rust | `3.0.0` | ✅ 稳定 | 89.1% |
| TypeScript | `3.0.0` | ✅ 稳定 | 90.5% |

### 版本历史

- **v3.0.0** (当前版本)
  - 统一所有 SDK 版本号为 3.0.0
  - 重构代码结构，降低重复率至 3%
  - 完善依赖倒转设计
  - 统一错误码体系

- **v2.0.0**
  - 添加 Rust SDK
  - 实现四层记忆系统
  - 统一配置管理

- **v1.0.0**
  - 初始版本
  - Python、Go、TypeScript SDK

## 📚 相关文档

- [DESIGN.md](./DESIGN.md) - Toolkit 模块整体设计
- [架构设计原则](../manuals/architecture/ARCHITECTURAL_PRINCIPLES.md) - AgentOS 五维正交系统
- [模块结构检查报告](../.本地/toolkit/20260326-02 次模块结构检查报告.md) - 模块结构完整性检查

## 🤝 贡献

欢迎贡献代码、报告问题或提出建议！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](../LICENSE) 文件

## 👥 作者

**SpharxWorks**

---

**AgentOS Toolkit** - 让多语言开发更简单
