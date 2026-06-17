# Monitor Daemon — 监控告警守护进程

> **模块路径**: `agentos/daemon/monit_d/` | **版本**: v0.1.0

## 概述

`daemon/monit_d/` 是 AgentRT 的监控告警守护进程，负责系统指标采集、健康检查、告警管理和分布式追踪。它从各守护进程采集运行时指标，执行定期健康检查，基于规则触发告警，并提供 Agent 执行状态监控和死循环检测等高级功能。monit_d 是 AgentOS 可观测性的核心组件。

### 核心职责

- **指标采集**：从各守护进程采集运行时指标（Counter/Gauge/Histogram/Summary）
- **健康检查**：定期检查各服务的健康状态
- **告警管理**：基于规则触发告警，支持多级别（INFO/WARNING/ERROR/CRITICAL）
- **日志记录**：集中式日志收集与管理
- **分布式追踪**：支持 TraceID/SpanID 的分布式追踪
- **Agent 状态监控**：监控 Agent 执行状态，支持 12 种状态追踪
- **死循环检测**：4 种检测模式（时间/模式/资源/混合），自动恢复
- **数据聚合**：汇总和聚合跨服务的监控数据
- **报告生成**：生成监控报告，支持多种格式导出

## 目录结构

```
monit_d/
├── CMakeLists.txt                    # 构建配置
├── README.md                         # 本文件
├── include/                          # 公共头文件
│   ├── monitor_service.h             # 监控服务接口定义
│   └── monit_svc_adapter.h           # 监控服务适配器接口
├── src/                              # 实现文件
│   ├── main.c                        # 守护进程入口
│   ├── service.c                     # 服务核心实现
│   ├── monit_svc_adapter.c           # 请求解析与标准化适配器
│   ├── metrics.c                     # 指标采集与管理
│   ├── alert.c                       # 告警管理
│   ├── tracing.c                     # 分布式追踪
│   └── logging.c                     # 日志收集
└── tests/                            # 单元测试
    ├── CMakeLists.txt
    ├── test_monitor.c                # 集成测试
    ├── test_metrics.c                # 指标测试
    ├── test_alert.c                  # 告警测试
    └── test_tracing.c                # 追踪测试
```

## 核心组件说明

### 指标采集（metrics）

支持 4 种指标类型：

| 类型 | 枚举值 | 说明 |
|------|--------|------|
| Counter | `METRIC_TYPE_COUNTER` | 单调递增计数器（如请求总数） |
| Gauge | `METRIC_TYPE_GAUGE` | 可增可减的仪表值（如当前并发数） |
| Histogram | `METRIC_TYPE_HISTOGRAM` | 直方图（如请求延迟分布） |
| Summary | `METRIC_TYPE_SUMMARY` | 摘要统计（如分位数） |

### 告警管理（alert）

支持 4 级告警：

| 级别 | 枚举值 | 说明 | 响应方式 |
|------|--------|------|----------|
| INFO | `ALERT_LEVEL_INFO` | 信息通知 | 日志记录 |
| WARNING | `ALERT_LEVEL_WARNING` | 警告 | 通知管理员 |
| ERROR | `ALERT_LEVEL_ERROR` | 错误 | 自动执行恢复操作 |
| CRITICAL | `ALERT_LEVEL_CRITICAL` | 严重 | 触发熔断机制 |

### 分布式追踪（tracing）

支持 OpenTelemetry 风格的分布式追踪：

- TraceID/SpanID/ParentSpanID 链路追踪
- 会话级追踪（SessionID）
- 跨服务调用链关联

### Agent 状态监控

监控 Agent 的执行状态，支持 12 种状态：

