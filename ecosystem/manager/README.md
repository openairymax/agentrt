# Manager — 统一配置与项目管理中心

**模块路径**: `ecosystem/manager/`
**版本**: v0.1.0

## 概述

Manager 是 AgentRT 的统一配置管理中心，负责管理全系统的配置定义、校验、分发和审计。模块基于 **JSON Schema 驱动** 的配置定义和校验机制，拥有 11 个 JSON Schema 文件、约 272 项校验规则，支持 **Base / Environment / Runtime 三层配置覆盖** 和 **热重载**，确保配置变更实时生效而无需重启服务。每次配置变更均记录审计日志，实现完整的配置变更可追溯性。

## 设计目标

- **配置中心化**：所有模块的配置集中在 Manager 管理，提供唯一真相源（Single Source of Truth）
- **Schema 驱动**：基于 JSON Schema 的配置定义和校验，11 个 Schema 文件覆盖约 272 项校验规则
- **热重载**：配置变更实时生效，无需重启服务
- **多环境支持**：支持 Base / Environment / Runtime 三层配置覆盖
- **审计追溯**：配置变更全量审计日志，包含操作人、变更内容、校验和
- **漂移检测**：自动检测配置是否偏离基线版本，支持多级严重程度

## 目录结构

```
manager/
├── schema/                           # JSON Schema 统一定义
│   ├── _metadata.schema.json         # Schema 元数据定义
│   ├── config-audit-log.schema.json  # 审计日志 Schema
│   ├── kernel-settings.schema.json   # 内核配置 Schema
│   ├── model.schema.json             # 模型配置 Schema
│   ├── security-policy.schema.json   # 安全策略 Schema
│   ├── sanitizer-rules.schema.json   # 清洗规则 Schema
│   ├── logging.schema.json           # 日志配置 Schema
│   ├── config-management.schema.json # 配置管理 Schema
│   ├── tool-service.schema.json      # 工具服务 Schema
│   ├── agent-registry.schema.json    # Agent 注册 Schema
│   └── skill-registry.schema.json    # 技能注册 Schema
├── kernel/                           # 内核配置
│   ├── kernel.yaml                   # 内核基础配置
│   └── settings.yaml                 # 内核运行设置
├── model/                            # 模型配置
│   ├── model.yaml                    # 模型定义
│   └── model.json                    # 模型参数
├── security/                         # 安全配置
│   ├── policy.yaml                   # 安全策略
│   └── permission_rules.yaml         # 权限规则
├── sanitizer/                        # 清洗器配置
│   └── sanitizer_rules.json          # 输入清洗规则（XSS/SQL 注入）
├── logging/                          # 日志配置
│   └── manager.yaml                  # 日志管理配置
├── agent/                            # Agent 配置
│   └── registry.yaml                 # Agent 注册表
├── skill/                            # 技能配置
│   └── registry.yaml                 # 技能注册表
├── service/                          # 服务配置
│   └── tool_d/tool.yaml              # tool_d 守护进程配置
├── deployment/                       # 部署配置
│   └── cupolas/environments.yaml     # Cupolas 环境配置
├── environment/                      # 环境配置
│   ├── development.yaml              # 开发环境
│   ├── staging.yaml                  # 预发布环境
│   └── production.yaml               # 生产环境
├── monitoring/                       # 监控配置
│   ├── alerts/cupolas-alerts.yml     # Cupolas 告警规则
│   └── dashboards/cupolas-dashboard.json  # Cupolas 仪表盘
├── audit/                            # 审计配置
│   └── sample_audit_log.json         # 审计日志样例
├── tests/                            # 测试套件（计划中）
│   └── README.md                     # 测试文档
├── tools/                            # 运维工具集
│   ├── config_diff.py                # 配置差异对比
│   ├── config_version_cleanup.py     # 版本历史清理
│   ├── drift_detector.py             # 配置漂移检测
│   ├── audit_log_generator.py        # 审计日志生成器
│   ├── base/                         # 工具基础库
│   │   ├── __init__.py               # 包初始化
│   │   └── utils.py                  # 公共工具函数
│   └── README.md                     # 工具文档
├── benchmark/                        # 性能基准测试
│   ├── benchmark_manager.py          # 基准测试脚本
│   └── README.md                     # 基准文档
├── manager_management.yaml           # Manager 自身管理配置
├── otel-collector-manager.yaml       # OpenTelemetry Collector 配置
├── example.yaml                      # 配置示例文件
├── .gitignore                        # Git 忽略规则
└── README.md                         # 本文件
```

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
|  │                  JSON Schema 校验引擎                    │  │            11 个 Schema · 约 272 项校验规则                  │    │    |
|  └─────────────────────────────────────────────────────────┘    |
+-------------------------------------------------------------------+
|              语义验证 → 审计日志 → 配置分发                       |
+-------------------------------------------------------------------+
```

## 配置分层

| 层 | 优先级 | 说明 | 示例 |
|----|--------|------|------|
| **Base** | 低 | 基础设施配置，所有环境共享 | 日志路径、数据目录、默认端口 |
| **Environment** | 中 | 环境相关配置，按 development/staging/production 区分 | 日志级别、Redis 地址、会话超时 |
| **Runtime** | 高 | 运行时动态配置，支持热重载 | 速率限制、功能开关、模型参数 |

高优先级配置覆盖低优先级中的相同键。

## Schema 定义

Manager 使用 JSON Schema 定义所有配置项的结构和约束。Schema 按领域模块组织：

| 配置领域 | Schema 文件 | 配置文件 | 说明 |
|----------|------------|---------|------|
| **Schema 元数据** | `_metadata.schema.json` | — | Schema 文件自身的版本与依赖定义 |
| **审计日志** | `config-audit-log.schema.json` | `audit/sample_audit_log.json` | 配置变更审计日志规范 |
| **内核配置** | `kernel-settings.schema.json` | `kernel/settings.yaml` | 内核级配置（内存/调度/并发限制） |
| **模型配置** | `model.schema.json` | `model/model.yaml` | 模型配置（Provider/参数/上下文窗口） |
| **安全配置** | `security-policy.schema.json` | `security/policy.yaml` | 安全配置（认证/授权/沙箱隔离） |
| **清洗器配置** | `sanitizer-rules.schema.json` | `sanitizer/sanitizer_rules.json` | 输入清洗配置（XSS/SQL 注入规则） |
| **日志配置** | `logging.schema.json` | `logging/manager.yaml` | 日志配置（级别/格式/输出目标） |
| **Agent 配置** | `agent-registry.schema.json` | `agent/registry.yaml` | Agent 配置定义与管理 |
| **技能配置** | `skill-registry.schema.json` | `skill/registry.yaml` | 技能配置与管理 |
| **工具服务** | `tool-service.schema.json` | `service/tool_d/tool.yaml` | tool_d 守护进程配置 |
| **配置管理** | `config-management.schema.json` | `manager_management.yaml` | Manager 自身管理配置 |

## 核心功能

### 配置校验

基于 JSON Schema 的多级校验：

1. **语法校验**：验证 YAML/JSON 文件格式正确性、UTF-8 编码
2. **Schema 校验**：验证配置内容是否符合 JSON Schema 定义（约 272 项规则）
3. **语义校验**：跨模块依赖关系验证、值域约束检查

### 热重载机制

配置热重载通过文件监控实现：

```bash
# 启用热重载（30 秒轮询）— 计划中
python manager/scripts/watch_config.py --interval 30

