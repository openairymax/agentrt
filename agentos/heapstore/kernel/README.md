# Heapstore Kernel — 内核级服务

**模块路径**: `agentos/heapstore/kernel/`
**版本**: v0.0.5

## 概述

`heapstore/kernel/` 包含 Heapstore 的内核级服务实现和数据目录，提供日志存储内核服务、追踪存储内核服务、IPC 通信基础设施和内存管理基础设施。内核服务直接操作底层存储，为上层 heapstore API 提供高性能的数据读写能力。

## 目录结构

```
kernel/
├── services/                     # 内核服务实现
│   ├── log_store_service.c       # 日志存储内核服务（按日期分文件、轮转、过期清理）
│   └── trace_store_service.c     # 追踪存储内核服务
├── ipc/                          # IPC 内核数据
│   ├── channels/.keep            # IPC 通道数据目录（占位）
│   └── binder/.keep              # IPC Binder 数据目录（占位）
├── memory/                       # 内存内核数据
│   ├── patterns/.keep            # 内存模式目录（占位）
│   ├── index/.keep               # 内存索引目录（占位）
│   ├── meta/.keep                # 内存元数据目录（占位）
│   └── raw/.keep                 # 原始内存数据目录（占位）
└── README.md                     # 本文件
```

## 核心组件

### services/ — 内核服务

| 服务 | 源文件 | 功能 |
|------|--------|------|
| **log_store_service** | `log_store_service.c` | 日志存储内核服务，按日期分文件存储、自动轮转、过期清理、按条件查询 |
| **trace_store_service** | `trace_store_service.c` | 追踪存储内核服务，Span 数据的持久化与查询 |

#### log_store_service API

| 函数 | 说明 |
|------|------|
| `log_store_service_init(storage_path, max_storage_bytes)` | 初始化日志存储服务 |
| `log_store_service_store_entry(level, component, message, timestamp)` | 存储日志条目 |
| `log_store_service_query_entries(start_time, end_time, level, component, ...)` | 按条件查询日志 |
| `log_store_service_free_entries(entries, count)` | 释放查询结果 |
| `log_store_service_cleanup_old_files(days_to_keep)` | 清理过期日志文件 |
| `log_store_service_get_status(total_bytes, file_count, oldest_timestamp)` | 获取存储状态 |
| `log_store_service_shutdown()` | 关闭日志存储服务 |

### ipc/ — IPC 数据目录

| 目录 | 说明 |
|------|------|
| `channels/` | IPC 消息通道数据存储（规划中） |
| `binder/` | IPC Binder 通信数据存储（规划中） |

### memory/ — 内存数据目录

| 目录 | 说明 |
|------|------|
| `patterns/` | 内存分配模式数据（规划中） |
| `index/` | 内存索引数据（规划中） |
| `meta/` | 内存元数据（规划中） |
| `raw/` | 原始内存操作数据（规划中） |

## 依赖关系

| 组件 | 用途 |
|------|------|
| heapstore 核心库 | 公共接口与类型定义 |
| agentos_common | 公共工具库（内存管理、原子操作、兼容性） |

---

© 2026 SPHARX Ltd. All Rights Reserved.
