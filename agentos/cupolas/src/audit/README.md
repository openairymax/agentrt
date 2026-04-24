# Audit — 审计系统

`cupolas/src/audit/` 提供安全审计日志记录与事件追踪能力，确保所有安全相关操作的完整可追溯性。

## 设计目标

- **完整记录**：记录所有安全相关事件，不遗漏任何关键操作
- **防篡改**：使用 HMAC 签名链确保日志完整性
- **高效查询**：支持按时间、级别、类型等多维度检索审计日志
- **合规支持**：满足 SOC 2、ISO 27001 等审计合规要求

## 事件分类

| 分类 | 说明 | 示例 |
|------|------|------|
| **安全事件** | 安全相关的操作 | 登录失败、权限拒绝、注入检测 |
| **系统事件** | 系统层面的操作 | 服务启动/停止、配置变更 |
| **访问事件** | 资源访问操作 | 文件读取、API 调用 |
| **任务事件** | 任务执行操作 | 任务创建、任务完成、任务失败 |

## 日志结构

每条审计日志包含以下字段：

```json
{
    "id": "audit-20240115-001",
    "timestamp": "2024-01-15T10:30:00.123Z",
    "category": "security",
    "event": "login_failed",
    "severity": "warning",
    "subject": {
        "user_id": "user-001",
        "ip": "192.168.1.100",
        "session_id": "sess-abc123"
    },
    "action": {
        "type": "authenticate",
        "resource": "/api/v1/login",
        "detail": "密码错误（第3次尝试）"
    },
    "result": "deny",
    "context": {
        "geo_location": "CN-Beijing",
        "device": "Chrome/120.0"
    },
    "hmac": "a1b2c3d4e5f6..."  // 防篡改签名
}
```

## 使用示例

```c
#include "cupolas/src/audit/audit.h"

// 创建审计器
audit_t* audit = audit_create("security-audit.log");

// 配置审计策略
audit_config(audit, (audit_config_t){
    .hmac_key = "your-secret-key",
    .auto_flush = true,
    .flush_interval_ms = 1000,
    .max_queue_size = 10000
});

// 记录审计事件
audit_event_t event = {
    .category = AUDIT_CATEGORY_SECURITY,
    .event = "permission_denied",
    .severity = AUDIT_SEVERITY_WARNING,
    .subject = "user-001",
    .action = "api:write",
    .resource = "/api/v1/config",
    .detail = "角色 viewer 无权执行写操作"
};
audit_record(audit, &event);

// 查询审计日志
audit_query_t query = {
    .category = AUDIT_CATEGORY_SECURITY,
    .start_time = "2024-01-01T00:00:00Z",
    .end_time = "2024-01-15T23:59:59Z",
    .limit = 100
};

audit_result_t* results = audit_query(audit, &query);
for (int i = 0; i < results->count; i++) {
    printf("[%s] %s: %s", 
        results->events[i].timestamp,
        results->events[i].event,
        results->events[i].detail);
}

// 验证日志完整性
bool valid = audit_verify_integrity(audit);
if (!valid) {
    printf("警告：审计日志可能被篡改！");
}
```

## 防篡改机制

审计日志使用 HMAC-SHA256 签名链确保完整性。每条日志的 HMAC 包含前一条日志的 HMAC，形成签名链：

```
日志1: [data1, hmac1=HMAC(data1)]
日志2: [data2, hmac2=HMAC(data2 + hmac1)]
日志3: [data3, hmac3=HMAC(data3 + hmac2)]
...
```

任何中间日志被篡改都会导致后续所有日志的 HMAC 校验失败。

---

*AgentOS Cupolas — Audit*
