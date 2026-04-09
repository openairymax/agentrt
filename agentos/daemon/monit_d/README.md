# 监控服务 (monit_d)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`monit_d` 是 AgentOS 的**可观测性中心**，提供指标采集、链路追踪、日志聚合和智能告警的完整监控解决方案。作为系统的"眼睛和耳朵"，本服务确保运维团队能够实时掌握系统健康状态。

### 核心价值

- **全栈监控**: 覆盖内核层、服务层、应用层的全维度指标
- **实时告警**: 基于阈值的智能告警，支持多渠道通知
- **分布式追踪**: OpenTelemetry 集成，全链路行为追踪
- **日志聚合**: 集中式日志管理，结构化查询分析

---

## 📁 目录结构

```
monit_d/
├── README.md                 # 本文档
├── CMakeLists.txt            # 构建配置
├── include/
│   └── monitor_service.h     # 服务接口定义
├── src/
│   ├── main.c                # 程序入口
│   ├── service.c             # 服务主逻辑
│   ├── metrics_collector.c   # 指标采集器
│   ├── logging_aggregator.c  # 日志聚合器
│   ├── tracing_backend.c     # 追踪后端
│   ├── alert_manager.c       # 告警管理器
│   ├── dashboard_exporter.c  # 仪表盘导出
│   └── storage_engine.c      # 存储引擎
├── tests/                    # 单元测试
│   ├── test_metrics.c
│   ├── test_alerts.c
│   └── test_logging.c
└── agentos/manager/
    └── monit.yaml            # 服务配置模板
```

---

## 🔧 核心功能

### 1. 指标采集 (Metrics Collection)

实时采集系统和业务指标：

#### 系统指标

| 指标类别 | 具体指标 | 采集频率 |
|---------|---------|---------|
| **CPU** | 使用率、负载、核心数 | 1s |
| **内存** | 总量、已用、缓存、交换 | 1s |
| **磁盘** | 容量、IO、使用率 | 5s |
| **网络** | 带宽、连接数、丢包率 | 1s |
| **进程** | 数量、状态、资源占用 | 5s |

#### AgentOS 特有指标

| 指标名称 | 类型 | 说明 |
|---------|------|------|
| `agentos_tasks_total` | Counter | 任务总数 |
| `agentos_tasks_active` | Gauge | 活跃任务数 |
| `agentos_memory_records` | Gauge | 记忆记录数 |
| `agentos_sessions_active` | Gauge | 活跃会话数 |
| `agentos_agents_registered` | Gauge | 注册 Agent 数 |
| `agentos_ipc_latency_us` | Histogram | IPC 延迟分布 |
| `agentos_syscall_duration_us` | Histogram | Syscall 耗时分布 |

### 2. 链路追踪 (Distributed Tracing)

基于 OpenTelemetry 的全链路追踪：

```json
{
  "trace_id": "abc123def456",
  "span_id": "span789",
  "parent_span_id": "span456",
  "name": "task.execute",
  "start_time": "2026-03-25T10:23:45.123Z",
  "end_time": "2026-03-25T10:23:46.789Z",
  "duration_ms": 1666,
  "attributes": {
    "service": "tool_d",
    "task_id": "12345",
    "skill": "browser_skill",
    "user_id": "user-001"
  },
  "status": "OK"
}
```

### 3. 日志聚合 (Log Aggregation)

集中式日志管理：

```yaml
# 日志收集配置
logging:
  sources:
    - path: /var/agentos/logs/kernel.log
      type: kernel
      format: json
    - path: /var/agentos/logs/services/*.log
      type: service
      format: text
    - path: /var/agentos/logs/apps/*.log
      type: application
      format: json
  
  processing:
    parse_timestamp: true
    extract_trace_id: true
    add_labels:
      environment: production
      region: cn-beijing
```

### 4. 智能告警 (Alerting)

基于规则的告警系统：

