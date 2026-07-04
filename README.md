# AgentRT - AI Agent Runtime Platform  
Powered by OpenAirymax
> The seminal fourth "Operating System Philosophy" in human computing history.

**Language:** English | [简体中文](README_zh.md)

[![AtomGit](https://atomgit.com/openairymax/agentos/star/badge.svg)](https://atomgit.com/openairymax/agentos)
[![star](https://gitee.com/spharx/agentos/badge/star.svg?theme=dark)](https://gitee.com/spharx/agentos)
[![GitHub stars](https://img.shields.io/github/stars/SpharxTeam/AgentRT)](https://github.com/SpharxTeam/AgentRT/stargazers)

[![Version](https://img.shields.io/badge/version-0.1.0-5a6b7e)](https://atomgit.com/openairymax/agentos)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![Build](https://img.shields.io/badge/build-passing-2ea44f)](https://atomgit.com/openairymax/agentos)

[![C/C++](https://img.shields.io/badge/C%2FC%2B%2B-11%2F17-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![Python](https://img.shields.io/badge/Python-3.10+-3776AB?logo=python&logoColor=white)](https://www.python.org)
[![Go](https://img.shields.io/badge/Go-1.21+-00ADD8?logo=go&logoColor=white)](https://go.dev)
[![Rust](https://img.shields.io/badge/Rust-1.70+-DEA584?logo=rust&logoColor=white)](https://www.rust-lang.org)
[![TypeScript](https://img.shields.io/badge/TypeScript-4.9+-3178C6?logo=typescript&logoColor=white)](https://www.typescriptlang.org)

---

## 🎉 Preview Released

|   User Type   |   Product   |   Version   |                                   Download                                  |
| :------: | :-----: | :------: | :-----------------------------------------------------------------------: |
| **Pro Users** |  Docker | `v0.1.0` |   [📦 Get Docker](https://atomgit.com/openairymax/docker/releases/v0.1.0)  |
| **Personal Users** | Desktop | `v0.1.0` | [🖥️ Get Desktop](https://atomgit.com/openairymax/desktop/releases/v0.1.0) |

## 1️⃣ Introduction

**AgentRT** is an intelligent agent runtime platform that provides comprehensive OS-level support for orchestrating agent teams.

**1.1  Project Preview**

Personal Client Preview

<div align="center">
<img src="scripts/resources/images/AgentRT-desktop-preview.gif" alt="AgentRT Preview" width="800">
</div>

## 2️⃣ Innovation Highlights

<div align="center">

⚡️

**Foundational Theory** **[Multibody Cybernetic Intelligent System](https://atomgit.com/openairymax/docs/blob/main/Basic_Theories/EN_01_Multibody_Cybernetic_Intelligent_System.md)**

</div>

- Pure Kernel: Kernel only provides atomic mechanisms, pure and efficient
- Cognitive Loop: Cognition, Planning, Action
- Memory Stratification: L1 Raw Layer → L2 Feature Layer → L3 Structure Layer → L4 Pattern Layer (OSS: L1+L2 built-in, PRO: L1-L4 via MemoryRovol)
- Inherent Security: Sandbox isolation, permission arbitration, input sanitization, audit trail
- Token Efficiency: Saves approximately 500% tokens compared to traditional frameworks
- Comprehensive SDKs: Native support for Go, Python, Rust, TypeScript

## 3️⃣ Core Philosophy

**3.1  Team Drive**
- Precisely coordinates multi-Agent collaboration
- Efficiently completes complex task orchestration and resource scheduling

**3.2  Autonomous Evolution**
- Possesses self-evolution capability
- Dynamically adjusts strategies
- Continuously optimizes execution effectiveness

## 4️⃣ System Architecture


<p align="center">
  <strong> ✨ </strong><br>
  <strong> Brand New Architecture · Inherent Security · Intelligence Emergence </strong>
</p>


**4.1  Architecture Design**
Complete architecture from kernel to application:
```
⬇️ Ecosystem Layer (ecosystem) — Apps/Config/Prompts/Hooks/Skills
⇅ Service Layer (daemon) — 12 daemon services
⇅ Protocol Layer (protocols) — 5-layer unified protocol stack
⇅ Gateway Layer (gateway) — HTTP/WS/Stdio → JSON-RPC 2.0
⇅ Storage Layer (heapstore) — Runtime data persistence
⇅ Security Layer (cupolas) — 4-layer inherent security
⇅ Kernel Layer (atoms) — 7 atomic modules
⇅ Support Layer (commons) — Unified foundation library
⬆️ SDK Layer (sdk) — Python/Go/Rust/TypeScript
```

**4.2  Design Principles**
Built upon [ARCHITECTURAL_PRINCIPLES](https://atomgit.com/openairymax/docs/blob/main/ARCHITECTURAL_PRINCIPLES.md):

- System Perspective: Real-time response <10ms
      Feedback loops, layered decomposition
      Holistic design, emergence management
- Kernel Perspective: Kernel ~6K LOC (full repo ~478K LOC)
      Minimalist kernel, contractual interfaces
      Service isolation, pluggable strategies
- Cognitive Perspective: Token savings 80%
      Dual-system synergy, incremental evolution
      Memory stratification, forgetting mechanism
- Engineering Perspective: Test coverage >95%
    Security built-in, observability
    Resource determinism, cross-platform consistency
- Design Aesthetics: API <50 per module
    Simplicity first, extreme attention to detail
    Human-centric, perfectionism

## 5️⃣ Quick Start

**5.1  Environment Requirements**

- **Operating System**: Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
- **Compiler**: GCC 11+ / Clang 14+ (C11/C++17)
- **Build Tools**: CMake 3.20+, Ninja
- **Python**: 3.10+ (Required for OpenLab)

**5.2  Installation & Build**

```bash
# 1. Clone repository
git clone https://atomgit.com/openairymax/agentos.git && cd agentos

# 2. Install dependencies (Ubuntu)
sudo apt install -y build-essential cmake gcc g++ libssl-dev libsqlite3-dev ninja-build

# 3. Build kernel (out-of-source build required by BAN-33)
mkdir /tmp/AgentRT-build && cd /tmp/AgentRT-build
cmake /path/to/AgentRT -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build . --parallel $(nproc)

# 4. Run tests
ctest --output-on-failure
```

**5.3  Docker Quick Start**

```text
# Build image
docker build -f deploy/docker/Dockerfile -t agentrt:latest .

# Run container
docker run -d --name agentrt -p 8080:8080 -v ./config:/app/config agentrt:latest
```

**5.4  Usage Methods**

| Language | Usage Method |
|:-----|:---------|
| C/C++ | Develop via `syscalls.h` system call interface |
| Python | Install via `pip install agentos` then directly import |
| Go | Use `import "github.com/spharx/agentos/sdk/go"` |
| Rust | Use `use agentos_toolkit::prelude::*;` |
| TypeScript | Install via `npm install @spharx/agentos-toolkit` then directly import |

**5.5  Reading Navigation**

| Document                                          | Core Content                 |
| :------------------------------------------ | :------------------- |
| [📘 Architectural Principles](https://atomgit.com/openairymax/docs/blob/main/ARCHITECTURAL_PRINCIPLES.md) | Five-dimensional orthogonal system, 24 core principles      |
| [🚀 Quick Start](https://atomgit.com/openairymax/docs/blob/main/QUICK_START.md)        | 5-minute getting-started guide             |
| [⚙️ Build Guide](https://atomgit.com/openairymax/docs/blob/main/BUILD_GUIDE.md)             | Detailed build steps and options            |
| [🧪 Testing Guide](https://atomgit.com/openairymax/docs/blob/main/TESTING_GUIDE.md)           | Unit/Integration/Contract testing           |
| [🐳 Deployment Guide](https://atomgit.com/openairymax/docs/blob/main/DEPLOYMENT_GUIDE.md)        | Docker/Kubernetes deployment |


**5.6  Common Questions**

<details>
<summary>👉 Q1: What is the difference between AgentRT and traditional AI Agent frameworks?</summary>

AgentRT is an operating system-level product, not a single framework:

| Dimension           | AgentRT    | Traditional Frameworks  |
| ------------ | ---------- | ----- |
| **Positioning**       | Multi-agent collaboration OS  | Single agent |
| **Architecture**       | Microkernel + strict layering | Loosely coupled modules |
| **Security**       | Four-layer inherent security     | Application-level protection |
| **Memory**       | Four-layer stratification system     | Vector database |
| **Token Efficiency** | Saves approximately 500%   | No optimization   |

</details>

<details>
<summary>👉 Q2: Which application scenarios is it suitable for?</summary>

✅ Especially suitable

- 🎯 Complex multi-step task orchestration
- 🧠 Long-term memory and knowledge accumulation needs
- 🔒 High-security enterprise applications
- 💾 Resource-constrained embedded scenarios (atomslite)
- 🌐 Multi-language development teams

❌ Not suitable

- 🚫 Simple single-call tasks (using a sledgehammer to crack a nut)

</details>

<details>
<summary>👉 Q3: How is security guaranteed?</summary>

Security built-in design, four-layer protection

| Protection Layer     | Implementation Method             |
| -------- | ---------------- |
| Virtual Workspace | Process/Container/WASM sandbox isolation  |
| Permission Arbitration | RBAC + YAML rule engine |
| Input Sanitization | Regex filtering + Type checking      |
| Audit Trail | Full-chain tamper-proof logging        |

See [cupolas security documentation](https://atomgit.com/openairymax/agentos/blob/main/agentos/cupolas/README.md)

</details>

<details>
<summary>👉 Q4: What prerequisite knowledge is needed for learning?</summary>

| Role        | Prerequisite Knowledge          | Time to Get Started    |
| --------- | ------------- | ------- |
| Application Developer | Python/Go basics  | 1-2 days |
| System Developer | C/C++, OS fundamentals | 1-2 weeks |
| Architect   | Microkernel, distributed systems     | 1 month   |

Recommended path: [Quick Start](https://atomgit.com/openairymax/docs/blob/main/QUICK_START.md) → [Architectural Principles](https://atomgit.com/openairymax/docs/blob/main/ARCHITECTURAL_PRINCIPLES.md) → [CoreLoopThree](https://atomgit.com/openairymax/agentos/blob/main/agentos/atoms/coreloopthree/README.md)

</details>

## 6️⃣ Participating in Contribution

We are walking into the future: "Intelligence emergence, and nothing less, is the ultimate sublimation of AI".

The power of belief

<div align="center">

☀️

This is not humanity's sunset, but the dawn of a new world

</div>

**Believe:**
The spirit of open source can maximize the wisdom of the group;
Collaboration will propel humanity to new heights.

**Witness:**
Every day of our work is part of history;
It will surely be engraved on the monument of human civilization's development.

**Contribution:**
Whether you are an experienced developer or just starting out:

**Discover:**
Report bugs, help us improve quality

**Ideas:**
New feature suggestions, make the project stronger

**Share:**
Improve documentation, help more people understand AgentRT

**Coding:**
Submit PRs, jointly create history

<p align="center">
  <strong> 🔥 </strong><br>
  <strong> A faint light cannot illuminate the entire path, yet it guides our direction forward </strong>
</p>

**6.1  Contribution Process:**
See [Contributing Guide](CONTRIBUTING.md)

```text
Fork Project → Create Branch → Develop & Test → Submit PR → Code Review → Merge to Main
```

**Main Platforms:** [AtomGit](https://atomgit.com/openairymax/agentos) (Recommended) · [Gitee](https://gitee.com/spharx/agentos) · [GitHub](https://github.com/SpharxTeam/AgentRT)

**6.2  Contributors:**
See [AUTHORS.md](AUTHORS.md) for the list of contributors.

## 7️⃣ License

This project is dual-licensed under **AGPL v3 + Apache 2.0** (SPDX: `AGPL-3.0-or-later OR Apache-2.0`). You may choose either license at your option. See [LICENSE](LICENSE) file for details. Copyright (c) 2025-2026 SPHARX Ltd.

***

<div align="center">

"From data intelligence emerges."\
From data, to intelligence.

<img src="scripts/resources/images/feishu-community-qr.png" width="200" />

<a href="https://atomgit.com/spharx/agentos">AtomGit</a> · <a href="https://gitee.com/spharx/agentos">Gitee</a> · <a href="https://github.com/SpharxTeam/AgentRT">GitHub</a> · <a href="https://spharx.cn">Official Website</a>

© 2026 SPHARX Ltd. All Rights Reserved.