# AgentOS Go SDK

**版本**: 2.0.0
**最后更新**: 2026-03-22
**Go 最低版本**: 1.16
**测试覆盖率**: 87.2%

## 概述

AgentOS Go SDK 提供了一个优雅、模块化的 Go 语言客户端，用于与 AgentOS 系统进行交互。SDK v2.0 经历了系统性的模块化改造，遵循工程控制论和双系统思维，具备生产级质量。

## 架构设计

### 模块结构

```
agentos/
├── errors.go           # 统一错误体系（错误码、哨兵错误、HTTP 状态映射）
├── config.go           # 配置管理（函数式选项模式、环境变量注入、零值合并）
├── types.go            # 类型定义（枚举、请求选项、分页/排序/过滤）
├── helpers.go          # 公共辅助函数（类型安全 map 提取、URL 构建、响应解析）
├── client.go           # 核心客户端（HTTP 通信、APIClient 接口、重试逻辑）
├── task_manager.go     # 任务管理器（提交、轮询等待、批量操作）
├── memory_manager.go   # 记忆管理器（多层级、语义搜索、演化）
├── session_manager.go  # 会话管理器（上下文、过期清理、活跃统计）
├── skill_manager.go    # 技能管理器（加载/执行/注册/验证）
├── *_test.go           # 各模块单元测试（共 170+ 测试用例）
```

### 核心设计原则

| 原则 | 实现方式 |
|------|---------|
| **依赖倒转** | `APIClient` 接口抽象 HTTP 层，Manager 仅依赖接口 |
| **函数式选项** | `ConfigOption` 模式，零值保护守卫 |
| **统一错误** | `ErrorCode` 类型 + `Err` 前缀哨兵变量 + `errors.Is/As` 链 |
| **类型安全** | `GetString`/`GetInt64`/`GetMap` 等辅助函数避免类型断言散落 |
| **资源管理** | RAII 风格 `defer client.Close()`，连接池自动回收 |

### 依赖关系

```
Client ──implements──▶ APIClient (interface)
                         ▲
         ┌───────────────┼───────────────┐
  TaskManager      MemoryManager    SessionManager    SkillManager
```

各 Manager 仅依赖 `APIClient` 接口，测试时注入 `mockAPIClient` 而无需启动 HTTP 服务。

## 安装

```bash
go get github.com/spharx/agentos/tools/go
```

## 快速开始

### 创建客户端

```go
package main

import (
    "context"
    "fmt"
    "time"

    "github.com/spharx/agentos/tools/go/agentos"
)

func main() {
    // 使用默认配置（endpoint: http://localhost:18789）
    client, err := agentos.NewClient()
    if err != nil {
        panic(err)
    }
    defer client.Close()

    // 使用自定义配置
    client, err = agentos.NewClient(
        agentos.WithEndpoint("http://agentos-server:18789"),
        agentos.WithTimeout(30*time.Second),
        agentos.WithAPIKey("your-api-key"),
        agentos.WithDebug(true),
    )
}
```

### 环境变量配置

```bash
export AGENTOS_ENDPOINT="http://localhost:18789"
export AGENTOS_TIMEOUT="30s"
export AGENTOS_MAX_RETRIES="3"
export AGENTOS_API_KEY="your-api-key"
export AGENTOS_DEBUG="true"
```

```go
// 从环境变量创建配置
config := agentos.NewConfigFromEnv()
client, err := agentos.NewClientWithConfig(config)
```

### 配置合并

```go
base := agentos.DefaultConfig()
override := &agentos.Config{
    Endpoint: "http://prod-server:18789",
    APIKey:   "prod-key",
}
// 非零值覆盖，零值保留 base 原值
merged := base.Merge(override)
```

## API 参考

### Client 核心

| 方法 | 描述 |
|------|------|
| `NewClient(opts ...ConfigOption)` | 函数式选项创建客户端 |
| `NewClientWithConfig(config *Config)` | 使用预构建配置创建 |
| `Tasks()` | 获取任务管理器 |
| `Memories()` | 获取记忆管理器 |
| `Sessions()` | 获取会话管理器 |
| `Skills()` | 获取技能管理器 |
| `Health(ctx)` | 健康检查 |
| `Metrics(ctx)` | 系统指标 |
| `Close()` | 关闭连接池 |

