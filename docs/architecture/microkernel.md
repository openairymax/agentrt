Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 微内核架构

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪
**相关原则**: K-1 (内核极简), K-2 (接口契约化), K-3 (服务隔离), K-4 (可插拔策略)
**核心文档**: [微内核设计](../../agentos/manuals/architecture/microkernel.md)

---

## 📐 设计哲学

AgentOS 采用**纯净微内核**设计理念，遵循 Liedtke 微内核构造定理：

> **内核最小职责 = IPC + 地址空间管理 + 线程调度**
>
> **机制与策略分离：内核提供机制，用户态实现策略**

### 核心设计原则

| 原则 | 编号 | 描述 | 实现位置 |
|------|------|------|----------|
| **K-1: 内核极简** | Kernel-1 | 内核只提供四大原子机制，其他功能全部用户态实现 | `agentos/corekern/` |
| **K-2: 接口契约化** | Kernel-2 | 所有跨层交互通过标准化的 Syscall 接口 | `agentos/atoms/syscall/` |
| **K-3: 服务隔离** | Kernel-3 | 每个守护进程独立运行在独立进程/容器中 | `agentos/daemon/` |
| **K-4: 可插拔策略** | Kernel-4 | 策略算法可运行时替换，无需重启系统 | 动态加载机制 |

---

## 🏗️ 架构层次

```
┌─────────────────────────────────────────────────────────────┐
│                    用户态服务层 (User Space)                   │
│  ┌─────────┬─────────┬─────────┬─────────┬─────────┐        │
│  │  llm_d  │market_d │ monit_d │ tool_d  │ sched_d │        │
│  └─────────┴─────────┴─────────┴─────────┴─────────┘        │
├─────────────────────────────────────────────────────────────┤
│                    系统调用接口层 (Syscall)                    │
│    task │ memory │ session │ telemetry │ agent              │
├──────────────────┬──────────────────────────────────────────┤
│   安全穹顶       │         微内核核心 (CoreKern)               │
│  (cupolas)      │  ┌─────────────────────────────┐          │
│                 │  │ IPC · Mem · Task · Time     │          │
│  workbench      │  └─────────────────────────────┘          │
│  permission     │                                        │
│  sanitizer      │  三层认知运行时 (CoreLoopThree)             │
│  audit          │  认知 → 规划 → 调度 → 执行                  │
├──────────────────┴──────────────────────────────────────────┤
│                    四层记忆系统 (MemoryRovol)                  │
│  L1原始卷 → L2特征层 → L3结构层 → L4模式层                     │
├─────────────────────────────────────────────────────────────┤
│                    公共基础库 (Commons)                        │
│  error │ logger │ metrics │ trace │ cost                    │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔧 四大原子机制

### 1. IPC (Inter-Process Communication)

**零拷贝Binder通信机制**

```c
// IPC Binder 调用示例
agentos_ipc_handle_t handle;
agentos_ipc_call(&handle, "memory.store", payload, &response);
```

**性能指标**:
- 延迟: < 1μs (本地调用)
- 吞吐量: > 1M calls/sec
- 机制: 共享内存 + 信号量

**详细文档**: [IPC Binder设计](../../agentos/manuals/architecture/ipc.md)

---

### 2. Memory Management

**分层内存管理**

```c
// 内存分配示例
void* ptr = agentos_mem_alloc(size, AGENTOS_MEM_KERNEL);
agentos_mem_free(ptr);
```

**特性**:
- 虚拟内存管理
- 内存池分配器
- 零拷贝优化
- 内存隔离保护

---

### 3. Task Scheduling

**加权轮询调度器**

```c
// 任务创建示例
agentos_task_t task;
agentos_task_create(&task, "my_agent", entry_point, priority);
agentos_task_schedule(&task);
```

**调度策略**:
- 加权公平调度 (WFQ)
- 优先级继承协议
- 时间片轮转
- 抢占式多任务

**性能指标**:
- 调度延迟: < 1ms
- 支持并发: 100+ 任务

---

### 4. Time Management

**高精度时钟子系统**

```c
// 时间获取示例
uint64_t now = agentos_time_now(AGENTOS_TIME_MONOTONIC);
uint64_t deadline = now + timeout_ms * 1000000ULL;
```

**特性**:
- 单调时钟 (CLOCK_MONOTONIC)
- 高精度定时器 (< 1μs)
- 时间戳同步
- 超时管理

---

## 🔌 系统调用接口 (Syscall)

所有用户态与内核态的交互必须通过标准化的 Syscall 接口：

### Syscall 分类

| 类别 | 接口前缀 | 功能描述 | 文档位置 |
|------|----------|----------|----------|
| **任务管理** | `agentos_task_*` | 任务创建、调度、销毁 | [task.md](../api/kernel-api.md#task-api) |
| **记忆管理** | `agentos_memory_*` | 记忆存储、查询、检索 | [memory.md](../../agentos/manuals/api/syscalls/memory.md) |
| **会话管理** | `agentos_session_*` | 会话创建、状态维护 | [session.md](../../agentos/manuals/api/syscalls/session.md) |
| **可观测性** | `agentos_telemetry_*` | 指标采集、日志、追踪 | [telemetry.md](../../agentos/manuals/api/syscalls/telemetry.md) |
| **Agent管理** | `agentos_agent_*` | Agent生命周期管理 | [agent.md](../../agentos/manuals/api/syscalls/agent.md) |

### Syscall 调用流程

```
用户态代码
    ↓
