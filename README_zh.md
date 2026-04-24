<div align="center">

# Agent OS  

Powered by OpenAirymax  

> 成为人类计算工程史上，第四个"操作系统哲学"

中文 | [English](README.md)  

[![AtomGit](https://atomgit.com/openairymax/agentos/star/badge.svg)](https://atomgit.com/openairymax/agentos)
[![star](https://gitee.com/spharx/agentos/badge/star.svg?theme=dark)](https://gitee.com/spharx/agentos)
[![GitHub stars](https://img.shields.io/github/stars/SpharxTeam/AgentOS)](https://github.com/SpharxTeam/AgentOS/stargazers)  
 
[![Version](https://img.shields.io/badge/version-1.0.0.9-5a6b7e)](https://atomgit.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-4a90d9)](LICENSE)
[![Build](https://img.shields.io/badge/build-passing-2ea44f)](https://atomgit.com/spharx/agentos)

[![C/C++](https://img.shields.io/badge/C%2FC%2B%2B-11%2F17-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![Python](https://img.shields.io/badge/Python-3.10+-3776AB?logo=python&logoColor=white)](https://www.python.org)
[![Go](https://img.shields.io/badge/Go-1.21+-00ADD8?logo=go&logoColor=white)](https://go.dev)
[![Rust](https://img.shields.io/badge/Rust-1.70+-DEA584?logo=rust&logoColor=white)](https://www.rust-lang.org)
[![TypeScript](https://img.shields.io/badge/TypeScript-4.9+-3178C6?logo=typescript&logoColor=white)](https://www.typescriptlang.org)

</div>

## 🌟 项目简介

**AgentOS** 是一个智能体底层操作系统，为驱动智能体团队提供完整的操作系统级支持。

### 🎥 项目预览  
个人用户客户端预告  

<div align="center">
<img src="scripts/source_other/AgentOS-desktop-preview.gif" alt="AgentOS 预览" width="800">
</div>

## 💡 创新要点

<div align="center">

⚡️  

**基石理论 [《体系并行论》](docs/Basic_Theories/CN_01_体系并行论.md)**  

</div>

- **纯净内核**：内核仅提供原子机制，纯净高效
- **认知循环**：认知，规划，行动
- **记忆卷载**：原始层，特征层，结构层，模式层
- **安全内生**：沙箱隔离，权限裁决，输入净化，审计追踪
- **高效 Token**：工程级比传统框架节省约**500%**
- **丰富 SDK**：原生支持 Go 、Python 、Rust 、TypeScript

## 🎯 基本理念

**驱动团队**
- 精准协调多 Agent 协作
- 高效完成复杂任务编排与资源调度  

**自主演进**：
- 具备自我进化能力
- 动态调整策略
- 持续优化执行效果  

<p align="center">
  <strong> ✨ </strong>  
  <p align="center">
  <strong> 全新架构 · 安全内生 · 智能涌现 </strong>  
</p>

## 🏗️ 系统架构

**架构设计**  
  从内核到应用的完整架构：

```
⬇️ 应用层 (openlab)
⇅ 服务层 (daemon)
⇅ 内核层 (atoms)
⇅ 安全层 (cupolas)
⇅ 支撑层 (commons)
⬆️ SDK 层 (toolkit)  
```

**设计原则**  
  基于 [ARCHITECTURAL_PRINCIPLES](docs/ARCHITECTURAL_PRINCIPLES.md) 构建：
- **系统观**：反馈闭环 · 层次分解 · 总体设计部 · 涌现管理 → 实时响应 <10ms
- **内核观**：内核极简 · 接口契约化 · 服务隔离 · 可插拔策略 → 内核 ~25K LOC
- **认知观**：双系统协同 · 增量演化 · 记忆卷载 · 遗忘机制 → Token 节省 500%
- **工程观**：安全内生 · 可观测性 · 资源确定性 · 跨平台一致 → 测试覆盖 >90%
- **设计美学**：简约至上 · 极致细节 · 人文关怀 · 完美主义 → API <50 个/模块  


## 🚀 快速上手

### 环境要求  

- **操作系统**：Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
- **编译器**：GCC 11+ / Clang 14+ (C11/C++17)
- **构建工具**：CMake 3.20+, Ninja
- **Python**：3.10+ (OpenLab 需要)  

### 安装与构建  

```ctext
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

### Docker快速启动  

```text
# 构建镜像
docker build -f scripts/deploy/docker/Dockerfile.kernel -t agentos:latest .

# 运行容器
docker run -d --name agentos -p 8080:8080 -v ./config:/app/config agentos:latest
```

### 使用方式   

```
| 语言 | 使用方式 |
|:-----|:---------|
| C/C++ | 通过 `syscalls.h` 系统调用接口开发 |
| Python | 通过 `pip install agentos` 后直接 import |
| Go | 通过 `import "github.com/spharx/agentos/toolkit/go"` |
| Rust | 通过 `use agentos_toolkit::prelude::*;` |
| TypeScript | 通过 `npm install @spharx/agentos-toolkit` 后直接 import |
```
### 阅读导航  

| 文档 | 核心内容 |
|:-----|:---------|
| [📘 架构原则](docs/ARCHITECTURAL_PRINCIPLES.md) | 五维正交体系，24 条核心原则 |
| [🚀 快速开始](docs/guides/quickstart.md) | 5 分钟上手指南 |
| [⚙️ 编译指南](docs/guides/build.md) | 详细构建步骤和选项 |
| [🧪 测试指南](docs/guides/testing.md) | 单元/集成/契约测试 |
| [🐳 部署指南](docs/guides/deployment.md) | Docker/Kubernetes 部署 |


### 常见问题  

<details>
<summary>👉 Q1: AgentOS 与传统 AI Agent 框架有什么区别？</summary>

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
<summary>👉 Q2: 适合哪些应用场景？</summary>

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
<summary>👉 Q3: 如何保证安全性？</summary>

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
<summary>👉 Q4: 学习需要哪些前置知识？</summary>

| 角色 | 前置知识 | 上手时间 |
|------|---------|----------|
| **应用开发者** | Python/Go 基础 | 1-2 天上手 |
| **系统开发者** | C/C++, 操作系统基础 | 1-2 周深入 |
| **架构师** | 微内核，分布式系统 | 1 月精通 |

**推荐路径**：[快速开始](agentos/manuals/guides/quickstart.md) → [架构原则](agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) → [CoreLoopThree](agentos/manuals/architecture/coreloopthree.md)  

</details>  



## 🎏 参与贡献  

我们正在走进未来："Intelligence emergence, and nothing less, is the ultimate sublimation of AI"。   

### 相信的力量

<div align="center">

☀️  

**这不是人类的日落，而是新世界的曙光**

</div>  

**相信**  

  开源的精神能最大发挥群体的智慧；  
  协作，推动人类族群走上新的高度。    

**见证**  

  我们每一天的工作都是历史的一部分；  
  必将铭刻在人类文明发展史的丰碑上。   

### 贡献的方式   

无论你是经验丰富的开发者，还是刚刚起步的新手：  
 
**发现问题**   
报告 Bug，帮助我们改进质量  

**提出想法**   
新功能建议，让项目更加强大  

**分享知识**   
完善文档，帮助更多人了解 AgentOS  

**编写代码**   
提交 PR，共同创造历史   

<p align="center">  
  <strong> 🔥 </strong>  
  <p align="center">  
  <strong> 微微的灯火，照不亮前路，但能指引前行的方向 </strong>
</p>  

### 贡献流程  

详见 [贡献指南](CONTRIBUTING.md)

```bash
Fork 项目 → 创建分支 → 开发测试 → 提交 PR → 代码审查 → 合并主分支
```

**主要平台**：[AtomGit](https://atomgit.com/spharx/agentos)（推荐） · [Gitee](https://gitee.com/spharx/agentos) · [GitHub](https://github.com/SpharxTeam/AgentOS)

### 贡献者名单

详见 [AUTHORS.md](AUTHORS.md)    


## 📜 许可证  

本项目采用 **Apache License 2.0** 开源许可，详情参阅 [LICENSE](LICENSE) 文件。

---

<div align="center">

"From data intelligence emerges."  
始于数据，终于智能。  

<img src="scripts/source_other/feishu-community-qr.png" width="200" />

<a href="https://atomgit.com/spharx/agentos">AtomGit</a> ·
<a href="https://gitee.com/spharx/agentos">Gitee</a> ·
<a href="https://github.com/SpharxTeam/AgentOS">GitHub</a> ·
<a href="https://spharx.cn">官方网站</a>

© 2026 SPHARX Ltd. All Rights Reserved.

</div>  

---

## ⭐️ Github Star History

[![Star History Chart](https://api.star-history.com/chart?repos=SpharxTeam/AgentOS&type=date&legend=top-left)](https://star-history.com/#SpharxTeam/AgentOS)
