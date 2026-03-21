# AgentOS 微内核架构详解

**版本**: v1.0.0.5  
**最后更新**: 2026-03-19  
**路径**: `atoms/core/`

---

## 1. 概述

AgentOS 采用微内核架构设计，将核心功能最小化，所有高级服务运行在用户态，确保系统的稳定性、安全性和可扩展性。

### 1.1 设计理念

```
┌─────────────────────────────────────────┐
│         应用层 (openhub/app)             │
│  • 智能体应用 • 业务逻辑                 │
└───────────────↑─────────────────────────┘
                ↓ 系统调用
┌─────────────────────────────────────────┐
│         服务层 (backs)                   │
│  • LLM 服务 • 工具服务 • 市场服务        │
│  • 调度服务 • 监控服务 • 权限服务        │
└───────────────↑─────────────────────────┘
                ↓ 系统调用
┌─────────────────────────────────────────┐
│         内核层 (atoms)                   │
│  ┌─────────────────────────────────┐   │
│  │      系统调用层 (syscall)        │   │
│  │  • 任务管理 • 记忆管理           │   │
│  │  • 会话管理 • 可观测性           │   │
│  └─────────────────────────────────┘   │
│  ┌─────────────────────────────────┐   │
│  │        微内核 (core)             │   │
│  │  • IPC Binder • Memory Manager   │   │
│  │  • Task Scheduler • Time Service │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

### 1.2 核心价值

- **最小化内核**: 仅保留最基础的通信、内存、调度和时间服务
- **用户态服务**: 所有高级功能以守护进程形式运行在用户态
- **稳定安全**: 服务崩溃不影响内核，故障隔离性好
- **高效通信**: IPC Binder 提供高性能进程间通信机制
- **可移植性**: 内核不依赖特定硬件或操作系统特性

### 1.3 关键特性

- ✅ **IPC Binder**: 基于共享内存和信号量的高效通信
- ✅ **内存管理**: RAII 模式、智能指针、内存池优化
- ✅ **任务调度**: 加权轮询算法、优先级队列
- ✅ **时间服务**: 高精度时间戳、定时器管理
- ✅ **系统调用**: 统一的 syscall 接口抽象

---

## 2. 微内核架构

### 2.1 整体架构图

```
┌─────────────────────────────────────────────────────────┐
│                    AgentOS Kernel                        │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │              System Call Layer                      │ │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐  │ │
│  │  │  Task    │  │  Memory  │  │    Session      │  │ │
│  │  │  Syscall │  │  Syscall │  │    Syscall      │  │ │
│  │  └──────────┘  └──────────┘  └─────────────────┘  │ │
│  │  ┌──────────┐  ┌──────────┐                       │ │
│  │  │ Observable│  │  Unified │                       │ │
│  │  │ Syscall   │  │  Entry   │                       │ │
│  │  └──────────┘  └──────────┘                       │ │
│  └────────────────────────────────────────────────────┘ │
│                           ↕                              │
│  ┌────────────────────────────────────────────────────┐ │
│  │                  Microkernel Core                   │ │
│  │  ┌──────────┐  ┌──────────┐  ┌─────────────────┐  │ │
│  │  │   IPC    │  │  Memory  │  │     Task        │  │ │
│  │  │  Binder  │  │  Manager │  │    Scheduler    │  │ │
│  │  └──────────┘  └──────────┘  └─────────────────┘  │ │
│  │  ┌──────────┐                                      │ │
│  │  │  Time    │                                      │ │
│  │  │  Service │                                      │ │
│  │  └──────────┘                                      │ │
│  └────────────────────────────────────────────────────┘ │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

### 2.2 目录结构

