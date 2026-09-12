# AgentRT — AI Agent Runtime Platform Engineering

> An OS-grade runtime substrate for AI agent teams — the layer that standardises
> how agents think, act, remember, and talk to each other, in the same way a
> container runtime standardises how services are packaged and scheduled.

**Language:** English | [简体中文](README_zh.md)

[![Version](https://img.shields.io/badge/version-0.1.15-5a6b7e)](https://atomgit.com/openairymax/agentrt/releases/tag/v0.1.15)
[![License](https://img.shields.io/badge/license-AGPL--3.0+Apache--2.0-4a90d9)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/11)

---

## Overview

**AgentRT** (full name **AirymaxAgentRT**) is the runtime layer of the Airymax
platform. It provides the mechanisms an agent team needs in order to run for a
long time on real hardware:

- a micro-kernel core with scheduling, syscalls and memory primitives,
- a three-stage cognitive loop (cognition → execution → memory),
- layered session and cross-session memory,
- a four-layer security dome that fails closed by default,
- an IPC and agent-to-agent protocol stack,
- a single gateway process that speaks HTTP / WebSocket / SSE / MCP / A2A /
  OpenAI and translates to JSON-RPC 2.0,
- a fleet of long-running daemons that orchestrate all of the above,
- a command-line interface and a terminal UI for interactive use.

Everything is written in C11 with language bindings for Python, Go, Rust and
TypeScript. The runtime is designed so that a single laptop and a multi-agent
deployment use exactly the same programming model.

This repository is the **release aggregate** for the runtime. The individual
components are maintained in their own repositories and are wired in as git
submodules, pinned to the exact commit that shipped with each release, so that
any published version can be reproduced commit for commit.

## Installation

### Linux and macOS

One command, no compilation required. The installer detects the platform,
downloads the matching prebuilt package, and verifies it:

```bash
curl -fsSL https://raw.githubusercontent.com/openairymax/agentrt/main/scripts/install.sh | bash
```

If GitHub raw content is not reachable from your network, the equivalent script
is published with every release:

```bash
curl -fsSL https://atomgit.com/openairymax/agentrt/releases/download/v0.1.15/install.sh | bash
```

### Windows

A native PowerShell installer is published for x86-64 and x86-32:

```powershell
irm https://atomgit.com/openairymax/agentrt/releases/download/v0.1.15/install.ps1 | iex
```

### Options

Append flags after `| bash -s --` (or pass them directly when running the
script from a file):

```bash
# Custom install prefix (default: $HOME/.airymaxrt)
... | bash -s -- --prefix "$HOME/.airymaxrt"

# Install from a release candidate instead of the stable channel
... | bash -s -- --channel rc

# Force a clean reinstall (clears the download cache, stops running daemons)
... | bash -s -- --reinstall

# Uninstall; add --keep-data to retain your memory data
... | bash -s -- --uninstall
```

The install root is recorded in `<root>/config/install.env`, so `airymaxrt
update`, `airymaxrt uninstall` and `--reinstall` all act on the installation
they belong to — you do not have to repeat `--prefix`. If several copies are
present on one machine, `which airymaxrt` shows which one your shell resolves.

### Supported platforms

| Platform | Architectures published |
|----------|-------------------------|
| Linux | x86-64, x86-32, arm-64 (aarch64), arm-32 (armv7l) |
| macOS | arm-64 (Apple Silicon), x86-64 |
| Windows | x86-64, x86-32 |

Packages are `.tar.gz` on Linux and macOS, `.zip` on Windows. Architectures not
listed above are not part of the prebuilt matrix; build them from source.

### First run

Start the runtime and its services by launching the terminal UI:

```bash
airymaxrt                 # terminal UI (falls back to the CLI on a dumb terminal)
airymaxrt cli             # force the plain command-line front end
airymaxrt status          # what is running right now
airymaxrt doctor          # component health check
airymaxrt logs 100        # last 100 lines of the runtime log
airymaxrt logs llm_d      # the log of one particular daemon
```

`airymaxrt profile` reports or overrides the hardware-derived runtime profile
(`full`, `minimal` or `auto`). The installer picks the profile for you, and
`airymaxrt monitor --daemon` keeps watching: if you add RAM or a GPU later, the
features that were trimmed are restored automatically. On Windows the same
commands are available through `airymaxrt`.

## Core capabilities

| Capability | What it gives you |
|------------|-------------------|
| **Chat and task modes** | A conversational mode for dialogue and a task mode for complex work, both driven by the CLI and the terminal UI. The same instruction automatically enters a plan-then-execute loop when the job is large. |
| **Multi-agent collaboration** | One lead agent orchestrates several child agents, splitting work in parallel and merging results back. Agents interoperate over a documented protocol. |
| **Planning and reflection** | Ambiguous goals are clarified interactively, then compiled into an executable task graph that is scheduled. Execution self-reflects and re-dispatches on failure. |
| **Persistent memory** | A built-in free memory backend with L1 session-level and L2 cross-session tiers, supporting retrieval and forgetting. Pluggable commercial backends are available. |
| **Tool loop** | The chat loop exposes real tools such as `web_search` and `web_fetch` to the model, feeds results back as tool messages, and renders the final answer as markdown. |
| **Security dome** | Four layers: sandbox isolation, RBAC authorisation, input/output sanitisation, audit logging. The default posture is fail-closed. |
| **Unified gateway** | `gateway_d` translates HTTP, WebSocket, SSE, MCP, A2A and OpenAI transports into one JSON-RPC 2.0 stream, so external systems have exactly one entry point. |
| **Fifteen runtime services** | Daemons for scheduling, agent execution, LLM bridging, tool dispatch, dual-think cognition, memory, marketplace, monitoring, notifications, channels, A2A, policy enforcement, maths and hooks — started in dependency order and self-healing. |
| **Observability** | A global event stream, a task board and health monitoring, surfaced by `airymaxrt status` and `airymaxrt doctor` and written to per-daemon log files. |

## Architecture

AgentRT is layered, and each layer depends only on the layers beneath it. The
SDK layer binds back to the support layer at the top of the stack, which is why
the dependency graph is drawn as a cycle.

```
SDK Layer        — CLI, terminal UI, Python / Go / Rust / TypeScript bindings
Service Layer    — 15 daemons that orchestrate the runtime        (daemons/)
Protocol Layer   — AgentsIPC and the A2A / A2T stacks             (protocols/)
Gateway Layer    — transports → JSON-RPC 2.0                      (gateway/)
Storage Layer    — heap-backed runtime persistence                (heapstore/)
Security Layer   — four-layer dome, policy decision and           (cupolas/)
                   enforcement points
Kernel Layer     — 5 atomic micro-kernel modules                  (atoms/)
Support Layer    — 32 cohesive utility modules + shared headers   (commons/)
```

- **Support layer (`commons`)** — the foundation every other layer builds on:
  logging, synchronisation, memory helpers, string handling, IPC helpers,
  configuration, observability, plus the authoritative type and error contracts.
- **Kernel layer (`atoms`)** — five atomic modules: `corekern` (init, lifecycle),
  `coreloopthree` (cognition → execution → memory loop), `syscall` (unified
  user/kernel-facing interface), `taskflow` (task graph and scheduling),
  `memory` (memory primitives and backends).
- **Security layer (`cupolas`)** — a policy decision point loads, distributes
  and rolls back policies; every daemon applies them locally through its own
  enforcement points, within seconds.
- **Storage layer (`heapstore`)** — persistence for runtime state, agent memory
  and transient data.
- **Gateway layer (`gateway`)** — protocol translation only, no business logic.
  It is the sole process boundary into the runtime.
- **Protocol layer (`protocols`)** — AgentsIPC with a fixed 128-byte message
  header for in-process and cross-process messaging, plus agent-to-agent (A2A)
  and agent-to-tool (A2T) stacks.
- **Service layer (`daemons`)** — the fifteen long-running processes that make
  the runtime an actual running system.
- **SDK layer** — lives in the sibling [`sdk/`](../sdk) tree and re-exports the
  layers below to application developers.

## Repository layout

```
agentrt/
├── atoms/                # micro-kernel primitives (submodule)
├── commons/              # shared foundation library (submodule)
├── cupolas/              # security dome (submodule)
├── gateway/              # protocol gateway (submodule)
├── heapstore/            # heap-backed persistence (submodule)
├── protocols/            # AgentsIPC, A2A / A2T (submodule)
├── daemons/              # 15 runtime daemons + daemon framework (submodule)
├── cmake/                # build-system modules
├── scripts/              # installers: install.sh, install.ps1
├── tests/                # smoke tests and toolchain self-tests
├── tools/                # developer tooling (airy_cli, airy_depgraph, codegen)
├── latest/               # rolling release manifests and signing keys
├── RELEASE_NOTES.d/      # user-facing notes, one file per release
├── LICENSES/             # additional license texts
├── CMakeLists.txt        # top-level build entry point
├── VERSION               # version this tree was released at
└── Doxyfile              # API documentation configuration
```

`dist/` is a local build-and-package staging directory. It is not part of the
repository.

## Components

Each component is developed in its own repository. Clone this repository
recursively to get all of them at the pinned revisions.

| Component | Repository | Responsibility |
|-----------|-----------|----------------|
| **atoms** | [openairymax/atoms](https://atomgit.com/openairymax/atoms) | Micro-kernel layer: `corekern`, `coreloopthree`, `syscall`, `taskflow`, `memory` |
| **commons** | [openairymax/commons](https://atomgit.com/openairymax/commons) | Type and error contracts, plus 32 cohesive utility modules |
| **cupolas** | [openairymax/cupolas](https://atomgit.com/openairymax/cupolas) | Four-layer security dome: policy decision point and enforcement points |
| **heapstore** | [openairymax/heapstore](https://atomgit.com/openairymax/heapstore) | Heap-backed runtime data persistence |
| **protocols** | [openairymax/protocols](https://atomgit.com/openairymax/protocols) | AgentsIPC (128-byte header), A2A and A2T protocol stacks |
| **gateway** | [openairymax/gateway](https://atomgit.com/openairymax/gateway) | HTTP / WS / SSE / MCP / A2A / OpenAI → JSON-RPC 2.0 |
| **daemons** | [openairymax/daemons](https://atomgit.com/openairymax/daemons) | `gateway_d`, `agent_d`, `llm_d`, `tool_d`, `sched_d`, `think_d`, `mem_d`, `market_d`, `monit_d`, `notify_d`, `channel_d`, `a2a_d`, `cupolas_d`, `maths_d`, `hook_d` |

## Building from source

### Prerequisites

| Requirement | Minimum |
|-------------|---------|
| Operating system | Ubuntu 22.04+, macOS 13+, or Windows with MSVC |
| C compiler | GCC 11+ or Clang 14+ (C11) |
| Build tool | CMake 3.20+, Ninja (recommended) or Make |
| Libraries | SQLite3, cJSON, libyaml, libcurl, OpenSSL; libmicrohttpd and libwebsockets for the gateway |
| Optional | Rust (terminal UI), Python 3 (bundled script runtime) |

On Windows, dependencies are most easily obtained through
[vcpkg](https://github.com/microsoft/vcpkg):
`sqlite3 cjson libyaml curl openssl zlib libmicrohttpd libwebsockets libevent nghttp2`.

### Steps

```bash
# 1. Clone with all components
git clone --recursive https://atomgit.com/openairymax/agentrt.git
cd agentrt

# 2. Configure — the build directory must live outside the source tree
cmake -S . -B ../agentrt-build -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build ../agentrt-build --parallel "$(nproc)"

# 4. Test
cd ../agentrt-build && ctest --output-on-failure
```

In-source builds are rejected: CMake raises a `FATAL_ERROR` if the build
directory is inside the source tree.

On Windows, add `-DBUILD_DAEMON=ON -DBUILD_CLI=ON` to the configure step if you
want the daemon fleet and the CLI; see the next section for why they are off by
default there.

### Key CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `AIRY_BUILD_ALL` | `ON` | Build every AgentRT component |
| `BUILD_TESTS` | `ON` | Build unit tests and enable CTest |
| `BUILD_SHARED_LIBS` | `OFF` | Build shared libraries instead of static ones |
| `BUILD_ATOMS` / `BUILD_COMMONS` / `BUILD_CUPOLAS` / `BUILD_GATEWAY` / `BUILD_HEAPSTORE` | `ON` | Per-component build switches |
| `BUILD_DAEMON` | `ON` (POSIX), `OFF` (Windows) | Build the daemon fleet |
| `BUILD_CLI` | `ON` (POSIX), `OFF` (Windows) | Build the interactive CLI |
| `BUILD_TOOLKIT` | `OFF` | Build the language-SDK toolkit modules |
| `AIRY_MEMORY_BACKEND` | `builtin` | Memory backend: `builtin` or `memoryrovol` |
| `AIRY_WITH_MEMORYROVOL` | `ON` | Build against the commercial memory provider; falls back to the built-in backend when the provider source is absent |
| `AIRY_COMPLIANCE_STRICT` | `ON` | Strict compliance mode; poisons unsafe functions such as `strcpy` |
| `ENABLE_SANITIZERS` | `OFF` | ASan + LSan + UBSan; enable explicitly when diagnosing memory bugs |
| `ENABLE_TSAN` | `OFF` | ThreadSanitizer; mutually exclusive with the above |
| `ENABLE_COVERAGE` | `OFF` | Code-coverage instrumentation |
| `WARNINGS_AS_ERRORS` | `OFF` | Treat compiler warnings as errors |

Released packages are built with sanitizers off. Turning `ENABLE_SANITIZERS` on
is a debugging aid, not a deployment setting.

## Releases and channels

- **Stable** is the default channel and the one the installer uses.
- **rc** carries release candidates for preview; interfaces may still change.
- **beta** is occasionally opened for targeted testing.

```bash
airymaxrt update              # move to the newest build on your channel
airymaxrt update --check      # report without changing anything
airymaxrt update --channel rc # switch channel
airymaxrt update --rollback   # return to the previously installed version
```

Each version ships with a GPG-signed manifest in [`latest/`](latest), which is
what the updater reads. Release notes live in
[`RELEASE_NOTES.d/`](RELEASE_NOTES.d), one file per version, and
[CHANGELOG.md](CHANGELOG.md) records the longer history.

The current release is **v0.1.15**. Highlights:

- `corekern` now enters the CLI startup path through `airy_init()` and emits
  traceable boot evidence; a batch of IPC routing, binder shutdown-order and
  zero-length-allocation defects were fixed.
- The terminal UI restores your terminal state on fatal signals — no more
  leftover garbled output or a hidden cursor — and consumes OSC replies
  correctly.
- Gateway entry authentication and socket binding were hardened, and ASan /
  UBSan quality gates were added across `commons`, `gateway` and `corekern` so
  memory-safety regressions are caught before they are merged.
- A new `AIRY_KEEP_SYMBOLS` switch lets a build retain symbols for field
  diagnosis.
- Windows packages for x86-64 and x86-32 are published on the same pipeline as
  Linux and macOS.

## Documentation

Design documents, interface references and the application-development guide
live in the [Airymax documentation repository](https://atomgit.com/openairymax/docs),
under `AirymaxRT/`. Start with `AirymaxRT/README.md` for the full navigation,
`AirymaxRT/140-application-development/01-getting-started.md` for a five-minute
first run, and `AirymaxRT/30-interfaces/` for the API references.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) for the development workflow and coding
conventions. Run the test suite locally before opening a pull request; CI
enforces the same quality gates.

- Report bugs or request features: <https://github.com/openairymax/agentrt/issues>
- Security vulnerabilities: see [SECURITY.md](SECURITY.md). Please do not file
  a public issue for anything exploitable.
- Support and questions: [SUPPORT.md](SUPPORT.md)

## License

Dual-licensed under **AGPL-3.0-or-later OR Apache-2.0**. You may choose either
licence; you are not obliged to comply with both, and you may not comply with
neither. Full text in [LICENSE](LICENSE).

| Your situation | Suggested licence | Why |
|----------------|-------------------|-----|
| Running a network service built on or modified from AgentRT | AGPL v3 | Its network-service clause requires publishing your modifications |
| Open-source derivative work | AGPL v3 | Copyleft keeps derivatives open |
| Embedding in a closed-source commercial product | Apache 2.0 | Permissive; proprietary derivatives allowed |
| Internal tooling | Apache 2.0 | No source-disclosure obligation |
| You need an explicit patent grant | Apache 2.0 | Contributors grant patents explicitly |
| Learning or research | Either | Both permit it |

The commercial memory provider under `products/memoryrovol` in the umbrella
project is distributed separately under a SPHARX Ltd. end-user licence and is
not covered by the dual licence above.

Copyright (c) 2025-2026 **SPHARX Ltd.** All Rights Reserved.
