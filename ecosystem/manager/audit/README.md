# Manager Audit — 审计配置与示例

**模块路径**: `ecosystem/manager/audit/`
**版本**: v0.1.0

## 概述

`manager/audit/` 包含 AgentOS Manager 模块的审计日志配置与示例数据。审计日志记录所有配置变更操作，支持 7 种动作类型，包含操作者信息、变更详情和 SHA-256 校验和，实现配置变更的完整可追溯性。

## 目录结构

```
audit/
└── sample_audit_log.json   # 审计日志示例
```

## 核心组件

### sample_audit_log.json

审计日志示例文件，遵循 `schema/config-audit-log.schema.json` 规范，每条记录包含：

| 字段 | 说明 |
|------|------|
| `timestamp` | ISO 8601 时间戳 |
| `action` | 操作类型（LOAD/CHANGE/RELOAD/VALIDATE/ROLLBACK/EXPORT/IMPORT） |
| `config_file` | 配置文件路径 |
| `operator` | 操作者信息（type/identity/ip_address/session_id） |
| `changes` | 变更项列表（path/old_value/new_value/field_type） |
| `checksum` | 校验和信息（algorithm/before/after） |
| `metadata` | 元数据（environment/version/correlation_id/source/reason） |
| `result` | 执行结果（success/duration_ms/error_code/error_message） |

### 动作类型

| 动作 | 说明 |
|------|------|
| `LOAD` | 配置首次加载 |
| `CHANGE` | 配置项变更 |
| `RELOAD` | 配置热重载 |
| `VALIDATE` | 配置校验 |
| `ROLLBACK` | 配置回滚 |
| `EXPORT` | 配置导出 |
| `IMPORT` | 配置导入 |

### 操作者类型

| 类型 | 说明 |
|------|------|
| `user` | 用户操作 |
| `system` | 系统自动操作 |
| `ci_cd` | CI/CD 流水线操作 |

## 依赖关系

| 组件 | 用途 |
|------|------|
| `schema/config-audit-log.schema.json` | 审计日志格式校验 |
| `tools/audit_log_generator.py` | 审计日志生成工具 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
