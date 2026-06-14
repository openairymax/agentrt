# Manager Schema — JSON Schema 定义

**模块路径**: `agentos/manager/schema/`
**版本**: v0.1.0

## 概述

`manager/schema/` 包含 AgentOS Manager 模块的全部 JSON Schema 定义，用于验证各模块配置文件的格式正确性和语义约束。共 11 个 Schema 文件，覆盖约 272 项校验规则，确保配置数据的完整性和一致性。

## 目录结构

```
schema/
├── _metadata.schema.json             # Schema 元数据定义
├── agent-registry.schema.json        # Agent 注册表 Schema
├── config-audit-log.schema.json      # 审计日志 Schema
├── config-management.schema.json     # 配置管理 Schema
├── kernel-settings.schema.json       # 内核配置 Schema
├── logging.schema.json               # 日志配置 Schema
├── model.schema.json                 # 模型配置 Schema
├── sanitizer-rules.schema.json       # 清洗规则 Schema
├── security-policy.schema.json       # 安全策略 Schema
├── skill-registry.schema.json        # 技能注册 Schema
└── tool-service.schema.json          # 工具服务 Schema
```

## 核心组件

### Schema 与配置映射

| Schema 文件 | 验证对象 | 配置文件 |
|-------------|---------|---------|
| `_metadata.schema.json` | Schema 文件自身的版本与依赖定义 | — |
| `agent-registry.schema.json` | Agent 注册表格式 | `agent/registry.yaml` |
| `config-audit-log.schema.json` | 配置变更审计日志规范 | `audit/sample_audit_log.json` |
| `config-management.schema.json` | Manager 自身管理配置 | `manager_management.yaml` |
| `kernel-settings.schema.json` | 内核级配置（内存/调度/并发限制） | `kernel/settings.yaml` |
| `logging.schema.json` | 日志配置（级别/格式/输出目标） | `logging/manager.yaml` |
| `model.schema.json` | 模型配置（Provider/参数/上下文窗口） | `model/model.yaml` |
| `sanitizer-rules.schema.json` | 输入清洗配置（XSS/SQL 注入规则） | `sanitizer/sanitizer_rules.json` |
| `security-policy.schema.json` | 安全配置（认证/授权/沙箱隔离） | `security/policy.yaml` |
| `skill-registry.schema.json` | 技能配置与管理 | `skill/registry.yaml` |
| `tool-service.schema.json` | tool_d 守护进程配置 | `service/tool_d/tool.yaml` |

### 校验层级

1. **语法校验**：验证 YAML/JSON 文件格式正确性、UTF-8 编码
2. **Schema 校验**：验证配置内容是否符合 JSON Schema 定义（约 272 项规则）
3. **语义校验**：跨模块依赖关系验证、值域约束检查

## 使用说明

```bash
# 通过 Python 校验配置
python -c "
import json, yaml
from jsonschema import validate
schema = json.load(open('schema/kernel-settings.schema.json'))
config = yaml.safe_load(open('kernel/settings.yaml'))
validate(instance=config, schema=schema)
"
```

## 依赖关系

| 组件 | 用途 |
|------|------|
| jsonschema | JSON Schema 校验库 |
| PyYAML | YAML 配置解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
