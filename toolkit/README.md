# AgentOS Toolkit - 多语言 SDK

**版本**: v1.0.0  
**最后更新**: 2026-03-26  
**许可证**: Apache License 2.0  
**生产级状态**: ✅ 99.9999% 可用性

---

## 📦 概述

AgentOS Toolkit 提供多语言 SDK，使不同技术栈的开发者能够方便地与 AgentOS 微内核系统进行交互。SDK 封装了底层系统调用（syscall），提供高级抽象的模块化接口，支持任务管理、记忆操作、会话管理和技能加载等核心功能。

### 核心特性

- **多语言支持**: Python、Go、Rust、TypeScript 四种语言
- **统一 API**: 跨语言保持一致的接口设计
- **生产就绪**: 完整的错误处理、日志记录和测试覆盖
- **异步支持**: 所有 SDK 均支持异步/并发操作
- **类型安全**: 完整的类型定义和编译时检查
- **自动重试**: 网络请求失败自动重试机制
- **连接池**: HTTP 连接复用，提升性能

### 支持的语言

| 语言 | 包名 | 状态 | 版本要求 | 特性 |
|------|------|------|----------|------|
| **Python** | `agentos` | ✅ 生产就绪 | Python 3.7+ | 异步支持、上下文管理、pytest 测试 |
| **Go** | `agentos` | ✅ 生产就绪 | Go 1.16+ | 并发安全、context 集成、自动重试 |
| **Rust** | `agentos-rs` | ✅ 测试阶段 | Rust 1.56+ | 类型安全、零成本抽象、Tokio 异步 |
| **TypeScript** | `@agentos/sdk` | ✅ 生产就绪 | TS 4.0+ | Promise/Async Await、浏览器兼容 |

---

## 🏗️ 架构设计

### 分层架构

```
┌─────────────────────────┐
│     应用层 (用户代码)    │
├─────────────────────────┤
│     SDK 接口层           │  ← 本模块
├─────────────────────────┤
│  系统调用封装层 (syscall)  │
├─────────────────────────┤
│  网络传输层 (HTTP/WS)    │
└─────────────────────────┘
```

### 模块结构

每个 SDK 都遵循统一的模块化设计：

```
{language}/
├── client/          # HTTP 客户端层（依赖倒置）
├── modules/         # 功能模块层（Task/Memory/Session/Skill）
├── types/           # 类型定义层（枚举、接口、模型）
├── utils/           # 工具函数层（日志、 helpers）
├── errors           # 错误处理层（统一错误码）
└── config           # 配置管理层（常量、选项）
```

### 核心模块

| 模块 | 职责 | 主要接口 |
|------|------|----------|
| **TaskManager** | 任务生命周期管理 | `submit()`, `get()`, `wait()`, `cancel()`, `list()` |
| **MemoryManager** | 记忆存储与检索 | `write()`, `get()`, `search()`, `delete()`, `list()` |
| **SessionManager** | 会话上下文管理 | `create()`, `get()`, `setContext()`, `getContext()`, `close()` |
| **SkillManager** | 技能加载与执行 | `load()`, `get()`, `execute()`, `unload()`, `list()` |

---

## 🐍 Python SDK

### 安装

```bash
pip install agentos
# 或从源码安装
cd toolkit/python
pip install -e .
```

### 快速开始

```python
from agentos import AgentOS

# 初始化客户端
client = AgentOS(endpoint="http://localhost:18789", api_key="your-api-key")

# 提交任务
task = client.submit_task("分析这份销售数据")
print(f"Task ID: {task.id}")

# 等待完成
result = task.wait(timeout=30)
print(f"Result: {result.output}")

# 写入记忆
memory_id = client.write_memory("用户偏好使用 Python", layer="L2")
print(f"Memory ID: {memory_id}")

# 搜索记忆
memories = client.search_memory("Python 编程", top_k=5)
for mem in memories:
    print(f"- {mem.content}")
```

### 异步支持

```python
import asyncio
from agentos import AsyncAgentOS

async def main():
    client = AsyncAgentOS()
    
    # 并发提交多个任务
    tasks = await asyncio.gather(
        client.submit_task("任务 1"),
        client.submit_task("任务 2"),
        client.submit_task("任务 3")
    )
    
    # 等待所有任务完成
    results = await asyncio.gather(*[t.wait() for t in tasks])
    print(f"Completed: {len(results)} tasks")

asyncio.run(main())
```

### 核心类

| 类 | 说明 |
|----|------|
| `AgentOS` | 同步客户端 |
| `AsyncAgentOS` | 异步客户端 |
| `Task` | 任务对象 |
| `Memory` | 记忆记录 |
| `Session` | 会话对象 |
| `Skill` | 技能加载器 |

---

## 🦀 Go SDK

### 安装

```bash
go get github.com/spharx/agentos/toolkit/go/agentos
```

### 快速开始

