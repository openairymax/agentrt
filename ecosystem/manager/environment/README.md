# Manager Environment — 多环境配置

**模块路径**: `ecosystem/manager/environment/`
**版本**: v0.1.0

## 概述

`manager/environment/` 包含 AgentRT Manager 模块的多环境配置定义，提供 development / staging / production 三套环境配置覆盖。环境配置在 Base 配置之上覆盖环境特定的参数，实现同一套基础配置在不同环境下的差异化运行。

## 目录结构

```
environment/
├── development.yaml    # 开发环境配置覆盖
├── staging.yaml        # 预发布环境配置覆盖
└── production.yaml     # 生产环境配置覆盖
```

## 核心组件

### 配置分层机制

环境配置遵循 Base / Environment / Runtime 三层覆盖模型：

| 层 | 优先级 | 说明 |
|----|--------|------|
| Base | 低 | 基础设施配置，所有环境共享（`kernel/`、`model/` 等） |
| Environment | 中 | 环境相关配置，按 development/staging/production 区分（本目录） |
| Runtime | 高 | 运行时动态配置，支持热重载 |

高优先级配置覆盖低优先级中的相同键。

### development.yaml

开发环境配置，特点：
- 日志级别：DEBUG / INFO
- 较宽松的安全限制
- 较低的资源限制
- 启用详细错误信息和堆栈跟踪

### staging.yaml

预发布环境配置，特点：
- 日志级别：INFO
- 接近生产环境的安全策略
- 中等资源限制
- 启用部分调试功能

### production.yaml

生产环境配置，特点：
- 日志级别：WARNING
- 严格的安全策略（deny-by-default、容器隔离）
- 高资源限制（4GB 内存池、500 并发）
- 密钥管理使用 Vault
- 启用审计日志加密（AES-256-GCM）
- 日志采样与异步写入

### 关键配置差异

| 配置项 | Development | Staging | Production |
|--------|-------------|---------|------------|
| `kernel.log_level` | debug | info | warning |
| `kernel.scheduler.max_concurrency` | 100 | 200 | 500 |
| `kernel.memory.pool_size_mb` | 1024 | 2048 | 4096 |
| `security.sandbox.isolation_type` | process | process | container |
| `security.secrets.provider` | env | env | vault |
| `security.session.timeout_sec` | 3600 | 1800 | 1800 |
| `logging.level` | debug | info | warning |
| `logging.format` | text | json | json |

## 依赖关系

| 组件 | 用途 |
|------|------|
| `kernel/` | 基础内核配置 |
| `security/` | 基础安全配置 |
| `logging/` | 基础日志配置 |
| PyYAML | YAML 配置解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
