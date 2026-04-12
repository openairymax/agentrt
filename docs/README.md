Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 文档中心

**版本**: Doc V1.9
**最后更新**: 2026-04-13
**维护者**: Team
  - Zhixian Zhou | Spharx Ltd. zhouzhixian@spharx.cn
  - Liren Wang | Spharx Ltd. wangliren@spharx.cn
  - Chen Zhang | SJTU CSC Lab. yoyoke@sjtu.edu.cn
  - Yunwen Xu | SJTU CSC Lab. willing419@sjtu.edu.cn
  - Daxiang Zhu | IndieBros. zdxever@sina.com

---

## 📚 文档导航

欢迎使用AgentOS智能体操作系统文档中心。本文档采用分层组织结构，从入门到精通，满足不同层次的需求。

### 🎯 快速入口

| 角色 | 推荐阅读路径 | 预计时间 |
|------|------------|---------|
| **初学者** | 快速开始 → 安装指南 → 配置指南 | 30分钟 |
| **开发者** | API参考 → 编码规范 → 创建Agent/Skill | 2小时 |
| **运维工程师** | 部署指南 → 监控运维 → 故障排查 | 1.5小时 |
| **架构师** | 设计原则 → 微内核架构 → 核心循环三层 | 3小时 |

---

## 📖 文档分类

### 1️⃣ 基础理论 (Basic Theories)

AgentOS的理论基石，理解设计哲学的必读材料：

- [**体系并行论 (MCIS)**](Basic_Theories/CN_01_体系并行论.md) — 多体控制智能系统理论（[English](Basic_Theories/EN_01_MCIS.md)）
- [**认知层理论**](Basic_Theories/CN_02_认知层理论.md) — 双系统认知与三层架构（[English](Basic_Theories/EN_02_Cognition_Theory.md)）
- [**记忆层理论**](Basic_Theories/CN_03_记忆层理论.md) — 四层记忆卷载模型（[English](Basic_Theories/EN_03_Memory_Theory.md)）
- [**设计原则**](Basic_Theories/CN_04_设计原则.md) — 五维正交设计原则导论（[English](Basic_Theories/EN_04_Design_Principles.md)）
- [**架构设计原则完整版**](ARCHITECTURAL_PRINCIPLES.md) — S/K/C/E/A五维24条原则详述

---

### 2️⃣ 入门指南 (Guides)

面向新用户的引导式教程，帮助快速上手：

- [**快速开始**](Capital_Guides/getting_started.md) — 5分钟体验AgentOS核心功能
- [**安装指南**](Capital_Guides/installation.md) — 详细的环境搭建步骤
- [**配置指南**](Capital_Guides/configuration.md) — 完整的配置选项说明
- [**部署指南**](Capital_Guides/deployment.md) — 生产环境部署最佳实践
- [**创建Agent**](Capital_Guides/create_agent.md) — Agent开发完整流程
- [**创建Skill**](Capital_Guides/create_skill.md) — Skill开发完整流程
- [**迁移指南**](Capital_Guides/migration_guide.md) — 版本升级与数据迁移

---

### 3️⃣ 架构设计 (Architecture)

深入理解AgentOS的设计哲学和技术实现：

- [**微内核架构**](Capital_Architecture/microkernel.md) — K-1~K-4 原则的实现
- [**核心循环三层**](Capital_Architecture/coreloopthree.md) — Cognition→Execution→Memory
- [**记忆卷载系统**](Capital_Architecture/memoryrovol.md) — L1→L2→L3→L4 四层记忆
- [**IPC通信机制**](Capital_Architecture/ipc.md) — Binder/Channel/Buffer 进程间通信
- [**系统调用架构**](Capital_Architecture/syscall.md) — 用户态与内核态的统一接口
- [**日志系统架构**](Capital_Architecture/logging_system.md) — 跨语言可观测性与动态反馈调节

---

### 4️⃣ API 参考 (API Reference)

完整的接口文档，包含请求示例和响应格式：

**系统调用 API**：

