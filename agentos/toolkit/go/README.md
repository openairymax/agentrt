# Go SDK

`toolkit/go/` 是 AgentOS 的 Go 语言 SDK，提供完整的 AgentOS 客户端功能。

## 版本

当前版本: **v1.0.0.9**

## 安装

```bash
cd toolkit/go
go build ./...
```

## 结构

```
go/
├── agentos/                  # SDK 主包
│   ├── client/              # HTTP 客户端
│   ├── modules/             # 模块管理器
│   │   ├── task/           # 任务管理
│   │   ├── session/        # 会话管理
│   │   ├── memory/         # 内存管理
│   │   └── skill/          # 技能管理
│   ├── syscall/            # 系统调用接口
│   ├── telemetry/          # 遥测
│   ├── types/              # 类型定义
│   ├── utils/              # 工具函数
│   ├── agentos.go          # AgentOS 核心
│   ├── config.go           # 配置管理
│   ├── errors.go           # 错误定义
│   └── protocol.go         # 协议实现
├── README.md                # 本文件
├── go.mod                   # Go 模块定义
└── coverage                 # 测试覆盖率报告
```

## 使用示例

```go
package main

import (
    "fmt"
    "agentos/client"
)

func main() {
    // 创建客户端
    c := client.NewClient("http://localhost:8080")

    // 创建 Agent
    agent, err := c.CreateAgent("agent-001", "example-agent")
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        return
    }
    fmt.Printf("Agent created: %s\n", agent.ID)

    // 提交任务
    task, err := c.CreateTask(agent.ID, "example_task", nil)
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        return
    }
    fmt.Printf("Task created: %s\n", task.ID)
}
```

## 运行测试

```bash
cd toolkit/go
go test ./... -v
```

---

*AgentOS Toolkit — Go SDK*
