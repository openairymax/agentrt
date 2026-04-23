# Manager — 统一配置与项目管理中心

`agentos/manager/` 是 AgentOS 的统一配置管理中心，负责管理全系统的配置定义、校验、分发和审计。

## 设计目标

- **配置中心化**：所有模块的配置集中在 Manager 管理，提供唯一真相源
- **Schema 驱动**：基于 JSON Schema 的配置定义和校验
- **热重载**：配置变更实时生效，无需重启服务
- **多环境支持**：支持 base / environment / runtime 三层配置覆盖

## 配置架构

```
+-------------------------------------------------------------------+
|                      配置来源（文件 / 环境变量 / API）               |
+-------------------------------------------------------------------+
|                        Manager 配置引擎                             |
|  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              |
|  │  Base 配置   │  │  环境配置    │  │ 运行时配置   │               |
|  │  (基础设施)   │  │ (环境差异)   │  │ (动态调整)   │               |
|  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘              |
|         └────────────────┼────────────────┘                     |
|                          ▼                                       |
|  ┌─────────────────────────────────────────────────────────┐    |
|  │                  合并配置（运行时生效）                    │    |
|  └─────────────────────────────────────────────────────────┘    |
|                          │                                      |
|  ┌─────────────────────────────────────────────────────────┐    |
|  │                  JSON Schema 校验引擎                    │    |
|  └─────────────────────────────────────────────────────────┘    |
+-------------------------------------------------------------------+
|              语义验证 → 审计日志 → 配置分发                       |
+-------------------------------------------------------------------+
```

## 配置分层

| 层 | 优先级 | 说明 |
|----|--------|------|
| **Base** | 低 | 基础设施配置，如日志路径、数据目录 |
| **Environment** | 中 | 环境相关配置，如开发/测试/生产差异 |
| **Runtime** | 高 | 运行时动态配置，支持热重载 |

高优先级配置覆盖低优先级中的相同键。

## Schema 定义

Manager 使用 JSON Schema 定义所有配置项的结构和约束。Schema 文件位于 `manager/schemas/` 目录：

| Schema | 说明 | 校验项数 |
|--------|------|----------|
| `kernel.schema.json` | 内核配置 | 32 |
| `model.schema.json` | 模型配置 | 28 |
| `security.schema.json` | 安全配置 | 35 |
| `sanitizer.schema.json` | 清洗器配置 | 24 |
| `logging.schema.json` | 日志配置 | 30 |
| `management.schema.json` | 管理配置 | 26 |
| `deployment.schema.json` | 部署配置 | 30 |
| `tls.schema.json` | TLS 配置 | 22 |
| `auth.schema.json` | 认证配置 | 25 |

## 配置示例

```json
{
    "kernel": {
        "log_level": "info",
        "max_tasks": 1000,
        "memory_limit": "4GB",
        "cpu_affinity": [0, 1, 2, 3]
    },
    "model": {
        "provider": "openai",
        "model": "gpt-4",
        "temperature": 0.7,
        "max_tokens": 4096
    },
    "security": {
        "auth_enabled": true,
        "rate_limit": 1000,
        "audit_enabled": true
    }
}
```

## 使用方式

```bash
# 从 JSON Schema 生成默认配置
python manager/scripts/generate_default_config.py

# 配置校验
python manager/scripts/validate_config.py --config config.json

# 查看当前配置
python manager/scripts/show_config.py

# 应用配置变更
python manager/scripts/apply_config.py --env production
```

## 子模块

| 子模块 | 路径 | 说明 |
|--------|------|------|
| 测试套件 | `tests/` | Manager 模块的单元测试和集成测试 |
| 工具集 | `tools/` | 配置对比、版本清理等运维工具 |
| 性能基准 | `benchmark/` | Manager 模块性能基准测试 |
| Schema 定义 | `schemas/` | 各模块的 JSON Schema 定义 |

## 热重载机制

配置热重载通过文件监控实现：

```bash
# 启用热重载
python manager/scripts/watch_config.py --interval 30

# 手动触发重载
python manager/scripts/apply_config.py --force
```

每次配置变更均记录审计日志，包括变更时间、操作人和变更内容。

---

*AgentOS Manager*
