Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 文档体系索引

**版本**: Doc V1.5
**最后更新**: 2026-03-24
**状态**: 生产就绪

---

## 1. 快速导航

### 新人入门

1. [快速开始](guides/folder/getting_started.md) — 环境搭建与 Hello World
2. [架构设计原则](architecture/folder/architectural_design_principles.md) — 系统设计的理论根基
3. [指南总览](guides/README.md) — 完整学习路径

### 开发者进阶

1. [三层认知运行时](architecture/folder/coreloopthree.md) — CoreLoopThree 架构
2. [四层记忆系统](architecture/folder/memoryrovol.md) — MemoryRovol 架构
3. [创建 Agent](guides/folder/create_agent.md) — Agent 生命周期与双系统路径
4. [创建 Skill](guides/folder/create_skill.md) — Skill 契约与实现模式
5. [C 编码规范](specifications/coding_standard/C_coding_style_guide.md)

### 运维部署

1. [部署指南](guides/folder/deployment.md) — 多环境部署
2. [故障排查](guides/folder/troubleshooting.md) — 分层诊断方法论
3. [内核调优](guides/folder/kernel_tuning.md) — 反馈闭环调优法
4. [迁移指南](guides/folder/migration_guide.md) — 版本升级策略

---

## 2. 文档地图

```
manuals/
│
├── 架构文档 (Architecture)
│   ├── architectural_design_principles.md   # 架构设计原则 v1.5
│   ├── coreloopthree.md                     # 三层认知运行时 v1.0.0.5
│   ├── memoryrovol.md                       # 四层记忆系统 v1.0.0.5
│   ├── microkernel.md                       # 微内核设计 v1.0.0.5
│   ├── ipc.md                               # IPC Binder 通信 v1.0.0.5
│   ├── syscall.md                           # 系统调用设计 v1.0.0.5
│   └── logging_system.md                    # 统一日志系统 v1.0.0.5
│
├── 开发指南 (Guides)
│   ├── getting_started.md                   # 快速开始 v1.0.0.5
│   ├── create_agent.md                      # Agent 开发教程 v1.0.0.5
│   ├── create_skill.md                      # Skill 开发教程 v1.0.0.5
│   ├── deployment.md                        # 部署指南 v1.0.0.5
│   ├── kernel_tuning.md                     # 内核调优 v1.0.0.5
│   ├── troubleshooting.md                   # 故障排查 v1.0.0.5
│   └── migration_guide.md                   # 迁移指南 v1.0.0.5
│
├── 设计哲学 (Philosophy)
│   ├── Cognition_Theory.md                 # 认知理论 v1.0
│   ├── Design_Principles.md                 # 设计原则 v1.0
│   └── Memory_Theory.md                    # 记忆理论 v1.0
│
├── 技术规范 (Specifications)
│   ├── TERMINOLOGY.md                       # 统一术语表 v1.0
│   ├── agentos_contract/                    # 契约规范集
│   │   ├── agent/                           # Agent 契约
│   │   ├── skill/                           # Skill 契约
│   │   ├── protocol/                        # 通信协议
│   │   ├── syscall/                         # 系统调用契约
│   │   └── log/                             # 日志格式
│   ├── coding_standard/                     # 编码规范
│   └── project_erp/                         # 项目 ERP
│
├── API 参考 (API Reference)
│   ├── syscall/                             # 系统调用 API
│   ├── python/                              # Python SDK
│   ├── rust/                                # Rust SDK
│   └── go/                                  # Go SDK
│
└── 多语言 README (Readme)
    ├── README.md                            # 中文
    ├── en/README.md                         # English
    ├── de/README.md                         # Deutsch
    └── fr/README.md                         # Francais
```

---

## 3. 文档状态矩阵

### 3.1 架构文档

| 文档 | 路径 | 状态 | 版本 |
|------|------|------|------|
| 架构设计原则 | architecture/folder/architectural_design_principles.md | 正式发布 | v1.5 |
| 三层认知运行时 | architecture/folder/coreloopthree.md | 生产就绪 | v1.0.0.5 |
| 四层记忆系统 | architecture/folder/memoryrovol.md | 生产就绪 | v1.0.0.5 |
| 微内核设计 | architecture/folder/microkernel.md | 生产就绪 | v1.0.0.5 |
| IPC 通信 | architecture/folder/ipc.md | 生产就绪 | v1.0.0.5 |
| 系统调用设计 | architecture/folder/syscall.md | 生产就绪 | v1.0.0.5 |
| 日志系统 | architecture/folder/logging_system.md | 生产就绪 | v1.0.0.5 |

