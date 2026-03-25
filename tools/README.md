# AgentOS 多语言 SDK

**版本**: v1.0.0.6  
**最后更新**: 2026-03-18  
**许可证**: Apache License 2.0

---

## 📦 概述

AgentOS 提供多语言 SDK，方便不同技术栈的开发者使用系统功能。SDK 封装了底层系统调用，提供更高级的抽象接口。

### 支持的语言

| 语言 | 状态 | 包名 | 特性 |
|------|------|------|------|
| **Python** | ✅ 生产就绪 | `agentos` | 异步支持、上下文管理 |
| **Go** | ✅ 生产就绪 | `agentos` | 并发安全、错误处理 |
| **Rust** | ✅ 测试阶段 | `agentos-rs` | 类型安全、零成本抽象 |
| **TypeScript** | ✅ 测试阶段 | `@agentos/sdk` | Promise/Async Await |

---

## 🐍 Python SDK


<!-- From data intelligence emerges. by spharx -->
### 安装

```bash
pip install agentos
# 或从源码安装
cd tools/python
pip install -e .
```

### 快速开始

```python
from agentos import AgentOS, Task, Memory

# 初始化客户端
client = AgentOS(endpoint="http://localhost:18789")

# 提交任务
task = client.submit_task("分析这份销售数据")
print(f"Task ID: {task.id}")

# 等待完成
result = task.wait(timeout=30)
print(f"Result: {result.output}")

# 写入记忆
memory_id = client.write_memory("用户偏好使用 Python")
print(f"Memory ID: {memory_id}")

# 搜索记忆
memories = client.search_memory("Python 编程", top_k=5)
for mem in memories:
    print(f"- {mem.content}")
```

### 核心类

| 类 | 说明 |
|----|------|
| `AgentOS` | 主客户端类 |
| `Task` | 任务对象 |
| `Memory` | 记忆记录 |
| `Session` | 会话管理 |
| `Skill` | 技能加载器 |

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

asyncio.run(main())
```

---

## 🦀 Go SDK

### 安装

```bash
go get github.com/spharx/agentos/tools/go/agentos
```

### 快速开始

```go
package main

import (
    "fmt"
    "github.com/spharx/agentos/tools/go/agentos"
)

func main() {
    // 初始化客户端
    client, err := agentos.NewClient("http://localhost:18789")
    if err != nil {
        panic(err)
    }
    
    // 提交任务
    taskID, err := client.SubmitTask("分析销售数据")
    if err != nil {
        panic(err)
    }
    fmt.Printf("Task ID: %d\n", taskID)
    
    // 等待任务完成
    result, err := client.WaitTask(taskID, 30)
    if err != nil {
        panic(err)
    }
    fmt.Printf("Result: %s\n", result.Output)
    
    // 写入记忆
    memoryID, err := client.WriteMemory("用户使用 Go 开发微服务")
    if err != nil {
        panic(err)
    }
    fmt.Printf("Memory ID: %d\n", memoryID)
}
```

### 核心特性

- **并发安全**: 所有方法线程安全
- **错误处理**: 详细的错误类型
- **上下文支持**: `context.Context` 集成
- **自动重试**: 网络请求失败自动重试

---

## 🦀 Rust SDK

### 安装

在 `Cargo.toml` 中添加：

```toml
[dependencies]
agentos-rs = "1.0.0"
```

### 快速开始

```rust
use agentos_rs::{Client, Task};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 初始化客户端
    let client = Client::new("http://localhost:18789")?;
    
    // 提交任务
    let task = client.submit_task("分析数据")?;
    println!("Task ID: {}", task.id());
    
    // 等待完成
    let result = task.wait_timeout(std::time::Duration::from_secs(30))?;
    println!("Result: {:?}", result);
    
    Ok(())
}
```

### 特性

- **类型安全**: 编译期错误检查
- **零成本抽象**: 高性能
- **异步运行时**: Tokio 集成
- **内存安全**: 无 GC 压力

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
    endpoint: 'http://localhost:18789'
  });
  
  // 提交任务
  const task = await client.submitTask('分析销售数据');
  console.log(`Task ID: ${task.id}`);
  
  // 等待完成
  const result = await task.wait({ timeout: 30000 });
  console.log(`Result: ${result.output}`);
  
  // 搜索记忆
  const memories = await client.searchMemory('Python 编程', { limit: 5 });
  memories.forEach(mem => {
    console.log(`- ${mem.content}`);
  });
}

main();
```

### 特性

- **Promise/Async Await**: 现代化异步 API
- **类型定义**: 完整的 TypeScript 类型
- **浏览器兼容**: 支持前端使用
- **自动重连**: 网络断开自动恢复

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
```

### 2. 记忆操作

支持记忆的写入、搜索、获取和删除：

```go
// Go 示例
memoryID, _ := client.WriteMemory("内容")
memories, _ := client.SearchMemory("关键词", 10)
memory, _ := client.GetMemory(memoryID)
client.DeleteMemory(memoryID)
```

### 3. 会话管理

创建和管理会话上下文：

```typescript
// TypeScript 示例
const session = await client.createSession();
await session.setContext({ key: "value" });
const context = await session.getContext();
await session.close();
```

### 4. 技能加载

动态加载和使用技能：

```rust
// Rust 示例
let skill = client.load_skill("browser_skill")?;
let result = skill.execute(&params)?;
```

---

## 📊 SDK 对比

| 特性 | Python | Go | Rust | TypeScript |
|------|--------|----|----|------------|
| **异步支持** | ✅ | ✅ | ✅ | ✅ |
| **类型安全** | ⚠️ | ✅ | ✅ | ✅ |
| **性能** | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| **易用性** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| **生态成熟度** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |

---

## 🛠️ 开发指南

### 添加新语言 SDK

1. 在 `tools/{language}/` 下创建目录
2. 实现核心接口（Task、Memory、Session）
3. 编写单元测试
4. 提供示例代码和文档

### SDK 设计原则

- **一致性**: 各语言 API 保持语义一致
- ** idiomatic**: 遵循各语言的惯用法
- **错误处理**: 清晰的错误类型和消息
- **文档完善**: 每个公开 API 都有文档注释

---

## 📖 学习资源

- [Python SDK API 文档](../partdocs/api/python/)
- [Go SDK API 文档](../partdocs/api/go/)
- [Rust SDK API 文档](../partdocs/api/rust/)
- [TypeScript SDK API 文档](../partdocs/api/typescript/)
- [系统调用规范](../partdocs/specifications/syscall_api.md)

---

## 🤝 贡献

欢迎贡献更多语言的 SDK！请参考：

1. [SDK 开发指南](../partdocs/guides/create_sdk.md)
2. [代码规范](../partdocs/specifications/coding_standards.md)
3. [测试要求](../partdocs/specifications/testing.md)

---

**Apache License 2.0 © 2026 SPHARX. "From data intelligence emerges."**

---

© 2026 SPHARX Ltd. 保留所有权利。
