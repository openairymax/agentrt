# 内核 API 参考

**版本**: 1.0.0  
**最后更新**: 2026-04-05  
**基础URL**: `http://localhost:8080`  

---

## 📌 概述

AgentOS内核提供RESTful API，供守护进程层和应用层调用。所有API遵循统一的错误处理和响应格式。

### 通用约定

**请求头**:

```http
Content-Type: application/json
Authorization: Bearer <token>  # 可选，用于认证
X-Request-ID: <uuid>          # 可选，用于请求追踪
```

**统一响应格式**:

```json
{
  "success": true,
  "data": { ... },
  "error": null,
  "request_id": "req_abc123",
  "timestamp": "2026-04-05T10:30:45.123Z"
}
```

**错误响应格式**:

```json
{
  "success": false,
  "data": null,
  "error": {
    "code": "AGENTOS_EINVAL",
    "message": "参数无效 (context: param=task_id, expected=uuid, received=invalid). Suggestion: 检查task_id格式是否为有效的UUID。",
    "details": { ... }
  },
  "request_id": "req_def456",
  "timestamp": "2026-04-05T10:30:46.789Z"
}
```

---

## 🔗 IPC API (进程间通信)

### 发送消息

发送消息到指定目标。

```http
POST /api/v1/ipc/send
```

**请求体**:

```json
{
  "target": "llm_d",
  "message_type": "task_request",
  "payload": {
    "task_id": "task_uuid",
    "description": "分析财报",
    "priority": "high"
  },
  "timeout_ms": 5000
}
```

**响应**:

```json
{
  "success": true,
  "data": {
    "message_id": "msg_abc123",
    "status": "sent",
    "timestamp": "2026-04-05T10:30:45Z"
  }
}
```

**参数说明**:

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `target` | string | ✅ | 目标服务名称（如 `llm_d`, `tool_d`） |
| `message_type` | string | ✅ | 消息类型标识符 |
| `payload` | object | ✅ | 消息载荷（JSON对象） |
| `timeout_ms` | integer | ❌ | 超时时间（毫秒），默认5000 |

**错误码**:

| 错误码 | HTTP状态码 | 说明 |
|--------|-----------|------|
| `AGENTOS_ETARGET` | 404 | 目标服务不存在 |
| `AGENTOS_ETIMEOUT` | 504 | 发送超时 |
| `AGENTOS_EPERM` | 403 | 无权限访问目标服务 |

---

### 接收消息

从消息队列中接收消息。

```http
GET /api/v1/ipc/recv?queue=my_queue&timeout=5000
```

**查询参数**:

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `queue` | string | ✅ | 队列名称 |
| `timeout` | integer | ❌ | 等待超时（毫秒），默认5000 |
| `batch_size` | integer | ❌ | 批量获取数量，默认1 |

**响应**:

```json
{
  "success": true,
  "data": {
    "messages": [
      {
        "message_id": "msg_abc123",
        "sender": "gateway_d",
        "message_type": "task_request",
        "payload": { ... },
        "timestamp": "2026-04-05T10:30:45Z"
      }
    ],
    "count": 1
  }
}
```

---

### 广播消息

向所有订阅者广播消息。

```http
POST /api/v1/ipc/broadcast
```

**请求体**:

```json
{
  "channel": "system_events",
  "message_type": "shutdown_notice",
  "payload": {
    "reason": "维护升级",
    "scheduled_at": "2026-04-06T02:00:00Z"
  }
}
```

---

## 💾 内存管理 API

### 分配内存

为任务分配内存块。

```http
POST /api/v1/memory/allocate
```

**请求体**:

```json
{
  "size_bytes": 1048576,  // 1MB
  "owner": "task_xyz789",
  "flags": ["zeroed", "aligned"]
}
```

**响应**:

```json
{
  "success": true,
  "data": {
    "memory_id": "mem_abc123",
    "address": "0x7f8a4b2c1000",
    "size_bytes": 1048576,
    "owner": "task_xyz789"
  }
}
```

**内存标志**:

| 标志 | 说明 |
|------|------|
| `zeroed` | 分配后清零 |
| `aligned` | 地址对齐（64字节） |
| `shared` | 共享内存（可跨进程） |
| `persistent` | 持久化到磁盘 |

---

### 释放内存

释放已分配的内存块。

```http
DELETE /api/v1/memory/{memory_id}
```

**响应**:

```json
{
  "success": true,
  "data": {
    "memory_id": "mem_abc123",
    "status": "released",
    "freed_bytes": 1048576
  }
}
```

---

### 查询内存使用情况

获取当前内存使用统计。