### 3.2 开发指南

| 文档 | 路径 | 状态 | 版本 |
|------|------|------|------|
| 快速开始 | guides/folder/getting_started.md | 生产就绪 | v1.0.0.5 |
| 创建 Agent | guides/folder/create_agent.md | 生产就绪 | v1.0.0.5 |
| 创建 Skill | guides/folder/create_skill.md | 生产就绪 | v1.0.0.5 |
| 部署指南 | guides/folder/deployment.md | 生产就绪 | v1.0.0.5 |
| 内核调优 | guides/folder/kernel_tuning.md | 生产就绪 | v1.0.0.5 |
| 故障排查 | guides/folder/troubleshooting.md | 生产就绪 | v1.0.0.5 |
| 迁移指南 | guides/folder/migration_guide.md | 生产就绪 | v1.0.0.5 |

### 3.3 设计哲学

| 文档 | 路径 | 状态 | 版本 |
|------|------|------|------|
| 认知理论 | philosophy/folder/Cognition_Theory.md | 生产就绪 | v1.0 |
| 设计原则 | philosophy/folder/Design_Principles.md | 生产就绪 | v1.0 |
| 记忆理论 | philosophy/folder/Memory_Theory.md | 生产就绪 | v1.0 |

### 3.4 API 文档

| 文档 | 路径 | 状态 |
|------|------|------|
| API 总览 | api/README.md | 生产就绪 |
| 任务 API | api/syscall/task_api.md | 生产就绪 |
| 记忆 API | api/syscall/memory_api.md | 生产就绪 |
| 会话 API | api/syscall/session_api.md | 生产就绪 |
| 遥测 API | api/syscall/telemetry_api.md | 生产就绪 |
| Python SDK | api/python/agentos.md | 生产就绪 |
| Rust SDK | api/rust/agentos_rs.md | 生产就绪 |
| Go SDK | api/go/agentos_go.md | 生产就绪 |

### 3.5 技术规范

| 文档 | 路径 | 状态 |
|------|------|------|
| 规范总览 | specifications/README.md | 生产就绪 |
| 术语表 | specifications/TERMINOLOGY.md | 生产就绪 |
| Agent 契约 | specifications/agentos_contract/agent/agent_contract.md | 生产就绪 |
| Skill 契约 | specifications/agentos_contract/skill/skill_contract.md | 生产就绪 |
| 通信协议 | specifications/agentos_contract/protocol/protocol_contract.md | 生产就绪 |
| 系统调用契约 | specifications/agentos_contract/syscall/syscall_api_contract.md | 生产就绪 |
| 日志格式 | specifications/agentos_contract/log/logging_format.md | 生产就绪 |
| C 编码规范 | specifications/coding_standard/C_coding_style_guide.md | 生产就绪 |
| 安全编程 | specifications/coding_standard/C&Cpp-secure-coding-guide.md | 生产就绪 |
| Python 规范 | specifications/coding_standard/Python_coding_style_guide.md | 生产就绪 |
| JS 规范 | specifications/coding_standard/JavaScript_coding_style_guide.md | 生产就绪 |
| 错误码参考 | specifications/project_erp/ErrorCodeReference.md | 生产就绪 |
| 资源管理表 | specifications/project_erp/ResourceManagementTable.md | 生产就绪 |
| SBOM | specifications/project_erp/SBOM.md | 生产就绪 |

### 3.6 多语言 README

| 语言 | 路径 | 状态 |
|------|------|------|
| 中文 | readme/README.md | 生产就绪 |
| English | readme/en/README.md | 生产就绪 |
| Deutsch | readme/de/README.md | 生产就绪 |
| Francais | readme/fr/README.md | 生产就绪 |

---

## 4. 学习路径

### 架构师路径

```
architectural_design_principles.md (架构设计原则 v1.5)
    → Cognition_Theory.md (认知理论)
    → Design_Principles.md (设计原则)
    → Memory_Theory.md (记忆理论)
    → coreloopthree.md (三层认知运行时)
    → memoryrovol.md (四层记忆系统)
    → agentos_contract/ (契约规范集)
```

### 核心开发者路径

