<div align="center">

# Agent OS  

<p align="center">
  成为人类计算科学工程史上，第四个"操作系统哲学"  
</p>  

<p align="center">
  <a href="https://atomgit.com/spharx/agentos">
    <img src="https://atomgit.com/spharx/agentos/star/badge.svg" alt="AtomGit">
  </a>
  <a href="https://gitee.com/spharx/agentos">
    <img src="https://gitee.com/spharx/agentos/badge/star.svg?theme=gray" alt="Gitee">
  </a>
  <a href="https://github.com/SpharxTeam/AgentOS">
    <img src="https://img.shields.io/github/stars/SpharxTeam/AgentOS?style=social" alt="GitHub">
  </a>
</p>

<p align="center">
  <a href="https://atomgit.com/spharx/agentos">
    <img src="https://img.shields.io/badge/version-1.0.0.9-5a6b7e" alt="Version">
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/license-Apache--2.0-4a90d9" alt="License">
  </a>
  <a href="https://atomgit.com/spharx/agentos">
    <img src="https://img.shields.io/badge/build-passing-2ea44f" alt="Build">
  </a>
</p>

<p align="center">
  <a href="https://isocpp.org">
    <img src="https://img.shields.io/badge/C%2FC%2B%2B-11%2F17-00599C?logo=c%2B%2B&logoColor=white" alt="C/C++">
  </a>
  <a href="https://www.python.org">
    <img src="https://img.shields.io/badge/Python-3.10+-3776AB?logo=python&logoColor=white" alt="Python">
  </a>
  <a href="https://go.dev">
    <img src="https://img.shields.io/badge/Go-1.21+-00ADD8?logo=go&logoColor=white" alt="Go">
  </a>
  <a href="https://www.rust-lang.org">
    <img src="https://img.shields.io/badge/Rust-1.70+-DEA584?logo=rust&logoColor=white" alt="Rust">
  </a>
  <a href="https://www.typescriptlang.org">
    <img src="https://img.shields.io/badge/TypeScript-4.9+-3178C6?logo=typescript&logoColor=white" alt="TypeScript">
  </a>
</p>

</div>

## 🌟 项目简介

**AgentOS** 是一个智能体操作系统，为驱动智能体团队提供完整的操作系统级支持。  

> "Intelligence emergence, and nothing less, is the ultimate sublimation of AI."  

## 💡 核心功能

1. 纯净内核：内核仅提供 IPC/Mem/Task/Time 四大原子机制，纯净高效
2. 认知循环：认知 → 规划 → 行动，双系统协同（System 1 + System 2）
3. 记忆卷载：L1 原始层 → L2 特征层 → L3 结构层 → L4 模式层，涌现智慧
4. 安全内生：四重防护：沙箱隔离 / 权限裁决 / 输入净化 / 审计追踪
5. 高效 Token：相比传统框架节省约 **500%** Token 消耗
6. 丰富 SDK：Go / Python / Rust / TypeScript 原生支持

## 🎯 基石思想

**🤝 驱动团队** · 面对复杂的多 Agent 协作场景——任务编排、资源调度、冲突消解、异常恢复，AgentOS 都能精准调度，有条不紊地完成工业级任务。

**🔄 自主演进** · 当任务失败或环境变化时，系统自动触发补偿事务、重新规划 DAG、调整策略参数，持续优化执行效果。

<p align="center">
  <strong>✨ 全新架构 · 自主可控 · 安全内生 · 智能涌现 ✨</strong>
</p>

## 🏗️ 系统架构

**架构设计** · 从内核到应用的完整技术栈

```
应用层 (openlab)
    ↕
服务层 (daemon) — gateway_d · llm_d · market_d · monit_d · sched_d · tool_d
    ↕
内核层 (atoms) — corekern · coreloopthree · memoryrovol · syscall
    ↕
安全层 (cupolas) — 净化 / 权限 / 审计 / 沙箱
    ↕
支撑层 (commons) — platform · utils (19 个模块)
    ↕
SDK 层 (toolkit) — Go / Python / Rust / TypeScript
```

**📐 设计原则** · 基于 [ARCHITECTURAL_PRINCIPLES.md V1.8](agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) 构建：

1. 系统观：反馈闭环 · 层次分解 · 总体设计部 · 涌现管理 → 实时响应 <10ms
2. 内核观：内核极简 · 接口契约化 · 服务隔离 · 可插拔策略 → 内核 ~25K LOC
3. 认知观：双系统协同 · 增量演化 · 记忆卷载 · 遗忘机制 → Token 节省 500%
4. 工程观：安全内生 · 可观测性 · 资源确定性 · 跨平台一致 → 测试覆盖 >90%
5. 设计美学：简约至上 · 极致细节 · 人文关怀 · 完美主义 → API <50 个/模块

## 🚀 快速上手

### 📦 环境要求

1. 操作系统：Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
2. 编译器：GCC 11+ / Clang 14+ (C11/C++17)
3. 构建工具：CMake 3.20+, Ninja
4. Python：3.10+ (OpenLab 需要)