- [**API总览**](Capital_API/README.md) — API层次结构与设计哲学
- [**任务管理 API**](Capital_API/syscalls/task.md) — submit/query/wait/cancel 任务全生命周期
- [**记忆管理 API**](Capital_API/syscalls/memory.md) — write/query/evolve/forget 四层记忆操作
- [**会话管理 API**](Capital_API/syscalls/session.md) — create/get/close/list 会话管理
- [**可观测性 API**](Capital_API/syscalls/telemetry.md) — metrics/traces 遥测接口
- [**Agent管理 API**](Capital_API/syscalls/agent.md) — spawn/terminate/invoke Agent管理

**多语言 SDK**：

- [**Python SDK**](Capital_API/toolkit/python/README.md) — Python语言绑定API
- [**Go SDK**](Capital_API/toolkit/go/README.md) — Go语言绑定API
- [**Rust SDK**](Capital_API/toolkit/rust/README.md) — Rust语言绑定API
- [**TypeScript SDK**](Capital_API/toolkit/typescript/README.md) — TypeScript语言绑定API

**核心算法**：

- [**算法实现文档**](Capital_API/algorithms/README.md) — 文档处理/搜索索引/质量验证/性能优化核心算法

---

### 5️⃣ 开发者指南 (Development)

参与AgentOS开发的必备知识：

**贡献与测试**：

- [**贡献指南**](../CONTRIBUTING.md) — 提交PR的完整流程
- [**测试指南**](Capital_Guides/testing.md) — 单元测试、集成测试、E2E测试

**编码规范**：

- [**C编码风格规范**](Capital_Specifications/coding_standard/C_coding_style_standard.md) — C语言命名/函数/错误处理/并发/安全规范
- [**C++编码风格规范**](Capital_Specifications/coding_standard/Cpp_coding_style_standard.md) — C++语言编码规范
- [**Python编码风格规范**](Capital_Specifications/coding_standard/Python_coding_style_standard.md) — Python类型设计/异步/错误处理规范
- [**JavaScript编码风格规范**](Capital_Specifications/coding_standard/JavaScript_coding_style_standard.md) — JavaScript/TypeScript编码规范
- [**代码注释模板**](Capital_Specifications/coding_standard/Code_comment_template.md) — Doxygen/docstring注释规范
- [**日志打印规范**](Capital_Specifications/coding_standard/Log_standard.md) — 日志级别/内容/格式/质量规范

**安全编码**：

- [**安全设计规范**](Capital_Specifications/coding_standard/Security_design_standard.md) — D1~D4四层防护/加密/认证/隐私保护
- [**C/C++安全编码规范**](Capital_Specifications/coding_standard/C_Cpp_secure_coding_standard.md) — C/C++安全编码实践
- [**Java安全编码规范**](Capital_Specifications/coding_standard/Java_secure_coding_standard.md) — Java安全编码实践

---

### 6️⃣ 运维手册 (Operations)

生产环境的运维保障：

- [**Docker部署**](../docker/README.md) — 容器化部署完整方案
- [**Kubernetes部署**](Capital_Guides/kubernetes-deployment.md) — K8s集群编排
- [**监控运维**](Capital_Guides/monitoring.md) — Prometheus+Grafana监控栈
- [**备份恢复**](Capital_Guides/backup-recovery.md) — 数据备份与灾难恢复
- [**内核调优**](Capital_Guides/kernel_tuning.md) — 内核参数调优与性能优化
- [**模块功能说明**](Capital_Guides/module_features.md) — manuals模块功能与配置

---

### 7️⃣ 故障排查 (Troubleshooting)

常见问题及解决方案：

- [**常见问题FAQ**](Capital_Guides/common-issues.md) — Top 20高频问题
- [**错误诊断**](Capital_Guides/diagnosis.md) — 日志分析与问题定位
- [**已知问题**](Capital_Guides/known-issues.md) — 已知Bug及临时解决方案

---

### 8️⃣ 规范与契约 (Specifications)

AgentOS项目的标准化规范体系：

**契约规范**：

