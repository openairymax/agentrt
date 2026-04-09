Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# Agent OS

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Mirror](https://img.shields.io/badge/mirror-GitHub-lightgrey.svg)](https://github.com/SpharxTeam/AgentOS)

---

**SuperAI Operating System**

*"From data intelligence emerges."*

---

馃摉 **[绠€浣撲腑鏂嘳(../../README.md)** | 馃嚞馃嚙 **English** | [Fran莽ais](../fr/README.md) | [Deutsch](../de/README.md)

</div>

---

## Introduction

- Engineered for task completion with maximum token efficiency
<!-- From data intelligence emerges. by spharx -->
- Novel architecture achieving 2-3脳 better token utilization than industry-standard frameworks
- 3-5脳 more efficient than OpenClaw for engineering tasks, saving ~60% token consumption

## 馃搵 Overview

- **Agent OS (SuperAI OS)** is the core intelligent agent operating system kernel of SpharxWorks, providing a complete runtime environment, memory system, cognitive engine, and execution framework for agents.
- As a production-ready physical world data infrastructure team, AgentOS implements a full closed loop from data processing to intelligent decision-making.

### Core Values

- **Microkernel**: Minimalist kernel design with all services running in user space for stability and extensibility
- **Three-Layer Architecture**: Cognition, Execution, and Memory layers working together for complete agent lifecycle management
- **Memory Roll System**: L1-L4 progressive memory abstraction supporting storage, retrieval, evolution, and forgetting
- **System Call Abstraction**: Stable and secure system call interfaces hiding kernel implementation details
- **Pluggable Strategies**: gateway loading and runtime replacement of cognition, planning, and scheduling algorithms
- **Unified Logging**: Cross-language logging interface with full-link tracing and OpenTelemetry integration
- **Multi-language SDK**: Native support for Go, Python, Rust, and TypeScript with FFI interfaces

### Version Status

**Current Version**: v1.0.0.6 (Production Ready)

- 鉁?Core architecture design completed
- 鉁?MemoryRovol Memory System
  - L1-L4 four-layer architecture fully implemented
  - Synchronous/Async write support (10,000+ entries/sec)
  - FAISS vector search integration (IVF/HNSW indexing)
  - Attractor network retrieval mechanism
  - Ebbinghaus forgetting curve implementation
  - LRU cache and vector persistence
- 鉁?CoreLoopThree Three-Layer Architecture
  - Cognition Layer: Intent understanding, task planning, multi-agent coordination (90%)
  - Execution Layer: Execution engine, compensation transactions, chain-of-responsibility tracing (85%)
  - Memory Layer: MemoryRovol FFI wrapper (80%)
- 鉁?Microkernel Base Module (core)
  - IPC Binder communication
  - Memory management (RAII, smart pointers)
  - Task scheduling (weighted round-robin)
  - High-precision time service
- 鉁?System Call Layer (syscall) - 100% complete
  - 鉁?Task syscalls: `sys_task_submit/query/wait/cancel`
  - 鉁?Memory syscalls: `sys_memory_write/search/get/delete`
  - 鉁?Session syscalls: `sys_session_create/get/close/list`
  - 鉁?Observability calls: `sys_telemetry_metrics/traces`
  - 鉁?Unified entry point: `agentos_syscall_invoke()`
- 馃敳 Complete end-to-end integration testing

---

## 馃彈锔?System Architecture

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?                   AgentOS Overall Architecture              鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?                                                            鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?             Application Layer (openlab)              鈹? 鈹?鈹? 鈹? docgen | ecommerce | research | videoedit | ...      鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                          鈫?                                鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?          Core Services Layer (daemon)                 鈹? 鈹?鈹? 鈹? llm_d | market_d | monit_d | perm_d | sched_d | ...  鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                          鈫?                                鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?           Kernel Layer (atoms)                       鈹? 鈹?鈹? 鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹? 鈹?鈹? 鈹? 鈹?  core       鈹? 鈹俢oreloopthree 鈹? 鈹俶emoryrovol  鈹? 鈹? 鈹?鈹? 鈹? 鈹?Microkernel  鈹? 鈹?-Layer Runtime鈹?鈹?-L Memory    鈹? 鈹? 鈹?鈹? 鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹? 鈹?鈹? 鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                                    鈹? 鈹?鈹? 鈹? 鈹?  syscall    鈹?                                    鈹? 鈹?鈹? 鈹? 鈹?System Calls 鈹?                                    鈹? 鈹?鈹? 鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                                    鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                          鈫?                                鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹? 鈹?          SDK Layer (toolkit)                           鈹? 鈹?鈹? 鈹? Go | Python | Rust | TypeScript | ...                鈹? 鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹? 鈹?鈹?                                                            鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

---

## 馃搧 Project Structure

```
AgentOS/
鈹溾攢鈹€ agentos/manager/                      # Configuration center
鈹?  鈹溾攢鈹€ agents.yaml             # Agent configuration
鈹?  鈹溾攢鈹€ kernel.yaml             # Kernel configuration (logging, scheduler, memory, IPC)
鈹?  鈹溾攢鈹€ logging.yaml            # Logging configuration
鈹?  鈹溾攢鈹€ models.yaml             # Model configuration
鈹?  鈹溾攢鈹€ security.yaml           # Security configuration
鈹?  鈹斺攢鈹€ services.yaml           # Services configuration
鈹?鈹溾攢鈹€ agentos/atoms/                       # Kernel layer (microkernel architecture)
鈹?  鈹溾攢鈹€ README.md               # Kernel design document
鈹?  鈹溾攢鈹€ BUILD.md                # Build guide
鈹?  鈹溾攢鈹€ CMakeLists.txt          # Top-level build file
鈹?  鈹?鈹?  鈹溾攢鈹€ core/                   # Microkernel base (IPC, memory, task, time)
鈹?  鈹?  鈹溾攢鈹€ include/            # Public headers
鈹?  鈹?  鈹斺攢鈹€ src/                # Source code implementation
鈹?  鈹?鈹?  鈹溾攢鈹€ coreloopthree/          # Three-layer runtime 猸?Core architecture
鈹?  鈹?  鈹溾攢鈹€ README.md           # Detailed design document
鈹?  鈹?  鈹溾攢鈹€ include/            # Public headers
鈹?  鈹?  鈹?  鈹溾攢鈹€ cognition.h     # Cognition layer interface
鈹?  鈹?  鈹?  鈹溾攢鈹€ execution.h     # Execution layer interface
鈹?  鈹?  鈹?  鈹溾攢鈹€ memory.h        # Memory layer interface
鈹?  鈹?  鈹?  鈹斺攢鈹€ loop.h          # Three-layer main interface
鈹?  鈹?  鈹斺攢鈹€ src/                # Source code implementation
鈹?  鈹?鈹?  鈹溾攢鈹€ memoryrovol/            # Four-layer memory roll system 猸?Core innovation
鈹?  鈹?  鈹溾攢鈹€ README.md           # Detailed design document
鈹?  鈹?  鈹溾攢鈹€ include/            # Public headers
鈹?  鈹?  鈹斺攢鈹€ src/                # Source code implementation
鈹?  鈹?鈹?  鈹溾攢鈹€ syscall/                # System call layer (鉁?100%)
鈹?  鈹?  鈹溾攢鈹€ README.md           # System call documentation
鈹?  鈹?  鈹溾攢鈹€ include/syscalls.h  # System call header
鈹?  鈹?  鈹斺攢鈹€ src/                # System call implementation (entry/table)
鈹?  鈹?鈹?  鈹斺攢鈹€ utils/                  # Utility libraries
鈹?      鈹溾攢鈹€ logger/             # Unified logging system
鈹?      鈹溾攢鈹€ tracer/             # Observability tracing
鈹?      鈹斺攢鈹€ errors/             # Error handling
鈹?鈹溾攢鈹€ agentos/toolkit/                       # Multi-language SDK
鈹?  鈹溾攢鈹€ go/                     # Go SDK
鈹?  鈹溾攢鈹€ python/                 # Python SDK
鈹?  鈹溾攢鈹€ rust/                   # Rust SDK
鈹?  鈹斺攢鈹€ typescript/             # TypeScript SDK
鈹?鈹溾攢鈹€ agentos/daemon/                       # Core background services (user-space)
鈹?  鈹溾攢鈹€ llm_d/                  # LLM service daemon
鈹?  鈹溾攢鈹€ market_d/               # Market service daemon
鈹?  鈹溾攢鈹€ monit_d/                # Monitoring service daemon
鈹?  鈹溾攢鈹€ perm_d/                 # Permission service daemon
鈹?  鈹溾攢鈹€ sched_d/                # Scheduling service daemon
鈹?  鈹斺攢鈹€ tool_d/                 # Tool service daemon
鈹?鈹溾攢鈹€ openlab/                     # Open ecosystem hub (official apps + community contributions)
鈹?  鈹溾攢鈹€ app/                    # Official application examples
鈹?  鈹?  鈹溾攢鈹€ docgen/             # Document generation app
鈹?  鈹?  鈹溾攢鈹€ ecommerce/          # E-commerce app
鈹?  鈹?  鈹溾攢鈹€ research/           # Research analysis app
鈹?  鈹?  鈹斺攢鈹€ videoedit/          # Video editing app
鈹?  鈹溾攢鈹€ contrib/                # Community contributions
鈹?  鈹?  鈹溾攢鈹€ agents/             # Community-contributed agents
鈹?  鈹?  鈹溾攢鈹€ skills/             # Community-contributed skills
鈹?  鈹?  鈹斺攢鈹€ strategies/         # Community-contributed strategies
鈹?  鈹斺攢鈹€ markets/                # Market infrastructure
鈹?鈹溾攢鈹€ agentos/heapstore/                    # Data partition (runtime data)
鈹?  鈹溾攢鈹€ kernel/                 # Kernel data
鈹?  鈹溾攢鈹€ logs/                   # Log files (centralized storage)
鈹?  鈹?  鈹溾攢鈹€ apps/               # Application layer logs
鈹?  鈹?  鈹溾攢鈹€ kernel/             # Kernel layer logs
鈹?  鈹?  鈹斺攢鈹€ services/           # Service layer logs
鈹?  鈹溾攢鈹€ traces/spans/           # OpenTelemetry trace data
鈹?  鈹斺攢鈹€ registry/               # Global registry (agents.db, skills.db, sessions.db)
鈹?鈹溾攢鈹€ agentos/docs/                    # Technical documentation center
鈹?  鈹溾攢鈹€ api/                    # API documentation
鈹?  鈹溾攢鈹€ architecture/           # Architecture design documents
鈹?  鈹溾攢鈹€ guides/                 # Development guides
鈹?  鈹溾攢鈹€ philosophy/             # Design philosophy
鈹?  鈹斺攢鈹€ specifications/         # Technical specifications
鈹?鈹溾攢鈹€ scripts/                     # Operations scripts
鈹?  鈹溾攢鈹€ build.sh                # Build script
鈹?  鈹溾攢鈹€ install.sh              # Installation script
鈹?  鈹斺攢鈹€ benchmark.py            # Performance benchmark
鈹?鈹斺攢鈹€ tests/                       # Test suite
    鈹溾攢鈹€ unit/                   # Unit tests
    鈹溾攢鈹€ integration/            # Integration tests
    鈹斺攢鈹€ security/               # Security tests
```

---

## 馃 CoreLoopThree: Three-Layer Architecture

### Design Philosophy

CoreLoopThree is AgentOS's core innovative architecture, dividing agent runtime into three orthogonal and synergistic layers for unified cognition, execution, and memory:

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?         Cognition Layer                  
   鈥?Intent Understanding 鈥?Task Planning  
   鈥?Agent Scheduling 鈥?Model Coordination 
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫撯攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?          Execution Layer                 
    鈥?Task Execution 鈥?Compensation       
    鈥?Chain Tracing 鈥?State Management    
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫撯攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                鈫?鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?          Memory Layer                    
    鈥?Memory Write 鈥?Query Retrieval      
    鈥?Context Mount 鈥?Evolution & Forgetting
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

### Core Components

#### 1. Cognition Layer
- **Intent Understanding Engine**: Parse user input, identify real intent
  - Raw text processing
  - Core goal extraction
  - Intent flags (urgent, complex, etc.)
- **Task Planner**: Automatic task decomposition and ordering based on goals
  - DAG task graph generation
  - Dependency management
  - Entry point identification
- **Agent Scheduler**: Multi-agent coordination and resource allocation
  - Weighted scheduling strategy
  - Candidate agent scoring
  - gateway task assignment
- **Model Coordinator**: LLM selection and prompt engineering
  - Multi-model input coordination
  - Output result fusion
  - Pluggable strategies
- **Strategy Interfaces**: Pluggable cognitive algorithm strategies
  - `agentos_plan_strategy_t` - Planning strategy
  - `agentos_coordinator_strategy_t` - Coordination strategy
  - `agentos_dispatching_strategy_t` - Dispatching strategy

#### 2. Execution Layer
- **Execution Engine**: Task execution and state tracking
  - Task state machine (Pending/Running/Succeeded/Failed/Cancelled/Retrying)
  - Concurrency control
  - Timeout management
- **Compensation Transactions**: Failure rollback and compensation logic
  - Compensation execution unit registration
  - Automatic rollback mechanism
  - Human intervention queue
- **Chain of Responsibility Tracing**: Complete execution chain recording
  - Trace ID correlation
  - Execution history archiving
  - State query interface
- **Execution Unit Registry**: Registration and discovery of atomic execution units
  - Metadata description
  - gateway registration/unregistration
- **Exception Handling**: Hierarchical exception capture and recovery
  - Retry strategy
  - Error message logging

#### 3. Memory Layer
- **Memory Service**: Encapsulates MemoryRovol providing high-level interfaces
  - Memory engine (`agentos_memory_engine_t`)
  - Record types (RAW/FEATURE/STRUCTURE/PATTERN)
- **Write Interface**: Synchronous/asynchronous memory write support
  - Memory record structure definition
  - Timestamp and source tracking
  - Importance scoring
- **Query Interface**: Semantic query and vector retrieval
  - Multi-dimensional query conditions (time, source, TraceID)
  - Limit and offset pagination
  - Include raw data option
- **Context Mount**: Automatic memory association based on context
  - Mount mechanism
  - Access count updates
  - Usage awareness
- **FFI Interface**: `rov_ffi.h` provides cross-language calling capability
  - C ABI compatible
  - Multi-language SDK support

### Interaction with Other Modules

- **With core**: CoreLoopThree calls core's IPC, memory management, task scheduling via syscall layer
- **With memoryrovol**: Memory layer calls MemoryRovol's core functions through FFI interface (`rov_ffi.h`)
- **With syscall**: Cognition and execution layers interact with kernel through system call interfaces

See: [CoreLoopThree Architecture Document](agentos/docs/architecture/coreloopthree.md)

---

## 馃捑 MemoryRovol: Memory Roll System

### Functional Positioning

MemoryRovol is AgentOS's kernel-level memory system, implementing full-stack memory management from raw data to advanced patterns. It's not just data storage but the core infrastructure for continuous learning, knowledge accumulation, and intelligent evolution.

### Four-Layer Architecture

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?               L4 Pattern Layer                            
   鈥?Persistent Homology (Ripser) 鈥?Stable Pattern Mining  
   鈥?HDBSCAN Clustering 鈥?Rule Generation                
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫戔攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                    鈫?Abstract Evolution
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?             L3 Structure Layer                            
   鈥?Bind/Unbind Operators 鈥?Relation Encoding 鈥?Temporal  
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫戔攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                    鈫?Feature Extraction
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?             L2 Feature Layer                              
   鈥?Embedding Models (OpenAI/DeepSeek/SentenceTransformers)
   鈥?FAISS Vector Index 鈥?Hybrid Search (Vector+BM25)     
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈫戔攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                    鈫?Data Compression
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?              L1 Raw Layer                                 
   鈥?File System Storage 鈥?Shard Management 鈥?Compression  
   鈥?Metadata Index 鈥?Integrity Check 鈥?Version Control   
鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

### Data Storage Model

#### L1 Raw Layer - Raw Data Storage
- **Storage Medium**: File system-based efficient storage
  - Shard file management
  - Automatic compression archiving
  - Root path configuration
- **Data Format**: Support for text, images, audio, video, etc.
- **Shard Management**: Automatic sharding and compression
  - Metadata index (SQLite)
  - Version control
- **Async Write**: Background thread pool
  - Write queue management
  - Completion callback notification
  - Configurable thread count

#### L2 Feature Layer - Vector Representation
- **Embedding Models**: Multiple pre-trained models
  - OpenAI embeddings (text-embedding-3-small/large)
  - DeepSeek embeddings
  - Sentence Transformers (all-MiniLM-L6-v2, etc.)
- **Vector Index**: FAISS efficient similarity search
  - IVF inverted index (configurable nlist)
  - HNSW graph index (configurable M parameter)
  - Quantization compression (PQ/OPQ)
- **Hybrid Search**: Vector search + BM25 keyword search
  - Weighted fusion ranking
  - Cross-encoder re-ranking
- **LRU Cache**: Hot vector cache
  - Configurable cache size
  - Automatic eviction policy
  - Hit/miss statistics
- **Vector Persistence**: SQLite storage backend
  - Record ID mapping
  - Dimension management

#### L3 Structure Layer - Structured Representation
- **Bind Operator**: Bind multiple memory units into composite structures
- **Unbind Operator**: Decompose composite memory structures
- **Relation Encoding**: Explicit encoding of semantic relationships
- **Temporal Encoding**: Record temporal order and causality
- **Graph Encoding**: Graph neural network-based representation

#### L4 Pattern Layer - Advanced Pattern Mining
- **Persistent Homology**: Topological data analysis using Ripser
- **Stable Patterns**: Identify invariant patterns across memories
- **HDBSCAN Clustering**: Density-based clustering
- **Rule Generation**: Extract reusable rules from patterns
- **Evolution Committee**: Coordinate with cognition layer for pattern evaluation

### Role in State Persistence

#### Integration with agentos/heapstore/registry
- **Registry Data**: MemoryRovol provides backing store for agents.db and skills.db
- **Agent State**: Each agent's runtime state and history stored in L1/L2 layers
- **Skill Memory**: Skill execution records and feedback stored in L2/L3 layers

#### Integration with agentos/heapstore/traces/spans
- **Trace Data**: OpenTelemetry spans stored as raw memory in L1 layer
- **Context Association**: Automatic memory correlation based on trace ID
- **Performance Analysis**: L4 layer mines performance bottlenecks and optimization patterns

### Core Functions

#### 1. Memory Storage
- **Synchronous Write**: Blocking write ensuring data persistence
  - `agentos_layer1_raw_write()`
  - Immediate record ID return
- **Async Write**: Batch writes for throughput
  - `agentos_layer1_raw_write_async()`
  - Background thread pool execution
  - Callback notification
  - Throughput: 10,000+ entries/sec
- **Transaction Support**: ACID semantics
- **Compression Archiving**: Automatic compression for low-frequency memories

#### 2. Memory Retrieval
- **Vector Search**: Cosine similarity search
  - FAISS index query
  - Top-K results
  - Latency: < 10ms (k=10)
- **Semantic Search**: Natural language query
  - Text vectorization
  - Similarity ranking
- **Context-Aware**: Automatic filtering based on context
  - Time range filtering
  - Source agent filtering
  - Trace ID correlation
- **LRU Cache**: Hot memory cache
  - Cache hit statistics
  - Automatic eviction
- **Re-ranking**: Cross-encoder precision ranking
  - Improved result relevance
  - Latency: < 50ms (top-100)

#### 3. Memory Evolution
- **Automatic Abstraction**: Progressive L1鈫扡2鈫扡3鈫扡4 abstraction
  - Feature extraction (L1鈫扡2)
  - Structure binding (L2鈫扡3)
  - Pattern mining (L3鈫扡4)
- **Pattern Discovery**: Identify high-frequency patterns and rules
  - Persistent homology analysis (Ripser)
  - HDBSCAN clustering
  - Stable pattern recognition
- **Weight Updates**: gateway weight adjustment based on access frequency and relevance
- **Evolution Evaluation**: Coordinate with cognition layer for memory value assessment
  - Evolution committee mechanism

#### 4. Memory Forgetting
- **Ebbinghaus Curve**: Intelligent pruning based on forgetting curve
  - Configurable decay rate (lambda)
  - Threshold control
- **Linear Decay**: Simple linear weight decay
- **Access Count**: LRU/LFU-based strategy
  - Minimum access count threshold
  - Access time tracking
- **Active Forgetting**: Cognition layer-triggered targeted forgetting
  - Configurable check interval
  - Archiving mechanism

See: [MemoryRovol Architecture Document](agentos/docs/architecture/memoryrovol.md)

---

## 馃洜锔?Development Guide

### Requirements

- **OS**: Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2)
- **Compiler**: GCC 11+ or Clang 14+
- **Build Tools**: CMake 3.20+, Ninja or Make
- **Dependencies**:
  - OpenSSL >= 1.1.1 (cryptography)
  - libevent (event loop)
  - pthread (threads)
  - FAISS >= 1.7.0 (vector search)
  - SQLite3 >= 3.35 (metadata storage)
  - libcurl >= 7.68 (HTTP client)
  - cJSON >= 1.7.15 (JSON parsing)
  - Ripser >= 2.3.1 (persistent homology, optional)
  - HDBSCAN >= 0.8.27 (clustering, optional)

### Quick Start

#### 1. Clone Repository

```bash
# Clone from official repository (recommended, faster in China)
git clone https://gitee.com/spharx/agentos.git
cd agentos

# Or clone from mirror repository
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS
```

#### 2. Initialize Configuration

```bash
# Copy environment variable template
cp .env.example .env

# Edit .env file, set necessary environment variables
# e.g., API keys, storage paths, etc.

# Run configuration initialization script
python scripts/init_config.py
```

#### 3. Build Project

```bash
# Create build directory
mkdir build && cd build

# Configure CMake
cmake ../atoms \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DENABLE_TRACING=ON

# Build
cmake --build . --parallel $(nproc)

# Run tests
ctest --output-on-failure

# Install (optional)
sudo cmake --install .
```

#### 4. Configuration Options

| CMake Variable | Description | Default |
| :--- | :--- | :--- |
| `CMAKE_BUILD_TYPE` | Debug/Release/RelWithDebInfo | `Release` |
| `BUILD_TESTS` | Build unit tests | `OFF` |
| `ENABLE_TRACING` | Enable OpenTelemetry tracing | `OFF` |
| `ENABLE_ASAN` | Enable AddressSanitizer | `OFF` |
| `USE_LLVM` | Use LLVM toolchain | `OFF` |

See: [BUILD.md](agentos/atoms/BUILD.md)

### Logging System

AgentOS uses a unified cross-language logging architecture:

#### Log Storage Location
```
agentos/heapstore/logs/
鈹溾攢鈹€ kernel/         # Kernel layer logs 鈫?agentos.log
鈹溾攢鈹€ services/       # Service layer logs 鈫?llm_d.log, tool_d.log, etc.
鈹斺攢鈹€ apps/           # Application layer logs 鈫?independent logs per app
```

#### Log Format
- **Human-readable format**: `%(asctime)s.%(msecs)03d [%(levelname)s] [%(name)s] %(message)s`
- **JSON format**: Structured logging for ELK/Splunk integration

#### Cross-Language Log Correlation
- Full-link tracing via `trace_id`
- C/Python/Go/Rust/TypeScript share the same logging specification
- OpenTelemetry integration as observability backend

See: [Logging System Architecture Document](agentos/docs/architecture/logging_system.md)

### Testing

```bash
# Unit tests
ctest -R unit --output-on-failure

# Integration tests
ctest -R integration --output-on-failure

# Performance benchmark
python scripts/benchmark.py
```

---

## 馃搳 Performance Metrics

Based on standard test environment (Intel i7-12700K, 32GB RAM, NVMe SSD):

### Processing Capability

| Metric | Value | Test Conditions |
| :--- | :--- | :--- |
| **Memory Write Throughput** | 10,000+ entries/sec | L1 layer, async batch write |
| **Vector Search Latency** | < 10ms | FAISS IVF1024,PQ64, k=10 |
| **Hybrid Search Latency** | < 50ms | Vector+BM25, top-100 re-ranking |
| **Memory Abstraction Speed** | 100 entries/sec | L2鈫扡3 progressive abstraction |
| **Pattern Mining Speed** | 100k entries/min | L4 persistent homology analysis |
| **Concurrent Connections** | 1024 | Binder IPC max connections |
| **Task Scheduling Latency** | < 1ms | Weighted round-robin |
| **Intent Parsing Latency** | < 50ms | Simple intent |
| **Task Planning Speed** | 100+ nodes/sec | DAG generation |
| **Agent Scheduling Latency** | < 5ms | Weighted round-robin |
| **Task Execution Throughput** | 1000+ tasks/sec | Concurrent execution |

### Resource Utilization

| Scenario | CPU Usage | Memory Usage | Disk IO |
| :--- | :--- | :--- | :--- |
| **Idle** | < 5% | 200MB | < 1MB/s |
| **Medium Load** | 30-50% | 1-2GB | 10-50MB/s |
| **High Load** | 80-100% | 4-8GB | 100-500MB/s |

### Scalability

- **Horizontal Scaling**: Multi-node distributed deployment (planned)
- **Vertical Scaling**: Configurable resource limits and allocation
- **Elastic Scaling**: Automatic resource adjustment based on load (planned)

Note: Detailed performance data available in [scripts/benchmark.py](scripts/benchmark.py)

---

## 馃摎 Documentation Resources

### Core Documentation

- [馃摌 CoreLoopThree Architecture](agentos/docs/architecture/coreloopthree.md) - Three-layer runtime
- [馃捑 MemoryRovol Architecture](agentos/docs/architecture/memoryrovol.md) - Memory roll system
- [馃敡 IPC Mechanism](agentos/docs/architecture/ipc.md) - Inter-process communication
- [鈿欙笍 Microkernel Design](agentos/docs/architecture/microkernel.md) - Microkernel architecture
- [馃摓 System Calls](agentos/docs/architecture/syscall.md) - System call interface
- [馃摑 Logging System](agentos/docs/architecture/logging_system.md) - Cross-language logging

### Development Guides

- [馃殌 Quick Start](agentos/docs/guides/getting_started.md) - Getting started guide
- [馃 Create Agent](agentos/docs/guides/create_agent.md) - Agent development tutorial
- [馃洜锔?Create Skill](agentos/docs/guides/create_skill.md) - Skill development tutorial
- [馃摝 Deployment Guide](agentos/docs/guides/deployment.md) - Production deployment
- [馃帥锔?Kernel Tuning](agentos/docs/guides/kernel_tuning.md) - Performance optimization guide
- [馃攳 Troubleshooting](agentos/docs/guides/troubleshooting.md) - commons issues

### Technical Specifications

- [馃搵 Coding Standards](agentos/docs/specifications/coding_standards.md) - Development standards
- [馃И Testing Standards](agentos/docs/specifications/testing.md) - Testing requirements
- [馃敀 Security Standards](agentos/docs/specifications/security.md) - Security practices
- [馃搳 Performance Metrics](agentos/docs/specifications/performance.md) - Performance requirements

### External Documentation

- [馃彮 Workshop Documentation](../Workshop/README.md) - Data collection factory
- [馃敩 Deepness Documentation](../Deepness/README.md) - Deep processing system
- [馃搳 Benchmark Documentation](../Benchmark/metrics/README.md) - Evaluation metrics

---

## 馃攧 Version Roadmap

### Current Version (v1.0.0.6) - Production Ready

**Completion**: 85%

- 鉁?Core architecture design completed
- 鉁?MemoryRovol memory system implementation
  - L1-L4 four-layer architecture fully implemented
    - L1 Raw: Sync/async write, file system storage, SQLite metadata, shard management
    - L2 Feature: FAISS index (IVF/HNSW), multi-embedder support, LRU cache, vector persistence
    - L3 Structure: Bind/unbind operators, relation encoding, temporal encoding
    - L4 Pattern: Persistent homology interface, HDBSCAN clustering, rule generation
  - Retrieval mechanism fully implemented
    - Attractor Network
    - Retrieval cache (LRU)
    - Mount mechanism
    - Re-ranking (Reranker)
  - Forgetting mechanism implemented
    - Ebbinghaus curve decay
    - Linear decay
    - Access count-based strategy
    - Automatic forgetting task
  - FAISS vector search integration
    - IVF, HNSW indexing
    - Hybrid search (vector+BM25)
- 鉁?CoreLoopThree three-layer runtime framework
  - Cognition layer foundation (90%)
    - Intent understanding engine (Intent structure)
    - Task planner (DAG generation)
    - Agent scheduler (weighted round-robin)
    - Multi-strategy interfaces (Plan/Coordinator/Dispatching)
  - Execution layer engine (85%)
    - Task state machine management
    - Compensation transaction framework
    - Execution unit registry
    - Chain of responsibility tracing
  - Memory layer FFI interface (80%)
    - MemoryRovol wrapper
    - Memory engine
    - Query and mount interfaces
- 鉁?Microkernel base module (core)
  - IPC Binder implementation
  - Memory management (RAII, smart pointers)
  - Task scheduling (weighted round-robin algorithm)
  - High-precision time service
- 鉁?System call layer (syscall) - 100%
  - 鉁?Task syscalls completed
    - `sys_task_submit()` - Submit task
    - `sys_task_query()` - Query status
    - `sys_task_wait()` - Wait for completion
    - `sys_task_cancel()` - Cancel task
  - 鉁?Memory syscalls completed
    - `sys_memory_write()` - Write memory
    - `sys_memory_search()` - Semantic search
    - `sys_memory_get()` - Get data
    - `sys_memory_delete()` - Delete memory
  - 鉁?Session syscalls completed
    - `sys_session_create()` - Create session
    - `sys_session_get()` - Get information
    - `sys_session_close()` - Close session
    - `sys_session_list()` - List sessions
  - 鉁?Observability syscalls completed
    - `sys_telemetry_metrics()` - Get metrics
    - `sys_telemetry_traces()` - Get traces
- 鉁?Unified logging system implementation
  - Cross-language logging interface (C/Python/Go/Rust/TS)
  - Centralized log storage (agentos/heapstore/logs/)
  - trace_id full-link tracing
  - OpenTelemetry integration
- 馃敳 Complete end-to-end integration testing

### Short-term Goals (2026 Q2-Q3)

**v1.0.0.4 - Enhancement & Optimization**
- Improve CoreLoopThree exception handling mechanism
- Optimize attractor network retrieval performance
- Increase LRU cache hit rate
- Improve memory evolution algorithms
- Add more execution units

**v1.0.1.0 - Performance Optimization**
- Optimize vector search performance
  - FAISS index parameter tuning
  - LRU cache hit rate improvement
- Improve memory abstraction algorithms
  - L3鈫扡4 evolution optimization
- Reduce system latency
  - Attractor network iteration optimization

**v1.0.2.0 - Developer Tools**
- Improve SDK (Go/Python/Rust/TS)
  - High-level abstraction interfaces
  - Async support
- Provide debugging tools
  - Memory visualization
  - Execution tracer
- Enhance documentation and examples

### Mid-term Planning (2026 Q4-2027)

**v1.0.3.0 - Production Ready**
- Complete end-to-end test coverage
- Performance benchmark杈炬爣
- Security audit passed
- Production deployment verification

**v1.0.4.0 - Distributed Support**
- Multi-node cluster deployment
- Distributed memory storage
- Cross-node task scheduling

**v1.0.5.0 - Intelligence Upgrade**
- Adaptive memory management
- Reinforcement learning optimization
- Autonomous evolution mechanism

### Long-term Vision (2027+)

- 馃寪 Become the de facto standard for agent operating systems
- 馃 Build a global open-source community ecosystem
- 馃弳 Lead next-generation AGI technology development
- 馃搱 Support trillion-scale memory capacity and millisecond retrieval

---

## 馃 Ecosystem Cooperation

We invite partners from all sectors to jointly build the agent operating system ecosystem:

### Technology Partners
- **AI Labs**: Experts in large models, memory systems, cognitive architectures
- **Hardware Vendors**: GPU, NPU, storage device providers
- **Application Enterprises**: Robotics, intelligent assistants, automation scenarios

### Community Contributions
- **Code Contributions**: Core feature development and optimization
- **Documentation**: Usage guides and technical documentation
- **Testing Validation**: Functional testing and performance evaluation
- **Ecosystem Building**: Community operations and knowledge sharing

---

## 馃摓 Technical Support

### Community Support
- **Gitee Issues**: [Official Issue Tracker](https://gitee.com/spharx/agentos/issues) (preferred)
- **GitHub Issues**: [Mirror Issue Tracker](https://github.com/SpharxTeam/AgentOS/issues)
- **Discussions**: [GitHub Discussions](https://github.com/SpharxTeam/AgentOS/discussions)
- **Documentation**: [Online Documentation](https://docs.spharx.cn/agentos)

### Commercial Support
- **Enterprise Edition**: Commercial licensing and technical support available
- **Custom Development**: Custom modules based on requirements
- **Training Services**: Training on system usage and development

For licensing inquiries, contact:
- Official email: lidecheng@spharx.cn, wangliren@spharx.cn
- Official website: https://spharx.cn

---

## 馃搫 License

AgentOS adopts a **business-friendly, ecosystem-open layered open-source licensing architecture**, consistent with mainstream OS licensing designs, balancing core IP protection, community openness, and commercial deployment freedom.

### Primary License Statement
Core kernel code defaults to **Apache License 2.0**. Full license text available in root [LICENSE](../../LICENSE) file.

### Layered License Details
| Module Directory | Applicable License | Description |
|----------|----------|----------|
| `agentos/atoms/` (Kernel) | Apache License 2.0 | CoreLoopThree architecture, MemoryRovol engine, runtime, security isolation layer, etc. |
| `agentos/cupolas/` (Extensions) | Apache License 2.0 | Core architecture extension modules, consistent with kernel license |
| `openlab/` (Ecosystem) | MIT License | Agent marketplace, skill marketplace, community contributions to lower contribution barriers |
| Third-party Dependencies | Original licenses | All third-party dependencies use permissive licenses with proper module isolation |

### You Are Free To
- 鉁?**Commercial Use**: Use in closed-source commercial products, enterprise projects, commercial services
- 鉁?**Modify**: Modify, customize, and create derivative works without open-sourcing business code
- 鉁?**Distribute**: Distribute and copy source code or compiled binaries
- 鉁?**Patent Use**: Permanent patent license for core code
- 鉁?**Private Use**: Use in personal/private projects without mandatory disclosure

### Your Only Obligations
- Preserve original copyright notices, license text, and NOTICE file
- Include modification records when changing core source files

### Commercial Services & Licensing
- No restrictions on commercial use under this open-source license
- Enterprise-grade technical support, custom development, and private deployment services available

---

## 馃檹 Acknowledgments

Thanks to all developers contributing to the open-source community, and partners supporting the AgentOS project.

Special thanks to:
- FAISS team (Facebook AI Research)
- Sentence Transformers team
- Rust and Go language communities
- All contributors and users

---

<div align="center">

<h4>"From data intelligence emerges"</h4>

---

#### 馃摓 Contact Us

馃摟 Email: lidecheng@spharx.cn; wangliren@spharx.cn

<p>
  <a href="https://gitee.com/spharx/agentos">Gitee (Official Repository)</a> 路
  <a href="https://github.com/SpharxTeam/AgentOS">GitHub (Mirror Repository)</a> 路
  <a href="https://spharx.cn">Official Website</a> 路
  <a href="mailto:lidecheng@spharx.cn">Technical Support</a>
</p>

漏 2026 SPHARX Ltd. All Rights Reserved.

</div>
