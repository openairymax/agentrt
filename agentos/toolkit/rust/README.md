# Rust SDK

`toolkit/rust/` 是 AgentOS 的 Rust 语言 SDK，提供完整的 AgentOS 客户端功能。

## 版本

当前版本: **v1.0.0.9**

## 安装

```bash
cd toolkit/rust
cargo build
```

## 结构

```
rust/
├── src/                      # 源代码
│   ├── client/              # HTTP 客户端
│   ├── modules/             # 模块管理器
│   │   ├── task/           # 任务管理
│   │   ├── session/        # 会话管理
│   │   ├── memory/         # 内存管理
│   │   └── skill/          # 技能管理
│   ├── types/              # 类型定义
│   ├── utils/              # 工具函数
│   ├── agent.rs            # Agent 核心
│   ├── error.rs            # 错误定义
│   ├── protocol.rs         # 协议实现
│   └── lib.rs              # 库入口
├── tests/                    # 测试用例
├── Cargo.toml                # 项目配置
└── README.md                 # 本文件
```

## 使用示例

```rust
use agentos::client::Client;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 创建客户端
    let client = Client::new("http://localhost:8080");

    // 创建 Agent
    let agent = client.create_agent("agent-001", "example-agent").await?;
    println!("Agent created: {}", agent.id);

    // 提交任务
    let task = client.create_task(&agent.id, "example_task", None).await?;
    println!("Task created: {}", task.id);

    // 查询任务状态
    let status = client.get_task_status(&task.id).await?;
    println!("Task status: {:?}", status);

    Ok(())
}
```

## 运行测试

```bash
cd toolkit/rust
cargo test
```

---

*AgentOS Toolkit — Rust SDK*
