# CoreKern — 微内核

`agentos/atoms/corekern/`

**版本**: v0.1.0 | **API 版本**: 1.0.0

---

## 概述

CoreKern 是 AgentOS 的**"核中之核"**，是系统最底层的微内核实现。它提供最小化的核心服务集合，遵循经典的微内核架构设计理念——在内核态只保留绝对必要的服务，所有其他系统服务都以用户态进程的形式运行。CoreKern 的设计受到 L4 微内核和 seL4 形式化验证内核的启发，强调最小特权、机制与策略分离、服务化三大原则。

CoreKern 以 C11 标准实现，通过 `AGENTOS_API` 宏导出符号，支持 Windows（`__declspec(dllexport/dllimport)`）和 POSIX（`__attribute__((visibility("default")))`）双平台。构建产物为静态库 `agentos_core`，启用安全编译选项（栈保护、FORTIFY_SOURCE、PIE、RELRO、控制流防护等）。

---

## 设计哲学

| 原则 | 说明 |
|------|------|
| **最小化** | 内核仅包含 IPC、内存管理、任务调度、时间服务、可观测性五个原子能力 |
| **机制与策略分离** | 内核只提供机制（如 IPC 通道），策略由用户态服务决定 |
| **服务化** | 所有非核心功能（文件系统、网络、设备驱动）作为用户态服务运行 |
| **安全性** | 通过 IPC 权限检查和能力模型确保访问控制 |
| **零依赖** | 内核级 IPC 消息仅 40 字节，不依赖 commons 或其他外部模块 |

---

## 目录结构

```
corekern/
├── include/                        # 公共头文件
│   ├── agentos.h                   # 统一入口（聚合所有子模块头文件）
│   ├── export.h                    # 符号导出宏（AGENTOS_API / AGENTOS_INTERNAL）
│   ├── error.h                     # 错误码定义 + 日志宏
│   ├── ipc.h                       # IPC/Binder 进程间通信接口
│   ├── mem.h                       # 内存管理接口（分配、池化、泄漏检测）
│   ├── task.h                      # 任务调度接口（优先级、依赖解析、资源预留）
│   ├── agentos_time.h              # 时间服务接口（单调/实时时钟、定时器、事件循环）
│   ├── observability.h             # 可观测性子系统（指标、追踪、健康检查）
│   ├── platform_core.h             # 平台类型委托（→ commons/platform）
│   └── stdatomic.h                 # 原子操作兼容层
├── src/                            # 内核实现
│   ├── core_init.c                 # agentos_core_init() / agentos_core_shutdown()
│   ├── error.c                     # 错误码工具函数
│   ├── ipc/
│   │   ├── binder.c                # Binder IPC 核心（通道管理、消息路由）
│   │   └── buffer.c                # IPC 缓冲区管理
│   ├── mem/
│   │   ├── alloc.c                 # 内存分配器（malloc/free/realloc 封装 + 调试追踪）
│   │   ├── pool.c                  # 内存池（固定块大小的高效分配）
│   │   ├── guard.c                 # 内存保护页（越界检测）
│   │   └── oom_handler.c           # OOM 分级响应框架实现
│   ├── task/
│   │   ├── scheduler.c             # 调度器入口
│   │   ├── scheduler_core.c        # 调度器核心逻辑（优先级队列、依赖解析）
│   │   ├── scheduler_platform.c    # 平台抽象层
│   │   ├── scheduler_posix.c       # POSIX 线程调度实现
│   │   ├── scheduler_windows.c     # Windows 线程调度实现
│   │   ├── scheduler_core.h        # 调度器内部头文件
│   │   ├── scheduler_platform.h    # 调度器平台头文件
│   │   └── thread.c                # 线程管理
│   ├── time/
│   │   ├── clock.c                 # 单调/实时时钟实现
│   │   ├── timer.c                 # 定时器管理（单次/周期性）
│   │   └── event.c                 # 事件同步原语 + 事件循环
│   └── observability/
│       └── observability.c         # 可观测性子系统实现
├── tests/                          # 单元测试
│   ├── test_corekern.c             # 内核核心功能测试
│   ├── test_kernel_state.c         # 内核状态测试
│   ├── test_memory_engine.c        # 内存引擎测试
│   ├── test_observability.c        # 可观测性测试
│   └── CMakeLists.txt
└── CMakeLists.txt                  # 构建配置
```

---

## 核心能力

### 1. IPC/Binder — 进程间通信

提供统一、高效的进程间通信机制，是微内核的核心通信方式。内核级 IPC 消息 `agentos_kernel_ipc_message_t` 仅 40 字节，与应用层 `agentos_ipc_message_t`（200+ 字节）区分设计。