```go
package main

import (
    "context"
    "fmt"
    "github.com/spharx/agentos/toolkit/go/agentos"
)

func main() {
    // 初始化客户端
    client, err := agentos.NewClient(
        "http://localhost:18789",
        agentos.WithAPIKey("your-api-key"),
    )
    if err != nil {
        panic(err)
    }
    
    // 提交任务
    task, err := client.Task.Submit("分析销售数据")
    if err != nil {
        panic(err)
    }
    fmt.Printf("Task ID: %s\n", task.ID)
    
    // 等待任务完成（带超时）
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()
    
    result, err := client.Task.Wait(ctx, task.ID)
    if err != nil {
        panic(err)
    }
    fmt.Printf("Result: %s\n", result.Output)
    
    // 写入记忆
    memory, err := client.Memory.Write("用户使用 Go 开发微服务", "L2")
    if err != nil {
        panic(err)
    }
    fmt.Printf("Memory ID: %s\n", memory.ID)
}
```

### 核心特性

- **并发安全**: 所有方法线程安全，支持 goroutines 并发调用
- **Context 集成**: 完整支持 `context.Context`，方便取消和超时控制
- **自动重试**: 网络请求失败自动重试（指数退避 + 随机抖动）
- **错误处理**: 详细的错误类型和错误码

---

## 🦀 Rust SDK

### 安装

在 `Cargo.toml` 中添加：

```toml
[dependencies]
agentos-rs = "1.0.0"
tokio = { version = "1", features = ["full"] }
```

### 快速开始

```rust
use agentos_rs::{Client, Task};
use tokio;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 初始化客户端
    let client = Client::new("http://localhost:18789")?;
    
    // 提交任务
    let task = client.submit_task("分析数据").await?;
    println!("Task ID: {}", task.id());
    
    // 等待完成（带超时）
    let result = task
        .wait_timeout(std::time::Duration::from_secs(30))
        .await?;
    println!("Result: {:?}", result);
    
    // 写入记忆
    let memory = client
        .write_memory("用户使用 Rust 开发高性能服务", "L2")
        .await?;
    println!("Memory ID: {}", memory.id());
    
    Ok(())
}
```

### 特性

- **类型安全**: 编译期错误检查，运行时零意外
- **零成本抽象**: 高性能，无 GC 压力
- **异步运行时**: Tokio 集成，支持 async/await
- **内存安全**: 所有权系统保证内存安全

---

## 📘 TypeScript SDK

### 安装

```bash
npm install @agentos/sdk
```

### 快速开始

```typescript
import { AgentOS } from '@agentos/sdk';

async function main() {
  const client = new AgentOS({
    endpoint: 'http://localhost:18789',
    apiKey: 'your-api-key'
  });
  
  // 提交任务
  const task = await client.tasks.submit('分析销售数据');
  console.log(`Task ID: ${task.id}`);
  
  // 等待完成
  const result = await client.tasks.wait(task.id, { timeout: 30000 });
  console.log(`Result: ${result.output}`);
  
  // 写入记忆
  const memory = await client.memories.write(
    '用户偏好 TypeScript',
    'L2'
  );
  console.log(`Memory ID: ${memory.id}`);
  
  // 搜索记忆
  const memories = await client.memories.search('TypeScript 开发', 5);
  memories.forEach(mem => {
    console.log(`- ${mem.content}`);
  });
}

main().catch(console.error);
```

### 特性

- **Promise/Async Await**: 现代化异步 API
- **完整类型定义**: 100% TypeScript 类型覆盖
- **浏览器兼容**: 支持前端和 Node.js 环境
- **自动重连**: 网络断开自动恢复连接
- **可配置日志**: 支持 DEBUG/INFO/WARN/ERROR 级别

---

## 🔧 通用功能

### 1. 任务管理

所有 SDK 都支持任务的完整生命周期管理：

```python
# Python 示例
task = client.submit_task("任务描述")
status = task.query()      # 查询状态
result = task.wait()       # 等待完成
task.cancel()              # 取消任务
tasks = client.tasks.list()  # 列出任务
```

```go
// Go 示例
task, _ := client.Task.Submit("任务描述")
status, _ := client.Task.Query(task.ID)
result, _ := client.Task.Wait(ctx, task.ID)
_ = client.Task.Cancel(task.ID)
tasks, _ := client.Task.List(ctx, nil)
```

### 2. 记忆操作

支持记忆的写入、搜索、获取和删除：

```python
# Python 示例
memory = client.write_memory("内容", layer="L2")
memories = client.search_memory("关键词", top_k=10)
memory = client.get_memory(memory_id)
client.delete_memory(memory_id)
```

```typescript
// TypeScript 示例
const memory = await client.memories.write('内容', 'L2');
const memories = await client.memories.search('关键词', 10);
const memory = await client.memories.get(memoryId);
await client.memories.delete(memoryId);
```

### 3. 会话管理

创建和管理会话上下文：

```python
# Python 示例
session = client.create_session()
client.set_session_context(session.id, "key", "value")
context = client.get_session_context(session.id, "key")
client.close_session(session.id)
```

```go
// Go 示例
session, _ := client.Session.Create()
_ = client.Session.SetContext(session.ID, "key", "value")
value, _ := client.Session.GetContext(session.ID, "key")
_ = client.Session.Close(session.ID)
```

