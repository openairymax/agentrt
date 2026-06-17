# Observe Daemon — 系统可观测性守护进程

> **模块路径**: `agentos/daemon/observe_d/` | **版本**: v0.1.0

## 概述

`daemon/observe_d/` 是 AgentRT 的系统可观测性守护进程，提供 Prometheus 格式的指标采集与暴露服务。它支持 Gauge 和 Counter 两种指标类型，内置 HTTP metrics 服务端点，是 AgentRT 与 Prometheus 等外部监控系统集成的主要桥梁，为运维监控和性能分析提供标准化指标数据。

### 架构定位

```
observe_d/ → IPC Service Bus → 各守护进程指标上报
    ↑
 可观测性层（Prometheus / Health / Metrics）
```

### 核心职责

- **指标管理**：支持 Gauge（可增减）和 Counter（只增）两种指标类型，最大 256 个指标
- **Prometheus 格式暴露**：标准 Prometheus exposition 格式输出（`# HELP`、`# TYPE`、指标值 + 毫秒时间戳）
- **HTTP Metrics 服务**：独立线程在端口 9090 提供 HTTP 接口
- **健康检查端点**：`/health` 和 `/healthz` 返回 JSON 健康状态
- **默认指标**：5 个内置指标，覆盖请求、错误、HTTP 请求、指标数量和运行时间
- **自动指标创建**：`find_or_create_metric` 自动创建不存在的指标

## 目录结构

```
observe_d/
├── CMakeLists.txt                    # 构建配置
├── README.md                         # 本文件
└── src/                              # 实现文件
    └── main.c                        # 守护进程入口（含指标管理与 HTTP 服务）
```

## 核心组件说明

### 指标类型

| 指标类型 | 枚举值 | 说明 |
|----------|--------|------|
| Gauge | `OBSERVE_METRIC_GAUGE` | 可增减的仪表盘指标，新值覆盖旧值 |
| Counter | `OBSERVE_METRIC_COUNTER` | 只增计数器指标，新值累加到旧值 |

### 指标管理

- **最大指标数**：256
- **自动创建**：`find_or_create_metric` 在指标不存在时自动创建
- **记录行为**：
  - Counter：`record_metric` 累加到现有值
  - Gauge：`record_metric` 覆盖现有值

### 默认指标

| 指标名称 | 类型 | 说明 |
|----------|------|------|
| `observe_requests_total` | Counter | 总请求数 |
| `observe_errors_total` | Counter | 总错误数 |
| `observe_http_requests_total` | Counter | HTTP 请求总数 |
| `observe_metrics_count` | Gauge | 当前指标数量 |
| `observe_uptime_seconds` | Gauge | 服务运行时间（秒） |

### HTTP Metrics 服务

独立线程在端口 9090 提供 HTTP 服务，backlog=16：

| 端点 | 方法 | 说明 |
|------|------|------|
| `/metrics` | GET | Prometheus 标准格式指标输出 |
| `/health` | GET | JSON 格式健康状态 |
| `/healthz` | GET | JSON 格式健康状态（Kubernetes 风格） |
| 其他 | - | 返回 404 |

### Prometheus 输出格式

```
# HELP observe_requests_total Total requests processed
# TYPE observe_requests_total counter
observe_requests_total 42 1717660800000
# HELP observe_uptime_seconds Service uptime in seconds
# TYPE observe_uptime_seconds gauge
observe_uptime_seconds 3600 1717660800000
```

## 接口说明

### 指标操作

```c
observe_metric_t *find_or_create_metric(const char *name, observe_metric_type_t type);
void record_metric(observe_metric_t *metric, double value);
```

### JSON-RPC 2.0 方法

| 方法 | 说明 |
|------|------|
| `observe.record` | 记录指标值 |
| `observe.metrics` | 获取所有指标列表 |
| `observe.health` | 查询可观测性服务健康状态 |

### 请求处理

通过 Unix Socket 接收请求，返回 JSON 格式的服务状态信息，包含指标统计和服务运行状态。

## 通信方式

| 方向 | 协议 | 端口/路径 | 说明 |
|------|------|-----------|------|
| 入站 | JSON-RPC 2.0 | IPC Service Bus | 接收指标上报与查询请求 |
| 入站 | TCP | 8085 | 服务监听端口 |
| 入站 | Unix Socket | `AGENTOS_RUNTIME_DIR/observe.sock` | IPC 通信 |
| 出站 | HTTP | 9090 | Prometheus metrics 端点 |

## 配置选项

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| TCP 端口 | 8085 | 服务监听端口 |
| Prometheus 端口 | 9090 | HTTP metrics 服务端口 |
| Unix Socket | `AGENTOS_RUNTIME_DIR/observe.sock` | IPC 通信 Socket 路径 |
| 最大指标数 | 256 | 指标存储容量上限 |
| HTTP backlog | 16 | HTTP 服务监听 backlog |

## 健康检查机制

- **Linux**：检查 `http_fd` 是否有效和 `http_running` 线程状态
- **Windows**：仅检查服务运行状态（Prometheus HTTP 服务在 Windows 上不可用）
- **HTTP 健康端点**：`/health` 和 `/healthz` 返回 JSON 格式健康状态
- **指标自监控**：`observe_errors_total` 和 `observe_requests_total` 反映服务自身健康状况

## 跨平台支持

| 平台 | 指标管理 | Prometheus HTTP | 说明 |
|------|----------|-----------------|------|
| Linux | ✅ | ✅ | 完整支持 |
| Windows | ✅ | ❌ | Prometheus HTTP 服务不可用，仅支持指标管理与 IPC 查询 |

> **Windows 限制**：Prometheus HTTP 服务端点（端口 9090）在 Windows 上不可用，指标仅可通过 IPC Service Bus 的 JSON-RPC 2.0 接口查询。

## 依赖关系

```
observe_d
├── common (agentos_common, svc_common)
├── Threads::Threads
└── Windows 额外: ws2_32
```

## 构建说明

```bash
# 构建可观测性守护进程
cmake -B build -DBUILD_TESTS=ON
cmake --build build --target agentos-observe-d
```

## 使用示例

### 启动可观测性守护进程

```bash
# 默认启动（TCP 8085, Prometheus 9090）
./agentos-observe-d

# 指定配置文件
./agentos-observe-d --config observe_config.json
```

### Prometheus 集成

在 Prometheus 配置文件中添加 scrape 目标：

```yaml
scrape_configs:
  - job_name: 'agentos'
    static_configs:
      - targets: ['localhost:9090']
```

### 查询指标

```bash
# 获取 Prometheus 格式指标
curl http://localhost:9090/metrics

# 查询健康状态
curl http://localhost:9090/health
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