```http
GET /api/v1/memory/stats
```

**响应**:

```json
{
  "success": true,
  "data": {
    "total_allocated_mb": 512.3,
    "total_free_mb": 1536.7,
    "utilization_percent": 25.0,
    "fragmentation_percent": 5.2,
    "top_consumers": [
      {"owner": "task_xyz789", "usage_mb": 256.0},
      {"owner": "task_abc456", "usage_mb": 128.0}
    ]
  }
}
```

---

## ⏱️ 任务调度 API

### 创建任务

创建新任务并加入调度队列。

```http
POST /api/v1/tasks
```

**请求体**:

```json
{
  "description": "分析Q1财报并生成摘要报告",
  "priority": "normal",
  "tags": ["finance", "analysis"],
  "timeout_seconds": 300,
  "callback_url": "https://your-app.com/callback",
  "metadata": {
    "source": "openlab",
    "user_id": "user_123"
  }
}
```

**优先级列表**:

| 优先级 | 权重 | 说明 |
|--------|------|------|
| `critical` | 100 | 关键任务，立即执行 |
| `high` | 75 | 高优先级 |
| `normal` | 50 | 普通优先级（默认） |
| `low` | 25 | 低优先级，空闲时执行 |

**响应**:

```json
{
  "success": true,
  "data": {
    "task_id": "task_xyz789",
    "status": "queued",
    "created_at": "2026-04-05T10:30:45Z",
    "estimated_start": "2026-04-05T10:30:47Z"
  }
}
```

---

### 查询任务状态

```http
GET /api/v1/tasks/{task_id}
```

**响应**:

```json
{
  "success": true,
  "data": {
    "task_id": "task_xyz789",
    "status": "running",
    "progress_percent": 65,
    "current_step": "action_layer.tool_execution",
    "started_at": "2026-04-05T10:30:47Z",
    "estimated_completion": "2026-04-05T10:31:15Z",
    "result": null,
    "error": null
  }
}
```

**任务状态机**:

```
queued → running → completed
                → failed
                → cancelled
                → timeout
```

---

### 取消任务

```http
POST /api/v1/tasks/{task_id}/cancel
```

**响应**:

```json
{
  "success": true,
  "data": {
    "task_id": "task_xyz789",
    "status": "cancelling",
    "message": "取消请求已发送，正在清理资源"
  }
}
```

---

### 列出任务

```http
GET /api/v1/tasks?status=running&limit=20&offset=0
```

**查询参数**:

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `status` | string | ❌ | all | 过滤状态：queued/running/completed/failed/cancelled/timeout |
| `limit` | integer | ❌ | 20 | 返回数量限制 |
| `offset` | integer | ❌ | 0 | 偏移量（分页） |
| `sort_by` | string | ❌ | created_at | 排序字段 |
| `order` | string | ❌ | desc | 排序方向：asc/desc |

---

## 🕐 时间服务 API

### 获取当前时间

获取高精度当前时间。

```http
GET /api/v1/time/now
```

**响应**:

```json
{
  "success": true,
  "data": {
    "timestamp_ns": 1743843045123456789,
    "timestamp_iso": "2026-04-05T10:30:45.123456789Z",
    "timezone": "Asia/Shanghai",
    "offset_seconds": 28800,
    "leap_seconds": 37
  }
}
```

---

### 创建定时器

创建一次性或周期性定时器。

```http
POST /api/v1/timers
```

**请求体**:

```json
{
  "type": "periodic",
  "interval_ms": 60000,  // 60秒
  "callback": {
    "target": "monit_d",
    "message_type": "health_check",
    "payload": {}
  },
  "initial_delay_ms": 5000,  // 首次延迟5秒
  "max_tries": 0             // 0表示无限次
}
```

**定时器类型**:

| 类型 | 说明 |
|------|------|
| `oneshot` | 单次触发 |
| `periodic` | 周期性触发 |
| `cron` | Cron表达式调度 |

**响应**:

```json
{
  "success": true,
  "data": {
    "timer_id": "timer_abc123",
    "status": "active",
    "next_trigger_at": "2026-04-05T10:30:50Z",
    "trigger_count": 0
  }
}
```

---

### 取消定时器

```http
DELETE /api/v1/timers/{timer_id}
```

---

## 🏥 健康检查 API

### 系统健康状态

```http
GET /health
```

**响应**:

```json
{
  "status": "ok",
  "version": "1.0.0",
  "uptime_seconds": 86400,
  "components": {
    "ipc": {"status": "ok", "latency_ms": 0.3},
    "memory": {"status": "ok", "utilization_percent": 25.0},
    "task_scheduler": {"status": "ok", "queue_length": 42},
    "time_service": {"status": "ok", "drift_ns": 123},
    "security": {"status": "ok", "last_audit": "2026-04-05T10:00:00Z"}
  }
}
```

