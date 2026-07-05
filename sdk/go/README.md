# Toolkit Go — AgentRT Go SDK

**版本**: v0.1.0

## 概述

AgentRT Go SDK 提供基于 Go 语言的 AgentRT 系统编程接口，采用惯用的 Go 风格设计，支持 goroutine 并发模型。SDK 包含客户端层、业务模块层（Task/Memory/Session/Skill）、系统调用绑定、遥测、插件系统和类型定义，与 Python/Rust/TypeScript SDK 保持 API 一致性。

## 目录结构

```
go/
├── agentrt/
│   ├── agentrt.go              # 版本信息（Version/Author/License）
│   ├── config.go               # 配置管理（Config/ConfigOption/环境变量）
│   ├── protocol.go             # 协议处理
│   ├── errors.go               # 错误定义与错误码
│   ├── client/
│   │   ├── client.go           # APIClient/Client/ClientConfig
│   │   └── mock.go             # MockClient 测试客户端
│   ├── modules/
│   │   ├── modules.go          # 模块导出
│   │   ├── base_manager.go     # BaseManager 基类
│   │   ├── task/
│   │   │   ├── manager.go      # TaskManager
│   │   │   ├── manager_test.go # TaskManager 测试
│   │   │   └── benchmark_test.go # 性能基准
│   │   ├── memory/
│   │   │   ├── manager.go      # MemoryManager
│   │   │   └── manager_test.go # MemoryManager 测试
│   │   ├── session/
│   │   │   ├── manager.go      # SessionManager
│   │   │   └── manager_test.go # SessionManager 测试
│   │   └── skill/
│   │       ├── manager.go      # SkillManager
│   │       └── manager_test.go # SkillManager 测试
│   ├── plugin/
│   │   ├── plugin.go           # Plugin 系统
│   │   └── plugin_test.go      # Plugin 测试
│   ├── syscall/
│   │   ├── syscall.go          # 系统调用绑定
│   │   └── syscall_test.go     # 系统调用测试
│   ├── telemetry/
│   │   ├── telemetry.go        # OpenTelemetry 遥测
│   │   └── telemetry_test.go   # 遥测测试
│   ├── types/
│   │   ├── types.go            # 类型定义
│   │   └── types_test.go       # 类型测试
│   └── utils/
│       ├── helpers.go          # 工具函数
│       └── helpers_test.go     # 工具函数测试
├── go.mod                      # Go 模块配置
└── README.md                   # 本文件
```

## 核心组件

### 客户端层

| 类型 | 说明 |
|------|------|
| `Client` | 同步 HTTP 客户端，支持 API Key 认证 |
| `APIClient` | 高级 API 客户端，封装所有业务模块 |
| `ClientConfig` | 客户端配置（endpoint/timeout/maxRetries/apiKey） |
| `MockClient` | 测试用 Mock 客户端 |

### 业务模块层

| 管理器 | 说明 | 核心方法 |
|--------|------|----------|
| `TaskManager` | 任务管理 | Submit/Get/Cancel/List/Wait |
| `MemoryManager` | 记忆管理 | Write/Read/Search/Delete/List |
| `SessionManager` | 会话管理 | Create/Get/Close/List |
| `SkillManager` | 技能管理 | Load/Execute/Unload/List |

### 类型定义

| 类型 | 说明 |
|------|------|
| `Task` / `TaskResult` | 任务与结果 |
| `Memory` / `MemorySearchResult` | 记忆与搜索结果 |
| `Session` | 会话 |
| `Skill` / `SkillResult` / `SkillInfo` | 技能与执行结果 |
| `TaskStatus` | 任务状态枚举 |
| `MemoryLayer` | 记忆层级（L1/L2/L3/L4） |
| `SessionStatus` | 会话状态枚举 |
| `SkillStatus` | 技能状态枚举 |

### 错误码体系

