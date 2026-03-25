Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 技术文档中心

**版本**: v1.0.0.6
**最后更新**: 2026-03-23
**状态**: 生产就绪

---

## 1. 概述

`partdocs/` 是 AgentOS 的技术文档中心，涵盖架构设计、API 参考、开发指南、设计哲学和技术规范五大模块。所有文档遵循统一的版权声明和版本管理规范。

---

## 2. 文档结构

```
partdocs/
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
│   │   ├── architectural_design_principles.md  # 架构设计原则 v3.0
│   │   ├── coreloopthree.md           # 三层认知运行时
│   │   ├── memoryrovol.md             # 四层记忆系统
│   │   ├── microkernel.md             # 微内核设计
│   │   ├── ipc.md                     # IPC Binder 通信
│   │   ├── syscall.md                 # 系统调用设计
│   │   └── logging_system.md          # 统一日志系统
│   └── diagrams/                      # 架构图
│
├── guides/                            # 开发指南
│   ├── README.md                      # 指南总览
│   └── folder/
│       ├── getting_started.md         # 快速开始
│       ├── create_agent.md            # 创建 Agent
│       ├── create_skill.md            # 创建 Skill
│       ├── deployment.md              # 部署指南
│       ├── kernel_tuning.md           # 内核调优
│       ├── troubleshooting.md         # 故障排查
│       └── migration_guide.md         # 迁移指南
│
├── philosophy/                        # 设计哲学
│   ├── README.md                      # 哲学总览
│   └── folder/
│       ├── Cognition_Theory.md        # 认知理论
│       ├── Design_Principles.md       # 设计原则
│       └── Memory_Theory.md           # 记忆理论
│
├── specifications/                    # 技术规范
│   ├── README.md                      # 规范总览
│   ├── TERMINOLOGY.md                 # 统一术语表
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
2. [架构设计原则 v3.0](architecture/folder/architectural_design_principles.md)
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

## 4. 文档状态总览

| 模块 | 文档数 | 状态 | 质量评分 |
|------|--------|------|----------|
| 架构文档 (architecture/) | 7+ | 生产就绪 | A |
| 开发指南 (guides/) | 7 | 生产就绪 | A |
| API 文档 (api/) | 10+ | 生产就绪 | A |
| 技术规范 (specifications/) | 12+ | 生产就绪 | A |
| 设计哲学 (philosophy/) | 3 | 生产就绪 | A |
| 多语言 README (readme/) | 4 | 生产就绪 | A |

---

## 5. 按主题索引

| 主题 | 相关文档 |
|------|----------|
| 微内核架构 | [architectural_design_principles.md](architecture/folder/architectural_design_principles.md), [microkernel.md](architecture/folder/microkernel.md) |
| 三层认知运行时 | [coreloopthree.md](architecture/folder/coreloopthree.md), [Cognition_Theory.md](philosophy/folder/Cognition_Theory.md) |
| 四层记忆系统 | [memoryrovol.md](architecture/folder/memoryrovol.md), [Memory_Theory.md](philosophy/folder/Memory_Theory.md) |
| Agent 开发 | [create_agent.md](guides/folder/create_agent.md), [Agent 契约](specifications/agentos_contract/agent/agent_contract.md) |
| Skill 开发 | [create_skill.md](guides/folder/create_skill.md), [Skill 契约](specifications/agentos_contract/skill/skill_contract.md) |
| 安全穹顶 | [architectural_design_principles.md](architecture/folder/architectural_design_principles.md) (domes 章节) |
| 系统调用 | [syscall.md](architecture/folder/syscall.md), [syscall API](api/syscall/) |
| 性能调优 | [kernel_tuning.md](guides/folder/kernel_tuning.md), [故障排查](guides/folder/troubleshooting.md) |
| 日志与追踪 | [logging_system.md](architecture/folder/logging_system.md), [日志格式](specifications/agentos_contract/log/logging_format.md) |
| 编码规范 | [C 编码规范](specifications/coding_standard/C_coding_style_guide.md), [安全编程](specifications/coding_standard/C&Cpp-secure-coding-guide.md) |

---

## 6. 贡献指南

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
