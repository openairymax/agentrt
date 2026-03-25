# AgentOS 微内核基础模块

**版本**: v1.0.0.6
**路径**: `atoms/corekern/`
**最后更新**: 2026-03-21

## 1. 模块概述

corekern 模块是 AgentOS 的微内核基础，提供最基本的系统服务，包括：

- **IPC (Binder)**：进程间通信机制，仿 Android Binder 设计，支持跨进程方法调用。
- **内存管理**：物理内存分配器、内存池、边界保护。
- **任务调度**：线程/任务管理、调度器（CFS/RR）、同步原语。
- **时间服务**：系统时钟、定时器、事件循环。

该模块遵循微内核设计原则，只保留必要机制，所有服务运行在用户态。模块间通过稳定接口交互，无循环依赖。

## 2. 目录结构
```
corekern/
├── README.md
├── include/
│ ├── agentos.h # 统一头文件
│ ├── ipc.h # IPC 接口
│ ├── mem.h # 内存接口
<!-- From data intelligence emerges. by spharx -->
│ ├── task.h # 任务接口
│ ├── time.h # 时间接口
│ └── error.h # 错误码
├── src/
│ ├── ipc/ # IPC 实现
│ ├── mem/ # 内存实现
│ ├── task/ # 任务实现
│ ├── time/ # 时间实现
│ └── main.c # 内核入口
└── CMakeLists.txt
```

## 3. 核心接口

所有对外接口通过 `include/agentos.h` 统一引入。系统调用层通过本模块提供的服务实现具体功能。

## 4. 设计原则

- **最小特权**：每个组件只拥有完成本职工作的最小权限。
- **清晰接口**：所有接口有明确的前置条件和后置条件。
- **错误处理**：所有可能失败的操作必须返回错误码，并提供详细错误信息。
- **可观测性**：关键路径埋点，支持 OpenTelemetry。

---

© 2026 SPHARX Ltd. All Rights Reserved.