```
getting_started.md (快速开始)
    → C_coding_style_guide.md (C 编码规范)
    → microkernel.md (微内核设计)
    → syscall.md (系统调用设计)
    → ipc.md (IPC 通信)
    → create_agent.md (创建 Agent)
    → create_skill.md (创建 Skill)
    → kernel_tuning.md (内核调优)
```

### 应用开发者路径

```
getting_started.md (快速开始)
    → create_agent.md (创建 Agent)
    → create_skill.md (创建 Skill)
    → deployment.md (部署指南)
    → troubleshooting.md (故障排查)
```

### 运维工程师路径

```
deployment.md (部署指南)
    → troubleshooting.md (故障排查)
    → kernel_tuning.md (内核调优)
    → migration_guide.md (迁移指南)
    → logging_system.md (日志系统)
```

---

## 5. 主题索引

| 主题 | 文档 |
|------|------|
| 微内核 | architectural_design_principles.md (架构设计原则), microkernel.md (微内核设计) |
| IPC 通信 | ipc.md (IPC Binder 通信), syscall.md (系统调用设计) |
| 认知运行时 | coreloopthree.md (三层认知运行时), Cognition_Theory.md (认知理论) |
| 记忆系统 | memoryrovol.md (四层记忆系统), Memory_Theory.md (记忆理论) |
| 安全穹顶 | architectural_design_principles.md (架构设计原则 - cupolas 章节) |
| Agent 开发 | create_agent.md (创建 Agent), agent_contract.md (Agent 契约) |
| Skill 开发 | create_skill.md (创建 Skill), skill_contract.md (Skill 契约) |
| 部署 | deployment.md (部署指南) |
| 调优 | kernel_tuning.md (内核调优), troubleshooting.md (故障排查) |
| 日志 | logging_system.md (日志系统), logging_format.md (日志格式) |
| 编码规范 | C_coding_style_guide.md (C 编码规范), C&Cpp-secure-coding-guide.md (安全编程) |
| 术语 | TERMINOLOGY.md (统一术语表) |

---

## 6. 核心技术特性

### 6.1 四维正交原则体系

AgentOS 架构设计基于四维正交原则体系，从系统观、内核观、认知观、工程观四个维度定义设计原则：

| 维度 | 原则数量 | 核心理念 |
|------|----------|----------|
| 系统观 (S) | S-1 ~ S-4 | 整体性、层次性、协调性、演化性 |
| 内核观 (K) | K-1 ~ K-4 | 最小化、正交性、安全性、可验证性 |
| 认知观 (C) | C-1 ~ C-4 | 双系统协同、渐进式规划、自我纠错、持续学习 |
| 工程观 (E) | E-1 ~ E-7 | 模块化、可测试性、可观测性、文档化等 |

详细说明请参阅 [架构设计原则 v1.5](architecture/folder/architectural_design_principles.md)。

### 6.2 三层认知运行时 (CoreLoopThree)

CoreLoopThree 是 AgentOS 的核心创新架构，实现认知、行动和记忆的有机统一：

- **认知层**: 意图理解、任务规划、Agent 调度
- **行动层**: 任务执行、补偿事务、责任链追踪
- **记忆层**: 记忆写入、查询检索、上下文挂载

### 6.3 四层记忆系统 (MemoryRovol)

MemoryRovol 实现从原始数据到高级模式的全栈记忆管理：

- **L1 原始卷**: 原始事件流、文件系统存储
- **L2 特征层**: 嵌入模型、FAISS 向量索引
- **L3 结构层**: 关系绑定、时序编码
- **L4 模式层**: 持久同调分析、HDBSCAN 聚类

### 6.4 安全穹顶 (cupolas)

cupolas 是 AgentOS 的多层安全防护体系：

- **虚拟工位**: 进程/容器级隔离
- **权限裁决**: 基于 YAML 的动态规则
- **输入净化**: 正则表达式规则过滤
- **审计日志**: 异步写入、轮转支持

---

## 7. 理论根基

AgentOS 的设计深受以下理论影响：

### 7.1 工程两论

- **《工程控制论》**: 反馈闭环理论，控制的核心
- **《论系统工程》**: 层次分解、综合集成、总体设计部

### 7.2 认知科学

- **双系统认知理论** (丹尼尔·卡尼曼): System 1 与 System 2 的协同
- **ACT-R 认知架构**: 模块划分、产生式系统
- **SOAR 认知架构**: 问题空间假设

### 7.3 计算机科学

- **Liedtke 微内核原则**: 机制与策略分离、最小特权
- **seL4 形式化验证**: 功能正确性、安全性质

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*