| 状态 | 枚举值 | 说明 |
|------|--------|------|
| CREATED | `AGENT_STATE_CREATED` | 已创建 |
| INITIALIZING | `AGENT_STATE_INITIALIZING` | 初始化中 |
| READY | `AGENT_STATE_READY` | 就绪 |
| RUNNING | `AGENT_STATE_RUNNING` | 运行中 |
| WAITING | `AGENT_STATE_WAITING` | 等待中 |
| THINKING | `AGENT_STATE_THINKING` | 思考中 |
| EXECUTING | `AGENT_STATE_EXECUTING` | 执行中 |
| EXECUTING_TOOL | `AGENT_STATE_EXECUTING_TOOL` | 执行工具中 |
| PAUSED | `AGENT_STATE_PAUSED` | 暂停 |
| COMPLETED | `AGENT_STATE_COMPLETED` | 完成 |
| FAILED | `AGENT_STATE_FAILED` | 失败 |
| STUCK | `AGENT_STATE_STUCK` | 卡住（可能死循环） |

### 死循环检测

4 种检测模式：

| 模式 | 枚举值 | 说明 |
|------|--------|------|
| 时间检测 | `LOOP_DETECTION_TIME_BASED` | 基于执行时间阈值 |
| 模式检测 | `LOOP_DETECTION_PATTERN_BASED` | 基于重复行为模式 |
| 资源检测 | `LOOP_DETECTION_RESOURCE_BASED` | 基于资源消耗异常 |
| 混合检测 | `LOOP_DETECTION_HYBRID` | 综合多种检测方式 |

默认配置：最大执行时间 30s，最大循环迭代 1000 次，资源阈值 0.9，启用自动恢复和告警。

## 接口说明

### 监控服务生命周期（monitor_service.h）

```c
int monitor_service_create(const monitor_config_t *manager, monitor_service_t **service);
int monitor_service_destroy(monitor_service_t *service);
int monitor_service_reload_config(monitor_service_t *service, const monitor_config_t *manager);
```

### 指标管理接口

```c
int monitor_service_record_metric(monitor_service_t *service, const metric_info_t *metric);
int monitor_service_get_metrics(monitor_service_t *service, const char *metric_name,
                                metric_info_t ***metrics, size_t *count);
```

### 告警管理接口

```c
int monitor_service_trigger_alert(monitor_service_t *service, const alert_info_t *alert);
int monitor_service_resolve_alert(monitor_service_t *service, const char *alert_id);
int monitor_service_get_alerts(monitor_service_t *service, alert_info_t ***alerts, size_t *count);
```

### 健康检查接口

```c
int monitor_service_health_check(monitor_service_t *service, const char *service_name,
                                 health_check_result_t **result);
```

### 日志接口

```c
int monitor_service_log(monitor_service_t *service, const log_info_t *log);
```

### 报告接口

```c
int monitor_service_generate_report(monitor_service_t *service, char **report);
```

### Agent 状态监控接口

```c
int monitor_service_start_agent_trace(monitor_service_t *service, const char *agent_id,
                                      const char *task_id,
                                      const loop_detection_config_t *loop_config,
                                      agent_execution_trace_t **trace);
int monitor_service_update_agent_state(monitor_service_t *service,
                                       agent_execution_trace_t *trace,
                                       agent_execution_state_t new_state,
                                       const char *location);
int monitor_service_check_loop(monitor_service_t *service,
                               agent_execution_trace_t *trace,
                               bool *is_loop, double *confidence);
int monitor_service_end_agent_trace(monitor_service_t *service,
                                    agent_execution_trace_t *trace,
                                    agent_execution_state_t final_state);
int monitor_service_get_agent_summary(monitor_service_t *service, const char *agent_id,
                                      uint64_t start_time, uint64_t end_time, char **summary);
int monitor_service_export_agent_trace(monitor_service_t *service,
                                       agent_execution_trace_t *trace,
                                       const char *format, char **data, size_t *size);
int monitor_service_get_active_agents(monitor_service_t *service,
                                      char ***agent_ids, size_t *count);
int monitor_service_reset_loop_detection(monitor_service_t *service,
                                         agent_execution_trace_t *trace);
```

### 核心数据结构