```
atoms/core/
├── CMakeLists.txt                 # 构建配置
├── README.md                      # 模块说明
├── include/                       # 公共头文件
│   ├── ipc_binder.h              # IPC 通信接口
│   ├── memory_manager.h          # 内存管理接口
│   ├── task_scheduler.h          # 任务调度接口
│   ├── time_service.h            # 时间服务接口
│   └── common.h                  # 通用类型定义
└── src/                           # 源代码实现
    ├── ipc_binder.c              # IPC 绑定器实现
    ├── memory_manager.c          # 内存管理器实现
    ├── task_scheduler.c          # 任务调度器实现
    └── time_service.c            # 时间服务实现
```

---

## 3. 核心组件详解

### 3.1 IPC Binder（进程间通信）

#### 功能定位

IPC Binder 是微内核的通信 backbone，提供:
- 基于共享内存的高效数据传输
- 信号量同步机制
- 消息队列管理
- 服务注册与发现

#### 数据结构

```c
typedef struct agentos_ipc_channel {
    char* channel_id;                // 通道唯一标识
    size_t buffer_size;              // 缓冲区大小
    void* shared_memory;             // 共享内存指针
    agentos_semaphore_t* semaphore;  // 同步信号量
    uint32_t flags;                  // 通道标志
} agentos_ipc_channel_t;

typedef struct agentos_ipc_binder {
    agentos_hashmap_t* channels;     // 通道哈希表
    agentos_mutex_t* lock;           // 线程锁
    uint32_t max_channels;           // 最大通道数
} agentos_ipc_binder_t;
```

#### 核心接口

```c
// 创建 IPC 通道
agentos_error_t agentos_ipc_channel_create(
    const char* channel_id,
    size_t buffer_size,
    agentos_ipc_channel_t** out_channel);

// 发送消息
agentos_error_t agentos_ipc_send(
    agentos_ipc_channel_t* channel,
    const void* data,
    size_t len,
    uint32_t timeout_ms);

// 接收消息
agentos_error_t agentos_ipc_recv(
    agentos_ipc_channel_t* channel,
    void** out_data,
    size_t* out_len,
    uint32_t timeout_ms);
```

#### 性能指标

| 指标 | 数值 | 测试条件 |
| :--- | :--- | :--- |
| **消息延迟** | < 10μs | 1KB 消息，共享内存 |
| **吞吐量** | 100,000+ msg/s | 小包消息 |
| **并发连接** | 1000+ channels | 单实例 |

---

### 3.2 Memory Manager（内存管理）

#### 功能定位

提供高效的内存管理机制:
- RAII 模式的自动内存管理
- 智能指针引用计数
- 内存池预分配优化
- 泄漏检测

#### 数据结构

```c
typedef struct agentos_smart_ptr {
    void* data;                      // 数据指针
    size_t* ref_count;               // 引用计数
    void (*destructor)(void*);       // 析构函数
} agentos_smart_ptr_t;

typedef struct agentos_memory_pool {
    void* memory_block;              // 内存块
    size_t block_size;               // 块大小
    size_t free_blocks;              // 空闲块数
    agentos_bitmap_t* allocation_map;// 分配位图
} agentos_memory_pool_t;
```

#### 核心接口

```c
// 创建智能指针
agentos_error_t agentos_smart_ptr_create(
    void* data,
    size_t data_size,
    void (*destructor)(void*),
    agentos_smart_ptr_t** out_ptr);

// 引用计数增加
agentos_error_t agentos_smart_ptr_add_ref(
    agentos_smart_ptr_t* ptr);

// 引用计数减少
agentos_error_t agentos_smart_ptr_release(
    agentos_smart_ptr_t* ptr);

// 创建内存池
agentos_error_t agentos_memory_pool_create(
    size_t block_size,
    size_t block_count,
    agentos_memory_pool_t** out_pool);
```

#### 内存优化策略

- **小对象池**: 64B/128B/256B/512B 固定大小池
- **大对象分配**: mmap 直接映射
- **对齐优化**: cache line 对齐（64 字节）
- **NUMA 感知**: 本地节点分配（规划中）

---

### 3.3 Task Scheduler（任务调度）

#### 功能定位

负责任务调度和资源分配:
- 加权轮询调度算法
- 优先级队列管理
- 公平调度保障
- QoS 支持

