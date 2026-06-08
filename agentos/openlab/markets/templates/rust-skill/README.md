# Rust Skill 模板

**模块路径**: `agentos/openlab/markets/templates/rust-skill/`
**版本**: v0.0.5

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Rust Skill 模板是 AgentOS 生态市场中提供的标准化 Skill 开发模板，帮助开发者使用 Rust 语言创建高性能的系统级技能。该模板利用 Rust 的内存安全特性和零成本抽象，适用于对性能和安全要求较高的场景，如数据处理、加密运算、系统调用和实时计算。

## 目录结构

```
rust-skill/
├── Cargo.toml                  # Rust 项目配置
├── src/
│   ├── lib.rs                  # 技能库入口
│   ├── skill.rs                # 技能核心实现（Skill trait）
│   ├── config.rs               # 配置管理
│   └── types.rs                # 类型定义
├── tests/
│   └── integration.rs          # 集成测试
├── examples/
│   └── basic_usage.rs          # 使用示例
└── README.md                   # 本文件
```

## 核心组件

### Skill Trait (`src/skill.rs`)

| 方法 | 说明 |
|------|------|
| `initialize(&mut self)` | 技能初始化，返回 `SkillResult<()>` |
| `execute(&self, input: &str)` | 技能执行，返回 `SkillResult<String>` |
| `cleanup(&mut self)` | 资源清理，返回 `SkillResult<()>` |
| `version(&self) -> &str` | 返回技能版本号 |

### MySkillConfig (`src/config.rs`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | `String` | 技能名称 |
| `max_retries` | `u32` | 最大重试次数 |
| `timeout_ms` | `u64` | 超时时间（毫秒） |

### Cargo.toml 配置

| 配置项 | 说明 | 值 |
|--------|------|-----|
| `crate-type` | 库类型 | `["cdylib", "lib"]` |
| `profile.release.opt-level` | 优化级别 | 3 |
| `profile.release.lto` | 链接时优化 | `true` |
| `profile.release.codegen-units` | 代码生成单元 | 1 |

## 接口说明

### Skill 生命周期

```rust
#[async_trait]
impl Skill for MySkill {
    async fn initialize(&mut self) -> SkillResult<()>
    async fn execute(&self, input: &str) -> SkillResult<String>
    async fn cleanup(&mut self) -> SkillResult<()>
    fn version(&self) -> &str
}
```

### 依赖配置

```toml
[dependencies]
agentos-sdk = { path = "../../agentos/toolkit/rust" }
tokio = { version = "1", features = ["full"] }
serde = { version = "1", features = ["derive"] }
serde_json = "1"
anyhow = "1"
thiserror = "1"
tracing = "0.1"
```

## 依赖关系

- **核心依赖**: agentos-sdk (AgentOS Rust SDK), tokio, serde, anyhow, thiserror, tracing
- **Rust Edition**: 2021

## 构建说明

```bash
# 创建新 Skill 项目
market install rust-skill my-rust-skill
cd my-rust-skill

# 构建
cargo build --release

# 运行测试
cargo test

# 运行示例
cargo run --example basic_usage
```

## 使用示例

```rust
use agentos_sdk::{Skill, SkillContext, SkillResult};
use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize)]
pub struct MySkillConfig {
    pub name: String,
    pub max_retries: u32,
    pub timeout_ms: u64,
}

pub struct MySkill {
    config: MySkillConfig,
    context: SkillContext,
}

#[async_trait]
impl Skill for MySkill {
    async fn initialize(&mut self) -> SkillResult<()> {
        println!("Skill '{}' 已初始化", self.config.name);
        Ok(())
    }

    async fn execute(&self, input: &str) -> SkillResult<String> {
        Ok(format!("Processed: {}", input))
    }

    async fn cleanup(&mut self) -> SkillResult<()> {
        println!("Skill '{}' 已清理", self.config.name);
        Ok(())
    }

    fn version(&self) -> &str {
        "1.0.0"
    }
}
```

## 性能特性

Rust Skill 适用于以下高性能场景：

- **数据处理** — 大规模数据流处理和转换
- **加密运算** — 高性能加密/解密操作
- **系统调用** — 底层系统资源管理和调度
- **实时计算** — 低延迟计算任务
- **嵌入式环境** — 资源受限环境下的技能运行

## 内存安全

利用 Rust 的所有权系统和生命周期机制，在编译期保证：

- 无空指针解引用
- 无数据竞争
- 无缓冲区溢出
- 无使用后释放
- 无二次释放

## 交叉编译

```bash
# 为 ARM64 平台编译
cargo build --release --target aarch64-unknown-linux-gnu

# 为 x86_64 平台编译
cargo build --release --target x86_64-unknown-linux-gnu

# 构建与 AgentOS 集成的动态库
cargo build --release --lib
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
