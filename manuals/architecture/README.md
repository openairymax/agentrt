Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 架构文档

**版本**: Doc V1.7
**最后更新**: 2026-03-31
**状态**: 生产就绪
**作者**: LirenWang

---

## 1. 概述

AgentOS 架构文档详细描述了系统的分层设计、核心组件及其交互关系。所有架构决策基于**工程两论**（《工程控制论》和《论系统工程》）和**双系统认知理论**（Kahneman）的理论基础。

---

## 2. 文档索引

| 文档 | 主题 | 版本 | 状态 |
|------|------|------|------|
| [架构设计原则 V1.7](ARCHITECTURAL_PRINCIPLES.md) | 五维正交原则、层次架构、术语定义 | V1.7 | 生产就绪 |
| [三层认知运行时](folder/coreloopthree.md) | CoreLoopThree 认知-规划-调度-执行循环 | v1.0 | 生产就绪 |
| [四层记忆系统](folder/memoryrovol.md) | MemoryRovol L1-L4 渐进抽象 | v1.0 | 生产就绪 |
| [微内核设计](folder/microkernel.md) | corekern IPC/Mem/Task/Time 原子机制 | v1.0 | 生产就绪 |
| [IPC 通信](folder/ipc.md) | Binder 驱动的进程间通信 | v1.0 | 生产就绪 |
| [系统调用接口](folder/syscall.md) | syscall 统一内核访问入口 | v1.0 | 生产就绪 |
| [日志系统](folder/logging_system.md) | 统一日志、trace_id 全链路追踪 | v1.0 | 生产就绪 |

---

## 3. 层次架构

```
┌─────────────────────────────────────────────┐
│              daemon/ 用户态服务                │
│     llm_d · market_d · monit_d · tool_d     │
├─────────────────────────────────────────────┤
│             gateway/ 网关层                  │
├─────────────────────────────────────────────┤
│          syscall/ 系统调用接口                │
├─────────────────────────────────────────────┤
│            cupolas/ 安全穹顶                   │
├──────────────┬──────────────────────────────┤
│ corekern/    │  coreloopthree/              │
│ 微内核       │  三层认知运行时                │
│ 7 headers    │                              │
│ 13 sources   │                              │
├──────────────┴──────────────────────────────┤
│         memoryrovol/ 四层记忆系统            │
├─────────────────────────────────────────────┤
│             utils/ 公共工具库                │
└─────────────────────────────────────────────┘
```

---

## 4. 核心设计原则

| 原则维度 | 核心理念 | 对应文档 |
|----------|----------|----------|
| 系统观 | 反馈闭环、层次分解、总体设计部、涌现性管理 | [架构设计原则](ARCHITECTURAL_PRINCIPLES.md) |
| 内核观 | 内核极简、接口契约化、服务隔离、可插拔策略 | [微内核设计](folder/microkernel.md) |
| 认知观 | 双系统分工、记忆持久化、注意力分配、偏差防护 | [三层认知运行时](folder/coreloopthree.md) |
| 工程观 | 安全内生、可观测性、资源确定性、文档即代码 | [架构设计原则](ARCHITECTURAL_PRINCIPLES.md) |
| 设计美学 | 极简主义、细节关注、人文关怀、完美主义 | [架构设计原则](ARCHITECTURAL_PRINCIPLES.md) |

---

## 5. 理论基础

- **工程两论**（《工程控制论》和《论系统工程》）：反馈闭环、层次分解、总体设计部、涌现性管理
- **双系统认知理论**（《思考，快与慢》）：System 1 快速直觉与 System 2 深度推理的协同
- **微内核哲学**：内核极简、接口契约化、服务隔离、可插拔策略
- **设计美学**（乔布斯设计哲学）：极简主义、极致细节、人文关怀、完美主义

---

## 相关文档

- [设计哲学](../philosophy/README.md) — 认知理论、记忆理论、设计原则
- [API 参考](../api/README.md) — SDK / Syscall / Core API
- [开发指南](../guides/README.md) — 入门到精通的学习路径

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*
