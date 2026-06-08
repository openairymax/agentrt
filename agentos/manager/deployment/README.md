# Manager Deployment — 部署环境配置

**模块路径**: `agentos/manager/deployment/`
**版本**: v0.0.5

## 概述

`manager/deployment/` 包含 AgentOS Manager 模块的部署环境配置，定义 Cupolas 子系统的多环境部署参数，包括各环境的资源限制、服务端点和监控配置。

## 目录结构

```
deployment/
└── cupolas/
    └── environments.yaml   # Cupolas 多环境部署配置
```

## 核心组件

### cupolas/environments.yaml

Cupolas 子系统的多环境部署配置，定义不同部署环境下的参数差异：

- 各环境的资源分配与限制
- 服务端点与连接配置
- 监控与告警阈值
- 环境特定的安全策略

## 依赖关系

| 组件 | 用途 |
|------|------|
| Cupolas 子系统 | 安全沙箱运行时 |
| `environment/` | 环境配置覆盖 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
