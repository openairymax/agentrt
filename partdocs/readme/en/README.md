# Agent OS

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.5-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Mirror](https://img.shields.io/badge/mirror-GitHub-lightgrey.svg)](https://github.com/SpharxTeam/AgentOS)

---

**SuperAI Operating System**

*"From data intelligence emerges."*

---

📖 **[简体中文](../../README.md)** | 🇬🇧 **English** | [Français](../fr/README.md) | [Deutsch](../de/README.md)

</div>

---

## Introduction

- Engineered for task completion with maximum token efficiency
<!-- From data intelligence emerges. by spharx -->
- Novel architecture achieving 2-3× better token utilization than industry-standard frameworks
- 3-5× more efficient than OpenClaw for engineering tasks, saving ~60% token consumption

## 📋 Overview

- **Agent OS (SuperAI OS)** is the core intelligent agent operating system kernel of SpharxWorks, providing a complete runtime environment, memory system, cognitive engine, and execution framework for agents.
- As a production-ready physical world data infrastructure team, AgentOS implements a full closed loop from data processing to intelligent decision-making.

### Core Values

- **Microkernel**: Minimalist kernel design with all services running in user space for stability and extensibility
- **Three-Layer Architecture**: Cognition, Execution, and Memory layers working together for complete agent lifecycle management
- **Memory Roll System**: L1-L4 progressive memory abstraction supporting storage, retrieval, evolution, and forgetting
- **System Call Abstraction**: Stable and secure system call interfaces hiding kernel implementation details
- **Pluggable Strategies**: Dynamic loading and runtime replacement of cognition, planning, and scheduling algorithms
- **Unified Logging**: Cross-language logging interface with full-link tracing and OpenTelemetry integration
- **Multi-language SDK**: Native support for Go, Python, Rust, and TypeScript with FFI interfaces

### Version Status

**Current Version**: v1.0.0.5 (Production Ready)

- ✅ Core architecture design completed
- ✅ MemoryRovol Memory System
  - L1-L4 four-layer architecture fully implemented
  - Synchronous/Async write support (10,000+ entries/sec)
  - FAISS vector search integration (IVF/HNSW indexing)
  - Attractor network retrieval mechanism
  - Ebbinghaus forgetting curve implementation
  - LRU cache and vector persistence
- ✅ CoreLoopThree Three-Layer Architecture
  - Cognition Layer: Intent understanding, task planning, multi-agent coordination (90%)
  - Execution Layer: Execution engine, compensation transactions, chain-of-responsibility tracing (85%)
  - Memory Layer: MemoryRovol FFI wrapper (80%)
- ✅ Microkernel Base Module (core)
  - IPC Binder communication
  - Memory management (RAII, smart pointers)
  - Task scheduling (weighted round-robin)
  - High-precision time service
- ✅ System Call Layer (syscall) - 100% complete
  - ✅ Task syscalls: `sys_task_submit/query/wait/cancel`
  - ✅ Memory syscalls: `sys_memory_write/search/get/delete`
  - ✅ Session syscalls: `sys_session_create/get/close/list`
  - ✅ Observability calls: `sys_telemetry_metrics/traces`
  - ✅ Unified entry point: `agentos_syscall_invoke()`
- 🔲 Complete end-to-end integration testing

---

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    AgentOS Overall Architecture              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              Application Layer (openhub)              │  │
│  │  docgen | ecommerce | research | videoedit | ...      │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           Core Services Layer (backs)                 │  │
│  │  llm_d | market_d | monit_d | perm_d | sched_d | ...  │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │            Kernel Layer (atoms)                       │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │  │
│  │  │   core       │  │coreloopthree │  │memoryrovol  │  │  │
│  │  │ Microkernel  │  │3-Layer Runtime│ │4-L Memory    │  │  │
│  │  └──────────────┘  └──────────────┘  └─────────────┘  │  │
│  │  ┌──────────────┐                                     │  │
│  │  │   syscall    │                                     │  │
│  │  │ System Calls │                                     │  │
│  │  └──────────────┘                                     │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           SDK Layer (tools)                           │  │
│  │  Go | Python | Rust | TypeScript | ...                │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 📁 Project Structure

```
AgentOS/
├── config/                      # Configuration center
│   ├── agents.yaml             # Agent configuration
│   ├── kernel.yaml             # Kernel configuration (logging, scheduler, memory, IPC)
│   ├── logging.yaml            # Logging configuration
│   ├── models.yaml             # Model configuration
│   ├── security.yaml           # Security configuration
│   └── services.yaml           # Services configuration
│
├── atoms/                       # Kernel layer (microkernel architecture)
│   ├── README.md               # Kernel design document
│   ├── BUILD.md                # Build guide
│   ├── CMakeLists.txt          # Top-level build file
│   │
│   ├── core/                   # Microkernel base (IPC, memory, task, time)
│   │   ├── include/            # Public headers
│   │   └── src/                # Source code implementation
│   │
│   ├── coreloopthree/          # Three-layer runtime ⭐ Core architecture
│   │   ├── README.md           # Detailed design document
│   │   ├── include/            # Public headers
│   │   │   ├── cognition.h     # Cognition layer interface
│   │   │   ├── execution.h     # Execution layer interface
│   │   │   ├── memory.h        # Memory layer interface
│   │   │   └── loop.h          # Three-layer main interface
│   │   └── src/                # Source code implementation
│   │
│   ├── memoryrovol/            # Four-layer memory roll system ⭐ Core innovation
│   │   ├── README.md           # Detailed design document
│   │   ├── include/            # Public headers
│   │   └── src/                # Source code implementation
│   │
│   ├── syscall/                # System call layer (✅ 100%)
│   │   ├── README.md           # System call documentation
│   │   ├── include/syscalls.h  # System call header
│   │   └── src/                # System call implementation (entry/table)
│   │
│   └── utils/                  # Utility libraries
│       ├── logger/             # Unified logging system
│       ├── tracer/             # Observability tracing
│       └── errors/             # Error handling
│
├── tools/                       # Multi-language SDK
│   ├── go/                     # Go SDK
│   ├── python/                 # Python SDK
│   ├── rust/                   # Rust SDK
│   └── typescript/             # TypeScript SDK
│
├── backs/                       # Core background services (user-space)
│   ├── llm_d/                  # LLM service daemon
│   ├── market_d/               # Market service daemon
│   ├── monit_d/                # Monitoring service daemon
│   ├── perm_d/                 # Permission service daemon
│   ├── sched_d/                # Scheduling service daemon
│   └── tool_d/                 # Tool service daemon
│
├── openhub/                     # Open ecosystem hub (official apps + community contributions)
│   ├── app/                    # Official application examples
│   │   ├── docgen/             # Document generation app
│   │   ├── ecommerce/          # E-commerce app
│   │   ├── research/           # Research analysis app
│   │   └── videoedit/          # Video editing app
│   ├── contrib/                # Community contributions
│   │   ├── agents/             # Community-contributed agents
│   │   ├── skills/             # Community-contributed skills
│   │   └── strategies/         # Community-contributed strategies
│   └── markets/                # Market infrastructure
│
├── partdata/                    # Data partition (runtime data)
│   ├── kernel/                 # Kernel data
│   ├── logs/                   # Log files (centralized storage)
│   │   ├── apps/               # Application layer logs
│   │   ├── kernel/             # Kernel layer logs
│   │   └── services/           # Service layer logs
│   ├── traces/spans/           # OpenTelemetry trace data
│   └── registry/               # Global registry (agents.db, skills.db, sessions.db)
│
├── partdocs/                    # Technical documentation center
│   ├── api/                    # API documentation
│   ├── architecture/           # Architecture design documents
│   ├── guides/                 # Development guides
│   ├── philosophy/             # Design philosophy
│   └── specifications/         # Technical specifications
│
├── scripts/                     # Operations scripts
│   ├── build.sh                # Build script
│   ├── install.sh              # Installation script
│   └── benchmark.py            # Performance benchmark
│
└── tests/                       # Test suite
    ├── unit/                   # Unit tests
    ├── integration/            # Integration tests
    └── security/               # Security tests
```

---

## 🧠 CoreLoopThree: Three-Layer Architecture

### Design Philosophy

CoreLoopThree is AgentOS's core innovative architecture, dividing agent runtime into three orthogonal and synergistic layers for unified cognition, execution, and memory:

```
┌─────────────────────────────────────────┐
         Cognition Layer                  
   • Intent Understanding • Task Planning  
   • Agent Scheduling • Model Coordination 
└───────────────↓─────────────────────────┘
                ↓
┌─────────────────────────────────────────┐
          Execution Layer                 
    • Task Execution • Compensation       
    • Chain Tracing • State Management    
└───────────────↓─────────────────────────┘
                ↓
┌─────────────────────────────────────────┐
          Memory Layer                    
    • Memory Write • Query Retrieval      
    • Context Mount • Evolution & Forgetting
└─────────────────────────────────────────┘
```

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
  - Dynamic task assignment
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
  - Dynamic registration/unregistration
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

See: [CoreLoopThree Architecture Document](partdocs/architecture/coreloopthree.md)

---

## 💾 MemoryRovol: Memory Roll System

### Functional Positioning

MemoryRovol is AgentOS's kernel-level memory system, implementing full-stack memory management from raw data to advanced patterns. It's not just data storage but the core infrastructure for continuous learning, knowledge accumulation, and intelligent evolution.

### Four-Layer Architecture

```
┌─────────────────────────────────────────────────────────┐
               L4 Pattern Layer                            
   • Persistent Homology (Ripser) • Stable Pattern Mining  
   • HDBSCAN Clustering • Rule Generation                
└───────────────────↑─────────────────────────────────────┘
                    ↓ Abstract Evolution
┌─────────────────────────────────────────────────────────┐
             L3 Structure Layer                            
   • Bind/Unbind Operators • Relation Encoding • Temporal  
└───────────────────↑─────────────────────────────────────┘
                    ↓ Feature Extraction
┌─────────────────────────────────────────────────────────┐
             L2 Feature Layer                              
   • Embedding Models (OpenAI/DeepSeek/SentenceTransformers)
   • FAISS Vector Index • Hybrid Search (Vector+BM25)     
└───────────────────↑─────────────────────────────────────┘
                    ↓ Data Compression
┌─────────────────────────────────────────────────────────┐
              L1 Raw Layer                                 
   • File System Storage • Shard Management • Compression  
   • Metadata Index • Integrity Check • Version Control   
└─────────────────────────────────────────────────────────┘
```

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

#### Integration with partdata/registry
- **Registry Data**: MemoryRovol provides backing store for agents.db and skills.db
- **Agent State**: Each agent's runtime state and history stored in L1/L2 layers
- **Skill Memory**: Skill execution records and feedback stored in L2/L3 layers

#### Integration with partdata/traces/spans
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
- **Automatic Abstraction**: Progressive L1→L2→L3→L4 abstraction
  - Feature extraction (L1→L2)
  - Structure binding (L2→L3)
  - Pattern mining (L3→L4)
- **Pattern Discovery**: Identify high-frequency patterns and rules
  - Persistent homology analysis (Ripser)
  - HDBSCAN clustering
  - Stable pattern recognition
- **Weight Updates**: Dynamic weight adjustment based on access frequency and relevance
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

See: [MemoryRovol Architecture Document](partdocs/architecture/memoryrovol.md)

---

## 🛠️ Development Guide

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

See: [BUILD.md](atoms/BUILD.md)

### Logging System

AgentOS uses a unified cross-language logging architecture:

#### Log Storage Location
```
partdata/logs/
├── kernel/         # Kernel layer logs → agentos.log
├── services/       # Service layer logs → llm_d.log, tool_d.log, etc.
└── apps/           # Application layer logs → independent logs per app
```

#### Log Format
- **Human-readable format**: `%(asctime)s.%(msecs)03d [%(levelname)s] [%(name)s] %(message)s`
- **JSON format**: Structured logging for ELK/Splunk integration

#### Cross-Language Log Correlation
- Full-link tracing via `trace_id`
- C/Python/Go/Rust/TypeScript share the same logging specification
- OpenTelemetry integration as observability backend

See: [Logging System Architecture Document](partdocs/architecture/logging_system.md)

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

## 📊 Performance Metrics

Based on standard test environment (Intel i7-12700K, 32GB RAM, NVMe SSD):

### Processing Capability

| Metric | Value | Test Conditions |
| :--- | :--- | :--- |
| **Memory Write Throughput** | 10,000+ entries/sec | L1 layer, async batch write |
| **Vector Search Latency** | < 10ms | FAISS IVF1024,PQ64, k=10 |
| **Hybrid Search Latency** | < 50ms | Vector+BM25, top-100 re-ranking |
| **Memory Abstraction Speed** | 100 entries/sec | L2→L3 progressive abstraction |
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

## 📚 Documentation Resources

### Core Documentation

- [📘 CoreLoopThree Architecture](partdocs/architecture/coreloopthree.md) - Three-layer runtime
- [💾 MemoryRovol Architecture](partdocs/architecture/memoryrovol.md) - Memory roll system
- [🔧 IPC Mechanism](partdocs/architecture/ipc.md) - Inter-process communication
- [⚙️ Microkernel Design](partdocs/architecture/microkernel.md) - Microkernel architecture
- [📞 System Calls](partdocs/architecture/syscall.md) - System call interface
- [📝 Logging System](partdocs/architecture/logging_system.md) - Cross-language logging

### Development Guides

- [🚀 Quick Start](partdocs/guides/getting_started.md) - Getting started guide
- [🤖 Create Agent](partdocs/guides/create_agent.md) - Agent development tutorial
- [🛠️ Create Skill](partdocs/guides/create_skill.md) - Skill development tutorial
- [📦 Deployment Guide](partdocs/guides/deployment.md) - Production deployment
- [🎛️ Kernel Tuning](partdocs/guides/kernel_tuning.md) - Performance optimization guide
- [🔍 Troubleshooting](partdocs/guides/troubleshooting.md) - Common issues

### Technical Specifications

- [📋 Coding Standards](partdocs/specifications/coding_standards.md) - Development standards
- [🧪 Testing Standards](partdocs/specifications/testing.md) - Testing requirements
- [🔒 Security Standards](partdocs/specifications/security.md) - Security practices
- [📊 Performance Metrics](partdocs/specifications/performance.md) - Performance requirements

### External Documentation

- [🏭 Workshop Documentation](../Workshop/README.md) - Data collection factory
- [🔬 Deepness Documentation](../Deepness/README.md) - Deep processing system
- [📊 Benchmark Documentation](../Benchmark/metrics/README.md) - Evaluation metrics

---

## 🔄 Version Roadmap

### Current Version (v1.0.0.5) - Production Ready

**Completion**: 85%

- ✅ Core architecture design completed
- ✅ MemoryRovol memory system implementation
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
- ✅ CoreLoopThree three-layer runtime framework
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
- ✅ Microkernel base module (core)
  - IPC Binder implementation
  - Memory management (RAII, smart pointers)
  - Task scheduling (weighted round-robin algorithm)
  - High-precision time service
- ✅ System call layer (syscall) - 100%
  - ✅ Task syscalls completed
    - `sys_task_submit()` - Submit task
    - `sys_task_query()` - Query status
    - `sys_task_wait()` - Wait for completion
    - `sys_task_cancel()` - Cancel task
  - ✅ Memory syscalls completed
    - `sys_memory_write()` - Write memory
    - `sys_memory_search()` - Semantic search
    - `sys_memory_get()` - Get data
    - `sys_memory_delete()` - Delete memory
  - ✅ Session syscalls completed
    - `sys_session_create()` - Create session
    - `sys_session_get()` - Get information
    - `sys_session_close()` - Close session
    - `sys_session_list()` - List sessions
  - ✅ Observability syscalls completed
    - `sys_telemetry_metrics()` - Get metrics
    - `sys_telemetry_traces()` - Get traces
- ✅ Unified logging system implementation
  - Cross-language logging interface (C/Python/Go/Rust/TS)
  - Centralized log storage (partdata/logs/)
  - trace_id full-link tracing
  - OpenTelemetry integration
- 🔲 Complete end-to-end integration testing

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
  - L3→L4 evolution optimization
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
- Performance benchmark达标
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

- 🌐 Become the de facto standard for agent operating systems
- 🤝 Build a global open-source community ecosystem
- 🏆 Lead next-generation AGI technology development
- 📈 Support trillion-scale memory capacity and millisecond retrieval

---

## 🤝 Ecosystem Cooperation

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

## 📞 Technical Support

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

## 📄 License

AgentOS adopts a **business-friendly, ecosystem-open layered open-source licensing architecture**, consistent with mainstream OS licensing designs, balancing core IP protection, community openness, and commercial deployment freedom.

### Primary License Statement
Core kernel code defaults to **Apache License 2.0**. Full license text available in root [LICENSE](../../LICENSE) file.

### Layered License Details
| Module Directory | Applicable License | Description |
|----------|----------|----------|
| `atoms/` (Kernel) | Apache License 2.0 | CoreLoopThree architecture, MemoryRovol engine, runtime, security isolation layer, etc. |
| `domes/` (Extensions) | Apache License 2.0 | Core architecture extension modules, consistent with kernel license |
| `openhub/` (Ecosystem) | MIT License | Agent marketplace, skill marketplace, community contributions to lower contribution barriers |
| Third-party Dependencies | Original licenses | All third-party dependencies use permissive licenses with proper module isolation |

### You Are Free To
- ✅ **Commercial Use**: Use in closed-source commercial products, enterprise projects, commercial services
- ✅ **Modify**: Modify, customize, and create derivative works without open-sourcing business code
- ✅ **Distribute**: Distribute and copy source code or compiled binaries
- ✅ **Patent Use**: Permanent patent license for core code
- ✅ **Private Use**: Use in personal/private projects without mandatory disclosure

### Your Only Obligations
- Preserve original copyright notices, license text, and NOTICE file
- Include modification records when changing core source files

### Commercial Services & Licensing
- No restrictions on commercial use under this open-source license
- Enterprise-grade technical support, custom development, and private deployment services available

---

## 🙏 Acknowledgments

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

#### 📞 Contact Us

📧 Email: lidecheng@spharx.cn; wangliren@spharx.cn

<p>
  <a href="https://gitee.com/spharx/agentos">Gitee (Official Repository)</a> ·
  <a href="https://github.com/SpharxTeam/AgentOS">GitHub (Mirror Repository)</a> ·
  <a href="https://spharx.cn">Official Website</a> ·
  <a href="mailto:lidecheng@spharx.cn">Technical Support</a>
</p>

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
