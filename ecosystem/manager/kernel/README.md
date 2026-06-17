# Manager Kernel — 内核级配置

**模块路径**: `ecosystem/manager/kernel/`
**版本**: v0.1.0

## 概述

`manager/kernel/` 包含 AgentRT 内核级配置文件，定义内核核心参数和运行时设置。配置覆盖调度器策略、内存管理、IPC 通信、定时器、错误处理、热更新和性能监控等子系统，遵循 `schema/kernel-settings.schema.json` 规范。按双重责任模型，内容定义责任归属 corekern 模块，管理责任归属 Manager 模块。

## 目录结构

```
kernel/
├── kernel.yaml       # 内核核心配置（compose kernel 服务使用）
└── settings.yaml     # 内核运行时设置（完整配置，含详细注释）
```

## 核心组件

### kernel.yaml

内核核心配置文件，供 compose 中的 kernel 服务使用，包含：

| 配置域 | 关键参数 |
|--------|---------|
| 调度器 | policy=weighted, time_slice_ms=10, max_concurrency=100 |
| 内存 | pool_size_mb=1024, guard_enabled=true, oom_policy=reject |
| IPC | max_connections=1024, encryption=tls13, auth_required=true |
| 定时器 | precision_ms=1, max_timers=10000 |
| 错误处理 | verbose=false, retention_days=30 |
| 热更新 | enabled=true |
| 监控 | enabled=true, metrics_interval_sec=10 |

### settings.yaml

内核运行时完整配置，包含详细注释和扩展设置，覆盖以下子系统：

| 子系统 | 说明 |
|--------|------|
| **调度器** | 策略选择、优先级权重、防饥饿、任务队列配置 |
| **Thinkdual 认知双思系统** | System1（快速直觉）/ System2（慢速推理）任务路由 |
| **内存管理** | 池配置（small/medium/large）、泄漏检测、NUMA 感知 |
| **IPC** | Binder 连接、零拷贝、背压控制、连接池 |
| **定时器** | 时间轮、精度优化（coalescing） |
| **错误处理** | 标准错误码体系（0x0000-0x7FFF）、自动恢复 |
| **热更新** | 支持热更新的配置项白名单 |
| **性能监控** | 指标收集、告警阈值 |
| **资源限制** | 文件描述符、线程数、CPU 时间 |

#### 错误码体系

| 范围 | 分类 |
|------|------|
| 0x0000-0x0FFF | 通用错误 |
| 0x1000-0x1FFF | 核心循环错误 |
| 0x2000-0x2FFF | 认知层错误 |
| 0x3000-0x3FFF | 执行层错误 |
| 0x4000-0x4FFF | 记忆层错误 |
| 0x5000-0x5FFF | 系统调用错误 |
| 0x6000-0x6FFF | 安全域错误 |
| 0x7000-0x7FFF | 动态模块错误 |

## 依赖关系

| 组件 | 用途 |
|------|------|
| `schema/kernel-settings.schema.json` | 配置格式校验 |
| corekern 模块 | 内容定义责任方 |
| PyYAML | YAML 配置解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
