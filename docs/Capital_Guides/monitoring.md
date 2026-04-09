Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# 监控运维指南

**版本**: Doc V1.8  
**最后更新**: 2026-04-09  
**适用场景**: 生产环境运维  

---

## 📊 监控体系概览

AgentOS采用**Prometheus + Grafana** 监控栈，提供完整的可观测性：

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  AgentOS    │───▶│ Prometheus  │───▶│  Grafana    │
│  (Metrics)  │    │  (采集存储)   │    │  (可视化)   │
└─────────────┘    └─────────────┘    └─────────────┘
       │                  │                   │
       ▼                  ▼                   ▼
   /metrics        告警规则          仪表盘面板
```

---

## 🔧 Prometheus 配置

### 核心配置文件

位置：`docker/monitoring/prometheus.yml`

```yaml
global:
  scrape_interval: 15s      # 采集间隔
  evaluation_interval: 15s  # 规则评估间隔

scrape_configs:
  - job_name: 'agentos-kernel'
    static_configs:
      - targets: ['kernel:9090']
    metrics_path: '/metrics'
    scrape_interval: 10s     # 内核指标高频采集

  - job_name: 'agentos-daemon'
    static_configs:
      - targets: ['daemon:8001', 'daemon:8002', ..., 'daemon:8006']
    scrape_interval: 15s

  - job_name: 'postgres-exporter'  # 需要部署postgres_exporter
    static_configs:
      - targets: ['postgres-exporter:9187']

  - job_name: 'redis-exporter'     # 需要部署redis_exporter
    static_configs:
      - targets: ['redis-exporter:9121']
```

### 启用监控栈

```bash
# 使用Docker Compose启用监控
docker compose -f docker/docker-compose.prod.yml --profile monitoring up -d prometheus grafana

# 访问地址：
# Prometheus: http://your-server:9091
# Grafana: http://your-server:3000 (admin/your_password)
```

---

## 📈 关键指标详解

### 1️⃣ 系统级指标

| 指标名 | 类型 | 说明 | 告警阈值 |
|--------|------|------|---------|
| `process_cpu_seconds_total` | Counter | CPU使用时间 | >80%持续5分钟 |
| `go_memstats_heap_inuse_bytes` | Gauge | 堆内存使用 | >80%限制 |
| `process_open_fds` | Gauge | 打开的文件描述符数 | >10000 |

**PromQL查询示例**:

```promql
# CPU使用率（最近5分钟）
rate(process_cpu_seconds_total{job="agentos-kernel"}[5m]) * 100

# 内存使用率
(go_memstats_heap_inuse_bytes / go_memstats_sys_bytes) * 100

# 文件描述符使用率
process_open_fds / process_max_fds * 100
```

---

### 2️⃣ IPC通信指标

| 指标名 | 类型 | 说明 | 正常范围 |
|--------|------|------|---------|
| `agentos_ipc_requests_total` | Counter | IPC请求总数 | 持续增长 |
| `agentos_ipc_request_duration_seconds` | Histogram | IPC请求延迟分布 | P99<50ms |
| `agentos_ipc_errors_total` | Counter | IPC错误总数 | 错误率<0.1% |
| `agentos_ipc_queue_length` | Gauge | 消息队列长度 | <1000 |

**关键查询**:

```promql
# IPC请求QPS（每秒请求数）
sum(rate(agentos_ipc_requests_total[1m])) by (method, target)

# IPC P99延迟
histogram_quantile(0.99,
  sum(rate(agentos_ipc_request_duration_seconds_bucket[5m])) by (le)
)

# IPC错误率
sum(rate(agentos_ipc_errors_total[5m])) /
sum(rate(agentos_ipc_requests_total[5m])) * 100