| 常量 | 值 | 说明 |
|------|-----|------|
| `CODE_SUCCESS` | `0x0000` | 成功 |
| `CODE_TIMEOUT` | `0x0004` | 超时 |
| `CODE_NOT_FOUND` | `0x0005` | 未找到 |
| `CODE_NETWORK_ERROR` | `0x000A` | 网络错误 |
| `CODE_TASK_FAILED` | `0x3001` | 任务失败 |
| `CODE_MEMORY_NOT_FOUND` | `0x4001` | 记忆未找到 |
| `CODE_SESSION_EXPIRED` | `0x4005` | 会话过期 |
| `CODE_SKILL_NOT_FOUND` | `0x4006` | 技能未找到 |

## 使用说明

### Client 初始化

```go
import "agentrt"

client, err := agentrt.NewClient("http://localhost:18789")
if err != nil {
    log.Fatal(err)
}
defer client.Close()

clientWithKey, err := agentrt.NewClientWithAPIKey("http://localhost:18789", "your-api-key")
```

### TaskManager

```go
taskManager := client.TaskManager()

task, err := taskManager.Submit(ctx, "analyze this data")
result, err := taskManager.Wait(ctx, task.ID, 30*time.Second)
tasks, err := taskManager.List(ctx, &agentrt.ListOptions{Limit: 10})
err := taskManager.Cancel(ctx, taskID)
```

### MemoryManager

```go
memoryManager := client.MemoryManager()

memoryID, err := memoryManager.Write(ctx, "important data", nil)
memories, err := memoryManager.Search(ctx, "query", 5)
memory, err := memoryManager.Read(ctx, memoryID)
err := memoryManager.Delete(ctx, memoryID)
```

### SessionManager

```go
sessionManager := client.SessionManager()

session, err := sessionManager.Create(ctx)
session, err := sessionManager.Get(ctx, sessionID)
err := sessionManager.Close(ctx, sessionID)
```

### SkillManager

```go
skillManager := client.SkillManager()

skill, err := skillManager.Load(ctx, "browser-skill")
result, err := skillManager.Execute(ctx, skill.ID, params)
err := skillManager.Unload(ctx, skill.ID)
```

### 配置说明

```go
config := agentrt.NewConfig(
    agentrt.WithEndpoint("http://localhost:18789"),
    agentrt.WithTimeout(30 * time.Second),
    agentrt.WithMaxRetries(3),
    agentrt.WithAPIKey("your-api-key"),
    agentrt.WithUserAgent("my-app/1.0"),
    agentrt.WithDebug(true),
)

client, err := agentrt.NewClientWithConfig(config)
```

也支持从环境变量加载配置：

```go
config := agentrt.NewConfigFromEnv()
// AGENTRT_ENDPOINT, AGENTRT_TIMEOUT, AGENTRT_API_KEY
```

## 依赖关系

- **Go 版本**: >= 1.22
- **核心依赖**: Go 标准库（net/http, encoding/json, context, sync）
- **无外部运行时依赖**

## 构建与测试

```bash
# 安装
go get agentrt

# 运行测试
go test ./...

# 运行基准测试
go test -bench=. ./...

# 运行特定模块测试
go test ./agentrt/modules/task/...
```

## 完整示例

```go
package main

import (
    "context"
    "fmt"
    "log"
    "time"

    "agentrt"
)

func main() {
    client, err := agentrt.NewClient("http://localhost:18789")
    if err != nil {
        log.Fatal(err)
    }
    defer client.Close()

    ctx := context.Background()

    task, err := client.TaskManager().Submit(ctx, "analyze sales data")
    if err != nil {
        log.Fatal(err)
    }

    result, err := client.TaskManager().Wait(ctx, task.ID, 60*time.Second)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Task result: %s\n", result.Output)

    memoryID, err := client.MemoryManager().Write(ctx, "analysis result", nil)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("Memory saved: %s\n", memoryID)
}
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
