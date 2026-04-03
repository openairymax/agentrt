# AgentOS 微内核基础模块 (CoreKern)

**版本**: v1.0.0.7
**最后更新**: 2026-04-03
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`corekern/` 是 AgentOS 的**微内核核心实现**，提供操作系统最基础的四大机制：

- **IPC (Binder)**: 进程间通信的唯一通道，仿 Android Binder 设计
- **内存管理**: 物理内存分配器、内存池、边界保护
- **任务调度**: 线程/任务管理、CFS/RR 调度器、同步原语
- **时间服务**: 系统时钟、定时器、事件循环

**重要**: 本模块仅包含机制（怎么做），不包含策略（做什么）。所有上层服务运行在用户态。

---

## 📁 目录结构

```
corekern/
├── README.md                 # 本文档
├── CMakeLists.txt            # 编译配置
├── .clang-format             # 代码格式化配置
├── .clang-tidy               # 代码静态分析配置
├── include/                  # 公共头文件
│   ├── agentos.h             # 统一头文件（对外接口）
│   ├── ipc.h                 # IPC 接口定义
│   ├── mem.h                 # 内存接口定义
│   ├── task.h                # 任务接口定义
│   ├── time.h                # 时间接口定义
│   ├── error.h               # 错误码定义
│   ├── export.h              # 跨平台导出宏
│   └── observability.h       # 可观测性接口定义
├── src/                      # 核心实现
│   ├── main.c                # 内核入口点
│   ├── error.c               # 错误处理实现
│   ├── ipc/                  # IPC 实现
│   │   ├── binder.c          # Binder 驱动模拟
│   │   ├── buffer.c          # 缓冲区管理
│   │   └── channel.c         # 通信通道管理
│   ├── mem/                  # 内存管理实现
│   │   ├── alloc.c           # 物理内存分配器（带追踪）
│   │   ├── guard.c           # 内存边界保护
│   │   └── pool.c            # 内存池分配器
│   ├── task/                 # 任务调度实现
│   │   ├── scheduler.c       # 调度器核心逻辑
│   │   ├── scheduler_core.c  # 跨平台调度算法
│   │   ├── scheduler_platform.c # 平台抽象层
│   │   ├── scheduler_posix.c # POSIX 平台实现
│   │   ├── scheduler_windows.c # Windows 平台实现
│   │   ├── sync.c            # 同步原语
│   │   └── thread.c          # 线程管理
│   ├── time/                 # 时间服务实现
│   │   ├── clock.c           # 系统时钟
│   │   ├── event.c           # 事件管理
│   │   └── timer.c           # 定时器
│   └── observability/        # 可观测性实现
│       └── observability.c   # 指标收集与追踪
└── tests/                    # 单元测试
    ├── CMakeLists.txt        # 测试编译配置
    ├── test_main.c           # 测试主入口
    ├── test_error.c          # 错误处理测试
    ├── test_ipc.c            # IPC 功能测试
    ├── test_mem.c            # 内存管理测试
    ├── test_task.c           # 任务调度测试
    └── test_time.c           # 时间服务测试
```

---

## 🔧 核心功能详解

### 1. IPC (Binder) 机制

仿 Android Binder 设计的跨进程通信方案。

#### 架构设计

```
+-------------+      +-------------+
|   Client    |      |   Server    |
|   Proxy     |      |   Service   |
+------+------+      +------+------+n       |
       |                    |
       +--------+  +--------+
                |  |
          +-----v--v-----+
          |  Binder     |
          |  Driver     |
          +-------------+
```

#### 使用示例

```c
#include <agentos.h>

// === Server 端 ===
// 1. 创建服务
class EchoService : public IInterface {
public:
    // 实现接口方法
    virtual status_t onTransact(uint32_t code,
                                const Parcel& data,
                                Parcel* reply) {
        switch(code) {
            case CMD_ECHO:
                String8 msg = data.readString();
                reply->writeString(msg);
                return OK;
        }
        return UNKNOWN_TRANSACTION;
    }
};

// 2. 注册到服务管理器
sp<EchoService> service = new EchoService();
defaultServiceManager()->addService(
    String16("echo"), service);

// === Client 端 ===
// 1. 获取远程服务
sp<IBinder> binder = defaultServiceManager()
    ->getService(String16("echo"));

// 2. 创建代理
sp<IEcho> echo = interface_cast<IEcho>(binder);

// 3. 调用远程方法
String8 result = echo->sayHello(String8("World"));
```

#### 性能指标

| 指标 | 数值 | 测试条件 |
|------|------|----------|
| 单次调用延迟 | < 50μs | 本地进程 |
| 吞吐量 | > 10,000 次/s | 1KB 数据 |
| 最大传输 | 1MB | 单次事务 |
| 内存开销 | < 1KB | 每次调用 |

### 2. 内存管理

三层内存管理体系：

#### 层级结构

```
+------------------+
|  Application     | malloc/free
+------------------+
        ↓
+------------------+
|  Memory Pool     | 预分配，快速响应
+------------------+
        ↓
+------------------+
|  Physical Alloc  | 页式管理，4KB 对齐
+------------------+
```

#### 使用示例

