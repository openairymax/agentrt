# Domes 配置目录 (config)

## ⚠️ 配置迁移通知 (2026-03-26)

**重要更新**: domes 模块的 CI/CD 配置文件已迁移到 AgentOS 统一配置目录。

### 迁移后的配置位置

| 原位置 | 新位置 | 说明 |
|--------|--------|------|
| `config/alerts.yml` | `AgentOS/config/monitoring/alerts/domes-alerts.yml` | Prometheus 告警规则 |
| `config/deployment.yaml` | `AgentOS/config/deployment/domes/environments.yaml` | 多环境部署配置 |
| `config/grafana-dashboard.json` | `AgentOS/config/monitoring/dashboards/domes-dashboard.json` | Grafana 监控面板 |

### 配置目录新结构

```
AgentOS/config/
├── monitoring/                    # 监控配置
│   ├── alerts/
│   │   └── domes-alerts.yml      # ← 已迁移
│   └── dashboards/
│       └── domes-dashboard.json   # ← 已迁移
├── deployment/
│   └── domes/
│       └── environments.yaml      # ← 已迁移
├── security/                      # 安全配置
│   ├── permission_rules.yaml
│   └── policy.yaml
└── ...
```

---

## 📂 规划中的配置文件结构

> 以下是 domes 运行时配置的规划目录结构，当前尚未实现。
> 运行时配置加载机制参考: `src/domes_config.c`

```
domes/config/
├── audit/                  # 审计日志配置 (规划中)
│   ├── audit_rules.yaml   # 审计规则
│   ├── log_rotation.yaml  # 日志轮转配置
│   └── retention.yaml     # 保留策略
├── permission/             # 权限裁决配置 (规划中)
│   ├── rbac_policies.yaml # RBAC 策略
│   ├── abac_rules.yaml    # ABAC 规则
│   └── capabilities.yaml  # 能力列表
├── sanitizer/              # 输入净化配置 (规划中)
│   ├── filters.yaml       # 过滤器规则
│   ├── patterns.yaml      # 风险模式
│   └── thresholds.yaml    # 阈值配置
├── workbench/              # 虚拟工位配置 (规划中)
│   ├── isolation.yaml     # 隔离策略
│   ├── resources.yaml     # 资源配额
│   └── security.yaml      # 安全配置
└── global/                 # 全局配置 (规划中)
    ├── domes_config.yaml  # 主配置文件
    ├── security_policy.yaml # 安全策略
    └── compliance.yaml    # 合规要求
```

---

## 🔧 运行时配置加载

domes 模块支持通过环境变量或代码配置运行时参数：

```c
#include "domes_config.h"

// 加载配置
int result = domes_config_load_from_env();
if (result != 0) {
    return result;
}

// 获取审计配置
audit_config_t* audit_cfg = domes_config_get_audit();

// 释放配置
domes_config_cleanup();
```

---

## 📚 相关文档

- [AgentOS 配置管理](../../config/README.md) - 全局配置说明
- [Domes 模块概览](../README.md) - 模块整体介绍

---

*最后更新：2026-03-26*
*维护者：AgentOS Domes Team*
