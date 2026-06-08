# Info Daemon — 系统信息守护进程

> **模块路径**: `agentos/daemon/info_d/` | **版本**: v0.0.5

## 概述

`daemon/info_d/` 是 AgentOS 的系统信息采集守护进程，负责周期性采集 CPU、内存、磁盘等系统资源指标，并通过 JSON-RPC 2.0 接口对外提供实时和历史系统信息查询服务。它是 AgentOS 可观测性体系的基础数据源，为调度、监控和告警等上层服务提供系统运行状态数据支撑。

### 架构定位

```
info_d/ → IPC Service Bus → sched_d / monit_d / observe_d
    ↑
 系统指标采集层（CPU/内存/磁盘）
```

### 核心职责

- **周期性指标采集**：每 5 秒采集一次系统资源快照，存储最新快照 + 环形缓冲区历史记录
- **多维度系统指标**：CPU 使用率、内存使用率、磁盘使用率、CPU 核心数、系统运行时间
- **历史数据管理**：64 条环形缓冲区历史快照，支持趋势分析
- **跨平台采集**：Linux 通过 sysinfo/statvfs/sysconf 采集，Windows 通过 GlobalMemoryStatusEx/GetDiskFreeSpaceExW 采集
- **健康状态检测**：自动检测采集线程是否正常工作

## 目录结构

```
info_d/
├── CMakeLists.txt                    # 构建配置
├── README.md                         # 本文件
└── src/                              # 实现文件
    └── main.c                        # 守护进程入口（含采集逻辑与请求处理）
```

## 核心组件说明

### 系统信息快照（system_info_snapshot_t）

每次采集生成一个系统资源快照，包含以下指标：

| 字段 | 类型 | 说明 |
|------|------|------|
| `cpu_usage_pct` | double | CPU 使用率（百分比） |
| `total_memory_kb` | uint64_t | 总内存（KB） |
| `free_memory_kb` | uint64_t | 空闲内存（KB） |
| `used_memory_kb` | uint64_t | 已用内存（KB） |
| `memory_usage_pct` | double | 内存使用率（百分比） |
| `disk_total_kb` | uint64_t | 磁盘总容量（KB） |
| `disk_free_kb` | uint64_t | 磁盘空闲容量（KB） |
| `disk_used_kb` | uint64_t | 磁盘已用容量（KB） |
| `disk_usage_pct` | double | 磁盘使用率（百分比） |
| `cpu_cores` | int | CPU 核心数 |
| `uptime_sec` | uint64_t | 系统运行时间（秒） |
| `timestamp` | uint64_t | 采集时间戳 |

### 采集机制

- **采集间隔**：5 秒
- **存储策略**：最新快照（`latest_snapshot`）+ 环形缓冲区历史（容量 64 条）
- **后台线程**：独立采集线程，周期性执行采集并更新快照数据
- **线程安全**：快照读写通过适当的同步机制保护

### 事件循环

- 基于 `agentos_event_loop`（epoll）实现
- 监听 `server_fd` 上的客户端连接
- 请求处理在事件循环中完成

## 接口说明

### 请求处理

请求处理器返回 JSON 格式的系统信息，包含以下字段：

```json
{
    "platform": "linux",
    "uptime": 86400,
    "request_count": 42,
    "error_count": 0,
    "cpu": {
        "usage_pct": 23.5,
        "cores": 8
    },
    "memory": {
        "total_kb": 16777216,
        "free_kb": 8388608,
        "used_kb": 8388608,
        "usage_pct": 50.0
    },
    "disk": {
        "total_kb": 524288000,
        "free_kb": 262144000,
        "used_kb": 262144000,
        "usage_pct": 50.0
    },
    "collection": {
        "interval_sec": 5,
        "history_capacity": 64,
        "last_collection_ts": 1717660800
    }
}
```

### JSON-RPC 2.0 方法

| 方法 | 说明 |
|------|------|
| `info.system` | 获取当前系统信息快照 |
| `info.history` | 获取历史系统信息记录 |
| `info.health` | 查询采集服务健康状态 |

## 通信方式

| 方向 | 协议 | 说明 |
|------|------|------|
| 入站 | JSON-RPC 2.0 | 通过 IPC Service Bus 接收请求 |
| 入站 | TCP | 默认监听端口 8083 |
| 入站 | Unix Socket | `AGENTOS_RUNTIME_DIR/info.sock` |

## 配置选项

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| TCP 端口 | 8083 | HTTP/TCP 监听端口 |
| Unix Socket | `AGENTOS_RUNTIME_DIR/info.sock` | IPC 通信 Socket 路径 |
| 采集间隔 | 5 秒 | 系统指标采集周期 |
| 历史容量 | 64 | 环形缓冲区历史快照条数 |

## 健康检查机制

- **采集线程检测**：检查最近一次采集时间是否超过 3 个采集间隔（15 秒），若超时则判定为不健康
- **运行状态检查**：验证服务是否处于正常运行状态
- **错误计数**：跟踪请求处理错误数，辅助故障诊断

## 跨平台支持

| 平台 | 采集方式 | 说明 |
|------|----------|------|
| Linux | sysinfo / statvfs / sysconf | 原生系统调用 |
| Windows | GlobalMemoryStatusEx / GetDiskFreeSpaceExW | Windows API |

## 依赖关系

```
info_d
├── common (agentos_common, svc_common)
├── Threads::Threads
└── Windows 额外: ws2_32
```

## 构建说明

```bash
# 构建系统信息守护进程
cmake -B build -DBUILD_TESTS=ON
cmake --build build --target agentos-info-d
```

## 使用示例

### 启动系统信息守护进程

```bash
# 默认启动（TCP 端口 8083）
./agentos-info-d

# 指定配置文件
./agentos-info-d --config info_config.json
```

### 信号处理

| 信号 | 行为 |
|------|------|
| SIGINT | 优雅关闭 |
| SIGTERM | 优雅关闭 |
| SIGUSR1 | 动态调整日志级别 |
| SIGPIPE | 忽略（防止写入已关闭连接导致进程终止） |

---

© 2026 SPHARX Ltd. All Rights Reserved.
