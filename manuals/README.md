Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 技术文档中心

**版本**: Doc V1.5
**最后更新**: 2026-03-24
**状态**: 生产就绪

---

## 1. 概述

`manuals/` 是 AgentOS 的技术文档中心，涵盖架构设计、API 参考、开发指南、设计哲学和技术规范五大模块。所有文档遵循统一的版权声明和版本管理规范。

---

## 2. 文档结构

```
manuals/
├── README.md                          # 本文件 - 文档中心总览
├── DOCSINDEX.md                       # 文档体系索引
│
├── api/                               # API 文档
│   ├── README.md                      # API 总览（三层架构）
│   ├── syscall/                       # 系统调用 API
│   │   ├── task_api.md
│   │   ├── memory_api.md
│   │   ├── session_api.md
│   │   └── telemetry_api.md
│   ├── python/                        # Python SDK
│   ├── rust/                          # Rust SDK
│   └── go/                            # Go SDK
│
├── architecture/                      # 架构设计文档
│   ├── README.md                      # 架构总览
│   ├── folder/
│   │   ├── architectural_design_principles.md  # 架构设计原则 v1.5
│   │   ├── coreloopthree.md           # 三层认知运行时 v1.0.0.5
│   │   ├── memoryrovol.md             # 四层记忆系统 v1.0.0.5
│   │   ├── microkernel.md             # 微内核设计 v1.0.0.5
│   │   ├── ipc.md                     # IPC Binder 通信 v1.0.0.5
│   │   ├── syscall.md                 # 系统调用设计 v1.0.0.5
│   │   └── logging_system.md          # 统一日志系统 v1.0.0.5
│   └── diagrams/                      # 架构图
│
├── guides/                            # 开发指南
│   ├── README.md                      # 指南总览
│   └── folder/
│       ├── getting_started.md         # 快速开始 v1.0.0.5
│       ├── create_agent.md            # 创建 Agent v1.0.0.5
│       ├── create_skill.md            # 创建 Skill v1.0.0.5
│       ├── deployment.md              # 部署指南 v1.0.0.5
│       ├── kernel_tuning.md           # 内核调优 v1.0.0.5
│       ├── troubleshooting.md         # 故障排查 v1.0.0.5
│       └── migration_guide.md         # 迁移指南 v1.0.0.5
│
├── philosophy/                        # 设计哲学
│   ├── README.md                      # 哲学总览
│   └── folder/
│       ├── Cognition_Theory.md        # 认知理论 v1.0
│       ├── Design_Principles.md       # 设计原则 v1.0
│       └── Memory_Theory.md           # 记忆理论 v1.0
│
├── specifications/                    # 技术规范
│   ├── README.md                      # 规范总览
│   ├── TERMINOLOGY.md                 # 统一术语表 v1.0
│   ├── agentos_contract/              # 契约规范集
│   │   ├── README.md
│   │   ├── agent/                     # Agent 契约
│   │   ├── skill/                     # Skill 契约
│   │   ├── protocol/                  # 通信协议
│   │   ├── syscall/                   # 系统调用契约
│   │   └── log/                       # 日志格式
│   ├── coding_standard/               # 编码规范
│   │   ├── C_coding_style_guide.md
│   │   ├── C&Cpp-secure-coding-guide.md
│   │   ├── Python_coding_style_guide.md
│   │   └── JavaScript_coding_style_guide.md
│   └── project_erp/                   # 项目 ERP
│       ├── ErrorCodeReference.md
│       ├── ResourceManagementTable.md
│       └── SBOM.md
│
└── readme/                            # 多语言 README
    ├── README.md                      # 中文 README
    ├── en/README.md                   # English
    ├── de/README.md                   # Deutsch
    └── fr/README.md                   # Francais
```

---

## 3. 学习路径

### 3.1 新人入门

```
快速开始 → 架构概览 → 创建 Agent → 部署运行
```

1. [快速开始](guides/folder/getting_started.md) — 环境搭建与首次编译
2. [架构设计原则](architecture/folder/architectural_design_principles.md) — 理解系统设计根基
3. [创建 Agent](guides/folder/create_agent.md) — 开发第一个 Agent
4. [部署指南](guides/folder/deployment.md) — 部署到生产环境

### 3.2 核心开发者

```
编码规范 → 微内核 → 三层运行时 → 四层记忆 → 系统调用 → 内核调优
```

1. [C 编码规范](specifications/coding_standard/C_coding_style_guide.md)
2. [微内核设计](architecture/folder/microkernel.md)
3. [三层运行时](architecture/folder/coreloopthree.md)
4. [四层记忆](architecture/folder/memoryrovol.md)
5. [系统调用](architecture/folder/syscall.md)
6. [内核调优](guides/folder/kernel_tuning.md)

### 3.3 架构师

```
设计哲学 → 架构原则 → 认知理论 → 记忆理论 → 契约规范
```

1. [设计哲学总览](philosophy/README.md)
2. [架构设计原则 v1.5](architecture/folder/architectural_design_principles.md)
3. [认知理论](philosophy/folder/Cognition_Theory.md)
4. [记忆理论](philosophy/folder/Memory_Theory.md)
5. [契约规范集](specifications/agentos_contract/README.md)