#### 数据结构

```c
typedef enum {
    TASK_PRIORITY_LOW = 0,
    TASK_PRIORITY_NORMAL = 1,
    TASK_PRIORITY_HIGH = 2,
    TASK_PRIORITY_REALTIME = 3
} agentos_task_priority_t;

typedef struct agentos_task {
    char* task_id;                   // 任务 ID
    agentos_task_priority_t priority;// 优先级
    uint64_t created_time;           // 创建时间
    uint64_t execution_time;         // 执行时间
    void (*entry_point)(void*);      // 入口函数
    void* argument;                  // 参数
} agentos_task_t;

typedef struct agentos_scheduler {
    agentos_priority_queue_t* queue; // 优先级队列
    agentos_mutex_t* lock;           // 线程锁
    uint32_t weight_table[4];        // 权重表
} agentos_scheduler_t;
```

#### 调度算法

**加权轮询实现**:
```c
void agentos_scheduler_run(agentos_scheduler_t* scheduler) {
    while (!scheduler->shutdown) {
        // 按权重选择优先级
        uint32_t total_weight = 0;
        for (int i = 0; i < 4; i++) {
            total_weight += scheduler->weight_table[i];
        }
        
        // 随机选择
        uint32_t choice = rand() % total_weight;
        uint32_t cumulative = 0;
        
        for (int i = 3; i >= 0; i--) {
            cumulative += scheduler->weight_table[i];
            if (choice < cumulative) {
                // 执行该优先级的任务
                agentos_task_t* task = 
                    agentos_priority_queue_pop(scheduler->queue, i);
                if (task) {
                    task->entry_point(task->argument);
                }
                break;
            }
        }
    }
}
```

#### 核心接口

```c
// 提交任务
agentos_error_t agentos_scheduler_submit(
    agentos_scheduler_t* scheduler,
    agentos_task_t* task);

// 取消任务
agentos_error_t agentos_scheduler_cancel(
    agentos_scheduler_t* scheduler,
    const char* task_id);

// 等待任务完成
agentos_error_t agentos_scheduler_wait(
    agentos_scheduler_t* scheduler,
    const char* task_id,
    uint32_t timeout_ms);
```

---

### 3.4 Time Service（时间服务）

#### 功能定位

提供高精度时间服务:
- 纳秒级时间戳
- 定时器管理
- 时间同步
- 性能计时

#### 数据结构

```c
typedef struct agentos_timer {
    uint64_t interval_ns;            // 间隔时间（纳秒）
    uint64_t next_fire_ns;           // 下次触发时间
    void (*callback)(void*);         // 回调函数
    void* user_data;                 // 用户数据
    bool repeat;                     // 是否重复
} agentos_timer_t;

typedef struct agentos_time_service {
    uint64_t epoch_ns;               // 纪元时间
    agentos_min_heap_t* timers;      // 定时器最小堆
    agentos_mutex_t* lock;           // 线程锁
} agentos_time_service_t;
```

#### 核心接口

```c
// 获取当前时间（纳秒）
uint64_t agentos_time_now_ns(void);

// 创建定时器
agentos_error_t agentos_timer_create(
    uint64_t interval_ms,
    void (*callback)(void*),
    void* user_data,
    bool repeat,
    agentos_timer_t** out_timer);

// 启动定时器
agentos_error_t agentos_timer_start(
    agentos_time_service_t* service,
    agentos_timer_t* timer);

// 停止定时器
agentos_error_t agentos_timer_stop(
    agentos_time_service_t* service,
    agentos_timer_t* timer);
```

#### 时间精度

| 操作 | 精度 | 延迟 |
| :--- | :--- | :--- |
| **时间戳获取** | 纳秒级 | < 10ns |
| **定时器精度** | 毫秒级 | ±1ms |
| **超时控制** | 毫秒级 | ±5ms |

---

## 4. 与其他模块的交互

### 4.1 与 syscall 层的关系

