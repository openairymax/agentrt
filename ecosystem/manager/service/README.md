# Manager Service — 服务配置

**模块路径**: `ecosystem/manager/service/`
**版本**: v0.1.0

## 概述

`manager/service/` 包含 AgentRT 各子服务的运行参数配置，每个守护进程拥有独立的配置子目录。当前包含 tool_d（工具守护进程）的服务配置，遵循 `schema/tool-service.schema.json` 规范。

## 目录结构

```
service/
└── tool_d/            # 工具守护进程服务配置
    └── tool.yaml      # tool_d 运行参数配置
```

## 核心组件

### tool_d/tool.yaml

tool_d 守护进程的运行参数配置，定义工具服务的运行时行为：

- 工具注册与发现配置
- 执行超时与资源限制
- 沙箱隔离策略
- 日志与监控配置

## 依赖关系

| 组件 | 用途 |
|------|------|
| `schema/tool-service.schema.json` | 配置格式校验 |
| tool_d 守护进程 | 配置消费者 |
| PyYAML | YAML 配置解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
