# AgentRT — AirymaxAgentRT (AI Agent Runtime Platform Engineering)

> The foundational runtime platform engineering for AI Agent teams — positioned analogously to JVM/containerd for languages/containers.
> A management repository under the [airymaxhub](https://atomgit.com/openairymax/airymaxhub) umbrella, aggregating 7 leaf repositories as git submodules.

**Language:** English | [简体中文](README_zh.md)

[![Version](https://img.shields.io/badge/version-0.1.9-5a6b7e)](https://atomgit.com/openairymax/agentrt)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)

---

## Overview

**AgentRT** (full name: **AirymaxAgentRT**, *AI Agent Runtime Platform Engineering*) is the runtime engineering layer of the Airymax platform — an OS-grade runtime substrate for AI Agent teams, positioned analogously to Kubernetes for microservices: AgentRT standardizes multi-agent cognitive loops, memory evolution, security isolation, and protocol interoperability into a unified runtime platform, defining how AI agent teams run.

This repository is a **management repo** (git superproject). It aggregates **7 leaf repositories** as git submodules and inherits the **complete git history** of the original AgentRT monorepo. The repository URL retains its historical name `git@atomgit.com:openairymax/agentrt.git` to preserve commit continuity. AgentRT exposes the OS-level mechanisms required to run agent teams at scale: micro-core primitives, cognitive loops, memory stratification, security domes, IPC protocols, gateway services, and long-running daemon processes.

AgentRT is a **management repo** (git superproject) under the user-space engineering super-repo `agent-workload` (renamed from `agent-runtim` in v0.1.4). Sibling repos under the `airymaxhub` umbrella include the kernel-space `agent-linux` plus toolchain and documentation repos; all user-space runtime source code lives under `agent-workload`. Each leaf repo is independently buildable and version-controlled, while the management repo pins them together via git submodules to produce a coherent, reproducible runtime platform.

## Core Capabilities

AgentRT delivers the following capabilities to end users and agent developers:

| Capability | Description |
|------------|-------------|
| **Chat & Task dual modes** | A chat mode for conversation and a task mode for complex execution, both driven by `airy_cli` / `agentrt-tui`; the same instruction automatically enters a plan-execute loop for large tasks |
| **Multi-agent collaboration** | One main Agent orchestrates multiple child Agents (executors), splitting and merging complex work in parallel; agents interoperate over a standard protocol |
| **Intelligent planning & reflection** | Interactive confirmation of ambiguous goals, plan generation as an executable task graph, and scheduled execution; execution includes self-reflection and failure re-dispatch |
| **Persistent memory** | Built-in free memory backend (L1 session-level + L2 cross-session) with forgetting and retrieval; extensible to commercial memory providers |
| **Built-in tool loop** | The chat loop exposes real tools such as `web_search` / `web_fetch` to the LLM (Bing search + URL fetch), feeds results back as `role="tool"`, and renders the final reply as markdown |
| **Security dome** | Four layers of inherent security: sandbox isolation, RBAC authorization, input/output sanitization, audit logging — fail-closed by default |
| **Unified gateway** | `gateway_d` translates HTTP, WebSocket, and stdio into a unified JSON-RPC 2.0 stream, giving external systems a single entry point into the runtime |
| **15 runtime services** | Long-running daemons for scheduling, tool dispatch, LLM bridging, dual-think cognition, memory, monitoring, notifications, tool/plugin execution, and Agent execution — orchestrated by dependency order with self-healing |
| **Observability** | Global event stream + task board + health monitoring; `airymaxrt monitor` provides a runtime status overview |

## Repository Structure

```
airymaxhub/                     ← Umbrella repo (git superproject root)
├── agent-workload/             ← user-space engineering super-repo (renamed from agent-runtim in v0.1.4)
│   ├── agentrt/                ← THIS REPO (management repo)
│   │   ├── atoms/              ← submodule: micro-core primitives (A-class)
│   │   ├── commons/            ← submodule: shared foundation utilities (A-class)
│   │   ├── cupolas/            ← submodule: safety dome (B-class)
│   │   ├── heapstore/          ← submodule: heap-backed storage (A-class)
│   │   ├── protocols/          ← submodule: AgentsIPC & A2A/A2T protocol stack
│   │   ├── gateway/            ← submodule: HTTP/WS/SSE/MCP/A2A/OpenAI → JSON-RPC 2.0 gateway
│   │   ├── daemons/            ← submodule: 15 runtime daemons + daemon framework
│   │   ├── cmake/              ← build-system modules
│   │   ├── scripts/            ← official installer install.sh/install.ps1
│   │   ├── tests/              ← smoke & toolchain self-tests
│   │   ├── tools/              ← internal tools & quality gates (airy_cli / airy_depgraph, …)
│   │   ├── CMakeLists.txt      ← top-level CMake entry point
│   │   ├── VERSION             ← version single source of truth (SSoT)
│   │   └── Doxyfile            ← API documentation configuration
│   ├── sdk/                    ← SDK management repo (cli/tui/sdk-rust/sdk-python/sdk-go/sdk-typescript)
│   ├── ecosystem/              ← Ecosystem management repo
│   └── products/               ← Products management repo
├── agent-linux/                ← kernel-space engineering super-repo (formerly agentrt-linux, renamed v0.1.3)
└── tools/                      ← CI / quality gates / release toolchain repo (renamed from devtools in v0.1.4)
```

## Leaf Repositories

| Module | Repository URL | Class | Description |
|--------|---------------|-------|-------------|
| **atoms** | `git@atomgit.com:openairymax/atoms.git` | A | Micro-core system layer: `corekern`, `coreloopthree`, `syscall`, `taskflow`, `memory` (5 modules) |
| **commons** | `git@atomgit.com:openairymax/commons.git` | A | Shared foundation library: authoritative type/error contracts + 32 cohesive util modules |
| **cupolas** | `git@atomgit.com:openairymax/cupolas.git` | B | Safety dome: 4-layer inherent security + policy decision point (PDP) / local enforcement points (PEPs) |
| **heapstore** | `git@atomgit.com:openairymax/heapstore.git` | A | Heap-backed runtime data persistence |
| **protocols** | `git@atomgit.com:openairymax/protocols.git` | — | AgentsIPC (128-byte message header) & A2A/A2T protocol stack |
| **gateway** | `git@atomgit.com:openairymax/gateway.git` | — | HTTP/WS/SSE/MCP/A2A/OpenAI → JSON-RPC 2.0 gateway — the sole process boundary (`gateway_d`) |
| **daemons** | `git@atomgit.com:openairymax/daemons.git` | — | 15 runtime daemons: `gateway_d`, `agent_d`, `llm_d`, `tool_d`, `sched_d`, `think_d`, `mem_d`, `market_d`, `monit_d`, `notify_d`, `channel_d`, `a2a_d`, `cupolas_d`, `maths_d`, `hook_d` (steady state since 0.1.9: plugin execution folded into `tool_d`, observability unified in `monit_d`) |

> **Class legend:** A = foundational/atomic (depended upon by upper layers); B = behavioral/safety; — = service/composition layer.

## Architecture (Layered)

AgentRT follows a cyclic layered architecture. Each layer depends only on the layers below it; the Support Layer provides the unified foundation that the SDK Layer ultimately binds back to, closing the loop.

```
⬇️  SDK Layer          — Rust CLI/TUI + Python / Go / Rust / TypeScript SDKs          (sdk/ repo)
⇅   Service Layer      — 15 daemon services (runtime orchestration)                   (daemons/)
⇅   Protocol Layer     — AgentsIPC & A2A/A2T protocol stack                          (protocols/)
⇅   Gateway Layer      — HTTP / WS / SSE / MCP / A2A / OpenAI → JSON-RPC 2.0         (gateway/)
⇅   Storage Layer      — Heap-backed runtime data persistence                        (heapstore/)
⇅   Security Layer     — 4-layer inherent safety dome (PDP / PEPs)                   (cupolas/)
⇅   Kernel Layer       — 5 atomic microkernel modules                                (atoms/)
⇅   Support Layer      — Unified foundation library (32 cohesive util modules)       (commons/)
⬆️  SDK Layer          — (cyclic) SDKs bind back to foundation & expose to consumers  (sdk/ repo)
```

**Layer responsibilities:**

- **SDK Layer** — Rust CLI/TUI and multi-language SDKs (Python/Go/Rust/TypeScript) that expose AgentRT APIs to agent developers. Sits at the top of the stack and closes the cycle by depending on the Support Layer foundation.
- **Service Layer** — 15 long-running daemon processes (steady state) that implement runtime orchestration: scheduling, tool dispatch & plugin execution, LLM bridging, dual-think cognition, memory, multi-agent collaboration (A2A), monitoring & alerting, and event notification — started by dependency order with self-healing.
- **Protocol Layer** — AgentsIPC (fixed 128-byte message header) for in-process and cross-process messaging, plus A2A (agent-to-agent) and A2T (agent-to-tool) protocol stacks.
- **Gateway Layer** — `gateway_d` translates HTTP, WebSocket, SSE, MCP, A2A, and OpenAI transports into a unified JSON-RPC 2.0 stream — the sole process boundary into the runtime (protocol translation only, no business logic).
- **Storage Layer** — `heapstore` provides heap-backed persistence for runtime state, agent memory, and transient data.
- **Security Layer** — `cupolas` enforces 4-layer inherent security (sandbox isolation, RBAC authorization, input/output sanitization, audit logging); its policy decision point (PDP) loads/distributes/rolls back policies, applied locally by each daemon via policy enforcement points (PEPs) within seconds.
- **Kernel Layer** — `atoms` contains the 5 atomic microkernel modules (`corekern`, `coreloopthree`, `syscall`, `taskflow`, `memory`) providing scheduling, the three-layer cognitive-loop mechanism, and memory primitives.
- **Support Layer** — `commons` provides the 32 cohesive utility modules (logging, synchronization, memory, string handling, IPC helpers) that every other layer builds upon.

## Quick Install (End Users)

One-line install for end users — no compilation required. The installer always
picks the **latest release** (`releases/download/latest/`), so the command never
needs a version bump, and runs GPG verification + sha256 checksum + architecture
self-check (x86_64 / aarch64 / riscv64) automatically:

```bash
# Linux / macOS
curl -fsSL "https://atomgit.com/openairymax/agentrt/releases/download/latest/install.sh" | bash

# Windows (atoms/commons are closed-source; native install unavailable —
# use WSL2): `wsl --install` (WSL2 + Ubuntu), then run the Linux one-liner
# above inside WSL. No Windows prebuilt package / atoms prebuilt module is
# published yet, so the PowerShell installer falls back to a source build
# that cannot complete without the closed modules (known 0.1.6b limitation).
powershell -ExecutionPolicy Bypass -Command "irm https://atomgit.com/openairymax/agentrt/releases/download/latest/install.ps1 | iex"
```

Common variants:

```bash
# Custom prefix (default: $HOME/.airymaxrt)
curl -fsSL "https://atomgit.com/openairymax/agentrt/releases/download/latest/install.sh" | \
   bash -s -- --prefix "$HOME/.airymaxrt"

# Beta channel (more aggressive updates)
curl -fsSL "https://atomgit.com/openairymax/agentrt/releases/download/latest/install.sh" | bash -s -- --channel beta

# Uninstall (--keep-data preserves memory data)
curl -fsSL "https://atomgit.com/openairymax/agentrt/releases/download/latest/install.sh" | bash -s -- --uninstall
```

After install, `airymaxrt` is on PATH and `airymaxrt start` launches the runtime.
The installer auto-profiles the hardware (full/minimal runtime profile) and the
`airymaxrt monitor` daemon restores trimmed features when you add RAM or GPUs.

## Build

### Prerequisites

- **OS**: Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2)
- **Compiler**: GCC 11+ / Clang 14+ (C11 required)
- **Build Tools**: CMake 3.20+, Ninja (recommended) or Make
- **Libraries**: libsqlite3-dev, libcjson-dev, libyaml-dev, libcurl4-openssl-dev, libssl-dev

### Build Steps

```bash
# 1. Clone the umbrella repo with all submodules (recursive)
git clone --recursive git@atomgit.com:openairymax/airymaxhub.git
cd airymaxhub/agentrt

# 2. Configure (out-of-source build is MANDATORY)
cmake -S . -B /tmp/agentrt-build \
    -DCMAKE_BUILD_TYPE=Release \
    -DAIRY_WITH_MEMORYROVOL=ON

# 3. Build (parallel)
cmake --build /tmp/agentrt-build --parallel $(nproc)

# 4. Run the test suite
cd /tmp/agentrt-build && ctest --output-on-failure
```

> **Note:** In-source builds are forbidden. The build directory must reside outside the source tree; CMake will emit a `FATAL_ERROR` if it detects a build directory inside the source tree.

### Key CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests (CTest enabled at the top level) |
| `BUILD_SHARED_LIBS` | OFF | Build shared libraries instead of static libraries |
| `AIRY_BUILD_ALL` | ON | Build all AgentRT components |
| `AIRY_WITH_MEMORYROVOL` | OFF | Enable MemoryRovol commercial memory provider |
| `AIRY_MEMORY_BACKEND` | `builtin` | Memory backend selection (`builtin` \| `memoryrovol`) |
| `BUILD_CLI` | ON | Build the interactive product CLI |
| `AIRY_COMPLIANCE_STRICT` | ON | Strict compliance mode (poisons unsafe functions, e.g. `strcpy`) |
| `ENABLE_SANITIZERS` | ON | Enable ASan + LSan + UBSan |
| `ENABLE_COVERAGE` | OFF | Enable code coverage reporting |
| `WARNINGS_AS_ERRORS` | OFF | Treat compiler warnings as errors |

## Branch Strategy

- **This management repo (agentrt)**: `main` is the active development branch — stable, release-tagged.
- **Leaf repositories**: `develop/hubs-01` is the standard development branch; `main` is a release snapshot (synced from `develop/hubs-01` once per release, not for day-to-day work).

Aggregation uses gitlinks (commit-hash pins), independent of branch names: this repo's `main` records the exact commit each leaf repo should resolve to, ensuring reproducible builds for every Airymax release. On each release the leaf snapshots are synced and the gitlink pins are bumped level by level.

## License

Dual-licensed under **AGPL v3 + Apache 2.0** (SPDX identifier: `AGPL-3.0-or-later OR Apache-2.0`). See [LICENSE](LICENSE) for the full text.

Recipients may choose either license to govern their use of AgentRT. The AGPL v3 applies to derivative network services; the Apache 2.0 applies to proprietary integrations.

### Dual License Guide

You may choose **either** license at your option — not both, not neither.

**SPDX Expression**: `AGPL-3.0-or-later OR Apache-2.0`

| If you are... | Choose | Why |
|---------------|--------|-----|
| Building a **SaaS** or network service that modifies AgentRT | **AGPL v3** | Network service clause requires source disclosure |
| Developing **open-source** derivative works (copyleft) | **AGPL v3** | Derivatives must remain open-source under AGPL |
| Using AgentRT in **commercial closed-source** products | **Apache 2.0** | Permissive, allows proprietary derivatives |
| Building **enterprise internal tools** | **Apache 2.0** | No source disclosure required |
| Needing **patent protection** | **Apache 2.0** | Explicit patent grant from contributors |
| Just learning or researching | **Either** | Both permit personal use |

For the authoritative license policy, see [12-license-policy.md](../docs/AirymaxOS/50-engineering-standards/12-license-policy.md).

Copyright (c) 2025-2026 **SPHARX Ltd.** All Rights Reserved.
