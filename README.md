<div align="center">

[![AtomGit](https://atomgit.com/spharx/agentos/star/badge.svg)](https://atomgit.com/spharx/agentos)
[![Gitee](https://gitee.com/spharx/agentos/badge/star.svg?theme=gray)](https://gitee.com/spharx/agentos)
[![GitHub](https://img.shields.io/github/stars/SpharxTeam/AgentOS)](https://github.com/SpharxTeam/AgentOS)

</div>

<div align="center">

# Agent OS

<p align="center">
  <strong>⚡ 自主可控安全内生的智能体操作系统 ⚡</strong>
</p>

[![Version](https://img.shields.io/badge/version-1.0.0.9-5a6b7e)](https://atomgit.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![Build](https://img.shields.io/badge/build-passing-7a9c8c)](.github/workflows/ci.yml)

[![C/C++](https://img.shields.io/badge/C%2FC%2B%2B-11%2F17-blue?logo=c%2B%2B)](https://isocpp.org)
[![Python](https://img.shields.io/badge/python-3.10+-6b93d6?logo=python&logoColor=white)](https://www.python.org)
[![Go](https://img.shields.io/badge/Go-1.21+-529ccb?logo=go&logoColor=white)](https://go.dev)
[![Rust](https://img.shields.io/badge/Rust-1.70+-b08968?logo=rust&logoColor=white)](https://www.rust-lang.org)

</div>

## 🌟 项目简介

**AgentOS** 是一个智能体操作系统，为驱动智能体团队提供完整的操作系统级支持。

> *"Intelligence emergence, and nothing less, is the ultimate sublimation of AI."*

## ✨ 核心功能

- **🧠 纯净内核** · 内核仅提供 IPC/Mem/Task/Time 四大原子机制，纯净高效
- **🔄 认知循环** · 认知→规划→行动，双系统协同（System 1 + System 2）
- **💾 记忆卷载** · L1 原始层 → L2 特征层 → L3 结构层 → L4 模式层，涌现智慧
- **🛡️ 安全内生** · 四重防护：沙箱隔离 / 权限裁决 / 输入净化 / 审计追踪
- **⚡️ 高效 Token** · 相比传统框架节省约 **500%** Token 消耗
- **🌐 丰富 SDK** · Go / Python / Rust / TypeScript 原生支持

## 🎯 基石思想

**🤝 驱动团队** · 面对复杂的多 Agent 协作场景——任务编排、资源调度、冲突消解、异常恢复，AgentOS 都能精准调度，有条不紊地完成工业级任务。

**🔄 自主演进** · 当任务失败或环境变化时，系统自动触发补偿事务、重新规划 DAG、调整策略参数，持续优化执行效果。

## 🏗️ 系统架构

**架构设计** · 从内核到应用的完整技术栈

```
应用层 (openlab)
    ↕
服务层 (daemon) — llm_d · market_d · monit_d · sched_d · tool_d
    ↕
内核层 (atoms) — corekern · coreloopthree · memoryrovol · syscall · cupolas
    ↕
支撑层 (commons) — platform · utils (19 个模块)
    ↕
SDK 层 (toolkit) — Go · Python · Rust · TypeScript
```

**📐 设计原则** · 基于 [ARCHITECTURAL_PRINCIPLES.md V1.8](agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) 构建：
- **系统观** · 反馈闭环 · 层次分解 · 总体设计部 · 涌现管理 → 实时响应 <10ms
- **内核观** · 内核极简 · 接口契约化 · 服务隔离 · 可插拔策略 → 内核 ~9K LOC
- **认知观** · 双系统协同 · 增量演化 · 记忆卷载 · 遗忘机制 → Token 节省 500%
- **工程观** · 安全内生 · 可观测性 · 资源确定性 · 跨平台一致 → 测试覆盖 >90%
- **艺术观** · 简约至上 · 极致细节 · 人文关怀 · 完美主义 → API <50 个/模块

## 🚀 快速上手

### 📦 环境要求

- **操作系统**: Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
- **编译器**: GCC 11+ / Clang 14+ (C11/C++17)
- **构建工具**: CMake 3.20+, Ninja
- **Python**: 3.10+ (OpenLab 需要)

### 🔨 安装与构建

```bash
# 1. 克隆仓库
git clone https://atomgit.com/spharx/agentos.git && cd agentos

# 2. 安装依赖（Ubuntu）
sudo apt install -y build-essential cmake gcc g++ libssl-dev libsqlite3-dev

# 3. 构建内核
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build . --parallel $(nproc)

# 4. 运行测试
ctest --output-on-failure
```

### 🐳 Docker 快速启动

```bash
# 构建镜像
docker build -f scripts/deploy/docker/Dockerfile.kernel -t agentos:latest .

# 运行容器
docker run -d --name agentos -p 8080:8080 -v ./config:/app/config agentos:latest
```

### 💻 使用方式

- **C/C++ SDK** · 通过 `syscalls.h` 系统调用接口开发
- **Python SDK** · `pip install agentos` 后直接 import
- **Go SDK** · `import "github.com/spharx/agentos/toolkit/go"`
- **Rust SDK** · `use agentos_toolkit::prelude::*;`

## 📁 项目结构

```
AgentOS/
├── agentos/        # 📦 核心模块集合
│   ├── agentos/atoms/      # 🏛️ 内核层 (微内核 + 三层循环 + 四层记忆)
│   ├── agentos/atomslite/  # 🪶 轻量级内核 (嵌入式优化，资源降低 70%)
│   ├── agentos/commons/    # 🧩 统一基础库 (19 个工具模块)
│   ├── agentos/cupolas/    # 🛡️ 安全穹顶 (净化/权限/审计/沙箱)
│   ├── agentos/daemon/     # 🛠️ 服务层 (7 大守护进程)
│   ├── agentos/gateway/    # 🌐 网关服务
│   ├── agentos/heapstore/  # 💾 全量运行时数据库
│   ├── agentos/manager/    # ⚙️ 统一配置管理中心
│   ├── agentos/toolkit/    # 🔌 多语言 SDK (Go/Python/Rust/TS)
│   └── agentos/manuals/    # 📚 文档体系 (V1.8)
├── openlab/        # 🖥️ 开放生态实验室
├── scripts/        # 📜 开发/构建/部署脚本
└── tests/          # 🧪 测试套件 (单元/集成/契约/安全)
```

## 📊 性能基准

**核心性能指标** · *测试环境：Intel i7-12700K / 32GB RAM / Ubuntu 22.04*

- **L1 写入吞吐** · 10,000+ 条/秒（异步批量写入）
- **L2 检索延迟** · < 10ms（FAISS IVF1024, k=10）
- **IPC 吞吐量** · >10,000 次/秒（单次调用 <50μs）
- **权限检查** · ~5μs（LRU 缓存命中）
- **空闲资源** · CPU<5%, 内存 200MB（默认配置）

## 📚 文档导航

**核心文档**
- [📘 架构设计原则 V1.8](agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) · 五维正交体系，24 条核心原则
- [🔄 CoreLoopThree 架构](agentos/manuals/architecture/coreloopthree.md) · 三层认知循环详解
- [🧠 MemoryRovol 架构](agentos/manuals/architecture/memoryrovol.md) · 四层记忆系统实现
- [⚙️ 编译指南](agentos/manuals/guides/build.md) · 详细构建步骤和选项
- [🚀 快速开始](agentos/manuals/guides/quickstart.md) · 5 分钟上手指南

**开发指南**
- [🧪 测试指南](agentos/manuals/guides/testing.md) · 单元/集成/契约测试
- [🐳 部署指南](agentos/manuals/guides/deployment.md) · Docker/Kubernetes 部署
- [🔌 C API 文档](agentos/manuals/api/c/README.md) · C 语言接口规范
- [🐍 Python SDK](agentos/toolkit/python/README.md) · Python 开发指南
- [🦀 Rust SDK](agentos/toolkit/rust/README.md) · Rust 开发指南

## ❓ 常见问题

<details>
<summary><b>Q1: AgentOS 与传统 AI Agent 框架有什么区别？</b></summary>

AgentOS 是**操作系统级**产品，而非单一框架：
- **定位** · 多智能体协作 OS（传统框架：单一智能体）
- **架构** · 微内核 + 严格分层（传统框架：松耦合模块）
- **安全** · 四重内生安全（传统框架：应用层防护）
- **记忆** · 四层卷载系统（传统框架：向量数据库）
- **Token 效率** · 节省约 500%（传统框架：无优化）

</details>

<details>
<summary><b>Q2: 适合哪些应用场景？</b></summary>

**✅ 特别适合**
- 复杂多步骤任务编排
- 长期记忆与知识积累需求
- 高安全性企业应用
- 资源受限嵌入式场景 (atomsmini)
- 多语言开发团队

**❌ 不适合**
- 简单单次调用任务（杀鸡用牛刀）

</details>

<details>
<summary><b>Q3: 如何保证安全性？</b></summary>

**安全内生设计，四重防护**
1. **虚拟工位** · 进程/容器/WASM 沙箱隔离
2. **权限裁决** · RBAC + YAML 规则引擎
3. **输入净化** · 正则过滤 + 类型检查
4. **审计追踪** · 全链路不可篡改日志

详见 [cupolas 安全穹顶文档](agentos/cupolas/README.md)

</details>

<details>
<summary><b>Q4: 学习需要哪些前置知识？</b></summary>

- **应用开发者** · Python/Go 基础 → 1-2 天上手
- **系统开发者** · C/C++, 操作系统基础 → 1-2 周深入
- **架构师** · 微内核，分布式系统 → 1 月精通

推荐路径：[快速开始](agentos/manuals/guides/quickstart.md) → [架构原则](agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) → [CoreLoopThree](agentos/manuals/architecture/coreloopthree.md)

</details>

## 🤝 参与贡献

我们热烈欢迎社区贡献！无论是提交 Bug、提出新功能建议，还是完善文档，都是对项目的宝贵支持。

```bash
Fork → 创建分支 (feature/xxx) → 开发测试 → 提交 PR → 代码审查 → 合并主分支
```

### 📋 贡献方式

- 🐛 **报告问题** · [提交 Issue](https://atomgit.com/spharx/agentos/issues)
- 💻 **提交代码** · [提交 PR](https://atomgit.com/spharx/agentos/pulls)（参考 [PR 模板](.github/PULL_REQUEST_TEMPLATE.md)）
- 📝 **完善文档** · 编辑 `agentos/manuals/` 目录下的文档
- 💬 **参与讨论** · [GitHub Discussions](https://github.com/SpharxTeam/AgentOS/discussions)

👉 详见 [贡献指南](CONTRIBUTING.md)

## 📜 许可证

本项目采用 **Apache License 2.0** 开源协议，详情请参阅 [LICENSE](LICENSE) 文件。

- ✅ **商用** · 允许用于商业目的
- ✅ **修改** · 允许修改源码和二次开发
- ✅ **分发** · 允许分发源码和二进制
- ✅ **专利** · 核心代码包含专利授权


<div align="center">

### 🌟 核心愿景

> **"From data intelligence emerges."**  
> **始于数据，终于智能。**

<p align="center">
  <strong>让我们一起期待，AI 智能涌现的极尽升华时刻</strong>
</p>

<p>
  <a href="#-agent-os">⬆️ 返回顶部</a>
</p>

<p>
  <a href="https://atomgit.com/spharx/agentos">AtomGit</a> ·
  <a href="https://gitee.com/spharx/agentos">Gitee</a> ·
  <a href="https://github.com/SpharxTeam/AgentOS">GitHub</a> ·
  <a href="https://spharx.cn">官方网站</a>
</p>


© 2026 SPHARX Ltd. All Rights Reserved.

</div>