```c
#include <agentos.h>

// === 物理内存分配 ===
phys_addr_t phys = phys_alloc_pages(2);  // 分配 2 页 (8KB)
if (phys == PHYS_ADDR_NONE) {
    ALOGE("Failed to allocate physical memory");
}

// === 内存池 ===
mem_pool_t* pool = mem_pool_create(4096);  // 4KB 池

// 快速分配 (无碎片)
void* ptr = mem_pool_alloc(pool, 256);

// 释放
mem_pool_free(pool, ptr);

// 统计信息
mem_pool_stats_t stats;
mem_pool_get_stats(pool, &stats);
printf("Used: %zu bytes\n", stats.used_bytes);

// === 边界保护 ===
void* guarded = mem_alloc_guarded(1024);
// 访问越界会触发 SIGSEGV
memset(guarded, 0, 2048);  // ❌ 崩溃!
```

#### 内存布局

```
+-----------------+ ← 0x1000
| Guard Page      | (不可访问)
+-----------------+ ← 0x2000
| User Data       | 1KB
+-----------------+ ← 0x2400
| Guard Page      | (不可访问)
+-----------------+ ← 0x3000
```

### 3. 任务调度

支持两种调度策略：CFS (完全公平) 和 RR (轮转)。

#### 使用示例

```c
#include <agentos.h>

// === 创建线程 ===
task_t* task = task_create("worker_thread",
    worker_func,      // 入口函数
    NULL,             // 参数
    PRIORITY_NORMAL,  // 优先级
    STACK_SIZE_DEFAULT // 栈大小 (8KB)
);

// === 设置调度策略 ===
sched_attr_t attr;
attr.sched_policy = SCHED_FIFO;  // 实时调度
attr.sched_priority = 50;        // 优先级 1-99
task_setscheduler(task, &attr);

// === 同步原语 ===
// 互斥锁
mutex_t* mtx = mutex_create();
mutex_lock(mtx);
// ... 临界区 ...
mutex_unlock(mtx);

// 信号量
sem_t* sem = sem_create(3);  // 初始值 3
sem_wait(sem);   // P 操作
sem_post(sem);   // V 操作
```

#### 优先级列表

| 优先级 | 范围 | 用途 |
|--------|------|------|
| REALTIME | 70-99 | 硬实时任务 |
| HIGH | 50-69 | 关键服务 |
| NORMAL | 20-49 | 普通任务 |
| LOW | 0-19 | 后台任务 |

### 4. 时间服务

高精度的时间管理和定时器。

#### 使用示例

```c
#include <agentos.h>

// === 获取时间 ===
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);
int64_t uptime_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

// === 一次性定时器 ===
timer_t* timer = timer_create();
timer_settime(timer, 1000, timer_callback, NULL);  // 1 秒后触发

// === 周期性定时器 ===
timer_periodic_t* periodic = timer_periodic_create();
timer_periodic_start(periodic, 500, periodic_cb, NULL);  // 每 500ms

// === 事件循环 ===
event_loop_t* loop = event_loop_create();

// 添加定时器事件
event_loop_add_timer(loop, 1000, timer_handler);

// 添加 IO 事件
event_loop_add_fd(loop, fd, EVENT_READ, read_handler);

// 运行循环
event_loop_run(loop);
```

---

## 🚀 快速开始

### 编译

```bash
cd AgentOS/atoms/corekern
mkdir build && cd build
cmake ..
make                    # 编译所有
make -j8                # 并行编译加速
```

### 测试

```bash
cd build
ctest                   # 运行所有测试
ctest -R ipc           # 只测 IPC
test_ipc               # 直接运行 IPC 测试
```

### 集成到你的项目

```cmake
# CMakeLists.txt
add_subdirectory(../atoms/corekern corekern_build)
target_link_libraries(your_app PRIVATE corekern)
```

---

## 📊 性能基准

### IPC 性能

| 数据大小 | 延迟 (P50) | 延迟 (P99) | 吞吐量 |
|----------|-----------|-----------|--------|
| 64B | 35μs | 52μs | 28,000/s |
| 1KB | 42μs | 68μs | 23,000/s |
| 10KB | 120μs | 210μs | 8,300/s |
| 100KB | 850μs | 1.2ms | 1,170/s |

### 内存性能

| 操作 | 延迟 | 说明 |
|------|------|------|
| phys_alloc | < 1μs | 页分配 |
| mem_pool_alloc | < 100ns | 池分配 |
| malloc (glibc) | ~50ns | 对比参考 |

### 调度性能

| 指标 | 数值 |
|------|------|
| 上下文切换开销 | < 5μs |
| 调度延迟 (P99) | < 100μs |
| 支持最大任务数 | 10,000+ |

---

## 🛠️ 调试技巧

### 启用调试日志

```c
// 编译时开启
-DCMAKE_BUILD_TYPE=Debug

// 运行时设置环境变量
export AGENTOS_LOG_LEVEL=DEBUG
export AGENTOS_LOG_IPC=1  # 记录所有 IPC 调用
```

### 内存泄漏检测

```bash
# 使用 ASAN
-DCMAKE_C_FLAGS="-fsanitize=address"

# 运行测试
ctest -R mem_leak_test
```

### IPC 追踪

```bash
# 使用 strace 追踪 Binder 调用
strace -e ioctl ./your_app

# 或使用内置追踪
export AGENTOS_TRACE_IPC=1
```

---

## 🔗 相关文档

- [原子层总览](../README.md)
- [系统调用层](../syscall/README.md)
- [轻量级内核](../../atomsmini/README.md)
- [项目根目录](../../README.md)

---

## 🤝 贡献指南

欢迎提交 Issue 或 Pull Request！

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"The foundation of intelligence. 智能之基石。"*

