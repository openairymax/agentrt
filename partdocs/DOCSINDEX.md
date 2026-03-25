Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 文档体系索引

**版本**: v1.0.0.6
**最后更新**: 2026-03-23
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
partdocs/
│
├── 架构文档 (Architecture)
│   ├── architectural_design_principles.md   # 架构设计原则 v3.0
│   ├── coreloopthree.md                     # 三层认知运行时
│   ├── memoryrovol.md                       # 四层记忆系统
│   ├── microkernel.md                       # 微内核设计
│   ├── ipc.md                               # IPC Binder 通信
│   ├── syscall.md                           # 系统调用设计
│   └── logging_system.md                    # 统一日志系统
│
├── 开发指南 (Guides)
│   ├── getting_started.md                   # 快速开始
│   ├── create_agent.md                      # Agent 开发教程
│   ├── create_skill.md                      # Skill 开发教程
│   ├── deployment.md                        # 部署指南
│   ├── kernel_tuning.md                     # 内核调优
│   ├── troubleshooting.md                   # 故障排查
│   └── migration_guide.md                   # 迁移指南
│
├── 设计哲学 (Philosophy)
│   ├── Cognition_Theory.md                  # 认知理论
│   ├── Design_Principles.md                 # 设计原则
│   └── Memory_Theory.md                     # 记忆理论
│
├── 技术规范 (Specifications)
│   ├── TERMINOLOGY.md                       # 统一术语表
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
| 架构设计原则 | architecture/folder/architectural_design_principles.md | 生产就绪 | v3.0 |
| 三层认知运行时 | architecture/folder/coreloopthree.md | 生产就绪 | v1.0 |
| 四层记忆系统 | architecture/folder/memoryrovol.md | 生产就绪 | v1.0 |
| 微内核设计 | architecture/folder/microkernel.md | 生产就绪 | v1.0 |
| IPC 通信 | architecture/folder/ipc.md | 生产就绪 | v1.0 |
| 系统调用设计 | architecture/folder/syscall.md | 生产就绪 | v1.0 |
| 日志系统 | architecture/folder/logging_system.md | 生产就绪 | v1.0 |

### 3.2 开发指南

| 文档 | 路径 | 状态 | 版本 |
|------|------|------|------|
| 快速开始 | guides/folder/getting_started.md | 生产就绪 | v1.0 |
| 创建 Agent | guides/folder/create_agent.md | 生产就绪 | v1.0 |
| 创建 Skill | guides/folder/create_skill.md | 生产就绪 | v1.0 |
| 部署指南 | guides/folder/deployment.md | 生产就绪 | v1.0 |
| 内核调优 | guides/folder/kernel_tuning.md | 生产就绪 | v1.0 |
| 故障排查 | guides/folder/troubleshooting.md | 生产就绪 | v1.0 |
| 迁移指南 | guides/folder/migration_guide.md | 生产就绪 | v1.0 |

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
architectural_design_principles.md
    → Cognition_Theory.md
    → Design_Principles.md
    → Memory_Theory.md
    → coreloopthree.md
    → memoryrovol.md
    → agentos_contract/
```

### 核心开发者路径

```
getting_started.md
    → C_coding_style_guide.md
    → microkernel.md
    → syscall.md
    → ipc.md
    → create_agent.md
    → create_skill.md
    → kernel_tuning.md
```

### 应用开发者路径

```
getting_started.md
    → create_agent.md
    → create_skill.md
    → deployment.md
    → troubleshooting.md
```

### 运维工程师路径

```
deployment.md
    → troubleshooting.md
    → kernel_tuning.md
    → migration_guide.md
    → logging_system.md
```

---

## 5. 主题索引

| 主题 | 文档 |
|------|------|
| 微内核 | architectural_design_principles.md, microkernel.md |
| IPC 通信 | ipc.md, syscall.md |
| 认知运行时 | coreloopthree.md, Cognition_Theory.md |
| 记忆系统 | memoryrovol.md, Memory_Theory.md |
| 安全穹顶 | architectural_design_principles.md (domes) |
| Agent 开发 | create_agent.md, agent_contract.md |
| Skill 开发 | create_skill.md, skill_contract.md |
| 部署 | deployment.md |
| 调优 | kernel_tuning.md, troubleshooting.md |
| 日志 | logging_system.md, logging_format.md |
| 编码规范 | C_coding_style_guide.md, C&Cpp-secure-coding-guide.md |
| 术语 | TERMINOLOGY.md |

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*
