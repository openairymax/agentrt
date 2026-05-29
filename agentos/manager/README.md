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

Manager 使用 JSON Schema 定义所有配置项的结构和约束。Schema 按领域模块组织在 `manager/` 目录下：

| 配置领域 | 目录 | 说明 |
|----------|------|------|
| **内核配置** | `kernel/` | 内核级配置（内存/调度/并发限制） |
| **模型配置** | `model/` | 模型配置（Provider/参数/上下文窗口） |
| **安全配置** | `security/` | 安全配置（认证/授权/加密） |
| **清洗器配置** | `sanitizer/` | 输入清洗配置（XSS/SQL 注入规则） |
| **日志配置** | `logging/` | 日志配置（级别/格式/输出目标） |
| **Agent 配置** | `agent/` | Agent 配置定义与管理 |
| **部署配置** | `deployment/` | 部署配置（cupolas/环境/集群/端口） |
| **环境配置** | `environment/` | 环境变量与运行时配置 |
| **监控配置** | `monitoring/` | 监控配置（告警规则/仪表盘） |
| **服务配置** | `service/` | 服务配置（tool_d 等守护进程） |
| **技能配置** | `skill/` | 技能配置与管理 |
| **审计配置** | `audit/` | 审计配置（事件分类/保留策略） |
| **Schema 定义** | `schema/` | 各模块 JSON Schema 统一定义 |

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
| 配置定义 | `agent/`, `kernel/`, `model/`, `security/`, `sanitizer/`, `logging/` | 各领域的配置 Schema 与默认值定义 |
| 配置管理 | `deployment/`, `environment/`, `monitoring/`, `service/`, `skill/`, `audit/` | 部署、环境、监控、服务与审计配置管理 |
| Schema 统一定义 | `schema/` | 跨模块共享的 JSON Schema 定义 |
| 测试套件 | `tests/` | Manager 模块的单元测试和集成测试 |
| 工具集 | `tools/` | 配置对比、版本清理等运维工具 |
| 性能基准 | `benchmark/` | Manager 模块性能基准测试 |

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
