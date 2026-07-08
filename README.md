# AgentRT — AirymaxAgentRT (AI Agent Runtime Platform Engineering)

> The foundational runtime platform engineering for AI Agent teams — positioned analogously to JVM/containerd for languages/containers.
> A management repository under the [airymaxhub](https://atomgit.com/openairymax/airymaxhub) umbrella, aggregating 7 leaf repositories as git submodules.

**Language:** English | [简体中文](README_zh.md)

[![Version](https://img.shields.io/badge/version-0.1.1-5a6b7e)](https://atomgit.com/openairymax/agentrt)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)](https://isocpp.org)

---

## Overview

**AgentRT** (full name: **AirymaxAgentRT**, *AI Agent Runtime Platform Engineering*) is the runtime engineering layer of the Airymax platform — an OS-grade runtime substrate for AI Agent teams, positioned analogously to the JVM for languages and containerd for containers. Where the JVM provides a virtual machine for bytecode and containerd provides a runtime for containers, AgentRT provides the platform engineering mechanisms for orchestrating, scheduling, isolating, and observing teams of AI agents. The **0.1.1** release is the sole foundational version (奠基版本) on which all subsequent Airymax releases are built.

This repository is a **management repo** (git superproject). It aggregates **7 leaf repositories** as git submodules and inherits the **complete git history** of the original AgentRT monorepo. The repository URL retains its historical name `git@atomgit.com:openairymax/agentrt.git` to preserve commit continuity. AgentRT exposes the OS-level mechanisms required to run agent teams at scale: microkernel primitives, cognitive loops, memory stratification, security domes, IPC protocols, gateway services, and long-running daemon processes.

AgentRT is **one of five management repositories** under the `airymaxhub` umbrella (the other four being `sdk`, `ecosystem`, `products`, and `agentrt-linux`). The Airymax workspace is partitioned into 38 repositories in total: 1 umbrella repo + 5 management repos + 29 leaf repos + 3 top-level repos. Each leaf repo is independently buildable and version-controlled, while the management repo pins them together via git submodules to produce a coherent, reproducible runtime platform.

## Repository Structure

```
airymaxhub/                     ← Umbrella repo (git superproject root)
├── agentrt/                    ← THIS REPO (management repo)
│   ├── atoms/                  ← submodule: microkernel primitives (A-class)
│   ├── commons/                ← submodule: shared foundation utilities (A-class)
│   ├── cupolas/                ← submodule: safety dome (B-class)
│   ├── heapstore/              ← submodule: heap-backed storage (A-class)
│   ├── protocols/              ← submodule: AgentsIPC & A2A/A2T protocol stack
│   ├── gateway/                ← submodule: HTTP/WS/Stdio → JSON-RPC 2.0 gateway
│   ├── daemons/                ← submodule: 12 runtime daemons
│   ├── contracts/              ← contract headers (symlink → atoms/contracts)
│   ├── CMakeLists.txt          ← top-level CMake entry point
│   └── Doxyfile                ← API documentation configuration
├── sdk/                        ← SDK management repo
├── ecosystem/                  ← Ecosystem management repo
├── products/                   ← Products management repo
├── agentrt-linux/              ← AgentRT-Linux management repo
├── cmake/                      ← Shared CMake modules (5 general + 4 AgentRT-specific)
├── devtools/                   ← Development tools
├── docs/                       ← Open documentation
└── docs-closed/                ← Internal documentation
```

## Leaf Repositories

| Module | Repository URL | Class | Description |
|--------|---------------|-------|-------------|
| **atoms** | `git@atomgit.com:openairymax/atoms.git` | A | Microkernel primitives: `corekern`, `coreloopthree`, `syscall`, `taskflow`, `frameworks`, `memory` |
| **commons** | `git@atomgit.com:openairymax/commons.git` | A | Shared foundation library: 24+ util modules (logging, sync, memory, string, ipc, etc.) |
| **cupolas** | `git@atomgit.com:openairymax/cupolas.git` | B | Safety dome: 4-layer inherent security (sandbox, RBAC, sanitization, audit) |
| **heapstore** | `git@atomgit.com:openairymax/heapstore.git` | A | Heap-backed runtime data persistence |
| **protocols** | `git@atomgit.com:openairymax/protocols.git` | — | AgentsIPC (128-byte message header) & A2A/A2T protocol stack |
| **gateway** | `git@atomgit.com:openairymax/gateway.git` | — | HTTP/WS/Stdio → JSON-RPC 2.0 gateway daemon (`gateway_d`) |
| **daemons** | `git@atomgit.com:openairymax/daemons.git` | — | 12 runtime daemons: `gateway_d`, `llm_d`, `tool_d`, `sched_d`, `market_d`, `monit_d`, `channel_d`, `info_d`, `notify_d`, `observe_d`, `hook_d`, `plugin_d` |

> **Class legend:** A = foundational/atomic (depended upon by upper layers); B = behavioral/safety; — = service/composition layer.

## Architecture (Layered)

AgentRT follows a cyclic layered architecture. Each layer depends only on the layers below it; the Support Layer provides the unified foundation that the SDK Layer ultimately binds back to, closing the loop.

```
⬇️  SDK Layer          — Python / Go / Rust / TypeScript SDKs                       (sdk/ repo)
⇅   Service Layer      — 12 daemon services                                          (daemons/)
⇅   Protocol Layer     — AgentsIPC & A2A/A2T protocol stack                          (protocols/)
⇅   Gateway Layer      — HTTP / WS / Stdio → JSON-RPC 2.0 gateway daemon             (gateway/)
⇅   Storage Layer      — Heap-backed runtime data persistence                        (heapstore/)
⇅   Security Layer     — 4-layer inherent safety dome                                (cupolas/)
⇅   Kernel Layer       — 7 atomic microkernel modules                                (atoms/)
⇅   Support Layer      — Unified foundation library (24+ util modules)               (commons/)
⬆️  SDK Layer          — (cyclic) SDKs bind back to foundation & expose to consumers  (sdk/ repo)
```

**Layer responsibilities:**

- **SDK Layer** — Language bindings (Python/Go/Rust/TypeScript) that expose AgentRT APIs to agent developers. Sits at the top of the stack and closes the cycle by depending on the Support Layer foundation.
- **Service Layer** — 12 long-running daemon processes that implement runtime orchestration: scheduling, tool dispatch, LLM bridging, monitoring, notifications, and plugin management.
- **Protocol Layer** — AgentsIPC (fixed 128-byte message header) for in-process and cross-process messaging, plus A2A (agent-to-agent) and A2T (agent-to-tool) protocol stacks.
- **Gateway Layer** — `gateway_d` translates HTTP, WebSocket, and stdio transports into a unified JSON-RPC 2.0 stream, providing the external entry point into the runtime.
- **Storage Layer** — `heapstore` provides heap-backed persistence for runtime state, agent memory, and transient data.
- **Security Layer** — `cupolas` enforces 4-layer inherent security: sandbox isolation, RBAC authorization, input/output sanitization, and audit logging.
- **Kernel Layer** — `atoms` contains the 7 atomic microkernel modules (`corekern`, `coreloopthree`, `syscall`, `taskflow`, `frameworks`, `memory`) that provide scheduling, cognitive loops, and memory primitives.
- **Support Layer** — `commons` provides the 24+ shared utility modules (logging, synchronization, memory, string handling, IPC helpers) that every other layer builds upon.

## Build

### Prerequisites

- **OS**: Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
- **Compiler**: GCC 11+ / Clang 14+ (C11 and C++17 required)
- **Build Tools**: CMake 3.20+, Ninja (recommended) or Make
- **Libraries**: libsqlite3-dev, libcjson-dev, libyaml-dev, libcurl4-openssl-dev, libssl-dev

### Build Steps

```bash
# 1. Clone the umbrella repo with all submodules (recursive)
git clone --recursive git@atomgit.com:openairymax/airymaxhub.git
cd airymaxhub/agentrt

# 2. Configure (out-of-source build is MANDATORY — enforced by BAN-33)
cmake -S . -B /tmp/agentrt-build \
    -DCMAKE_BUILD_TYPE=Release \
    -DAGENTRT_WITH_MEMORYROVOL=ON

# 3. Build (parallel)
cmake --build /tmp/agentrt-build --parallel $(nproc)

# 4. Run the test suite
cd /tmp/agentrt-build && ctest --output-on-failure
```

> **BAN-33:** In-source builds are forbidden. The build directory must reside outside the source tree; CMake will emit a `FATAL_ERROR` if it detects a build directory inside the source tree.

### Key CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests (CTest enabled at the top level) |
| `BUILD_SHARED_LIBS` | OFF | Build shared libraries instead of static libraries |
| `AGENTRT_BUILD_ALL` | ON | Build all AgentRT components |
| `AGENTRT_WITH_MEMORYROVOL` | OFF | Enable MemoryRovol commercial memory provider |
| `AGENTRT_MEMORY_BACKEND` | `builtin` | Memory backend selection (`builtin` \| `memoryrovol`) |
| `AGENTRT_COMPLIANCE_STRICT` | ON | Strict compliance mode (poisons unsafe functions, e.g. `strcpy`) |
| `ENABLE_SANITIZERS` | ON | Enable ASan + LSan + UBSan |
| `ENABLE_COVERAGE` | OFF | Enable code coverage reporting |
| `WARNINGS_AS_ERRORS` | OFF | Treat compiler warnings as errors |
| `AGENTRT_DOCKER_BUILD` | OFF | Docker build mode |

## Branch Strategy

- **This management repo**: `main` branch only — stable, release-tagged.
- **Leaf repositories**: `feature/official-hubs-01` — active development branch tracked by each submodule.

Submodule pins in `.gitmodules` reference `feature/official-hubs-01` for all 7 leaf repos. The management repo's `main` branch records the exact commit each submodule should resolve to, ensuring reproducible builds across the Airymax 0.1.1 foundational release.

## License

Dual-licensed under **AGPL v3 + Apache 2.0** (SPDX identifier: `AGPL-3.0-or-later OR Apache-2.0`). See [LICENSE](LICENSE) for the full text.

Recipients may choose either license to govern their use of AgentRT. The AGPL v3 applies to derivative network services; the Apache 2.0 applies to proprietary integrations.

Copyright (c) 2025-2026 **SPHARX Ltd.** All Rights Reserved.