# 消息队列积压情况
agentos_ipc_queue_length
```

---

### 3️⃣ 内存管理指标

| 指标名 | 类型 | 说明 | 告警阈值 |
|--------|------|------|---------|
| `agentos_memory_allocated_bytes` | Gauge | 已分配内存总量 | >80%池大小 |
| `agentos_memory_free_bytes` | Gauge | 可用内存量 | <20%池大小 |
| `agentos_memory_allocations_total` | Counter | 分配次数 | - |
| `agentos_memory_deallocations_total` | Counter | 释放次数 | - |
| `agentos_memory_leak_detected` | Gauge | 检测到泄漏 | =1时告警 |

**关键查询**:

```promql
# 内存利用率
agentos_memory_allocated_bytes /
(agentos_memory_allocated_bytes + agentos_memory_free_bytes) * 100

# 分配/释放比率（接近1表示正常）
sum(rate(agentos_memory_allocations_total[5m])) /
sum(rate(agentos_memory_deallocations_total[5m]))

# 内存碎片率
1 - (agentos_memory_largest_free_block / agentos_memory_free_bytes)

# 泄漏检测
agentos_memory_leak_detected == 1
```

---

### 4️⃣ 任务调度指标

| 指标名 | 类型 | 说明 | 告警阈值 |
|--------|------|------|---------|
| `agentos_tasks_created_total` | Counter | 创建任务总数 | - |
| `agentos_tasks_completed_total` | Counter | 完成任务总数 | - |
| `agentos_tasks_failed_total` | Counter | 失败任务总数 | 失败率>5% |
| `agentos_tasks_active` | Gauge | 当前活跃任务数 | >最大并发*0.9 |
| `agentos_task_queue_length` | Gauge | 任务队列长度 | >5000 |

**关键查询**:

```promql
# 任务吞吐量（完成速率）
sum(rate(agentos_tasks_completed_total[5m]))

# 任务失败率
sum(rate(agentos_tasks_failed_total[5m])) /
sum(rate(agentos_tasks_created_total[5m])) * 100

# 平均任务执行时间
avg(agentos_task_duration_seconds) by (priority)

# 队列积压时间估算
agentos_task_queue_length / sum(rate(agentos_tasks_completed_total[5m]))
```

---

### 5️⃣ 安全指标 (Cupolas)

| 指标名 | 类型 | 说明 | 告警阈值 |
|--------|------|------|---------|
| `agentos_cupolas_permission_checks_total` | Counter | 权限检查总次数 | - |
| `agentos_cupolas_permission_denied_total` | Counter | 权限拒绝次数 | 拒绝率突然升高 |
| `agentos_cupolas_sanitization_blocked_total` | Counter | 净化拦截次数 | 异常增高 |
| `agentos_cupolas_audit_events_total` | Counter | 审计事件总数 | - |
| `agentos_cupolas_sandbox_violations_total` | Counter | 沙箱违规次数 | >0即告警 |

**安全事件查询**:

```promql
# 权限拒绝率
sum(rate(agentos_cupolas_permission_denied_total[5m])) /
sum(rate(agentos_cupolas_permission_checks_total[5m])) * 100

# 输入净化拦截率（按类型）
sum(rate(agentos_cupolas_sanitization_blocked_total[5m])) by (attack_type)

# 最近1小时的沙箱违规事件
increase(agentos_cupolas_sandbox_violations_total[1h])

# 审计事件趋势（检测异常活动）
sum(rate(agentos_cupolas_audit_events_total[5m])) by (event_type, severity)
```

---

### 6️⃣ LLM服务指标

| 指标名 | 类型 | 说明 | 告警阈值 |
|--------|------|------|---------|
| `agentos_llm_requests_total` | Counter | LLM API请求总数 | - |
| `agentos_llm_request_duration_seconds` | Histogram | LLM推理延迟 | P99>30s |
| `agentos_llm_tokens_used_total` | Counter | Token消耗总量 | 超预算 |
| `agentos_llm_errors_total` | Counter | LLM调用错误 | 错误率>1% |
| `agentos_llm_cost_usd_total` | Counter | 累计成本(USD) | 日预算超限 |

**LLM相关查询**:

```promql
# 平均推理延迟（按模型）
avg(agentos_llm_request_duration_seconds) by (model)