### 🔨 安装与构建

```bash
# 1. 克隆仓库
git clone https://atomgit.com/spharx/agentos.git && cd agentos

# 2. 安装依赖（Ubuntu）
sudo apt install -y build-essential cmake gcc g++ libssl-dev libsqlite3-dev ninja-build

# 3. 构建内核
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
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

1. C/C++：通过 `syscalls.h` 系统调用接口开发
2. Python：通过 `pip install agentos` 后直接 import
3. Go：通过 `import "github.com/spharx/agentos/toolkit/go"`
4. Rust：通过 `use agentos_toolkit::prelude::*;`
5. TypeScript：通过 `npm install @spharx/agentos-toolkit` 后直接 import

### 📖 核心文档

1. [📘 架构设计原则 V1.8](agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) · 五维正交体系，24 条核心原则
2. [🔄 CoreLoopThree 架构](agentos/manuals/architecture/coreloopthree.md) · 三层认知循环详解
3. [🧠 MemoryRovol 架构](agentos/manuals/architecture/memoryrovol.md) · 四层记忆卷载实现
4. [⚙️ 编译指南](agentos/manuals/guides/build.md) · 详细构建步骤和选项
5. [🚀 快速开始](agentos/manuals/guides/quickstart.md) · 5 分钟上手指南

### 🛠️ 开发指南

1. [测试指南](agentos/manuals/guides/testing.md) · 单元/集成/契约测试
2. [部署指南](agentos/manuals/guides/deployment.md) · Docker/Kubernetes 部署
3. [C API 文档](agentos/manuals/api/c/README.md) · C 语言接口规范
4. [Python SDK](agentos/toolkit/python/README.md) · Python 开发指南
5. [Rust SDK](agentos/toolkit/rust/README.md) · Rust 开发指南
6. [TypeScript SDK](agentos/toolkit/typescript/README.md) · TypeScript 开发指南

## ❔ 常见问题

<details>
<summary><b> Q1: AgentOS 与传统 AI Agent 框架有什么区别？</b></summary>

AgentOS 是**操作系统级**产品，而非单一框架：

| 维度 | AgentOS | 传统框架 |
|------|---------|----------|
| **定位** | 多智能体协作 OS | 单一智能体 |
| **架构** | 微内核 + 严格分层 | 松耦合模块 |
| **安全** | 四重内生安全 | 应用层防护 |
| **记忆** | 四层卷载系统 | 向量数据库 |
| **Token 效率** | 节省约 500% | 无优化 |

</details>

<details>
<summary><b> Q2: 适合哪些应用场景？</b></summary>

**✅ 特别适合**
- 🎯 复杂多步骤任务编排
- 🧠 长期记忆与知识积累需求
- 🔒 高安全性企业应用
- 💾 资源受限嵌入式场景 (atomslite)
- 🌐 多语言开发团队

**❌ 不适合**
- 🚫 简单单次调用任务（杀鸡用牛刀）

</details>

<details>
<summary><b> Q3: 如何保证安全性？</b></summary> 

**安全内生设计，四重防护**

| 防护层级 | 实现方式 |
|---------|---------|
| **虚拟工位** | 进程/容器/WASM 沙箱隔离 |
| **权限裁决** | RBAC + YAML 规则引擎 |
| **输入净化** | 正则过滤 + 类型检查 |
| **审计追踪** | 全链路不可篡改日志 |

详见 [cupolas 安全穹顶文档](agentos/cupolas/README.md)

</details>

<details>
<summary><b> Q4: 学习需要哪些前置知识？</b></summary>

| 角色 | 前置知识 | 上手时间 |
|------|---------|----------|
| **应用开发者** | Python/Go 基础 | 1-2 天上手 |
| **系统开发者** | C/C++, 操作系统基础 | 1-2 周深入 |
| **架构师** | 微内核，分布式系统 | 1 月精通 |

**推荐路径**：[快速开始](agentos/manuals/guides/quickstart.md) → [架构原则](agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) → [CoreLoopThree](agentos/manuals/architecture/coreloopthree.md)

</details>

## 🤝 参与贡献

我们热烈欢迎社区贡献！无论是提交 Bug、提出新功能建议，还是完善文档，都是对项目的宝贵支持。

1. Fork 项目 [AtomGit](https://atomgit.com/spharx/agentos)  
2. 创建分支 (feature/xxx)
3. 开发测试
4. 提交 PR
5. 代码审查
6. 合并主分支

详见 [贡献指南](CONTRIBUTING.md)


## 📜 许可证

本项目采用 **Apache License 2.0** 开源协议，详情请参阅 [LICENSE](LICENSE) 文件。

---

<div align="center">

**"From data intelligence emerges."**  
**始于数据，终于智能。**  

<p align="center">
  <a href="https://atomgit.com/spharx/agentos">AtomGit</a> ·
  <a href="https://gitee.com/spharx/agentos">Gitee</a> ·
  <a href="https://github.com/SpharxTeam/AgentOS">GitHub</a> ·
  <a href="https://spharx.cn">官方网站</a>
</p>

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