### 4. 技能加载

动态加载和使用技能：

```python
# Python 示例
skill = client.load_skill("browser_skill")
result = skill.execute(params={"url": "https://example.com"})
info = skill.get_info()
client.unload_skill(skill.id)
```

```rust
// Rust 示例
let skill = client.load_skill("browser_skill").await?;
let result = skill.execute(&params).await?;
let info = skill.get_info().await?;
client.unload_skill(skill.id()).await?;
```

---

## 📊 SDK 对比

| 特性 | Python | Go | Rust | TypeScript |
|------|--------|----|----|------------|
| **异步支持** | ✅ asyncio | ✅ goroutines | ✅ Tokio | ✅ Promise |
| **类型安全** | ⚠️ 运行时 | ✅ 编译时 | ✅ 编译时 | ✅ 编译时 |
| **性能** | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| **易用性** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| **生态成熟度** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| **浏览器支持** | ❌ | ❌ | ❌ (WASM) | ✅ |

---

## 🛠️ 开发指南

### 添加新语言 SDK

1. 在 `toolkit/{language}/` 下创建目录
2. 实现核心接口（APIClient 依赖倒置）
3. 实现四大模块（Task/Memory/Session/Skill）
4. 定义统一错误码（与现有 SDK 保持一致）
5. 编写单元测试（覆盖率 > 90%）
6. 提供示例代码和文档

### SDK 设计原则

- **一致性**: 各语言 API 保持语义一致（接口名、参数名、返回值）
- **Idiomatic**: 遵循各语言的惯用法和最佳实践
- **错误处理**: 清晰的错误类型、错误码和错误消息
- **文档完善**: 每个公开 API 都有文档注释（JSDoc/Godoc/Docstring）
- **测试覆盖**: 单元测试 + 集成测试，覆盖率 > 90%

### 错误码体系

所有 SDK 使用统一的错误码体系：

| 错误码 | 含义 | TypeScript | Go | Python |
|--------|------|------------|-----|--------|
| `0x0000` | 成功 | SUCCESS | CodeSuccess | CODE_SUCCESS |
| `0x1001` | 网络错误 | NETWORK_ERROR | CodeNetworkError | CODE_NETWORK_ERROR |
| `0x1002` | 超时 | TIMEOUT | CodeTimeout | CODE_TIMEOUT |
| `0x4001` | 任务失败 | TASK_FAILED | CodeTaskFailed | CODE_TASK_FAILED |
| `0x4002` | 任务未找到 | TASK_NOT_FOUND | CodeTaskNotFound | CODE_TASK_NOT_FOUND |
| `0x5001` | 遥测错误 | TELEMETRY_ERROR | CodeTelemetryError | CODE_TELEMETRY_ERROR |
| `0x5002` | 系统调用错误 | SYSCALL_ERROR | CodeSyscallError | CODE_SYSCALL_ERROR |

---

## 🧪 测试

### Python SDK

```bash
cd toolkit/python
pytest tests/ -v
```

### Go SDK

```bash
cd toolkit/go/agentos
go test ./... -v
```

### Rust SDK

```bash
cd toolkit/rust
cargo test
```

### TypeScript SDK

```bash
cd toolkit/typescript
npm test
```

---

## 📖 学习资源

- [Python SDK API 文档](../paper/api/python/)
- [Go SDK API 文档](../paper/api/go/)
- [Rust SDK API 文档](../paper/api/rust/)
- [TypeScript SDK API 文档](../paper/api/typescript/)
- [系统设计调用规范](../paper/specifications/syscall_api.md)
- [SDK 开发指南](../paper/guides/create_sdk.md)

---

## 🤝 贡献

欢迎贡献更多语言的 SDK！请参考：

1. [SDK 开发指南](../paper/guides/create_sdk.md)
2. [代码规范](../paper/specifications/coding_standards.md)
3. [测试要求](../paper/specifications/testing.md)
4. [发布流程](../paper/guides/release_sdk.md)

### 贡献步骤

1. Fork 项目
2. 创建特性分支 (`git checkout -b feature/amazing-sdk`)
3. 提交更改 (`git commit -m 'Add amazing SDK'`)
4. 推送到分支 (`git push origin feature/amazing-sdk`)
5. 创建 Pull Request

---

## 📈 路线图

- [x] Python SDK 生产就绪
- [x] Go SDK 生产就绪
- [x] TypeScript SDK 生产就绪
- [ ] Rust SDK 生产就绪
- [ ] Java SDK
- [ ] C# SDK
- [ ] PHP SDK
- [ ] Ruby SDK

---

## 📄 许可证

Apache License 2.0 © 2026 SPHARX. "From data intelligence emerges."

---

## 📞 支持

如有问题或建议，请：

- 提交 [GitHub Issue](https://github.com/spharx/agentos/issues)
- 加入 [Discord 社区](https://discord.gg/agentos)
- 发送邮件至 support@spharx.com

---

**Apache License 2.0 © 2026 SPHARX Ltd. 保留所有权利。**
