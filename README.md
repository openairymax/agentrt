# AgentRT — AI Agent Runtime Platform

> Airymax Runtime Engineering — The foundational runtime platform for AI Agent teams.
> One of four management repositories under the [airymaxhub](https://atomgit.com/openairymax/airymaxhub) umbrella.

**Language:** English | [简体中文](README_zh.md)

[![Version](https://img.shields.io/badge/version-0.1.1-5a6b7e)](https://atomgit.com/openairymax/agentos)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org)

---

## Overview

**AgentRT** is the runtime engineering layer of Airymax — an AI Agent Runtime Platform positioned analogously to JVM/containerd for languages/containers. It provides OS-level mechanisms for orchestrating agent teams: microkernel primitives, cognitive loops, memory stratification, security domes, IPC protocols, gateway services, and daemon processes.

This repository is a **management repo** (git superproject) that aggregates 7 leaf repositories as git submodules. It inherits the full git history of the original AgentRT monorepo.

## Repository Structure

```
airymaxhub/                 ← Umbrella repo
├── agentrt/                ← THIS REPO (management repo)
│   ├── atoms/              ← submodule: microkernel primitives (A-class)
│   ├── commons/            ← submodule: shared utilities (A-class foundation)
│   ├── cupolas/            ← submodule: safety dome (B-class)
│   ├── heapstore/          ← submodule: heap-backed storage (A-class)
│   ├── protocols/          ← submodule: AgentsIPC & A2A/A2T protocols
│   ├── gateway/            ← submodule: HTTP/gRPC gateway (gateway_d)
│   ├── daemons/            ← submodule: 12 runtime daemons
│   ├── contracts/          ← contract headers (symlink to atoms/contracts)
│   ├── cmake/ → ../cmake/  ← (removed, unified to umbrella cmake/)
│   ├── CMakeLists.txt      ← top-level CMake entry
│   └── Doxyfile            ← API documentation config
├── sdk/                    ← SDK management repo
├── ecosystem/              ← Ecosystem management repo
├── products/               ← Products management repo
├── cmake/                  ← Shared CMake modules (5 general + 4 AgentRT-specific)
├── devtools/               ← Development tools
├── docs/                   ← Open documentation
└── docs-closed/            ← Internal documentation
```

## Leaf Repositories

| Module | Repository | Class | Description |
|--------|-----------|-------|-------------|
| **atoms** | `git@atomgit.com:openairymax/atoms.git` | A | Microkernel primitives: corekern, coreloopthree, syscall, taskflow, frameworks, memory |
| **commons** | `git@atomgit.com:openairymax/commons.git` | A | Shared utilities: 24+ util modules (logging, sync, memory, string, ipc, etc.) |
| **cupolas** | `git@atomgit.com:openairymax/cupolas.git` | B | Safety dome: 4-layer inherent security (sandbox, RBAC, sanitization, audit) |
| **heapstore** | `git@atomgit.com:openairymax/heapstore.git` | A | Heap-backed runtime data persistence |
| **protocols** | `git@atomgit.com:openairymax/protocols.git` | — | AgentsIPC (128-byte message header) & A2A/A2T protocol stack |
| **gateway** | `git@atomgit.com:openairymax/gateway.git` | — | HTTP/WS/Stdio → JSON-RPC 2.0 gateway daemon (`gateway_d`) |
| **daemons** | `git@atomgit.com:openairymax/daemons.git` | — | 12 runtime daemons: gateway_d, llm_d, tool_d, sched_d, market_d, monit_d, channel_d, info_d, notify_d, observe_d, hook_d, plugin_d |

## Architecture (Layered)

```
⬇️ Ecosystem Layer    — Apps / Config / Prompts / Skills (in ecosystem/ repo)
⇅ Service Layer       — 12 daemon services (daemons/)
⇅ Protocol Layer      — AgentsIPC & A2A/A2T (protocols/)
⇅ Gateway Layer       — HTTP/WS/Stdio → JSON-RPC 2.0 (gateway/)
⇅ Storage Layer       — Runtime data persistence (heapstore/)
⇅ Security Layer      — 4-layer inherent security (cupolas/)
⇅ Kernel Layer        — 7 atomic modules (atoms/)
⇅ Support Layer       — Unified foundation library (commons/)
⬆️ SDK Layer          — Python/Go/Rust/TypeScript (in sdk/ repo)
```

## Build

### Prerequisites

- **OS**: Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
- **Compiler**: GCC 11+ / Clang 14+ (C11/C++17)
- **Build Tools**: CMake 3.20+
- **Libraries**: libsqlite3-dev, libcjson-dev, libyaml-dev, libcurl4-openssl-dev, libssl-dev

### Build Steps

```bash
# 1. Clone with submodules (from umbrella repo)
git clone --recursive git@atomgit.com:openairymax/airymaxhub.git
cd airymaxhub/agentrt

# 2. Configure (out-of-source build required by BAN-33)
cmake -S . -B /tmp/agentrt-build -DCMAKE_BUILD_TYPE=Release -DAGENTRT_WITH_MEMORYROVOL=ON

# 3. Build
cmake --build /tmp/agentrt-build --parallel $(nproc)

# 4. Test
cd /tmp/agentrt-build && ctest --output-on-failure
```

### Key CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests |
| `AGENTRT_WITH_MEMORYROVOL` | OFF | Enable MemoryRovol commercial memory provider |
| `AGENTRT_COMPLIANCE_STRICT` | ON | Strict compliance mode (poison unsafe functions) |
| `ENABLE_SANITIZERS` | ON | Enable ASan + LSan + UBSan |

## Branch Strategy

- This management repo: **`main`** only
- Leaf repos: **`feature/official-hubs-01`** (active development)

## License

Dual-licensed under **AGPL v3 + Apache 2.0** (SPDX: `AGPL-3.0-or-later OR Apache-2.0`). See [LICENSE](LICENSE) for details.

Copyright (c) 2025-2026 **SPHARX Ltd.** All Rights Reserved.