- [**契约规范总览**](Capital_Specifications/agentos_contract/glossary_index.md) — 术语表与快速索引
- [**Agent契约**](Capital_Specifications/agentos_contract/agent_contract.md) — Agent能力描述规范（[Schema](Capital_Specifications/agentos_contract/agent_contract_schema.json)）
- [**Skill契约**](Capital_Specifications/agentos_contract/skill_contract.md) — Skill技能描述规范（[Schema](Capital_Specifications/agentos_contract/skill_contract_schema.json)）
- [**通信协议规范**](Capital_Specifications/agentos_contract/protocol_contract.md) — HTTP/WS/Stdio网关+JSON-RPC 2.0
- [**系统调用API规范**](Capital_Specifications/agentos_contract/syscall_api_contract.md) — 系统调用接口契约
- [**日志格式规范**](Capital_Specifications/agentos_contract/logging_format.md) — 结构化JSON日志格式

**集成标准**：

- [**集成标准总览**](Capital_Specifications/integration_standards/README.md) — 模块间集成标准索引
- [**Manager配置集成标准**](Capital_Specifications/integration_standards/INTEGRATION_STANDARD.md) — Manager模块与统一配置库集成

**项目管理**：

- [**错误码参考**](Capital_Specifications/project_erp/error_code_reference.md) — 完整的错误码定义和处理建议
- [**资源管理表**](Capital_Specifications/project_erp/resource_management_table.md) — 资源创建/释放/所有权规范
- [**软件物料清单 (SBOM)**](Capital_Specifications/project_erp/SBOM.md) — 组件/依赖/许可证/安全信息
- [**模块功能需求**](Capital_Specifications/project_erp/manuals_module_requirements.md) — manuals模块需求与技术规范

**术语**：

- [**统一术语表**](Capital_Specifications/TERMINOLOGY.md) — AgentOS统一术语定义

---

### 9️⃣ 白皮书与模板 (White Paper & Templates)

- [**技术白皮书**](White_Paper/README.md) — AgentOS官方技术白皮书（中/英）
- [**文档模板**](Quote_Templates/_template.md) — 通用文档模板
- [**API文档模板**](Quote_Templates/_template_api.md) — API文档编写模板
- [**指南文档模板**](Quote_Templates/_template_guide.md) — 指南文档编写模板

---

### 🔟 参考资料 (References)

补充材料和外部链接：

- [**统一术语表**](Capital_Specifications/TERMINOLOGY.md) — 统一术语定义
- [**变更日志**](../CHANGELOG.md) — 版本更新历史
- [**许可证**](../LICENSE) — Apache-2.0 许可证全文

---

## 🔍 文档使用技巧

### 搜索功能

使用 `Ctrl+F` 或 `Cmd+F` 在当前页面搜索关键词。

跨页面搜索可以使用以下命令：

```bash
# 在所有Markdown文件中搜索"IPC"
grep -r "IPC" docs/ --include="*.md"
```

### 文档反馈

发现文档错误或有改进建议？