**便捷方法**: `SubmitTask`, `WriteMemory`, `SearchMemory`, `CreateSession`, `LoadSkill`

### TaskManager

| 方法 | 描述 |
|------|------|
| `Submit(ctx, description)` | 提交任务 |
| `SubmitWithOptions(ctx, desc, priority, metadata)` | 带优先级提交 |
| `Get(ctx, taskID)` | 获取任务详情 |
| `Query(ctx, taskID)` | 查询任务状态 |
| `Wait(ctx, taskID, timeout)` | 阻塞等待终态 |
| `WaitForAll(ctx, taskIDs, timeout)` | 批量等待 |
| `Cancel(ctx, taskID)` | 取消任务 |
| `GetResult(ctx, taskID)` | 获取结果 |
| `BatchSubmit(ctx, descriptions)` | 批量提交 |
| `List(ctx, opts)` | 列出任务 |
| `Delete(ctx, taskID)` | 删除任务 |

### MemoryManager

| 方法 | 描述 |
|------|------|
| `Write(ctx, content, layer)` | 写入记忆 |
| `WriteWithOptions(ctx, content, layer, metadata)` | 带元数据写入 |
| `Get(ctx, memoryID)` | 获取记忆 |
| `Search(ctx, query, topK)` | 语义搜索 |
| `SearchByLayer(ctx, query, topK, layer)` | 按层级搜索 |
| `Update(ctx, memoryID, content)` | 更新记忆 |
| `Delete(ctx, memoryID)` | 删除记忆 |
| `List(ctx, opts)` | 列出记忆 |
| `ListByLayer(ctx, layer, opts)` | 按层级列出 |
| `Count(ctx)` | 记忆计数 |
| `Clear(ctx)` | 清空记忆 |
| `BatchWrite(ctx, items)` | 批量写入 |
| `Evolve(ctx, memoryID, newLayer)` | 记忆演化（层级提升） |
| `GetStats(ctx)` | 记忆统计 |

### SessionManager

| 方法 | 描述 |
|------|------|
| `Create(ctx, userID)` | 创建会话 |
| `CreateWithOptions(ctx, userID, metadata)` | 带元数据创建 |
| `Get(ctx, sessionID)` | 获取会话 |
| `SetContext(ctx, sessionID, key, value)` | 设置上下文键值 |
| `GetContext(ctx, sessionID, key)` | 获取上下文值 |
| `GetAllContext(ctx, sessionID)` | 获取全部上下文 |
| `DeleteContext(ctx, sessionID, key)` | 删除上下文键 |
| `Close(ctx, sessionID)` | 关闭会话 |
| `List(ctx, opts)` | 列出会话 |
| `ListByUser(ctx, userID)` | 按用户列出 |
| `ListActive(ctx)` | 列出活跃会话 |
| `Update(ctx, sessionID, metadata)` | 更新元数据 |
| `Refresh(ctx, sessionID)` | 刷新会话 |
| `IsExpired(ctx, sessionID)` | 检查是否过期 |
| `Count(ctx)` | 会话计数 |
| `CountActive(ctx)` | 活跃会话计数 |
| `CleanExpired(ctx)` | 清理过期会话 |

### SkillManager

| 方法 | 描述 |
|------|------|
| `Load(ctx, skillID)` | 加载技能 |
| `Get(ctx, skillID)` | 获取技能 |
| `Execute(ctx, skillID, parameters)` | 执行技能 |
| `ExecuteWithContext(ctx, skillID, parameters, context)` | 带上下文执行 |
| `Unload(ctx, skillID)` | 卸载技能 |
| `Register(ctx, name, description, parameters)` | 注册新技能 |
| `Update(ctx, skillID, description, parameters)` | 更新技能 |
| `Delete(ctx, skillID)` | 删除技能 |
| `GetInfo(ctx, skillID)` | 获取技能只读元信息 |
| `Validate(ctx, skillID)` | 验证技能 |
| `List(ctx, opts)` | 列出技能 |
| `ListLoaded(ctx)` | 列出已加载技能 |
| `Count(ctx)` | 技能计数 |
| `CountLoaded(ctx)` | 已加载技能计数 |
| `Search(ctx, query, topK)` | 搜索技能 |
| `BatchExecute(ctx, skillIDs, parameters)` | 批量执行 |
| `GetStats(ctx)` | 技能统计 |