```yaml
alerts:
  - name: high_cpu_usage
    metric: system.cpu.usage_percent
    condition: "> 80"
    duration: 5m
    severity: warning
    channels:
      - email: ops-team@company.com
      - webhook: https://hooks.slack.com/xxx
  
  - name: agent_crash
    metric: agentos.agents.crashed
    condition: "> 0"
    duration: 1m
    severity: critical
    channels:
      - sms: +86-138-xxxx-xxxx
      - pagerduty: xxx
```

---

## 🌐 API 接口

### RESTful API

#### 查询指标数据

```http
GET /api/v1/metrics/query
Query Parameters:
  - metric: 指标名称
  - start: 开始时间 (RFC3339)
  - end: 结束时间
  - step: 步长 (如 1m, 5m)

Response 200:
{
  "metric": "system.cpu.usage_percent",
  "data_points": [
    {"timestamp": "2026-03-25T10:00:00Z", "value": 45.2},
    {"timestamp": "2026-03-25T10:01:00Z", "value": 47.8},
    ...
  ]
}
```

#### 查询追踪数据

```http
GET /api/v1/traces/{trace_id}

Response 200:
{
  "trace_id": "abc123",
  "spans": [...],
  "services": ["llm_d", "tool_d"],
  "duration_ms": 2345
}
```

#### 搜索日志

```http
POST /api/v1/logs/search
Content-Type: application/json

{
  "query": "ERROR AND task_id:12345",
  "time_range": {
    "from": "2026-03-25T09:00:00Z",
    "to": "2026-03-25T10:00:00Z"
  },
  "limit": 100
}

Response 200:
{
  "total": 15,
  "logs": [...]
}
```

#### 创建告警规则

```http
POST /api/v1/alerts/rules
Content-Type: application/json

{
  "name": "高内存使用率",
  "metric": "system.memory.used_percent",
  "condition": "> 90",
  "duration": "10m",
  "severity": "warning",
  "channels": ["email"]
}

Response 201:
{
  "rule_id": "alert-rule-001",
  "status": "active"
}
```

### Prometheus Exporter

```bash
# 访问 Prometheus 格式指标
curl http://localhost:9090/metrics

# 输出示例
# HELP agentos_tasks_total Total number of tasks
# TYPE agentos_tasks_total counter
agentos_tasks_total 1250

# HELP agentos_memory_records Total memory records
# TYPE agentos_memory_records gauge
agentos_memory_records 58432
```

---

## ⚙️ 配置选项

### 服务配置 (agentos/manager/monit.yaml)

```yaml
server:
  port: 9090
  grpc_port: 9091
  max_connections: 500

metrics:
  collection_interval_sec: 5
  retention_days: 15
  storage_backend: prometheus  # prometheus/influxdb
  prometheus:
    url: http://localhost:9090
    bucket: agentos_metrics

tracing:
  enabled: true
  backend: jaeger  # jaeger/zipkin
  jaeger:
    collector_url: http://localhost:14268/api/traces
    service_name: agentos

logging:
  aggregation_enabled: true
  storage_type: elasticsearch  # elasticsearch/loki
  elasticsearch:
    hosts: ["http://localhost:9200"]
    index_prefix: agentos-logs
    rotation: daily

alerting:
  enabled: true
  evaluation_interval_sec: 30
  notification_channels:
    email:
      smtp_server: smtp.company.com
      from: alerts@agentos.local
    slack:
      webhook_url: https://hooks.slack.com/xxx
    pagerduty:
      service_key: xxx

storage:
  data_dir: /var/agentos/data/monit_d
  max_size_gb: 50
  cleanup_policy: oldest_first
```

---

## 🚀 使用指南

### 启动服务

```bash
# 方式 1: 直接启动
./build/agentos/daemon/monit_d/agentos-monit-d

# 方式 2: 指定配置文件
./build/agentos/daemon/monit_d/agentos-monit-d --manager ../agentos/manager/service/monit_d/monit.yaml

# 方式 3: systemd 管理 (Linux)
sudo systemctl enable agentos-monit-d
sudo systemctl start agentos-monit-d
```

### 查看系统指标