1. 在对应文档页面点击右上角的 **编辑此页** 按钮
2. 直接修改并提交PR
3. 或者在 [GitCode Issues](https://gitcode.com/spharx/agentos/issues) 反馈

### 版本选择

本文档始终与最新稳定版代码同步。如需查看历史版本文档：

```bash
# 切换到指定版本的文档
git checkout v1.0.0 -- docs/
```

---

## 📊 文档统计

| 类别 | 文档数量 | 总字数(估) | 最后更新 |
|------|---------|-----------|---------|
| 基础理论 | 9篇 | ~35,000字 | 2026-04-09 |
| 入门指南 | 7篇 | ~25,000字 | 2026-04-09 |
| 架构设计 | 6篇 | ~40,000字 | 2026-04-09 |
| API参考 | 11篇 | ~45,000字 | 2026-04-09 |
| 开发者指南 | 10篇 | ~40,000字 | 2026-04-09 |
| 运维手册 | 6篇 | ~30,000字 | 2026-04-09 |
| 故障排查 | 3篇 | ~12,000字 | 2026-04-09 |
| 规范与契约 | 14篇 | ~55,000字 | 2026-04-09 |
| 白皮书与模板 | 4篇 | ~15,000字 | 2026-04-09 |
| 参考资料 | 3篇 | ~8,000字 | 2026-04-09 |

**总计**: 73篇文档，约305,000字

---

## 📂 文档目录结构

```
docs/
├── ARCHITECTURAL_PRINCIPLES.md    # 五维正交设计原则完整版
├── README.md                      # 本文档
├── Basic_Theories/                # 基础理论（中/英双语）
│   ├── CN_01_体系并行论.md
│   ├── CN_02_认知层理论.md
│   ├── CN_03_记忆层理论.md
│   ├── CN_04_设计原则.md
│   ├── EN_01_MCIS.md
│   ├── EN_02_Cognition_Theory.md
│   ├── EN_03_Memory_Theory.md
│   └── EN_04_Design_Principles.md
├── Capital_Architecture/          # 架构设计
│   ├── microkernel.md
│   ├── coreloopthree.md
│   ├── memoryrovol.md
│   ├── ipc.md
│   ├── syscall.md
│   ├── logging_system.md
│   └── diagrams/                 # 架构图（drawio）
├── Capital_API/                   # API参考
│   ├── README.md
│   ├── syscalls/                 # 系统调用API
│   │   ├── task.md
│   │   ├── memory.md
│   │   ├── session.md
│   │   ├── telemetry.md
│   │   └── agent.md
│   ├── toolkit/                  # 多语言SDK
│   │   ├── python/README.md
│   │   ├── go/README.md
│   │   ├── rust/README.md
│   │   └── typescript/README.md
│   └── algorithms/               # 核心算法
│       └── README.md
├── Capital_Guides/                # 入门与运维指南
│   ├── getting_started.md         # 快速开始
│   ├── installation.md            # 安装指南
│   ├── configuration.md           # 配置指南
│   ├── deployment.md              # 部署指南
│   ├── create_agent.md           # 创建Agent
│   ├── create_skill.md           # 创建Skill
│   ├── migration_guide.md        # 迁移指南
│   ├── testing.md               # 测试指南
│   ├── monitoring.md            # 监控运维
│   ├── kubernetes-deployment.md # K8s部署
│   ├── backup-recovery.md       # 备份恢复
│   ├── kernel_tuning.md         # 内核调优
│   ├── performance-tuning.md    # 性能调优
│   ├── security-hardening.md    # 安全加固
│   ├── module_features.md       # 模块功能
│   ├── common-issues.md         # 常见问题
│   ├── diagnosis.md             # 故障诊断
│   └── known-issues.md          # 已知问题
├── Capital_Specifications/        # 规范与契约
│   ├── TERMINOLOGY.md
│   ├── README.md
│   ├── agentos_contract/         # 契约规范
│   │   ├── agent_contract.md
│   │   ├── agent_contract_schema.json
│   │   ├── skill_contract.md
│   │   ├── skill_contract_schema.json
│   │   ├── protocol_contract.md
│   │   ├── syscall_api_contract.md
│   │   ├── logging_format.md
│   │   └── glossary_index.md
│   ├── coding_standard/          # 编码规范
│   │   ├── C_coding_style_standard.md
│   │   ├── Cpp_coding_style_standard.md
│   │   ├── Python_coding_style_standard.md
│   │   ├── JavaScript_coding_style_standard.md
│   │   ├── C_Cpp_secure_coding_standard.md
│   │   ├── Java_secure_coding_standard.md
│   │   ├── Security_design_standard.md
│   │   ├── Log_standard.md
│   │   └── Code_comment_template.md
│   ├── integration_standards/    # 集成标准
│   │   ├── README.md
│   │   └── INTEGRATION_STANDARD.md
│   └── project_erp/             # 项目管理
│       ├── SBOM.md
│       ├── error_code_reference.md
│       ├── manuals_module_requirements.md
│       └── resource_management_table.md
├── Source_Other/                 # 其他资源
│   └── AgentOS-desktop-preview.gif
├── White_Paper/                  # 白皮书
│   ├── README.md
│   └── history/
└── Quote_Templates/              # 文档模板
    ├── _template.md
    ├── _template_api.md
    └── _template_guide.md
```

---

## 🎯 文档质量标准

AgentOS文档遵循**完美主义原则 (A-4)**：

✅ **完整性**：每个公共API都有文档  
✅ **准确性**：示例代码可运行，配置参数经过验证  
✅ **及时性**：代码变更后24小时内同步更新文档  
✅ **易读性**：使用清晰的语言，避免过度技术化  
✅ **可操作性**：每个指南都提供分步操作说明  

---

## 📞 联系方式

- **问题**: wangliren@spharx.cn

---

**© 2026 SPHARX Ltd. All Rights Reserved.**