| 接口 | 功能 | 线程安全 |
|------|------|----------|
| `agentos_ipc_init()` | 初始化 IPC 子系统 | 否 |
| `agentos_ipc_create_channel()` | 创建命名 IPC 通道 | 否 |
| `agentos_ipc_connect()` | 连接到已存在的通道 | 否 |
| `agentos_ipc_send()` | 发送消息（异步） | 否 |
| `agentos_ipc_recv()` | 接收消息（带超时） | 否 |
| `agentos_ipc_call()` | 同步调用（请求-响应） | 是 |
| `agentos_ipc_reply()` | 回复消息 | 是 |
| `agentos_ipc_close()` | 关闭通道 | 否 |

**消息结构**:

```c
typedef struct {
    uint32_t code;       // 消息码（0x01=数据, 0x02=控制, 0x03=响应）
    const void *data;    // 消息负载指针（零拷贝设计）
    size_t size;         // 负载大小
    int32_t fd;          // 文件描述符（传递文件句柄）
    uint64_t msg_id;     // 消息唯一标识（请求-响应匹配）
} agentos_kernel_ipc_message_t;
```

### 2. Memory Manager — 内存管理

提供基础内存分配、内存池和泄漏检测能力。

| 接口 | 功能 | 线程安全 |
|------|------|----------|
| `agentos_mem_init()` | 初始化内存系统（指定堆大小） | 否 |
| `agentos_mem_alloc()` | 分配内存（自动清零） | 是 |
| `agentos_mem_alloc_ex()` | 分配内存（带调试信息：文件名+行号） | 是 |
| `agentos_mem_aligned_alloc()` | 对齐内存分配 | 是 |
| `agentos_mem_free()` | 释放内存（NULL 安全） | 是 |
| `agentos_mem_realloc()` | 重新分配内存 | 是 |
| `agentos_mem_pool_create()` | 创建内存池 | 否 |
| `agentos_mem_pool_alloc()` | 从内存池分配 | 是 |
| `agentos_mem_pool_free()` | 释放到内存池（双重释放检测） | 是 |
| `agentos_mem_stats()` | 获取内存使用统计 | 是 |
| `agentos_mem_check_leaks()` | 检查内存泄漏 | 否 |

> **OOM Handler**：提供五级内存压力分级响应框架（NORMAL → WARNING → DEGRADED → CRITICAL → FATAL），在内存压力递增时依次触发预警、限流、降级、紧急释放和致命处理策略，确保系统在内存紧张场景下的可控降级与安全退出。

### 3. Scheduler — 任务调度器

负责任务/线程的调度和执行，支持优先级抢占、依赖解析和资源预留。

| 接口 | 功能 | 线程安全 |
|------|------|----------|
| `agentos_task_init()` | 初始化调度系统 | 否 |
| `agentos_task_self()` | 获取当前任务 ID | 是 |
| `agentos_task_sleep()` | 任务休眠（毫秒） | 是 |
| `agentos_task_yield()` | 让出 CPU 时间片 | 是 |
| `agentos_task_set_priority()` | 设置任务优先级（0-100） | 是 |
| `agentos_task_get_state()` | 获取任务状态 | 是 |
| `agentos_scheduler_resolve_dependencies()` | 拓扑排序 + 循环依赖检测 | 否 |
| `agentos_scheduler_priority_inherit()` | 优先级继承（防止优先级反转） | 是 |
| `agentos_scheduler_resource_reserve()` | 资源预留检查 | 是 |

**任务状态**: `CREATED → READY → RUNNING → BLOCKED → TERMINATED`

**优先级常量**: `MIN(0)` / `LOW(25)` / `NORMAL(50)` / `HIGH(75)` / `MAX(100)`

**依赖解析增强** (IMP-A1):
- `agentos_dep_result_t` — 拓扑排序结果 + 优先级继承 + 循环报告
- `agentos_cycle_report_t` — 循环依赖检测报告（节点数组 + 描述）
- `AGENTOS_ECYCLE` — 循环依赖错误码

### 4. Time Service — 时间服务

提供统一的时间基准、定时器和事件同步原语。

| 接口 | 功能 |
|------|------|
| `agentos_time_monotonic_ns()` | 单调时钟（纳秒，不受系统时间调整影响） |
| `agentos_time_monotonic_ms()` | 单调时钟（毫秒） |
| `agentos_time_current_ns()` | 当前时间（纳秒） |
| `agentos_time_realtime_ns()` | 实时时钟（纳秒） |
| `agentos_time_nanosleep()` | 纳秒级休眠 |
| `agentos_timer_create()` | 创建定时器（回调驱动） |
| `agentos_timer_start()` | 启动定时器（单次/周期性） |
| `agentos_event_create()` | 创建事件同步原语 |
| `agentos_event_wait()` | 等待事件（带超时） |
| `agentos_event_signal()` | 触发事件 |
| `agentos_time_eventloop_run()` | 运行事件循环（阻塞） |

