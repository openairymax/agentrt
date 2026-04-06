# AgentOS 配置变更审计日志格式规范

**文档版本**: V1.0  
**创建日期**: 2026-04-05  
**适用范围**: AgentOS Manager模块  
**Schema版本**: config-audit-log.schema.json (Draft-07)  

---

## 目录

1. [概述](#1-概述)
2. [日志格式定义](#2-日志格式定义)
3. [字段详细说明](#3-字段详细说明)
4. [动作类型定义](#4-动作类型定义)
5. [使用示例](#5-使用示例)
6. [最佳实践](#6-最佳实践)
7. [工具与集成](#7-工具与集成)
8. [常见问题](#8-常见问题)

---

## 1. 概述

### 1.1 目的

配置变更审计日志用于记录AgentOS Manager模块中所有配置文件的变更操作，提供：

- **可追溯性**: 完整记录谁在什么时间做了什么变更
- **合规性**: 满足安全审计和合规要求
- **故障排查**: 快速定位配置问题根源
- **变更回滚**: 支持基于审计日志的配置回滚

### 1.2 设计原则

遵循 **E-2 可观测性原则** 和 **E-1 安全内生原则**：

- ✅ **完整性**: 记录所有关键信息，无遗漏
- ✅ **不可篡改**: 日志采用加密存储（AES-256-GCM）
- ✅ **结构化**: JSON格式，便于解析和查询
- ✅ **标准化**: 遵循JSON Schema Draft-07规范

### 1.3 相关文件

| 文件 | 路径 | 说明 |
|------|------|------|
| Schema定义 | `schema/config-audit-log.schema.json` | JSON Schema规范 |
| 示例日志 | `audit/sample_audit_log.json` | 完整示例 |
| 生成器工具 | `tools/audit_log_generator.py` | 日志生成工具 |
| 验证测试 | `tests/test_audit_log_validation.py` | 格式验证测试 |

---

## 2. 日志格式定义

### 2.1 整体结构

审计日志采用 **JSON数组** 格式，每个数组元素为一条独立的审计日志条目：

```json
[
  {
    "timestamp": "2026-04-05T10:30:00.123456Z",
    "action": "CHANGE",
    "config_file": "kernel/settings.yaml",
    "operator": { ... },
    "changes": [ ... ],
    "checksum": { ... },
    "metadata": { ... },
    "result": { ... }
  },
  ...
]
```

### 2.2 必需字段

每条审计日志 **必须** 包含以下字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `timestamp` | string | ISO 8601格式的时间戳 |
| `action` | string | 审计动作类型 |
| `config_file` | string | 配置文件相对路径 |
| `operator` | object | 操作者信息 |
| `checksum` | object | 文件校验和 |

### 2.3 可选字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `changes` | array | 变更详情列表 |
| `metadata` | object | 扩展元数据 |
| `result` | object | 操作结果 |

---

## 3. 字段详细说明

### 3.1 timestamp（时间戳）

**格式**: ISO 8601 (RFC 3339)  
**示例**: `"2026-04-05T10:30:00.123456Z"`

**规范**:
- 必须使用UTC时间（以`Z`结尾）
- 精确到微秒（6位小数）
- 格式: `YYYY-MM-DDTHH:MM:SS.ffffffZ`

**示例**:
```json
{
  "timestamp": "2026-04-05T10:30:00.123456Z"
}
```

---

### 3.2 action（动作类型）

**类型**: 枚举字符串  
**可选值**:

| 值 | 说明 | 触发场景 |
|---|------|---------|
| `LOAD` | 配置加载 | 系统启动、手动加载 |
| `RELOAD` | 热更新重载 | 文件监听器检测到变更 |
| `CHANGE` | 配置变更 | 用户/API修改配置 |
| `ROLLBACK` | 回滚 | 验证失败、手动回滚 |
| `VALIDATE` | Schema验证 | CI/CD、定期检查 |
| `EXPORT` | 导出 | 备份、迁移 |
| `IMPORT` | 导入 | 部署、恢复 |

**示例**:
```json
{
  "action": "CHANGE"
}
```

---

### 3.3 config_file（配置文件路径）

**格式**: 相对路径（相对于`manager/`目录）  
**模式**: 正则表达式 `^.*\.(ya?ml|json)$`

**示例**:
```json
{
  "config_file": "kernel/settings.yaml"
}
```

**常见配置文件**:
- `kernel/settings.yaml` - 内核配置
- `model/model.yaml` - 模型配置
- `security/policy.yaml` - 安全策略
- `agent/registry.yaml` - Agent注册表
- `environment/production.yaml` - 生产环境配置

---

### 3.4 operator（操作者信息）

**结构**:
```json
{
  "type": "user | system | ci_cd",
  "identity": "操作者标识",
  "ip_address": "IP地址（可选）",
  "session_id": "会话ID（可选）"
}
```

**字段说明**:

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `type` | string | ✅ | 操作者类型 |
| `identity` | string | ✅ | 操作者标识（用户名/组件名/CI Job名） |
| `ip_address` | string | ❌ | IP地址（仅`user`类型） |
| `session_id` | string | ❌ | 会话ID（UUID格式） |

**操作者类型**:

| 类型 | 说明 | identity示例 |
|------|------|-------------|
| `user` | 真实用户 | admin, devops_engineer |
| `system` | 系统组件 | config-manager, file-watcher |
| `ci_cd` | CI/CD流水线 | github-actions, jenkins |

**示例**:
```json
{
  "operator": {
    "type": "user",
    "identity": "admin",
    "ip_address": "192.168.1.100",
    "session_id": "550e8400-e29b-41d4-a716-446655440000"
  }
}
```

---

### 3.5 changes（变更详情）

**类型**: 数组  
**结构**:
```json
{
  "changes": [
    {
      "path": "YAML路径表达式",
      "old_value": "旧值",
      "new_value": "新值",
      "field_type": "字段类型（可选）"
    }
  ]
}
```

**字段说明**:

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `path` | string | ✅ | YAML/JSON路径（点号分隔） |
| `old_value` | any | ✅ | 变更前的值（`null`表示新增字段） |
| `new_value` | any | ✅ | 变更后的值（`null`表示删除字段） |
| `field_type` | string | ❌ | 字段类型提示 |

**路径表达式示例**:

| 路径 | 说明 |
|------|------|
| `kernel.log_level` | 顶层字段 |
| `model.primary.provider` | 嵌套字段 |
| `security.rules[0].action` | 数组元素 |
| `agents[0].max_concurrent_tasks` | 数组元素的属性 |

**示例**:
```json
{
  "changes": [
    {
      "path": "kernel.log_level",
      "old_value": "info",
      "new_value": "debug",
      "field_type": "string"
    },
    {
      "path": "security.audit.retention_days",
      "old_value": 30,
      "new_value": 90,
      "field_type": "integer"
    }
  ]
}
```

---

### 3.6 checksum（校验和）

**结构**:
```json
{
  "checksum": {
    "algorithm": "sha256",
    "before": "变更前的SHA-256哈希",
    "after": "变更后的SHA-256哈希"
  }
}
```

**字段说明**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `algorithm` | string | 固定为`sha256` |
| `before` | string | 变更前的文件SHA-256哈希（64位十六进制） |
| `after` | string | 变更后的文件SHA-256哈希（64位十六进制） |

**用途**:
- 验证文件完整性
- 快速判断文件是否变更
- 支持基于哈希的去重

**示例**:
```json
{
  "checksum": {
    "algorithm": "sha256",
    "before": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "after": "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"
  }
}
```

---

### 3.7 metadata（扩展元数据）

**结构**:
```json
{
  "metadata": {
    "environment": "production | staging | development",
    "version": "Manager模块版本号",
    "correlation_id": "关联ID（UUID）",
    "source": "变更来源",
    "reason": "变更原因",
    "approved_by": "审批人",
    "ticket_id": "关联工单ID"
  }
}
```

**字段说明**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `environment` | string | 运行环境 |
| `version` | string | Manager模块版本号（如`1.0.0.14`） |
| `correlation_id` | string | 关联ID（用于跨系统追踪） |
| `source` | string | 变更来源（manual/api/file_watcher等） |
| `reason` | string | 变更原因说明 |
| `approved_by` | string | 审批人（如适用） |
| `ticket_id` | string | 关联工单ID |

**示例**:
```json
{
  "metadata": {
    "environment": "production",
    "version": "1.0.0.14",
    "correlation_id": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
    "source": "manual",
    "reason": "调试生产环境问题",
    "approved_by": "tech_lead",
    "ticket_id": "OPS-2026-0425"
  }
}
```

---

### 3.8 result（操作结果）

**结构**:
```json
{
  "result": {
    "success": true | false,
    "error_code": "错误码（失败时）",
    "error_message": "错误消息（失败时）",
    "duration_ms": 操作耗时（毫秒）
  }
}
```

**字段说明**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | boolean | 操作是否成功 |
| `error_code` | string | 错误码（失败时） |
| `error_message` | string | 错误消息（失败时） |
| `duration_ms` | integer | 操作耗时（毫秒） |

**常见错误码**:

| 错误码 | 说明 |
|--------|------|
| `SCHEMA_VIOLATION` | Schema验证失败 |
| `PERMISSION_DENIED` | 权限不足 |
| `FILE_NOT_FOUND` | 文件不存在 |
| `PARSE_ERROR` | YAML/JSON解析错误 |
| `ENCRYPTION_ERROR` | 加密失败 |

**示例**:
```json
{
  "result": {
    "success": false,
    "error_code": "SCHEMA_VIOLATION",
    "error_message": "Field 'security.sandbox.invalid_field' is not allowed by schema",
    "duration_ms": 23
  }
}
```

---

## 4. 动作类型定义

### 4.1 LOAD（配置加载）

**触发场景**:
- 系统启动时自动加载配置
- 用户手动执行加载命令
- 服务重启后重新加载

**特点**:
- `changes`数组为空
- `checksum.before`为空字符串
- `checksum.after`包含加载后的文件哈希

**示例**:
```json
{
  "timestamp": "2026-04-05T10:00:00.123456Z",
  "action": "LOAD",
  "config_file": "kernel/settings.yaml",
  "operator": {
    "type": "system",
    "identity": "config-manager"
  },
  "changes": [],
  "checksum": {
    "algorithm": "sha256",
    "before": "",
    "after": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
  },
  "metadata": {
    "environment": "production",
    "version": "1.0.0.14",
    "source": "system_startup"
  },
  "result": {
    "success": true,
    "duration_ms": 23
  }
}
```

---

### 4.2 RELOAD（热更新重载）

**触发场景**:
- 文件监听器检测到配置文件变更
- 用户手动触发热更新
- API调用触发热更新

**特点**:
- `changes`数组包含变更详情
- `operator.type`通常为`system`
- `metadata.source`为`file_watcher`

**示例**:
```json
{
  "action": "RELOAD",
  "operator": {
    "type": "system",
    "identity": "file-watcher"
  },
  "metadata": {
    "source": "file_watcher"
  }
}
```

---

### 4.3 CHANGE（配置变更）

**触发场景**:
- 用户通过CLI/API修改配置
- 管理界面修改配置
- 自动化脚本修改配置

**特点**:
- `changes`数组包含详细变更
- `operator.type`通常为`user`
- `metadata`包含`reason`和`approved_by`

**示例**:
```json
{
  "action": "CHANGE",
  "operator": {
    "type": "user",
    "identity": "admin",
    "ip_address": "192.168.1.100"
  },
  "changes": [
    {
      "path": "kernel.log_level",
      "old_value": "info",
      "new_value": "debug"
    }
  ],
  "metadata": {
    "reason": "调试生产环境问题",
    "approved_by": "tech_lead",
    "ticket_id": "OPS-2026-0425"
  }
}
```

---

### 4.4 ROLLBACK（回滚）

**触发场景**:
- Schema验证失败自动回滚
- 用户手动执行回滚
- 系统异常自动回滚

**特点**:
- `changes`数组包含回滚的变更
- `metadata.source`为`validation_failure`或`manual`

**示例**:
```json
{
  "action": "ROLLBACK",
  "operator": {
    "type": "system",
    "identity": "auto-rollback"
  },
  "metadata": {
    "source": "validation_failure",
    "reason": "Schema验证失败，自动回滚"
  }
}
```

---

### 4.5 VALIDATE（Schema验证）

**触发场景**:
- CI/CD流水线中的pre-commit hook
- 定期的配置完整性检查
- 用户手动验证

**特点**:
- `changes`数组为空
- `checksum.before`和`after`相同
- `result`包含验证结果

**示例**:
```json
{
  "action": "VALIDATE",
  "operator": {
    "type": "ci_cd",
    "identity": "github-actions"
  },
  "changes": [],
  "checksum": {
    "algorithm": "sha256",
    "before": "c7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434c",
    "after": "c7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434c"
  },
  "result": {
    "success": true,
    "duration_ms": 156
  }
}
```

---

### 4.6 EXPORT（导出）

**触发场景**:
- 用户导出配置备份
- 迁移配置到其他环境
- 审计需求导出

**示例**:
```json
{
  "action": "EXPORT",
  "operator": {
    "type": "user",
    "identity": "devops_engineer"
  },
  "metadata": {
    "reason": "备份生产环境配置"
  }
}
```

---

### 4.7 IMPORT（导入）

**触发场景**:
- 部署新版本时导入配置
- 恢复备份配置
- 环境迁移

**示例**:
```json
{
  "action": "IMPORT",
  "operator": {
    "type": "ci_cd",
    "identity": "jenkins"
  },
  "changes": [
    {
      "path": "model.primary.provider",
      "old_value": "openai",
      "new_value": "anthropic"
    }
  ],
  "metadata": {
    "source": "deployment_pipeline",
    "ticket_id": "DEPLOY-2026-0456"
  }
}
```

---

## 5. 使用示例

### 5.1 生成测试日志

使用审计日志生成器工具：

```bash
# 生成10条随机审计日志
cd agentos/manager/tools
python audit_log_generator.py --count 10 --output sample_audit.json

# 生成特定动作的日志
python audit_log_generator.py --action CHANGE --file kernel/settings.yaml --reason "测试变更"

# 批量生成不同环境的日志
python audit_log_generator.py --count 20 --environment staging --output staging_audit.json
```

---

### 5.2 验证日志格式

运行验证测试：

```bash
# 运行所有验证测试
cd agentos/manager/tests
python test_audit_log_validation.py

# 使用pytest运行
pytest test_audit_log_validation.py -v
```

**预期输出**:
```
======================================================================
AgentOS Config Audit Log Validation Tests
======================================================================

✅ PASS: Schema Structure Validation
  Schema structure is valid and complete

✅ PASS: Schema Fields Validation
  All required fields are properly defined

✅ PASS: Sample Log File Existence
  Sample log file exists with 10 entries

...

======================================================================
Test Summary: 10 passed, 0 failed
======================================================================

✅ All tests passed!
```

---

### 5.3 查询审计日志

使用`jq`工具查询审计日志：

```bash
# 查询所有CHANGE动作
jq '.[] | select(.action == "CHANGE")' audit/sample_audit_log.json

# 查询特定用户的操作
jq '.[] | select(.operator.identity == "admin")' audit/sample_audit_log.json

# 查询失败的变更
jq '.[] | select(.result.success == false)' audit/sample_audit_log.json

# 统计各动作类型的数量
jq 'group_by(.action) | .[] | {action: .[0].action, count: length}' audit/sample_audit_log.json
```

---

## 6. 最佳实践

### 6.1 日志记录原则

1. **及时记录**: 变更发生后立即记录，不要延迟
2. **完整记录**: 包含所有必需字段和尽可能多的可选字段
3. **准确记录**: 确保时间戳、哈希值等信息准确无误
4. **安全存储**: 使用AES-256-GCM加密存储日志文件

### 6.2 字段填写建议

| 场景 | 建议填写的字段 |
|------|---------------|
| 用户手动变更 | `operator.ip_address`, `metadata.reason`, `metadata.approved_by` |
| 系统自动变更 | `operator.type=system`, `metadata.source` |
| CI/CD变更 | `operator.type=ci_cd`, `metadata.ticket_id` |
| 失败的变更 | `result.error_code`, `result.error_message` |

### 6.3 日志保留策略

根据`manager_management.yaml`中的配置：

```yaml
audit:
  enabled: true
  log_path: "audit/config_audit.log"
  events:
    - config.load
    - config.reload
    - config.change
    - config.rollback
    - config.validate
    - config.export
    - config.import
  retention_days: 90  # 保留90天
```

---

## 7. 工具与集成

### 7.1 相关工具

| 工具 | 路径 | 用途 |
|------|------|------|
| 审计日志生成器 | `tools/audit_log_generator.py` | 生成测试日志 |
| 审计日志验证器 | `tests/test_audit_log_validation.py` | 验证日志格式 |
| Schema验证器 | `tests/test_schema_validation.py` | 验证配置文件 |

### 7.2 CI/CD集成

在`.github/workflows/quality-gate.yml`中添加审计日志验证：

```yaml
- name: Validate audit logs
  run: |
    cd agentos/manager/tests
    python test_audit_log_validation.py
```

### 7.3 监控集成

审计日志可与监控系统（如Prometheus、Grafana）集成，实现：

- 变更频率监控
- 失败率告警
- 异常操作检测

---

## 8. 常见问题

### Q1: 审计日志存储在哪里？

**A**: 默认存储在`manager/audit/config_audit.log`，可在`manager_management.yaml`中配置。

### Q2: 审计日志是否加密？

**A**: 是的，敏感字段使用AES-256-GCM加密。加密配置在`manager_management.yaml`中：

```yaml
encryption:
  enabled: true
  algorithm: "aes-256-gcm"
  encrypted_fields:
    - "operator.ip_address"
    - "metadata.reason"
```

### Q3: 如何查询历史审计日志？

**A**: 使用`jq`工具或编写Python脚本查询。示例：

```python
import json

with open('audit/config_audit.log') as f:
    logs = json.load(f)
    
# 查询特定时间范围的日志
from datetime import datetime
start = datetime(2026, 4, 1)
end = datetime(2026, 4, 5)

filtered = [
    log for log in logs
    if start <= datetime.fromisoformat(log['timestamp'].replace('Z', '+00:00')) <= end
]
```

### Q4: 审计日志丢失怎么办？

**A**: 审计日志采用追加写入模式，即使系统崩溃也不会丢失。建议：

1. 定期备份审计日志
2. 配置日志同步到远程存储
3. 启用日志压缩和归档

### Q5: 如何自定义审计字段？

**A**: 在`metadata`字段中添加自定义字段：

```json
{
  "metadata": {
    "custom_field1": "value1",
    "custom_field2": "value2"
  }
}
```

---

## 附录

### A. Schema完整定义

参见: [config-audit-log.schema.json](../schema/config-audit-log.schema.json)

### B. 示例日志

参见: [sample_audit_log.json](../audit/sample_audit_log.json)

### C. 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| V1.0 | 2026-04-05 | 初始版本 |

---

**文档维护**: AgentOS Team  
**最后更新**: 2026-04-05  
**反馈渠道**: GitHub Issues
