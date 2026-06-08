# Manager Monitoring — 监控配置

**模块路径**: `agentos/manager/monitoring/`
**版本**: v0.0.5

## 概述

`manager/monitoring/` 包含 AgentOS Manager 模块的监控配置，提供 Cupolas 子系统的告警规则和 Grafana 可视化仪表盘，用于实时监控系统运行状态和异常告警。

## 目录结构

```
monitoring/
├── alerts/                            # 告警规则
│   └── cupolas-alerts.yml             # Cupolas 子系统告警规则
└── dashboards/                        # 仪表盘
    └── cupolas-dashboard.json         # Cupolas 监控仪表盘
```

## 核心组件

### alerts/cupolas-alerts.yml

Cupolas 子系统的 Prometheus 告警规则，定义：

- 资源使用率告警（CPU / 内存 / 磁盘）
- 服务可用性告警
- 性能异常告警
- 安全事件告警

### dashboards/cupolas-dashboard.json

Cupolas 子系统的 Grafana 仪表盘配置，提供：

- 系统概览面板
- 资源使用趋势图
- 请求延迟分布
- 错误率监控
- 安全事件时间线

## 依赖关系

| 组件 | 用途 |
|------|------|
| Prometheus | 告警规则引擎 |
| Grafana | 仪表盘可视化 |
| Cupolas 子系统 | 被监控目标 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
