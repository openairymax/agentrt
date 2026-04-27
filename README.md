<div align="center">

# Agent OS  

Powered by OpenAirymax  

> The seminal fourth "Operating System Philosophy" in human computing history.

English | [简体中文](README_CN.md)

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

## 🚀 Airymax AgentOS Preview Released  

| User Type | Product | Version | Download |
|:---------:|:-------:|:-------:|:--------:|
|  **Pro Users** | Docker | `v0.0.4` | [📦 Get Docker](https://atomgit.com/openairymax/docker/releases/v0.0.4) |
|  **Personal Users** | Desktop | `v0.0.4` | [🖥️ Get Desktop](https://atomgit.com/openairymax/desktop/releases/v0.0.4) |


## 🌟 Introduction

**AgentOS** is an intelligent agent operating system that provides comprehensive OS-level support for orchestrating agent teams.

### 🎥 Project Preview

Personal Client Preview

<div align="center">
<img src="scripts/source_other/AgentOS-desktop-preview.gif" alt="AgentOS Preview" width="800">
</div>

## 💡 Innovation Highlights

<div align="center">

⚡️  

**Foundational [Multibody Cybernetic Intelligent System](docs/Basic_Theories/EN_01_MCIS.md)**  

</div>  

- **Pure Kernel**: Only atomic mechanisms, ensuring purity and efficiency.
- **Cognitive Loop**: Perception, Planning, Action.
- **Memory Stratification**: L1 Raw → L2 Features → L3 Structures → L4 Patterns.
- **Inherent Security**: Sandbox Isolation, Permission Arbitration, Input Sanitization, Audit Trail.
- **Token Efficiency**: Saves approximately **500%** tokens compared to traditional frameworks.
- **Comprehensive SDKs**: Native support for Go / Python / Rust / TypeScript.

## 🎯 Core Philosophy

**Drive Teams**
- Precisely coordinates multi-Agent collaboration
- Efficiently completes complex task orchestration and resource scheduling

**Autonomous Evolution**:
- Possesses self-evolution capability
- Dynamically adjusts strategies
- Continuously optimizes execution effectiveness

<p align="center">
  <strong> ✨ </strong>  
  <p align="center">
  <strong> Brand New Architecture · Inherent Security · Intelligence Emergence </strong>  
</p>

## 🏗️ System Architecture

**Architecture Design**  
  Complete architecture from kernel to application:

```
⬇️ Application Layer (openlab)
⇅ Service Layer (daemon)
⇅ Kernel Layer (atoms)
⇅ Security Layer (cupolas)
⇅ Support Layer (commons)
⬆️ SDK Layer (toolkit)  
```

**Design Principles**  
  Built upon [ARCHITECTURAL_PRINCIPLES](docs/ARCHITECTURAL_PRINCIPLES.md):
- **System Perspective**: Feedback loops · Layered decomposition · Holistic design · Emergence management → Real-time response <10ms
- **Kernel Perspective**: Minimalist kernel · Contractual interfaces · Service isolation · Pluggable strategies → Kernel ~25K LOC
- **Cognitive Perspective**: Dual-system synergy · Incremental evolution · Memory stratification · Forgetting mechanism → Token savings 500%
- **Engineering Perspective**: Security built-in · Observability · Resource determinism · Cross-platform consistency → Test coverage >90%
- **Design Aesthetics**: Simplicity first · Extreme attention to detail · Human-centric · Perfectionism → API <50/module

## 🚀 Quick Start

### Environment Requirements

- **Operating System**: Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
- **Compiler**: GCC 11+ / Clang 14+ (C11/C++17)
- **Build Tools**: CMake 3.20+, Ninja
- **Python**: 3.10+ (Required for OpenLab)

### Installation & Build

```ctext
# 1. Clone repository
git clone https://atomgit.com/spharx/agentos.git && cd agentos

# 2. Install dependencies (Ubuntu)
sudo apt install -y build-essential cmake gcc g++ libssl-dev libsqlite3-dev ninja-build

# 3. Build kernel
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build . --parallel $(nproc)

# 4. Run tests
ctest --output-on-failure  
```

### Docker Quick Start

```text
# Build image
docker build -f scripts/deploy/docker/Dockerfile.kernel -t agentos:latest .

# Run container
docker run -d --name agentos -p 8080:8080 -v ./config:/app/config agentos:latest
```

### Usage Methods

```
| Language | Usage Method |
|:-----|:---------|
| C/C++ | Develop via `syscalls.h` system call interface |
| Python | Install via `pip install agentos` then directly import |
| Go | Use `import "github.com/spharx/agentos/toolkit/go"` |
| Rust | Use `use agentos_toolkit::prelude::*;` |
| TypeScript | Install via `npm install @spharx/agentos-toolkit` then directly import |
```

### Reading Navigation

| Document | Core Content |
|:-----|:---------|
| [📘 Architectural Principles](docs/ARCHITECTURAL_PRINCIPLES.md) | Five-dimensional orthogonal system, 24 core principles |
| [🚀 Quick Start](docs/guides/quickstart.md) | 5-minute getting-started guide |
| [⚙️ Build Guide](docs/guides/build.md) | Detailed build steps and options |
| [🧪 Testing Guide](docs/guides/testing.md) | Unit/Integration/Contract testing |
| [🐳 Deployment Guide](docs/guides/deployment.md) | Docker/Kubernetes deployment |

### Common Questions

<details>
<summary>👉 Q1: What is the difference between AgentOS and traditional AI Agent frameworks?</summary>

AgentOS is an **operating system-level** product, not a single framework:

| Dimension | AgentOS | Traditional Frameworks |
|------|---------|----------|
| **Positioning** | Multi-agent collaboration OS | Single agent |
| **Architecture** | Microkernel + strict layering | Loosely coupled modules |
| **Security** | Four-layer inherent security | Application-level protection |
| **Memory** | Four-layer stratification system | Vector database |
| **Token Efficiency** | Saves ~500% | No optimization |

</details>

<details>
<summary>👉 Q2: Which application scenarios is it suitable for?</summary>

**✅ Especially suitable**
- 🎯 Complex multi-step task orchestration
- 🧠 Long-term memory and knowledge accumulation needs
- 🔒 High-security enterprise applications
- 💾 Resource-constrained embedded scenarios (atomslite)
- 🌐 Multi-language development teams

**❌ Not suitable**
- 🚫 Simple single-call tasks (using a sledgehammer to crack a nut)

</details>

<details>
<summary>👉 Q3: How is security guaranteed?</summary>

**Security built-in design, four-layer protection**

| Protection Layer | Implementation Method |
|---------|---------|
| **Virtual Workspace** | Process/Container/WASM sandbox isolation |
| **Permission Arbitration** | RBAC + YAML rule engine |
| **Input Sanitization** | Regex filtering + Type checking |
| **Audit Trail** | Full-chain tamper-proof logging |

See [cupolas security documentation](agentos/cupolas/README.md)

</details>

<details>
<summary>👉 Q4: What prerequisite knowledge is needed for learning?</summary>

| Role | Prerequisite Knowledge | Time to Get Started |
|------|---------|----------|
| **Application Developer** | Python/Go basics | 1-2 days |
| **System Developer** | C/C++, OS fundamentals | 1-2 weeks |
| **Architect** | Microkernel, distributed systems | 1 month |

**Recommended Path**: [Quick Start](agentos/manuals/guides/quickstart.md) → [Architectural Principles](agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) → [CoreLoopThree](agentos/manuals/architecture/coreloopthree.md)

</details>

## 🎏 Participating in Contribution

We are walking into the future: "Intelligence emergence, and nothing less, is the ultimate sublimation of AI".

### The Power of Belief

<p align="center">
  <strong> ☀️ </strong>  
  <p align="center">
  <strong> This is not humanity's sunset, but the dawn of a new world </strong>  
</p>

**Believe**

  The spirit of open source can maximize the wisdom of the group;  
  Collaboration will propel humanity to new heights.

**Witness**

  Every day of our work is part of history;  
  It will surely be engraved on the monument of human civilization's development.

### Ways to Contribute

Whether you are an experienced developer or just starting out:

**Find Issues**  
Report bugs, help us improve quality

**Share Ideas**  
Suggest new features, make the project stronger

**Share Knowledge**  
Improve documentation, help more people understand AgentOS

**Write Code**  
Submit PRs, jointly create history

<p align="center">  
  <strong> 🔥 </strong>  
  <p align="center">  
  <strong> A faint light cannot illuminate the entire path, yet it guides our direction forward </strong>
</p>

### Contribution Process

See [Contributing Guide](CONTRIBUTING.md)

```bash
Fork Project → Create Branch → Develop & Test → Submit PR → Code Review → Merge to Main
```

**Main Platforms**: [AtomGit](https://atomgit.com/openairymax/agentos) (Recommended) · [Gitee](https://gitee.com/spharx/agentos) · [GitHub](https://github.com/SpharxTeam/AgentOS)

### Contributors

See [AUTHORS.md](AUTHORS.md) for the list of contributors.

## 📜 License

This project is licensed under the **Apache License 2.0**. See [LICENSE](LICENSE) file for details.

---

<div align="center">

**"From data intelligence emerges."**

<img src="scripts/source_other/feishu-community-qr.png" width="200" />

<a href="https://atomgit.com/openairymax/agentos">AtomGit</a> ·
<a href="https://gitee.com/spharx/agentos">Gitee</a> ·
<a href="https://github.com/SpharxTeam/AgentOS">GitHub</a> ·
<a href="https://spharx.cn">Official Website</a>

© 2026 SPHARX Ltd. All Rights Reserved.

</div>

---

## ⭐️ Github Star History

[![Star History Chart](https://api.star-history.com/chart?repos=SpharxTeam/AgentOS&type=date&legend=top-left)](https://star-history.com/#SpharxTeam/AgentOS)
