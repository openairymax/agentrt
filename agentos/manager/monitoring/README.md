# Manager Monitoring

AgentOS Manager 模块的监控配置，包含告警规则和 Grafana 仪表盘。

## 目录结构

```
monitoring/
├── alerts/                    # 告警规则
│   └── cupolas-alerts.yml     # Cupolas 子系统告警
└── dashboards/                # 仪表盘
    └── cupolas-dashboard.json # Cupolas 监控仪表盘
```

## 版本

v0.1.0