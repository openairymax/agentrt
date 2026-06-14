# Manager Logging — 日志配置

**模块路径**: `ecosystem/manager/logging/`
**版本**: v0.1.0

## 概述

`manager/logging/` 包含 AgentOS Manager 模块的日志管理配置，定义日志级别、输出格式、输出目标和轮转策略。日志配置遵循 `schema/logging.schema.json` 规范，并可通过环境配置覆盖实现不同环境的差异化日志策略。

## 目录结构

```
logging/
└── manager.yaml       # Manager 日志配置（级别、格式、输出目标）
```

## 核心组件

### manager.yaml

日志管理配置，包含以下配置项：

| 配置域 | 说明 |
|--------|------|
| **级别** | 日志级别控制（DEBUG/INFO/WARNING/ERROR/CRITICAL/OFF） |
| **格式** | 输出格式选择（text/json） |
| **输出目标** | 日志输出方式（stdout/file/syslog） |
| **文件轮转** | 文件大小限制、备份数量、压缩策略 |
| **字段掩码** | 敏感字段脱敏（password/token/api_key 等） |
| **采样** | 高吞吐场景下的日志采样配置 |
| **异步写入** | 缓冲区大小、刷新间隔 |
| **保留策略** | 最大保留天数、总大小限制 |

### 环境差异

| 配置项 | Development | Production |
|--------|-------------|------------|
| `level` | debug | warning |
| `format` | text | json |
| `sampling.enabled` | false | true |
| `async.enabled` | false | true |

## 依赖关系

| 组件 | 用途 |
|------|------|
| `schema/logging.schema.json` | 配置格式校验 |
| `environment/` | 环境配置覆盖 |
| PyYAML | YAML 配置解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