```
CoreLoopThree / Backs Services
          ↓
    Syscall Layer (统一入口)
          ↓
    Microkernel Core
    ├─ IPC Binder
    ├─ Memory Manager
    ├─ Task Scheduler
    └─ Time Service
```

**示例**: 任务提交流程
```c
// 1. 应用层调用 syscall
agentos_syscall_invoke("task.submit", task_params, &result);

// 2. Syscall 层转发到内核调度器
agentos_scheduler_submit(&g_scheduler, task);

// 3. 调度器将任务加入队列
agentos_priority_queue_push(scheduler->queue, task);

// 4. 调度线程唤醒并执行
```

### 4.2 与 CoreLoopThree 的关系

CoreLoopThree 通过 syscall 层间接使用微内核服务:

```c
// CoreLoopThree 认知层
agentos_cognition_process(input, &plan);
    ↓
// 通过 syscall 提交任务
sys_task_submit(plan.tasks);
    ↓
// 微内核调度器执行
agentos_scheduler_run();
```

---

## 5. 实现状态 (v1.0.0.5)

### 5.1 已完成功能

| 组件 | 完成度 | 状态 |
| :--- | :--- | :--- |
| **IPC Binder** | 100% | ✅ 生产就绪 |
| **Memory Manager** | 95% | ✅ 基础功能完整 |
| **Task Scheduler** | 100% | ✅ 加权轮询实现 |
| **Time Service** | 100% | ✅ 高精度定时器 |

### 5.2 性能基准

**测试环境**: Intel i7-12700K, 32GB RAM, Linux 6.5

| 指标 | 数值 | 备注 |
| :--- | :--- | :--- |
| **IPC 消息延迟** | < 10μs | 1KB 共享内存 |
| **内存分配速度** | < 5ns | 池分配 |
| **任务调度延迟** | < 1ms | 加权轮询 |
| **时间戳精度** | 纳秒级 | clock_gettime |

---

## 6. 开发指南

### 6.1 使用 IPC 通信

```c
#include "ipc_binder.h"

// 创建通道
agentos_ipc_channel_t* channel;
agentos_ipc_channel_create("my_service", 4096, &channel);

// 发送消息
const char* msg = "Hello from service";
agentos_ipc_send(channel, msg, strlen(msg), 1000);

// 接收消息
char* recv_msg;
size_t recv_len;
agentos_ipc_recv(channel, (void**)&recv_msg, &recv_len, 1000);
```

### 6.2 使用智能指针

```c
#include "memory_manager.h"

// 创建带析构函数的智能指针
struct my_data* data = malloc(sizeof(struct my_data));
agentos_smart_ptr_t* ptr;
agentos_smart_ptr_create(data, sizeof(*data), free, &ptr);

// 自动引用计数管理
agentos_smart_ptr_add_ref(ptr);
agentos_smart_ptr_release(ptr);  // 自动释放
```

---

## 7. 故障排查

### 7.1 常见问题

#### 问题：IPC 通信超时
**症状**: `agentos_ipc_send()` 返回超时错误  
**排查**:
1. 检查通道是否已创建
2. 验证缓冲区大小是否足够
3. 查看接收端是否正常工作

#### 问题：内存泄漏
**症状**: 系统内存持续增长  
**排查**:
1. 启用内存泄漏检测
2. 检查智能指针引用计数
3. 验证析构函数是否正确注册

### 7.2 调试技巧

- 启用 Debug 日志级别
- 使用 `agentos_memory_stats()` 查看内存使用
- 监控 IPC 通道队列长度

---

## 8. 参考资料

- [README.md](../../README.md) - 项目总览
- [syscall.md](syscall.md) - 系统调用接口文档
- [coreloopthree.md](coreloopthree.md) - CoreLoopThree 架构详解
- [ipc_binder.h](../include/ipc_binder.h) - IPC 头文件
- [memory_manager.h](../include/memory_manager.h) - 内存管理头文件

---

<div align="center">

**© 2026 SPHARX Ltd. All Rights Reserved.**

*From data intelligence emerges*

</div>