### 3.4 运维工程师

```
部署指南 → 故障排查 → 内核调优 → 迁移指南
```

1. [部署指南](guides/folder/deployment.md)
2. [故障排查](guides/folder/troubleshooting.md)
3. [内核调优](guides/folder/kernel_tuning.md)
4. [迁移指南](guides/folder/migration_guide.md)

---

## 4. 核心技术特性

### 4.1 四维正交原则体系

AgentOS 架构设计基于四维正交原则体系，从系统观、内核观、认知观、工程观四个维度定义设计原则：

| 维度 | 原则数量 | 核心理念 |
|------|----------|----------|
| 系统观 (S) | S-1 ~ S-4 | 整体性、层次性、协调性、演化性 |
| 内核观 (K) | K-1 ~ K-4 | 最小化、正交性、安全性、可验证性 |
| 认知观 (C) | C-1 ~ C-4 | 双系统协同、渐进式规划、自我纠错、持续学习 |
| 工程观 (E) | E-1 ~ E-7 | 模块化、可测试性、可观测性、文档化等 |

### 4.2 三层认知运行时 (CoreLoopThree)

CoreLoopThree 是 AgentOS 的核心创新架构，实现认知、行动和记忆的有机统一：

- **认知层**: 意图理解、任务规划、Agent 调度
- **行动层**: 任务执行、补偿事务、责任链追踪
- **记忆层**: 记忆写入、查询检索、上下文挂载

### 4.3 四层记忆系统 (MemoryRovol)

MemoryRovol 实现从原始数据到高级模式的全栈记忆管理：

- **L1 原始卷**: 原始事件流、文件系统存储
- **L2 特征层**: 嵌入模型、FAISS 向量索引
- **L3 结构层**: 关系绑定、时序编码
- **L4 模式层**: 持久同调分析、HDBSCAN 聚类

### 4.4 安全穹顶 (cupolas)

cupolas 是 AgentOS 的多层安全防护体系：

- **虚拟工位**: 进程/容器级隔离
- **权限裁决**: 基于 YAML 的动态规则
- **输入净化**: 正则表达式规则过滤
- **审计日志**: 异步写入、轮转支持

---

## 5. 文档状态总览

| 模块 | 文档数 | 状态 | 质量评分 |
|------|--------|------|----------|
| 架构文档 (architecture/) | 7 | 正式发布/生产就绪 | A |
| 开发指南 (guides/) | 7 | 生产就绪 | A |
| API 文档 (api/) | 10+ | 生产就绪 | A |
| 技术规范 (specifications/) | 12+ | 生产就绪 | A |
| 设计哲学 (philosophy/) | 3 | 生产就绪 | A |
| 多语言 README (readme/) | 4 | 生产就绪 | A |

---

## 6. 按主题索引

| 主题 | 相关文档 |
|------|----------|
| 微内核架构 | [architectural_design_principles.md](architecture/folder/architectural_design_principles.md), [microkernel.md](architecture/folder/microkernel.md) |
| 三层认知运行时 | [coreloopthree.md](architecture/folder/coreloopthree.md), [Cognition_Theory.md](philosophy/folder/Cognition_Theory.md) |
| 四层记忆系统 | [memoryrovol.md](architecture/folder/memoryrovol.md), [Memory_Theory.md](philosophy/folder/Memory_Theory.md) |
| Agent 开发 | [create_agent.md](guides/folder/create_agent.md), [Agent 契约](specifications/agentos_contract/agent/agent_contract.md) |
| Skill 开发 | [create_skill.md](guides/folder/create_skill.md), [Skill 契约](specifications/agentos_contract/skill/skill_contract.md) |
| 安全穹顶 | [architectural_design_principles.md](architecture/folder/architectural_design_principles.md) (cupolas 章节) |
| 系统调用 | [syscall.md](architecture/folder/syscall.md), [syscall API](api/syscall/) |
| 性能调优 | [kernel_tuning.md](guides/folder/kernel_tuning.md), [故障排查](guides/folder/troubleshooting.md) |
| 日志与追踪 | [logging_system.md](architecture/folder/logging_system.md), [日志格式](specifications/agentos_contract/log/logging_format.md) |
| 编码规范 | [C 编码规范](specifications/coding_standard/C_coding_style_guide.md), [安全编程](specifications/coding_standard/C&Cpp-secure-coding-guide.md) |

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

## 8. 贡献指南

### 文档结构标准

每份文档应包含：
- **版权声明**：`Copyright (c) 2026 SPHARX. All Rights Reserved.`
- **版本信息**：版本号、最后更新日期
- **结构化章节**：概述 → 核心内容 → 示例 → 相关文档
- **交叉引用**：链接到相关文档的精确路径

### 提交流程

1. Fork 项目
2. 创建分支：`git checkout -b docs/topic-name`
3. 编写文档，遵循上述结构标准
4. 验证链接有效性
5. 提交 PR

---

## 相关资源

- [主项目 README](../README.md)
- [文档体系索引](DOCSINDEX.md)

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*