# 手动触发重载 — 计划中
python manager/scripts/apply_config.py --force
```

每次配置变更均记录审计日志，包括变更时间、操作人和变更内容。

### 配置漂移检测

通过 `drift_detector.py` 检测配置是否偏离基线版本：

- 基于 SHA-256 哈希的文件完整性校验
- 三级严重程度：INFO / WARNING / CRITICAL
- 敏感文件（security/、kernel/、model/）变更标记为 CRITICAL
- 支持 JSON 和 Markdown 格式报告导出

### 审计日志

配置变更全量审计，符合 `config-audit-log.schema.json` 规范：

- 7 种动作类型：LOAD / RELOAD / CHANGE / ROLLBACK / VALIDATE / EXPORT / IMPORT
- 3 种操作者类型：user / system / ci_cd
- 包含变更前后的 SHA-256 校验和
- 支持关联 ID（correlation_id）和工单号（ticket_id）

## 使用方式

```bash
# 配置校验 — 计划中
python manager/scripts/validate_config.py --config config.json

# 查看当前配置 — 计划中
python manager/scripts/show_config.py

# 应用配置变更 — 计划中
python manager/scripts/apply_config.py --env production

# 配置漂移检测
python manager/tools/drift_detector.py --action both --output drift_report.json

# 配置差异对比
python manager/tools/src/config_diff.py config_v1.json config_v2.json

# 版本历史清理
python manager/tools/config_version_cleanup.py --keep 10

# 审计日志生成
python manager/tools/src/audit_log_generator.py --count 10 --output audit.json

# 运行所有测试 — 计划中
python manager/tests/run_all_tests.py --verbose

# 性能基准测试
python manager/benchmark/benchmark_manager.py --iterations 1000
```

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
        "audit_enabled": true,
        "sandbox": {
            "default_isolation": "process"
        }
    },
    "logging": {
        "level": "info",
        "format": "json",
        "output": "file"
    }
}
```

## 子模块

| 子模块 | 路径 | 说明 |
|--------|------|------|
| Schema 定义 | `schema/` | 11 个 JSON Schema 文件，约 272 项校验规则 |
| 内核配置 | `kernel/` | 内核基础配置与运行设置 |
| 模型配置 | `model/` | 模型定义与参数配置 |
| 安全配置 | `security/` | 安全策略与权限规则 |
| 清洗器配置 | `sanitizer/` | 输入清洗规则（XSS/SQL 注入） |
| 日志配置 | `logging/` | 日志管理配置 |
| Agent 配置 | `agent/` | Agent 注册表 |
| 技能配置 | `skill/` | 技能注册表 |
| 服务配置 | `service/` | tool_d 等守护进程配置 |
| 部署配置 | `deployment/` | Cupolas 环境配置 |
| 环境配置 | `environment/` | development/staging/production 三环境 |
| 监控配置 | `monitoring/` | 告警规则与仪表盘 |
| 审计配置 | `audit/` | 审计日志样例 |
| 测试套件 | `tests/` | 测试文档（测试脚本计划中） |
| 工具集 | `tools/` | 4 个运维工具 + 基础库 |
| 性能基准 | `benchmark/` | Manager 模块性能基准测试 |

## 依赖关系

| 组件 | 用途 |
|------|------|
| Python ≥ 3.10 | 脚本运行环境 |
| jsonschema | JSON Schema 校验 |
| pytest | 测试框架 |
| PyYAML | YAML 配置解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
