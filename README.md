# AgentRT — AirymaxAgentRT (AI Agent Runtime Platform Engineering)

> The foundational runtime platform engineering for AI Agent teams — positioned analogously to JVM/containerd for languages/containers.
> A management repository under the [airymaxhub](https://atomgit.com/openairymax/airymaxhub) umbrella, aggregating 7 leaf repositories as git submodules.

**Language:** English | [简体中文](README_zh.md)

[![Version](https://img.shields.io/badge/version-0.1.6a-5a6b7e)](https://atomgit.com/openairymax/agentrt)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)

---

## Overview

**AgentRT** (full name: **AirymaxAgentRT**, *AI Agent Runtime Platform Engineering*) is the runtime engineering layer of the Airymax platform — an OS-grade runtime substrate for AI Agent teams, positioned analogously to the JVM for languages and containerd for containers. Where the JVM provides a virtual machine for bytecode and containerd provides a runtime for containers, AgentRT provides the platform engineering mechanisms for orchestrating, scheduling, isolating, and observing teams of AI agents. The **0.1.1** release laid the sole foundational version (奠基版本); **0.1.2** converges it into a single source of truth (SSoT) and completes the platform essence (declarative self-healing + unified extension mechanism).

This repository is a **management repo** (git superproject). It aggregates **7 leaf repositories** as git submodules and inherits the **complete git history** of the original AgentRT monorepo. The repository URL retains its historical name `git@atomgit.com:openairymax/agentrt.git` to preserve commit continuity. AgentRT exposes the OS-level mechanisms required to run agent teams at scale: micro-core primitives, cognitive loops, memory stratification, security domes, IPC protocols, gateway services, and long-running daemon processes.

AgentRT is a **management repo** (git superproject) under the user-space engineering super-repo `agent-runtim` (sibling super-repos under the `airymaxhub` umbrella: kernel-space `agent-linux` plus top-level `docs`, `closed-docs`, `devtools`, `closed-dev-build`). The Airymax workspace is partitioned into 38 repositories in total: 1 umbrella repo + 2 engineering super-repos + 4 top-level repos + the management/leaf repos underneath. Each leaf repo is independently buildable and version-controlled, while the management repo pins them together via git submodules to produce a coherent, reproducible runtime platform.

### 0.1.2 Capabilities (SSoT Convergence + Platform Essence)

The 0.1.2 release applies the **Unify Design SSoT (Single Source of Truth)** methodology to converge all 8 multi-authority conflict points (S-1~S-8) inside agentrt into one authoritative implementation each — not by cutting features, but by removing redundant authorities:

| Area | SSoT convergence |
|------|------------------|
| **Error codes (A-UEF)** | `airy_types.h` unified to [SC] `airymax/error.h` as the sole authority |
| **Logging (A-ULP)** | `AIRY_LOG_*` becomes the only log macro family across the repo |
| **Dual IPC headers (S-3)** | `are_ipc.h` and kernel `uapi/airymax/ipc.h` layout divergences merged (Layout C v4, magic `'ARE1'`) |
| **Dual DAG engines (S-4)** | Half-finished Pregel BSP superstep path removed; `graph_engine` = execution authority, `taskflow_advanced` = scheduling authority |
| **Event stream (S-6)** | hall_store task-file model + gseq global monotonic sequence becomes the authoritative event-stream substrate |
| **[SC] contracts (S-8)** | commons 11 headers byte-aligned with the kernel side |

Platform essence completion (analogous to Kubernetes controllers):

- **Declarative self-healing (reconcile)**: cognition-layer (blueprint→GRAD→replan), execution-layer (task failure auto re-dispatch), and lifecycle-layer (agent daemon self-heal restart via `AIRY_SELF_HEAL`, with restart caps + backoff) — all driven from the CLI main loop.
- **Unified extension mechanism**: memory provider vtable capability-seam generalized to a unified registry (`airy_ext.h`, first validated on the memory domain), the base for LLM/tool/storage/sandbox extension seams.

Branded IDs (`airy_trace_id_t` / `airy_msg_id_t` opaque types) and a CLI/TUI experience overhaul complete the release:

- **Cognitive Parallel Review (CPR)**: in the cognition stage (after GCCP, before planning), multiple cognitive sub-agents run in parallel threads — intent confirmation (independent task/chat/agent verdict) + problem review (ambiguity/risk/missing-info) — aggregated into a cognitive decision (confidence-weighted intent vote + risk merge) written to working memory for Phase-1 planning; degrades gracefully without blocking when the LLM is unavailable.
- **Built-in web search / fetch tool loop**: the chat loop exposes `web_search`/`web_fetch` to the LLM (OpenAI function-calling) — CLI executes them against the real `tool_d` implementations (Bing search + URL fetch), feeds results back as `role="tool"`, and continues until no tool calls remain, then renders the final reply as markdown; tool-use cards (`⛏` + collapsed summaries) keep the screen clean.
- **GCCP one-question-at-a-time**: after each user answer the LLM thinks over the answered content (`airy_gccp_step`) to decide convergence or a targeted follow-up, avoiding mechanical questioning (follow-up cap = question count + 4).
- **TUI input polish (readline/Claude Code paradigm)**: word movement (Ctrl/Alt+←→, Alt+b/f), Ctrl+T transpose, kill-ring + Ctrl+Y yank, Ctrl+S forward search (symmetric to Ctrl+R), bracketed paste, SIGWINCH redraw, full raw mode (flow control cleared so Ctrl+S is never swallowed).

### 0.1.1 Framework-ization (Mechanism/Policy Separation)

The 0.1.1 foundation release brings **mechanism/policy framework-ization** into the cognition pipeline, closing the product loop of "understanding user intent" and "driving real agents":

| Capability | Mechanism layer (agentrt) | Policy layer (product/caller) |
|------------|---------------------------|-------------------------------|
| **GCCP goal confirmation** | Two-phase protocol (probe/confirm) + 5-question goal model + heuristic fallback | Interaction callback (ask endpoint/start/bottleneck/audience) |
| **Work Hall** | Task-graph registration / status board / cancel + `airy_orch_ops_t` injection | Plan→DAG adaptation + handler binding |
| **Interactive product CLI** | `tools/airy_cli` (CMake option `BUILD_CLI`, default ON) | Product-level interaction policy |

Product loop: **natural-language big-task instruction → GCCP intent confirmation → cognition planning → Plan→TaskFlow DAG adaptation → Work Hall submit/board → agent_d drives ecosystem/agents for real execution**.

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
│   │   ├── gateway/            ← submodule: HTTP/WS/Stdio → JSON-RPC 2.0 gateway
│   │   ├── daemons/            ← submodule: 18 runtime daemons
│   │   ├── contracts/          ← contract headers (symlink → atoms/contracts)
│   │   ├── cmake/              ← build-system modules (moved from umbrella since v0.1.2, IRON-9 [IND])
│   │   ├── scripts/            ← official installer install.sh/install.ps1 (moved from umbrella since v0.1.2)
│   │   ├── CMakeLists.txt      ← top-level CMake entry point
│   │   └── Doxyfile            ← API documentation configuration
│   ├── sdk/                    ← SDK management repo
│   ├── ecosystem/              ← Ecosystem management repo
│   └── products/               ← Products management repo
├── agent-linux/                ← kernel-space engineering super-repo (formerly agentrt-linux, renamed v0.1.3)
├── tools/                      ← Development tools (renamed from devtools in v0.1.4)
├── docs/                       ← Open documentation
├── closed-docs/                ← Internal documentation
└── devbuild-closed/           ← Internal build/deploy (formerly closed-dev-build / build-closed)
```

## Leaf Repositories

| Module | Repository URL | Class | Description |
|--------|---------------|-------|-------------|
| **atoms** | `git@atomgit.com:openairymax/atoms.git` | A | Micro-core primitives: `corekern`, `coreloopthree`, `syscall`, `taskflow`, `memory` (5 modules, frameworks removed since 0.1.5a) |
| **commons** | `git@atomgit.com:openairymax/commons.git` | A | Shared foundation library: 24+ util modules (logging, sync, memory, string, ipc, etc.) |
| **cupolas** | `git@atomgit.com:openairymax/cupolas.git` | B | Safety dome: 4-layer inherent security (sandbox, RBAC, sanitization, audit) |
| **heapstore** | `git@atomgit.com:openairymax/heapstore.git` | A | Heap-backed runtime data persistence |
| **protocols** | `git@atomgit.com:openairymax/protocols.git` | — | AgentsIPC (128-byte message header) & A2A/A2T protocol stack |
| **gateway** | `git@atomgit.com:openairymax/gateway.git` | — | HTTP/WS/Stdio → JSON-RPC 2.0 gateway daemon (`gateway_d`) |
| **daemons** | `git@atomgit.com:openairymax/daemons.git` | — | 18 runtime daemons: `gateway_d`, `llm_d`, `tool_d`, `sched_d`, `market_d`, `monit_d`, `channel_d`, `info_d`, `notify_d`, `observe_d`, `hook_d`, `plugin_d`, `mem_d`, `agent_d`, `a2a_d`, `think_d`, `cupolas_d`, `maths_d` |

> **Class legend:** A = foundational/atomic (depended upon by upper layers); B = behavioral/safety; — = service/composition layer.

## Architecture (Layered)

AgentRT follows a cyclic layered architecture. Each layer depends only on the layers below it; the Support Layer provides the unified foundation that the SDK Layer ultimately binds back to, closing the loop.

```
⬇️  SDK Layer          — Python / Go / Rust / TypeScript SDKs                       (sdk/ repo)
⇅   Service Layer      — 18 daemon services                                          (daemons/)
⇅   Protocol Layer     — AgentsIPC & A2A/A2T protocol stack                          (protocols/)
⇅   Gateway Layer      — HTTP / WS / Stdio → JSON-RPC 2.0 gateway daemon             (gateway/)
⇅   Storage Layer      — Heap-backed runtime data persistence                        (heapstore/)
⇅   Security Layer     — 4-layer inherent safety dome                                (cupolas/)
⇅   Kernel Layer       — 5 atomic microkernel modules                                (atoms/)
⇅   Support Layer      — Unified foundation library (24+ util modules)               (commons/)
⬆️  SDK Layer          — (cyclic) SDKs bind back to foundation & expose to consumers  (sdk/ repo)
```

**Layer responsibilities:**

- **SDK Layer** — Language bindings (Python/Go/Rust/TypeScript) that expose AgentRT APIs to agent developers. Sits at the top of the stack and closes the cycle by depending on the Support Layer foundation.
- **Service Layer** — 18 long-running daemon processes that implement runtime orchestration: scheduling, tool dispatch, LLM bridging, dual-think cognition, memory, multi-agent collaboration, monitoring, notifications, and plugin management.
- **Protocol Layer** — AgentsIPC (fixed 128-byte message header) for in-process and cross-process messaging, plus A2A (agent-to-agent) and A2T (agent-to-tool) protocol stacks.
- **Gateway Layer** — `gateway_d` translates HTTP, WebSocket, and stdio transports into a unified JSON-RPC 2.0 stream, providing the external entry point into the runtime.
- **Storage Layer** — `heapstore` provides heap-backed persistence for runtime state, agent memory, and transient data.
- **Security Layer** — `cupolas` enforces 4-layer inherent security: sandbox isolation, RBAC authorization, input/output sanitization, and audit logging.
- **Kernel Layer** — `atoms` contains the 5 atomic microkernel modules (`corekern`, `coreloopthree`, `syscall`, `taskflow`, `memory`) that provide scheduling, cognitive loops, and memory primitives.
- **Support Layer** — `commons` provides the 24+ shared utility modules (logging, synchronization, memory, string handling, IPC helpers) that every other layer builds upon.

## Quick Install (End Users)

One-line install for end users — no compilation required. The installer always
picks the **latest release** (`releases/download/latest/`), so the command never
needs a version bump, and runs GPG verification + sha256 checksum + architecture
self-check (x86_64 / aarch64 / riscv64) automatically:

```bash
# Linux / macOS
curl -fsSL "https://atomgit.com/openairymax/agentrt/releases/download/latest/install.sh" | bash

# Windows PowerShell
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

# 2. Configure (out-of-source build is MANDATORY — enforced by BAN-33)
cmake -S . -B /tmp/agentrt-build \
    -DCMAKE_BUILD_TYPE=Release \
    -DAIRY_WITH_MEMORYROVOL=ON

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
| `AIRY_BUILD_ALL` | ON | Build all AgentRT components |
| `AIRY_WITH_MEMORYROVOL` | OFF | Enable MemoryRovol commercial memory provider |
| `AIRY_MEMORY_BACKEND` | `builtin` | Memory backend selection (`builtin` \| `memoryrovol`) |
| `BUILD_CLI` | ON | Build the interactive product CLI (GCCP + Work Hall loop) |
| `AIRY_COMPLIANCE_STRICT` | ON | Strict compliance mode (poisons unsafe functions, e.g. `strcpy`) |
| `ENABLE_SANITIZERS` | ON | Enable ASan + LSan + UBSan |
| `ENABLE_COVERAGE` | OFF | Enable code coverage reporting |
| `WARNINGS_AS_ERRORS` | OFF | Treat compiler warnings as errors |

## Branch Strategy

- **This management repo**: `main` branch only — stable, release-tagged.
- **Leaf repositories**: `feature/official-hubs-01` — active development branch tracked by each submodule.

Submodule pins in `.gitmodules` reference `feature/official-hubs-01` for all 7 leaf repos. The management repo's `main` branch records the exact commit each submodule should resolve to, ensuring reproducible builds for every Airymax release (0.1.1 foundation and 0.1.2 onwards).

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