```c
typedef struct {
    uint32_t metrics_collection_interval_ms;
    uint32_t health_check_interval_ms;
    uint32_t log_flush_interval_ms;
    uint32_t alert_check_interval_ms;
    char *log_file_path;
    char *metrics_storage_path;
    bool enable_tracing;
    bool enable_alerting;
    double loop_threshold;
} monitor_config_t;

typedef struct {
    char *name;
    char *description;
    metric_type_t type;
    char **labels;
    size_t label_count;
    double value;
    uint64_t timestamp;
} metric_info_t;

typedef struct {
    char *alert_id;
    char *message;
    alert_level_t level;
    char *service_name;
    char *resource_id;
    char **labels;
    size_t label_count;
    uint64_t timestamp;
    bool is_resolved;
} alert_info_t;

typedef struct {
    loop_detection_mode_t mode;
    uint64_t max_execution_time_ms;
    size_t max_loop_iterations;
    size_t pattern_window_size;
    double resource_threshold;
    bool enable_auto_recovery;
    bool enable_alerting;
} loop_detection_config_t;
```

### JSON-RPC 2.0 方法

| 方法 | 说明 |
|------|------|
| `monit.metrics` | 查询当前监控指标 |
| `monit.health` | 查询服务健康状态 |
| `monit.alert.list` | 告警列表 |
| `monit.alert.ack` | 确认告警 |
| `monit.rules.set` | 配置告警规则 |
| `monit.agent.trace.start` | 开始 Agent 执行追踪 |
| `monit.agent.trace.update` | 更新 Agent 执行状态 |
| `monit.agent.trace.end` | 结束 Agent 执行追踪 |
| `monit.agent.summary` | 获取 Agent 执行摘要 |

## 通信方式

| 方向 | 协议 | 说明 |
|------|------|------|
| 入站 | JSON-RPC 2.0 | 通过 IPC Service Bus 接收请求 |
| 入站 | 主动推送 | 各守护进程主动上报指标和告警 |
| 出站 | 通知 | 通过 notify_d 发送告警通知 |

## 依赖关系

```
monit_d
├── common (svc_common, svc_logger, svc_config, ipc_service_bus,
│           method_dispatcher, jsonrpc_helpers, alert_manager, unified_metrics)
├── notify_d  # 告警通知推送
└── observe_d # OpenTelemetry 可观测性数据
```

## 构建说明

```bash
# 构建监控守护进程
cmake -B build -DBUILD_TESTS=ON
cmake --build build --target agentos-monit-d

# 运行监控测试
ctest --test-dir build -R "test_monitor|test_metrics|test_alert|test_tracing" -V
```

## 使用示例

### 启动监控守护进程

```bash
# 启动监控守护进程
./monit_d --config monit_config.json

# 指定指标采集间隔
./monit_d --collect-interval 15

# 配置告警渠道
./monit_d --alert-webhook https://hooks.example.com/alert
```

### 代码调用示例 — Agent 执行追踪

```c
#include "daemon/monit_d/include/monitor_service.h"

monitor_config_t config = {
    .metrics_collection_interval_ms = 15000,
    .health_check_interval_ms = 30000,
    .enable_tracing = true,
    .enable_alerting = true,
    .loop_threshold = 0.85
};

monitor_service_t *svc = NULL;
monitor_service_create(&config, &svc);

loop_detection_config_t loop_cfg = LOOP_DETECTION_CONFIG_DEFAULT;

agent_execution_trace_t *trace = NULL;
monitor_service_start_agent_trace(svc, "agent-001", "task-123", &loop_cfg, &trace);

monitor_service_update_agent_state(svc, trace, AGENT_STATE_THINKING, "llm_service");
monitor_service_update_agent_state(svc, trace, AGENT_STATE_EXECUTING_TOOL, "tool_service");

bool is_loop = false;
double confidence = 0.0;
monitor_service_check_loop(svc, trace, &is_loop, &confidence);

monitor_service_end_agent_trace(svc, trace, AGENT_STATE_COMPLETED);

char *summary = NULL;
monitor_service_get_agent_summary(svc, "agent-001", 0, 0, &summary);

monitor_service_destroy(svc);
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
