# Audit — 审计系统

**模块路径**: `agentos/cupolas/src/audit/`
**版本**: v0.1.0

## 概述

Audit 模块提供安全审计日志记录与事件追踪能力，确保所有安全相关操作的完整可追溯性。采用异步写入架构，后台线程批量写入，不阻塞业务线程。审计日志使用 HMAC 签名链确保防篡改性，支持日志轮转、溢出处理和多维度查询。

## 设计目标

- **完整记录**：记录所有安全相关事件，不遗漏任何关键操作
- **防篡改**：使用 HMAC 签名链确保日志完整性
- **异步写入**：后台线程批量写入，不阻塞业务线程
- **高效查询**：支持按时间、级别、类型等多维度检索审计日志
- **合规支持**：满足 SOC 2、ISO 27001 等审计合规要求
- **溢出保护**：队列满时的降级策略，防止审计日志丢失

## 目录结构

```
audit/
├── audit.h                      # 审计系统公共接口
├── audit_logger.c               # 审计日志器实现
├── audit_queue.h                # 线程安全生产者-消费者队列接口
├── audit_queue.c                # 队列实现
├── audit_rotator.h              # 日志轮转接口
├── audit_rotator.c              # 轮转实现
├── audit_overflow.h             # 溢出处理接口
├── audit_overflow.c             # 溢出处理实现
└── README.md                    # 本文档
```

## 事件分类

| 枚举值 | 分类 | 说明 |
|--------|------|------|
| `AUDIT_EVENT_PERMISSION` | 权限事件 | 权限检查（允许/拒绝） |
| `AUDIT_EVENT_SANITIZER` | 清洗事件 | 输入清洗（通过/拒绝） |
| `AUDIT_EVENT_WORKBENCH` | 工作台事件 | 命令执行（退出码） |
| `AUDIT_EVENT_SYSTEM` | 系统事件 | 服务启停、配置变更 |
| `AUDIT_EVENT_CUSTOM` | 自定义事件 | 用户定义的安全事件 |

## 核心数据结构

### audit_entry_t — 审计日志条目

| 字段 | 类型 | 说明 |
|------|------|------|
| `timestamp_ms` | `uint64_t` | 事件时间戳（毫秒） |
| `type` | `audit_event_type_t` | 事件类型 |
| `agent_id` | `char *` | Agent 标识 |
| `action` | `char *` | 执行的操作 |
| `resource` | `char *` | 访问的资源 |
| `detail` | `char *` | 附加详情 |
| `result` | `int` | 结果（1=成功，0=失败） |
| `next` | `struct audit_entry *` | 链表下一项 |

### audit_queue_t — 线程安全队列

| 字段 | 类型 | 说明 |
|------|------|------|
| `head / tail` | `audit_entry_t *` | 队列头尾指针 |
| `size / max_size` | `size_t` | 当前/最大容量 |
| `lock` | `cupolas_mutex_t` | 互斥锁 |
| `not_empty / not_full` | `cupolas_cond_t` | 条件变量 |
| `total_pushed / total_popped` | `cupolas_atomic64_t` | 原子计数器 |

## 接口说明

### 审计日志器 API

| 函数 | 说明 |
|------|------|
| `audit_logger_create(log_dir, prefix, max_size, max_files)` | 创建审计日志器 |
| `audit_logger_destroy(logger)` | 销毁审计日志器（刷新待写日志） |
| `audit_logger_log(logger, type, agent_id, action, resource, detail, result)` | 记录审计事件 |
| `audit_logger_log_permission(logger, agent_id, action, resource, allowed)` | 记录权限事件 |
| `audit_logger_log_sanitizer(logger, agent_id, input, output, passed)` | 记录清洗事件 |
| `audit_logger_log_workbench(logger, agent_id, command, exit_code)` | 记录工作台事件 |
| `audit_logger_flush(logger)` | 刷新日志缓冲 |
| `audit_logger_stats(logger, total_logged, total_failed)` | 获取日志统计 |

### 审计队列 API

| 函数 | 说明 |
|------|------|
| `audit_queue_create(max_size)` | 创建队列（0=无限制） |
| `audit_queue_destroy(queue)` | 销毁队列 |
| `audit_queue_push(queue, entry)` | 阻塞式入队 |
| `audit_queue_try_push(queue, entry)` | 非阻塞式入队 |
| `audit_queue_pop(queue, entry)` | 阻塞式出队 |
| `audit_queue_timed_pop(queue, entry, timeout_ms)` | 超时出队 |
| `audit_queue_try_pop(queue, entry)` | 非阻塞式出队 |
| `audit_queue_pop_batch(queue, entries, max, actual)` | 批量出队 |
| `audit_queue_shutdown(queue, wait_empty)` | 关闭队列 |
| `audit_queue_size(queue)` | 获取队列大小 |
| `audit_entry_create(type, agent_id, action, resource, detail, result)` | 创建审计条目 |
| `audit_entry_destroy(entry)` | 销毁审计条目 |

## 使用示例

```c
#include "audit.h"

audit_logger_t *logger = audit_logger_create(
    "/var/log/agentos/audit", "security", 10 * 1024 * 1024, 100);

audit_logger_log_permission(logger, "agent-001", "api:write", "/api/v1/config", 0);
audit_logger_log_sanitizer(logger, "agent-002", "<script>alert(1)</script>", "", 0);
audit_logger_log_workbench(logger, "agent-003", "rm -rf /tmp/test", 0);

audit_logger_flush(logger);

uint64_t total, failed;
audit_logger_stats(logger, &total, &failed);
printf("Logged: %lu, Failed: %lu\n", total, failed);

audit_logger_destroy(logger);
```

## 防篡改机制

审计日志使用 HMAC-SHA256 签名链确保完整性：

```
日志1: [data1, hmac1=HMAC(data1)]
日志2: [data2, hmac2=HMAC(data2 + hmac1)]
日志3: [data3, hmac3=HMAC(data3 + hmac2)]
```

任何中间日志被篡改都会导致后续所有日志的 HMAC 校验失败。

## 相关子系统

| 子系统 | 关系 |
|--------|------|
| [Permission](../permission/README.md) | 权限拒绝事件写入审计日志 |
| [Sanitizer](../sanitizer/README.md) | 输入清洗拒绝事件写入审计日志 |
| [Security](../security/README.md) | 安全防护事件写入审计日志 |
| [Workbench](../workbench/README.md) | 工作台执行事件写入审计日志 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