# Token使用效率（输出Token/输入Token）
sum(rate(agentos_llm_output_tokens_total[1h])) /
sum(rate(agentos_llm_input_tokens_total[1h]))

# 成本追踪
sum(increase(agentos_llm_cost_usd_total[1d]))

# 缓存命中率
agentos_llm_cache_hits_total /
(agentos_llm_cache_hits_total + agentos_llm_cache_misses_total) * 100
```

---

## 🚨 告警规则

### 告警配置文件

创建 `docker/monitoring/alert_rules.yml`:

```yaml
groups:
  - name: agentos-critical
    interval: 30s
    rules:

      # === 系统健康 ===

      - alert: AgentOSDown
        expr: up{job="agentos-kernel"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "AgentOS内核服务宕机"
          description: "AgentOS内核已停止运行超过1分钟"

      - alert: HighMemoryUsage
        expr: (go_memstats_heap_inuse_bytes / go_memstats_sys_bytes) * 100 > 85
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "内存使用率过高"
          description: "AgentOS内存使用率达到{{ $value }}%，超过85%阈值"

      - alert: HighCPUUsage
        expr: rate(process_cpu_seconds_total[5m]) * 100 > 80
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "CPU使用率过高"
          description: "CPU使用率达到{{ $value }}%，持续超过10分钟"

      # === IPC通信 ===

      - alert: IPCHighLatency
        expr: histogram_quantile(0.99, sum(rate(agentos_ipc_request_duration_seconds_bucket[5m])) by (le)) > 1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "IPC延迟过高"
          description: "IPC请求P99延迟达到{{ $value }}秒，超过1秒阈值"

      - alert: IPCQueueBacklog
        expr: agentos_ipc_queue_length > 5000
        for: 3m
        labels:
          severity: critical
        annotations:
          summary: "IPC消息队列积压"
          description: "消息队列长度达到{{ $value }}，存在处理瓶颈"

      # === 任务调度 ===

      - alert: TaskFailureRateHigh
        expr: |
          sum(rate(agentos_tasks_failed_total[5m])) /
          sum(rate(agentos_tasks_created_total[5m])) * 100 > 10
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "任务失败率过高"
          description: "任务失败率达到{{ $value }}%，超过10%阈值"

      - alert: TaskQueueBacklog
        expr: agentos_task_queue_length > 3000
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "任务队列积压"
          description: "待处理任务数量达到{{ $value }}个"

      # === 安全事件 ===

      - alert: PermissionDeniedSpike
        expr: |
          sum(rate(agentos_cupolas_permission_denied_total[5m])) /
          sum(rate(agentos_cupolas_permission_checks_total[5m])) * 100 > 5
        for: 2m
        labels:
          severity: warning
        annotations:
          summary: "权限拒绝率异常升高"
          description: "权限拒绝率达到{{ $value }}%，可能存在攻击或误配置"

      - alert: SanitizationBlockSpike
        expr: rate(agentos_cupolas_sanitization_blocked_total[5m]) > 10
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "输入净化拦截激增"
          description: "输入净化拦截速率达到{{ $value }}/秒，可能遭受攻击"

      - alert: SandboxViolation
        expr: increase(agentos_cupolas_sandbox_violations_total[5m]) > 0
        for: 0m
        labels:
          severity: critical
        annotations:
          summary: "沙箱违规事件"
          description: "检测到{{ $value }}次沙箱违规，可能存在逃逸尝试"

      # === 数据库 ===

      - alert: PostgreSQLDown
        expr: up{job="postgres"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "PostgreSQL数据库不可用"
          description: "PostgreSQL连接失败，AgentOS核心功能受影响"

      - alert: PostgreSQLSlowQueries
        expr: pg_stat_activity_count{state='active'} > 50
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "数据库活跃连接过多"
          description: "当前有{{ $value }}个活跃查询，可能存在慢查询"

      # === Redis ===

      - alert: RedisDown
        expr: up{job="redis"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Redis缓存不可用"
          description: "Redis连接失败，记忆系统和权限缓存受影响"

      - alert: RedisHighMemory
        expr: redis_memory_used_bytes / redis_memory_max_bytes * 100 > 85
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Redis内存使用率过高"
          description: "Redis内存使用率达到{{ $value }}%"
```

### 配置告警通知

编辑 Grafana AlertManager 配置或使用 Webhook：

```yaml
# docker/monitoring/alertmanager/config.yml
global:
  resolve_timeout: 5m

route:
  receiver: 'default-receiver'
  group_by: ['alertname', 'severity']
  group_wait: 30s
  group_interval: 5m
  repeat_interval: 12h
  routes:
    - match:
        severity: critical
      receiver: 'critical-alerts'
      repeat_interval: 1h

receivers:
  - name: 'default-receiver'
    webhook_configs:
      - url: '${ALERT_WEBHOOK_URL}'
        send_resolved: true

  - name: 'critical-alerts'
    webhook_configs:
      - url: '${CRITICAL_ALERT_WEBHOOK_URL}'
        send_resolved: true
    email_configs:
      - to: 'oncall@spharx.cn'
        send_resolved: true
```

---

## 📊 Grafana 仪表盘

### 导入预置仪表盘

AgentOS提供以下预配置仪表盘JSON文件：

| 仪表盘 | 用途 | 文件路径 |
|--------|------|---------|
| **系统概览** | CPU、内存、网络、磁盘 | `docker/monitoring/grafana/dashboards/system-overview.json` |
| **IPC性能** | 请求延迟、吞吐量、错误率 | `docker/monitoring/grafana/dashboards/ipc-performance.json` |
| **任务调度** | 队列状态、执行统计 | `docker/monitoring/grafana/dashboards/task-scheduler.json` |
| **记忆系统** | L1-L4各层统计 | `docker/monitoring/grafana/dashboards/memory-system.json` |
| **安全态势** | Cupolas四层防护统计 | `docker/monitoring/grafana/dashboards/security-posture.json` |
| **LLM服务** | Token使用、成本、延迟 | `docker/monitoring/grafana/dashboards/llm-service.json` |

导入方法：

1. 打开Grafana → Dashboards → Import
2. 上传JSON文件或粘贴内容
3. 选择Prometheus数据源
4. 点击Import

### 自定义仪表盘示例

创建一个**系统健康仪表盘**的JSON模板：

```json
{
  "dashboard": {
    "title": "AgentOS System Health",
    "uid": "agentos-health",
    "panels": [
      {
        "title": "Service Uptime",
        "type": "stat",
        "targets": [
          {
            "expr": "up{job=\"agentos-kernel\"}",
            "legendFormat": "{{job}}"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "thresholds": {
              "mode": "absolute",
              "steps": [
                {"color": "red", "value": 0},
                {"color": "green", "value": 1}
              ]
            },
            "mappings": [
              {"options": {"0": {"text": "DOWN", "color": "red"}}, "type": "value"},
              {"options": {"1": {"text": "UP", "color": "green"}}, "type": "value"}
            ],
            "unit": "none"
          }
        },
        "gridPos": {"h": 4, "w": 6, "x": 0, "y": 0}
      },
      {
        "title": "Request Rate (QPS)",
        "type": "graph",
        "targets": [
          {
            "expr": "sum(rate(agentos_ipc_requests_total[5m])) by (method)",
            "legendFormat": "{{method}}"
          }
        ],
        "gridPos": {"h": 8, "w": 12, "x": 6, "y": 0}
      },
      {
        "title": "P99 Latency",
        "type": "gauge",
        "targets": [
          {
            "expr": "histogram_quantile(0.99, sum(rate(agentos_ipc_request_duration_seconds_bucket[5m])) by (le))"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "unit": "s",
            "max": 1,
            "min": 0,
            "thresholds": {
              "mode": "absolute",
              "steps": [
                {"color": "green", "value": 0},
                {"color": "yellow", "value": 0.1},
                {"color": "red", "value": 0.5}
              ]
            }
          }
        },
        "gridPos": {"h": 4, "w": 6, "x": 18, "y": 0}
      }
    ],
    "refresh": "30s",
    "schemaVersion": 38,
    "version": 1
  }
}
```

---

## 🔍 故障排查命令速查

```bash
# ==========================================
# 快速诊断命令集合
# ==========================================

# 1. 服务状态检查
docker compose -f docker/docker-compose.prod.yml ps --format "table {{.Name}}\t{{.Status}}\t{{.Ports}}"

# 2. 资源使用概览
docker stats --no-stream --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}\t{{.BlockIO}}"

# 3. 最近错误日志
for svc in kernel daemon postgres redis; do
  echo "=== $svc errors ==="
  docker compose -f docker/docker-compose.prod.yml logs $svc 2>&1 | grep -i error | tail -5
done

# 4. Prometheus目标检查
curl -s http://localhost:9091/api/v1/targets | jq '.data.activeTargets[] | {job:.labels.job,health:.health,lastScrape:.lastScrapeTime}'

# 5. 关键指标快照
echo "=== IPC QPS ==="
curl -s 'http://localhost:9090/api/v1/query?query=sum(rate(agentos_ipc_requests_total[1m]))' | jq '.data.result[0].value[1]'

echo "=== Memory Usage % ==="
curl -s 'http://localhost:9090/api/v1/query?query=(agentos_memory_allocated_bytes/(agentos_memory_allocated_bytes+agentos_memory_free_bytes))*100' | jq '.data.result[0].value[1]'

echo "=== Active Tasks ==="
curl -s 'http://localhost:9090/api/v1/query?query=agentos_tasks_active' | jq '.data.result[0].value[1]'

# 6. 数据库连接检查
docker exec agentos-postgres-prod psql -U agentos -d agentos -c "
  SELECT state, count(*) FROM pg_stat_activity GROUP BY state;
"

# 7. Redis内存检查
docker exec agentos-redis-prod redis-cli -a $REDIS_PASSWORD INFO memory | grep used_memory_human

# 8. 网络连通性测试
docker exec agentos-kernel-prod curl -sf http://postgres:5432 && echo "PostgreSQL OK" || echo "PostgreSQL FAIL"
docker exec agentos-kernel-prod curl -sf http://redis:6379 && echo "Redis OK" || echo "Redis FAIL"
```

---

## 📅 运维日历

### 日常巡检（每日）

- [ ] 检查所有容器运行状态（`docker ps`）
- [ ] 查看Grafana仪表盘确认无红色告警
- [ ] 检查磁盘空间使用（`df -h`）
- [ ] 快速浏览错误日志（`grep ERROR logs`）

### 周度维护（每周）

- [ ] 检查Prometheus数据保留策略，清理过期数据
- [ ] 审计安全日志（Cupolas审计事件）
- [ ] 检查LLM API成本是否在预算内
- [ ] 更新Docker镜像到最新补丁版本
- [ ] 执行数据库VACUUM FULL（低峰期）

### 月度维护（每月）

- [ ] 全量备份验证（恢复测试）
- [ ] 性能基线对比分析
- [ ] SSL证书有效期检查
- [ ] 安全漏洞扫描
- [ ] 容量规划评估（基于增长趋势）

### 季度审查（每季度）

- [ ] 灾难恢复演练
- [ ] 架构优化评估
- [ ] 成本效益分析
- [ ] 技术债务清理计划制定

---

## 📚 相关文档

- [**Docker部署指南**](../guides/deployment.md) — 生产环境部署步骤
- [**性能调优**](performance-tuning.md) — 性能优化详细指南
- [**故障排查**](../troubleshooting/common-issues.md) — 问题诊断手册
- [**安全加固**](security-hardening.md) — 企业级安全配置清单

---

> *"可观测性是现代系统的神经系统——没有它，你就是在盲目飞行。"*

**© 2026 SPHARX Ltd. All Rights Reserved.**