## 错误处理

### 错误码体系

```go
// 错误码（Code 前缀常量）
agentos.CodeInvalidConfig     // 配置无效
agentos.CodeMissingParameter  // 缺少必要参数
agentos.CodeInvalidResponse   // 响应格式异常
agentos.CodeTaskTimeout       // 任务超时
agentos.CodeNetworkError      // 网络错误
agentos.CodeServerError       // 服务端错误

// 哨兵错误（Err 前缀变量，支持 errors.Is）
agentos.ErrInvalidConfig
agentos.ErrNotFound
agentos.ErrTimeout
agentos.ErrNetworkError
```

### 使用示例

```go
task, err := client.Tasks().Get(ctx, "nonexistent")
if err != nil {
    // 方式1：错误码判断
    if agentos.IsErrorCode(err, agentos.CodeNotFound) {
        fmt.Println("任务未找到")
    }
    // 方式2：哨兵错误判断
    if errors.Is(err, agentos.ErrNotFound) {
        fmt.Println("任务未找到")
    }
    // 方式3：分类判断
    if agentos.IsNetworkError(err) {
        // 处理网络错误
    }
    if agentos.IsServerError(err) {
        // 处理服务端错误
    }
}
```

### 错误链

```go
// SDK 错误支持 Unwrap()，可追踪完整错误链
var agentErr *agentos.AgentOSError
if errors.As(err, &agentErr) {
    fmt.Printf("Code: %s, Message: %s\n", agentErr.Code, agentErr.Message)
    if agentErr.Cause != nil {
        fmt.Printf("Caused by: %v\n", agentErr.Cause)
    }
}
```

## 类型定义

### 枚举类型

```go
// TaskStatus 任务状态（IsTerminal() 判断终态）
agentos.TaskStatusPending / Running / Completed / Failed / Cancelled

// MemoryLayer 记忆层级（IsValid() 校验合法性）
agentos.MemoryLayerL1 / L2 / L3 / L4

// SessionStatus 会话状态
agentos.SessionStatusActive / Inactive / Expired

// SkillStatus 技能状态
agentos.SkillStatusActive / Inactive / Error
```

### 请求选项

```go
// 分页
opts := &agentos.ListOptions{
    Pagination: &agentos.PaginationOptions{Page: 1, PageSize: 20},
    Sort:       &agentos.SortOptions{Field: "created_at", Order: "desc"},
    Filter:     &agentos.FilterOptions{Key: "status", Value: "active"},
}

// 单次请求选项
agentos.WithRequestTimeout(5 * time.Second)
agentos.WithHeader("X-Custom", "value")
agentos.WithQueryParam("include_deleted", "true")
```

## 测试

```bash
# 运行全部测试
go test ./agentos -v

# 带覆盖率
go test ./agentos -cover

# 生成覆盖率报告
go test ./agentos -coverprofile=coverage.out
go tool cover -func=coverage.out
```

## 最佳实践

1. **始终使用 Context**: 传递 `context.Context` 以支持超时和取消
2. **资源清理**: 使用 `defer client.Close()` 确保连接释放
3. **配置零值保护**: `WithEndpoint("")` 不会覆盖默认值，这是设计行为
4. **错误链追踪**: 使用 `errors.Is`/`errors.As` 处理嵌套错误
5. **批量操作优先**: 使用 `BatchSubmit`/`BatchExecute` 减少网络往返

## 版本历史

- **2.0.0** (2026-03-22): 系统性模块化改造
  - 引入 `APIClient` 接口实现依赖倒转
  - 统一错误体系（ErrorCode + 哨兵错误 + HTTP 状态映射）
  - 提取 `helpers.go` 消除 Manager 间代码重复
  - 函数式选项模式零值保护守卫
  - 170+ 单元测试，87.2% 覆盖率
- **1.0.0.6**: 初始模块化重构
