# Monitor Daemon — 监控告警守护进程

`daemon/monit_d/` 是 AgentOS 的监控告警守护进程，负责系统指标采集、健康检查和告警管理。

## 核心职责

- **指标采集**：从各守护进程采集运行时指标
- **健康检查**：定期检查各服务的健康状态
- **告警管理**：基于规则触发告警，通知管理员
- **数据聚合**：汇总和聚合跨服务的监控数据
- **可视化输出**：提供 Prometheus 格式的指标端点

## 监控指标

| 指标类别 | 示例 |
|----------|------|
| 系统指标 | CPU 使用率、内存使用量、磁盘 I/O |
| 服务指标 | 请求延迟、错误率、吞吐量 |
| 业务指标 | 任务数、Agent 数、工具调用数 |
| 安全指标 | 登录失败数、权限拒绝数、注入检测数 |

## 告警级别

| 级别 | 说明 | 响应方式 |
|------|------|----------|
| INFO | 信息通知 | 日志记录 |
| WARNING | 警告 | 通知管理员 |
| CRITICAL | 严重 | 自动执行恢复操作 |
| FATAL | 致命 | 触发熔断机制 |

## 核心能力

| 能力 | 说明 |
|------|------|
| `monit.metrics` | 查询当前监控指标 |
| `monit.health` | 查询服务健康状态 |
| `monit.alert.list` | 告警列表 |
| `monit.alert.ack` | 确认告警 |
| `monit.rules.set` | 配置告警规则 |

## 使用方式

```bash
# 启动监控守护进程
./monit_d --config monit_config.json

# 指定指标采集间隔
./monit_d --collect-interval 15

# 配置告警渠道
./monit_d --alert-webhook https://hooks.example.com/alert
```

---

*AgentOS Daemon — Monitor Daemon*