```bash
# CPU 使用率
curl "http://localhost:9090/api/v1/metrics/query?metric=system.cpu.usage_percent&start=-1h"

# 内存使用趋势
curl "http://localhost:9090/api/v1/metrics/query?metric=system.memory.used_percent&start=-24h&step=5m"
```

### 查询 AgentOS 指标

```bash
# 当前活跃任务数
curl "http://localhost:9090/api/v1/metrics/agentos_tasks_active"

# 记忆记录总数
curl "http://localhost:9090/api/v1/metrics/agentos_memory_records"
```

### 搜索特定任务的日志

```bash
curl -X POST http://localhost:9090/api/v1/logs/search \
  -H "Content-Type: application/json" \
  -d '{
    "query": "task_id:12345",
    "time_range": {"from": "-1h", "to": "now"},
    "limit": 50
  }'
```

### 配置告警规则

```bash
curl -X POST http://localhost:9090/api/v1/alerts/rules \
  -H "Content-Type: application/json" \
  -d '{
    "name": "任务堆积告警",
    "metric": "agentos_tasks_active",
    "condition": "> 100",
    "duration": "5m",
    "severity": "warning",
    "channels": ["email"]
  }'
```

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM):

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| 指标采集延迟 | < 10ms | 单次采集 |
| 日志写入吞吐 | 50MB/s | 压缩后 |
| 追踪数据延迟 | < 100ms | 端到端 |
| 告警触发延迟 | < 30s | 从满足条件到发送 |
| 并发查询 | 100+ | 每秒查询数 |
| 存储压缩比 | 5:1 | 原始数据 vs 存储 |

---

## 🧪 测试

### 运行单元测试

```bash
cd agentos/daemon/build
ctest -R monit_d --output-on-failure

# 运行特定测试
ctest -R test_metrics --verbose
ctest -R test_alerts --verbose
```

### Python SDK 测试

```python
import requests

# 测试指标查询
response = requests.get(
    'http://localhost:9090/api/v1/metrics/query',
    params={
        'metric': 'system.cpu.usage_percent',
        'start': '-1h'
    }
)
assert response.status_code == 200
assert 'data_points' in response.json()

# 测试日志搜索
response = requests.post(
    'http://localhost:9090/api/v1/logs/search',
    json={
        'query': 'ERROR',
        'time_range': {'from': '-1h', 'to': 'now'}
    }
)
assert response.status_code == 200
assert 'logs' in response.json()
```

---

## 🔧 故障排查

### 问题 1: 指标采集失败

**症状**: 指标数据为空

**解决方案**:
```bash
# 检查采集器状态
curl http://localhost:9090/health

# 查看采集器日志
tail -f /var/agentos/logs/monit_d.log | grep collector

# 重启采集器
systemctl restart agentos-monit-d
```

### 问题 2: 告警不触发

**症状**: 达到阈值但未收到告警

**解决方案**:
```bash
# 检查告警规则状态
curl http://localhost:9090/api/v1/alerts/rules

# 手动触发测试告警
curl -X POST http://localhost:9090/api/v1/alerts/test \
  -H "Content-Type: application/json" \
  -d '{"rule_name": "high_cpu_usage"}'

# 检查通知渠道配置
cat /var/agentos/manager/monit_d/alert_channels.yaml
```

### 问题 3: 日志丢失

**症状**: 部分日志未聚合

**解决方案**:
```bash
# 检查日志源路径
ls -la /var/agentos/logs/services/

# 验证日志格式
head -n 10 /var/agentos/logs/services/llm_d.log

# 检查存储空间
df -h /var/agentos/data/monit_d
```

---

## 🔗 相关文档

- [服务层总览](../README.md) - daemon 架构说明
- [统一日志系统](../../paper/architecture/folder/logging_system.md) - 日志架构详解
- [OpenTelemetry 集成](../../paper/guides/folder/observability.md) - 可观测性指南
- [部署指南](../../paper/guides/folder/deployment.md) - 生产环境部署

---

<div align="center">

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges 始于数据，终于智能。"*

</div>