### 5. Observability — 可观测性子系统

提供生产级可观测性功能，支持 99.999% 可靠性标准的监控需求。

| 能力 | 接口 | 说明 |
|------|------|------|
| 指标收集 | `agentos_metric_counter_create/inc()` | Counter 计数器 |
| | `agentos_metric_gauge_create/set()` | Gauge 仪表 |
| | `agentos_metric_record()` | 通用指标采样 |
| 分布式追踪 | `agentos_trace_span_start/end()` | OpenTelemetry 标准追踪 |
| | `agentos_trace_set_tag/log()` | 追踪标签和日志 |
| 健康检查 | `agentos_health_check_register/run()` | 注册和执行健康检查 |
| 性能监控 | `agentos_performance_get_metrics()` | CPU/内存/线程数 |
| 导出 | `agentos_observability_export_prometheus()` | Prometheus 格式导出 |

---

## 系统引导流程

```
┌───────────────────┐
│  Phase 1: 硬件初始化 │
│  - CPU 模式设置     │
│  - 中断控制器初始化  │
│  - 内存控制器检测    │
└─────────┬─────────┘
          ▼
┌───────────────────┐
│  Phase 2: 内核初始化 │
│  - 页表建立         │
│  - 内核堆初始化     │
│  - 调度器初始化     │
│  - IPC 子系统初始化  │
│  - 时间服务初始化    │
│  - 可观测性初始化    │
└─────────┬─────────┘
          ▼
┌───────────────────┐
│  Phase 3: 服务启动   │
│  - 启动系统守护进程  │
│  - 注册系统服务     │
│  - 启动用户初始化   │
│  - 进入主循环       │
└───────────────────┘
```

---

## 错误码体系

CoreKern 使用 `agentos_error_t`（int32_t）统一错误类型，正值表示成功，负值表示错误。基础错误码由 `agentos_types.h` 提供，CoreKern 扩展错误码：

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `AGENTOS_EINTR` | -31 | 操作被信号中断 |
| `AGENTOS_EBADF` | -32 | 错误的文件描述符 |
| `AGENTOS_ERESOURCE` | -33 | 资源不足 |
| `AGENTOS_ENOSYS` | -34 | 系统调用未实现 |
| `AGENTOS_ECYCLE` | -35 | 检测到循环依赖 |
| `AGENTOS_EFAIL` | -36 | 通用失败 |

---

## 依赖关系

| 依赖项 | 来源 | 用途 |
|--------|------|------|
| `agentos_types.h` | commons | 统一类型定义（task_id_t、error 码等） |
| `platform.h` | commons/platform | 线程同步原语（mutex、cond、thread） |
| `error.h` | commons/utils/error | 统一错误码基础定义 |
| `Threads::Threads` | CMake | POSIX 线程库 |
| `agentos_platform_libs` | CMake | 平台特定链接库 |

---

## 构建说明

CoreKern 构建为静态库 `agentos_core`：

```bash
# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target agentos_core

# 运行单元测试
cmake -B build -DBUILD_TESTS=ON
ctest --test-dir build -R corekern
```

**安全编译选项**:
- GCC/Clang: `-fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -Wformat -Wformat-security -fno-omit-frame-pointer`
- MSVC: `/GS /sdl /guard:cf`
- 链接器: `-Wl,-z,relro -Wl,-z,now -pie`（Linux）、`-Wl,-dead_strip -pie`（macOS）

---

## 使用示例

```c
#include "agentos.h"

int main(void) {
    // 初始化 AgentOS 核心
    int rc = agentos_core_init();
    if (rc != AGENTOS_SUCCESS) return 1;

    // 创建 IPC 通道
    agentos_ipc_channel_t *ch;
    agentos_ipc_create_channel("my_service", NULL, NULL, &ch);

    // 分配内存
    void *buf = agentos_mem_alloc(1024);

    // 获取单调时间
    uint64_t ts = agentos_time_monotonic_ns();

    // 使用完毕后清理
    agentos_mem_free(buf);
    agentos_ipc_close(ch);
    agentos_core_shutdown();
    return 0;
}
```

---

## 与上层模块的关系

- **Syscall 层**: 基于 CoreKern 的 IPC 和调度能力，为上层提供标准化的系统调用接口
- **CoreLoopThree**: 在 CoreKern 的调度器基础上实现三层循环运行时
- **Memory/MemoryRovol**: 利用 CoreKern 的内存管理能力实现记忆存储
- **TaskFlow**: 基于 CoreKern 的任务调度能力，提供高级任务编排
- **Frameworks**: 通过 CoreKern 的能力间接访问系统资源

---

© 2026 SPHARX Ltd. All Rights Reserved.