Syscall Wrapper (参数校验、序列化)
    ↓
IPC Binder (跨进程通信)
    ↓
内核态处理 (权限检查、执行)
    ↓
返回结果 (反序列化、错误处理)
    ↓
用户态继续执行
```

**详细文档**: [系统调用设计](../../agentos/manuals/architecture/syscall.md)

---

## 🛡️ 安全穹顶 (Cupolas)

四层纵深防御的安全体系：

### 防护层级

| 层级 | 组件 | 机制 | 安全等级 |
|------|------|------|----------|
| **L1: 虚拟工位** | workbench/ | 进程/容器/WASM沙箱隔离 | 进程级隔离 |
| **L2: 权限裁决** | permission/ | YAML规则引擎 + RBAC | 细粒度访问控制 |
| **L3: 输入净化** | sanitizer/ | 正则过滤 + 类型检查 | 注入攻击防护 |
| **L4: 审计追踪** | audit/ | 全链路追踪 + 不可篡改日志 | 合规审计 |

### 安全配置示例

```yaml
# cupolas 配置示例
cupolas:
  workbench:
    isolation_type: container  # process | container | wasm
    resource_limits:
      memory: 512MB
      cpu_cores: 2
      network: restricted

  permission:
    mode: strict  # strict | relaxed | disabled
    rules_file: /app/config/permission-rules.yaml

  sanitizer:
    mode: strict
    filters:
      - type: sql_injection
      - type: xss
      - type: command_injection
      - type: path_traversal

  audit:
    enabled: true
    log_path: /app/logs/audit.log
    retention_days: 90
```

**详细文档**: [安全穹顶设计](architecture/cupolas.md)

---

## 📊 性能指标

| 指标 | 数值 | 测试条件 | 说明 |
|------|------|---------|------|
| **IPC 延迟** | < 1μs | 本地调用 | 零拷贝优化 |
| **任务调度延迟** | < 1ms | 100并发任务 | 加权轮询 |
| **内存分配延迟** | < 100ns | 小块内存 | 内存池 |
| **Syscall 开销** | < 5% | 相比直接调用 | 内核态切换 |
| **启动时间** | < 500ms | Cold start | 完整初始化 |
| **内存占用** | ~2MB | 典型场景 | 基础运行时 |

---

## 🔨 开发指南

### 编译内核

```bash
# 从源码构建
cd AgentOS
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
make install
```

### 运行内核

```bash
# 启动内核服务
./bin/agentos-kernel --config /path/to/config.yaml

# 查看帮助
./bin/agentos-kernel --help
```

### 调试内核

```bash
# 使用 GDB 调试
gdb ./bin/agentos-kernel
(gdb) break main
(gdb) run --config /path/to/config.yaml

# 使用 Valgrind 检测内存泄漏
valgrind --leak-check=full ./bin/agentos-kernel
```

---

## 📚 相关文档

- **[架构设计原则 V1.8](../../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md)** — 五维正交原则体系
- **[三层认知运行时](architecture/coreloopthree.md)** — CoreLoopThree 架构
- **[四层记忆系统](architecture/memoryrovol.md)** — MemoryRovol 架构
- **[IPC Binder 通信](../../agentos/manuals/architecture/ipc.md)** — IPC 机制详解
- **[系统调用设计](../../agentos/manuals/architecture/syscall.md)** — Syscall 接口契约
- **[统一日志系统](../../agentos/manuals/architecture/logging_system.md)** — 日志规范
- **[C 编码规范](../../agentos/manuals/specifications/coding_standard/C_coding_style_guide.md)** — 代码风格

---

## 🧪 测试

### 单元测试

```bash
# 运行内核单元测试
ctest --test-dir build/tests/kernel --output-on-failure

# 运行特定测试
./build/tests/kernel/test_ipc_binder
```

### 集成测试

```bash
# 运行 IPC 集成测试
ctest --test-dir build/tests/integration --label-regex ipc

# 运行完整集成测试套件
ctest --test-dir build/tests/integration --output-on-failure
```

### 性能基准测试

```bash
# IPC 吞吐量测试
./build/benchmarks/ipc_throughput --iterations 1000000

# 任务调度延迟测试
./build/benchmarks/task_scheduling_latency --tasks 100
```

---

## ⚠️ 已知限制

1. **平台支持**: 目前主要支持 Linux (x86_64, aarch64)
2. **最大并发任务数**: 默认限制 1000（可通过配置调整）
3. **内存限制**: 单个任务默认最大 512MB
4. **IPC 消息大小**: 单次传输最大 64KB

---

## 🔮 未来规划

- [ ] 支持 Windows 和 macOS 平台
- [ ] 引入 eBPF 进行内核监控
- [ ] 实现实时调度策略 (RT Scheduler)
- [ ] 支持硬件虚拟化加速
- [ ] 引入形式化验证 (seL4 风格)

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
