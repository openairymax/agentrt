Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 架构文档

**版本**: Doc V1.5
**最后更新**: 2026-03-23
**状态**: 生产就绪

---

## 1. 概述

AgentOS 架构文档详细描述了系统的分层设计、核心组件及其交互关系。所有架构决策基于**工程两论**（《工程控制论》和《论系统工程》）和**双系统认知理论**（Kahneman）的理论基础。

---

## 2. 文档索引

| 文档 | 主题 | 版本 | 状态 |
|------|------|------|------|
| [架构设计原则 v3.0](folder/architectural_design_principles.md) | 四维正交原则、层次架构、术语定义 | v3.0 | 生产就绪 |
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
| 系统观 | 分层架构，层次间严格单向依赖 | [architectural_design_principles.md](folder/architectural_design_principles.md) |
| 内核观 | 最小化内核，仅 4 个原子机制 | [microkernel.md](folder/microkernel.md) |
| 认知观 | 双系统路径：System 1 快速 + System 2 深度 | [coreloopthree.md](folder/coreloopthree.md) |
| 工程观 | 控制论反馈闭环，三时间尺度监控 | [logging_system.md](folder/logging_system.md) |

---

## 5. 理论基础

- **工程两论**（《工程控制论》和《论系统工程》）：负反馈调节、自适应控制、系统稳定性、层级分解、接口协调、全局优化
- **双系统认知理论**（Kahneman）：快速直觉（S1）vs 深度推理（S2）

---

## 相关文档

- [设计哲学](../philosophy/README.md) — 认知理论、记忆理论、设计原则
- [API 参考](../api/README.md) — SDK / Syscall / Core API
- [开发指南](../guides/README.md) — 入门到精通的学习路径

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*
