# Manager Audit

AgentOS Manager 模块的审计配置与示例。

## 文件

| 文件 | 说明 |
|------|------|
| `sample_audit_log.json` | 审计日志示例（7 种动作类型：LOAD/CHANGE/RELOAD/VALIDATE/ROLLBACK/EXPORT/IMPORT） |

## 审计日志格式

审计日志遵循 `schema/config-audit-log.schema.json` 规范，每条记录包含：

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

## 版本

v0.1.0