---

### 就绪探针（Kubernetes就绪检查）

```http
GET /ready
```

**响应**:

```json
{
  "ready": true,
  "checks": {
    "database_connected": true,
    "redis_connected": true,
    "ipc_operational": true,
    "memory_pool_ready": true
  }
}
```

---

## 📊 Prometheus 指标端点

```http
GET /metrics
```

暴露Prometheus格式的监控指标：

```
# HELP agentos_ipc_requests_total IPC请求总数
# TYPE agentos_ipc_requests_total counter
agentos_ipc_requests_total{method="send",target="llm_d"} 15234

# HELP agentos_ipc_request_duration_seconds IPC请求延迟
# TYPE agentos_ipc_request_duration_seconds histogram
agentos_ipc_request_duration_seconds_bucket{le="0.001"} 12000
agentos_ipc_request_duration_seconds_bucket{le="0.005"} 14800
agentos_ipc_request_duration_seconds_bucket{le="0.01"} 15000
agentos_ipc_request_duration_seconds_bucket{le="+Inf"} 15023
agentos_ipc_request_duration_seconds_sum 12.567
agentos_ipc_request_duration_seconds_count 15023

# HELP agentos_memory_records_total 记忆条目数
# TYPE agentos_memory_records_total gauge
agentos_memory_records_total{layer="L1"} 1024
agentos_memory_records_total{layer="L2"} 8192
agentos_memory_records_total{layer="L3"} 4096
agentos_memory_records_total{layer="L4"} 256

# HELP agentos_cupolas_permission_checks_total 权限检查次数
# TYPE agentos_cupolas_permission_checks_total counter
agentos_cupolas_permission_checks_total{result="allowed"} 98765
agentos_cupolas_permission_checks_total{result="denied"} 1234
```

**完整指标列表**: 请参考 [监控运维文档](../operations/monitoring.md)

---

## 🔐 认证与授权

### 获取访问令牌

```http
POST /api/v1/auth/token
```

**请求体**:

```json
{
  "client_id": "your_client_id",
  "client_secret": "your_client_secret",
  "grant_type": "client_credentials"
}
```

**响应**:

```json
{
  "access_token": "eyJhbGciOiJIUzI1NiIs...",
  "token_type": "Bearer",
  "expires_in": 3600,
  "scope": "read write"
}
```

---

### 验证令牌

```http
GET /api/v1/auth/verify
Authorization: Bearer eyJhbGciOiJIUzI1NiIs...
```

---

## 📝 SDK 使用示例

### Python SDK

```python
from agentos import AgentOSClient, TaskPriority

# 初始化客户端
client = AgentOSClient(
    base_url="http://localhost:8080",
    api_key="your_api_key"
)

# 创建任务
task = client.tasks.create(
    description="分析财报数据",
    priority=TaskPriority.HIGH,
    timeout_seconds=300
)

# 等待完成
result = task.wait_for_completion()
print(f"结果: {result.output}")
print(f"耗时: {result.duration_ms}ms")
```

### Go SDK

```go
package main

import (
    "context"
    "fmt"
    "log"
    "time"

    agentos "github.com/spharx/agentos/toolkit/go"
)

func main() {
    client := agentos.NewClient(
        agentos.WithBaseURL("http://localhost:8080"),
        agentos.WithAPIKey("your_api_key"),
    )

    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Minute)
    defer cancel()

    task, err := client.Tasks().Create(ctx, &agentos.TaskCreateRequest{
        Description: "分析财报数据",
        Priority:    agentos.PriorityHigh,
        TimeoutSecs: 300,
    })
    if err != nil {
        log.Fatal(err)
    }

    result, err := task.WaitForCompletion(ctx)
    if err != nil {
        log.Fatal(err)
    }

    fmt.Printf("结果: %s\n", result.Output)
    fmt.Printf("耗时: %dms\n", result.DurationMs)
}
```

---

## 📚 相关文档

- [**Python SDK API**](python-sdk.md) — Python完整API文档
- [**Go SDK API**](go-sdk.md) — Go完整API文档
- [**错误码手册**](error-codes.md) — 完整错误码定义
- [**认证机制详解**](../../agentos/manuals/guides/authentication.md) — OAuth2/JWT实现细节

---

> *"好的API像一本好书——清晰、一致、易于理解。"*

**© 2026 SPHARX Ltd. All Rights Reserved.**
