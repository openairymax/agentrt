# AgentOS (OpenApex) C++ 编码规范

**版本**: Doc V1.6  
**备选名称**: OpenApex (开源极境 / 极境OS)  
**更新日期**: 2026-03-25  
**适用范围**: AgentOS 所有 C++ 代码模块  
**理论基础**: 工程两论（反馈闭环）、系统工程（层次分解）、五维正交系统（系统观、内核观、认知观、工程观、设计美学）、双系统认知理论、微内核哲学  
**原则映射**: S-1至S-4（系统设计）、K-1至K-4（内核设计）、C-1至C-4（认知设计）、E-1至E-8（工程设计）、A-1至A-4（设计美学）

---

## 一、概述

### 1.1 编制目的

本规范为 AgentOS 项目中的 C++ 代码提供统一的编码标准。基于项目架构设计原则的五维正交系统，本规范聚焦于工程观维度，为开发者提供可操作的代码实现指南。

### 1.2 理论基础

本规范基于 AgentOS 架构设计原则的五维正交系统，聚焦于**工程观**维度：

- **《工程控制论》**（原则 S-1, E-2）：通过错误码、日志、健康检查和指标构建反馈闭环，使系统能自我观测并对异常自动响应
- **《论系统工程》**（原则 S-2, K-2）：模块化、接口驱动，边界清晰、实现可替换
- **双系统认知理论**（原则 C-1）：提供 System 1（快速、低延迟）与 System 2（安全、全面）两条路径，并允许运行时策略切换
- **微内核哲学**（原则 K-1, K-4）：接口精炼、命名优雅、注释说明"为什么"，而非"做什么"

**关联原则**:
- E-1 安全内生原则
- E-2 可观测性原则
- E-3 资源确定性原则
- E-5 命名语义化原则
- E-6 错误可追溯原则
- E-7 文档即代码原则
- E-8 可测试性原则

### 1.3 适用范围

| 模块 | 语言标准 | 编译器要求 |
|------|----------|-----------|
| atoms/corekern | C++17 | GCC 11+ / Clang 14+ |
| daemon/ | C++17 | GCC 11+ / Clang 14+ |
| openlab/ | C++17 | GCC 11+ / Clang 14+ |

### 1.4 与 AgentOS 架构的关系

本规范适用于 AgentOS 的以下层次：

| 层次 | 组件 | C++ 代码位置 | 关联原则 |
|------|------|-------------|---------|
| 原子内核 | corekern/ | atoms/corekern/ | K-1, K-2 |
| 认知层 | coreloopthree/ | atoms/cognition/ | C-1, C-2 |
| 记忆层 | memoryrovol/ | atoms/memory/ | C-3, C-4 |
| 安全层 | cupolas/ | atoms/cupolas/ | E-1, S-2 |
| 系统调用 | syscall/ | atoms/syscall/ | K-2, K-3 |
| 守护进程 | daemon/ | daemon/*_d/ | S-3, K-4 |

**层次纪律**（原则 S-2）:
- `atoms/` 内部模块只能通过 `corekern/` 的 IPC 机制通信
- `daemon/` 守护进程只能通过 `syscalls.h` 与内核交互
- 禁止跨层访问，禁止循环依赖

---

## 二、文件组织

### 2.1 文件命名

- **头文件**：使用 `.h` 扩展名，采用 `snake_case` 风格
- **源文件**：使用 `.cpp` 扩展名，采用 `snake_case` 风格
- **命名空间目录**：与命名空间层次对应

```
atoms/corekern/
├── include/
│   ├── agentos_types.h
│   ├── memory_manager.h
│   └── ipc_binder.h
└── src/
    ├── memory_manager.cpp
    └── ipc_binder.cpp
```

### 2.2 头文件结构

```cpp
// Copyright (c) 2026 SPHARX. All Rights Reserved.

#ifndef AGENTOS_MODULE_NAME_H
#define AGENTOS_MODULE_NAME_H

#include "agentos_base.h"
#include "agentos_types.h"

namespace agentos {

// 外部 C 链接声明
#ifdef __cplusplus
extern "C" {
#endif

// 常量定义
constexpr uint32_t AGENTOS_MAX_NAME_LEN = 256;

// 类型声明
class ModuleClass;
struct ModuleConfig;

// 函数声明
int module_init(const ModuleConfig* manager);
void module_shutdown();

#ifdef __cplusplus
}
#endif

// 命名空间内容
namespace module_internal {
    // 内部实现细节
} // namespace module_internal

} // namespace agentos

#endif // AGENTOS_MODULE_NAME_H
```

### 2.3 包含顺序

```cpp
// 1. 对应头文件（如果是在 .cpp 中）
#include "module.h"

// 2. C 系统头文件
#include <cstdint>
#include <cstring>
#include <cerrno>

// 3. C++ 标准库头文件
#include <memory>
#include <vector>
#include <string>
#include <optional>

// 4. 其他库头文件
#include <openssl/aes.h>
#include <uv.h>

// 5. 项目内部头文件
#include "agentos_types.h"
#include "module_base.h"
```

---

## 三、命名规范

### 3.1 命名风格

| 类型 | 风格 | 示例 |
|------|------|------|
| 命名空间 | snake_case | `agentos::corekern`, `agentos::memory_rovol` |
| 类名 | UpperCamelCase | `class MemoryManager`, `struct TaskContext` |
| 函数名 | snake_case | `memory_alloc()`, `task_submit()` |
| 变量名 | snake_case | `uint32_t task_id`, `std::string config_path` |
| 常量名 | kConstant | `kMaxRetryCount`, `kDefaultTimeout` |
| 枚举值 | kEnumValue 或 UPPER_CASE | `kStatusIdle`, `AGENTOS_STATUS_IDLE` |
| 模板参数 | UpperCamelCase | `typename Allocator`, `class NodeType` |
| 宏定义 | UPPER_CASE | `#define AGENTOS_MAX_PATH_LEN 4096` |

### 3.2 命名前缀规则

- **模块前缀**：公共 API 使用模块前缀
  - `atoms_`: 原子内核
  - `cog_`: 认知层
  - `mem_`: 记忆层
  - `exec_`: 执行层
  - `cupolas_`: 安全穹顶层

### 3.3 命名示例

```cpp
// 正确的命名
namespace agentos {
    constexpr uint32_t kMaxTaskNameLength = 256;
    
    class TaskScheduler {
    public:
        enum class Status {
            kIdle,
            kRunning,
            kCompleted
        };
        
        int submit(const TaskPlan& plan);
        int wait(const std::string& task_id, uint32_t timeout_ms);
        
    private:
        std::vector<Task> tasks_;
        uint32_t max_workers_;
    };
}

// 错误的命名
class task_scheduler {  // 类名应该 UpperCamelCase
public:
    int SubmitTask();    // 函数名应该 snake_case
    int m_nMaxWorkers;  // 成员变量应该 snake_case，不使用 m_ 前缀
};
```

---

## 四、类型设计

### 4.1 类型别名

```cpp
// 使用 using 声明类型别名
using ErrorCode = int32_t;
using TaskId = std::string;
using ByteBuffer = std::vector<uint8_t>;

// 智能指针类型别名（放在头文件中）
using SharedConfig = std::shared_ptr<const ModuleConfig>;
using UniqueHandle = std::unique_ptr<Handle, HandleDeleter>;
```

### 4.2 枚举类

```cpp
// 使用枚举类替代枚举
enum class TaskStatus : uint32_t {
    kIdle = 0,
    kPending = 1,
    kRunning = 2,
    kCompleted = 4,
    kFailed = 5,
    kCancelled = 6
};

// 枚举类位域操作
enum class MemFlags : uint32_t {
    kNone = 0,
    kRead = 1 << 0,
    kWrite = 1 << 1,
    kExecute = 1 << 2,
    kShared = 1 << 3
};

inline MemFlags operator|(MemFlags a, MemFlags b) {
    return static_cast<MemFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}
```

### 4.3 结构体设计

```cpp
// 公开结构体用于跨模块数据传递
struct TaskResult {
    TaskId task_id;
    ErrorCode error_code;
    ByteBuffer output_data;
    uint64_t execution_time_us;
};

// 不透明句柄类型
class SchedulerImpl;
using SchedulerHandle = SchedulerImpl*;  // 不透明指针
```

---

## 五、函数设计

### 5.1 函数签名规范

```cpp
// 公共 API 必须使用 Doxygen 注释
/**
 * @brief 提交任务计划并返回任务 ID
 * 
 * 将任务计划加入调度队列，返回任务 ID 用于后续查询。
 * 调度器会根据优先级和依赖关系决定执行顺序。
 * 
 * @param engine 引擎句柄，非 NULL
 * @param plan 待调度计划，只读
 * @param priority 任务优先级，1-10
 * @param out_id 输出任务 ID，调用者负责释放
 * @return AGENTOS_OK (0) 成功；其他值为错误码
 * 
 * @note 调用者负责释放 out_id（调用 agentos_task_id_free）
 * @thread safe
 * @see cognition_wait()
 */
int cognition_submit(
    CognitionEngine* engine,
    const TaskPlan* plan,
    uint32_t priority,
    char** out_id
);
```

### 5.2 参数验证

```cpp
int cognition_submit(
    CognitionEngine* engine,
    const TaskPlan* plan,
    uint32_t priority,
    char** out_id
) {
    // 参数验证遵循 AGENTOS_CHECK 系列宏
    AGENTOS_CHECK_PTR(engine);
    AGENTOS_CHECK_PTR(plan);
    AGENTOS_CHECK_PTR(out_id);
    
    // 范围验证
    if (priority < 1 || priority > 10) {
        log_error("Invalid priority: %u, must be in [1, 10]", priority);
        return AGENTOS_ERR_INVALID_ARG;
    }
    
    // ...
}
```

### 5.3 返回值约定

| 返回类型 | 成功条件 | 失败处理 |
|----------|----------|----------|
| `int` (错误码) | 返回 0 (`AGENTOS_OK`) | 返回负值错误码 |
| 指针类型 | 返回非空指针 | 返回 `nullptr` |
| `std::optional<T>` | 返回包含值 | 返回 `std::nullopt` |
| `std::expected<T, E>` | 返回包含值 | 返回包含错误 |

```cpp
// 示例：使用 std::expected（C++23）
std::expected<TaskResult, ErrorCode> task_execute(TaskId id);

// 示例：使用 std::optional
std::optional<TaskMetrics> get_metrics(const std::string& task_id);
```

---

## 六、类设计

### 6.1 类结构模板

```cpp
/**
 * @brief 调度器类
 * 
 * 负责管理任务的生命周期和执行。调度器采用双系统架构，
 * System 1 处理简单任务，System 2 处理复杂任务。
 * 
 * @note 此类是线程安全的
 */
class Scheduler : public std::enable_shared_from_this<Scheduler> {
public:
    /**
     * @brief 调度器配置
     */
    struct manager {
        std::string name;
        uint32_t max_workers;
        uint32_t default_timeout_ms;
        bool enable_retry;
    };
    
    /**
     * @brief 构造调度器
     * @param manager 调度器配置
     * @throws std::invalid_argument 当配置无效时
     */
    explicit Scheduler(const manager& manager);
    
    /**
     * @brief 析构函数
     * 
     * 确保所有待处理任务已完成或取消。
     */
    ~Scheduler();
    
    // 禁止拷贝
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    
    // 允许移动
    Scheduler(Scheduler&&) noexcept;
    Scheduler& operator=(Scheduler&&) noexcept;
    
    /**
     * @brief 提交任务
     */
    int submit(const TaskPlan& plan, TaskId& out_id);
    
    /**
     * @brief 等待任务完成
     */
    int wait(const TaskId& id, uint32_t timeout_ms, TaskResult& result);
    
private:
    class Impl;  // 不透明实现
    std::unique_ptr<Impl> impl_;
};
```

### 6.2 成员变量布局

```cpp
class MemoryPool {
public:
    // 公开接口
    
private:
    // 1. 简单类型成员变量
    size_t block_size_;
    uint32_t pool_id_;
    
    // 2. 智能指针成员变量
    std::unique_ptr<Allocator> allocator_;
    
    // 3. STL 容器成员变量
    std::vector<void*> free_list_;
    std::unordered_map<void*, BlockMeta> allocated_;
    
    // 4. 句柄/指针成员变量（放在最后）
    void* backing_memory_;
};
```

### 6.3 资源管理

```cpp
class ResourceHandle {
public:
    ResourceHandle() = default;
    
    // 移动语义
    ResourceHandle(ResourceHandle&& other) noexcept
        : resource_(other.resource_) {
        other.resource_ = nullptr;
    }
    
    ResourceHandle& operator=(ResourceHandle&& other) noexcept {
        if (this != &other) {
            release();
            resource_ = other.resource_;
            other.resource_ = nullptr;
        }
        return *this;
    }
    
    // 禁止拷贝
    ResourceHandle(const ResourceHandle&) = delete;
    ResourceHandle& operator=(const ResourceHandle&) = delete;
    
    /**
     * @brief 资源释放
     * 
     * 释放关联的系统资源。此函数保证线程安全。
     */
    ~ResourceHandle() { release(); }
    
    /**
     * @brief 获取底层资源
     */
    handle_type get() const { return resource_; }
    
    /**
     * @brief 检查资源是否有效
     */
    explicit operator bool() const { return resource_ != nullptr; }
    
private:
    void release() {
        if (resource_ != nullptr) {
            release_impl(resource_);
            resource_ = nullptr;
        }
    }
    
    handle_type resource_ = nullptr;
};
```

---

## 七、内存管理

### 7.1 智能指针使用

```cpp
// 独占所有权：使用 unique_ptr
std::unique_ptr<manager> manager = std::make_unique<manager>();
std::unique_ptr<Task[]> tasks = std::make_unique<Task[]>(count);

// 共享所有权：使用 shared_ptr
std::shared_ptr<Scheduler> scheduler = std::make_shared<Scheduler>(manager);

// 禁止直接使用 new/delete
// 错误：
Task* task = new Task();
// 正确：
auto task = std::make_unique<Task>();

// 循环引用检测：使用 weak_ptr 打破循环
class Parent;
class Child;

class Parent {
public:
    void set_child(std::shared_ptr<Child> child) {
        child_ = child;
    }
private:
    std::weak_ptr<Child> child_;  // 使用 weak_ptr 避免循环引用
};
```

### 7.2 内存分配

```cpp
// 使用allocator进行内存分配
#include <memory_resource>

void* operator new(size_t size) {
    void* ptr = std::pmr::get_default_resource()->allocate(size, alignof(std::max_align_t));
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    if (ptr) {
        std::pmr::get_default_resource()->deallocate(ptr, 0, alignof(std::max_align_t));
    }
}

// 局部内存池分配
class SmallObjectPool {
public:
    void* allocate(size_t size) {
        if (size <= kMaxSmallSize) {
            return allocate_from_pool(size);
        }
        return ::operator new(size);
    }
    
    void deallocate(void* ptr, size_t size) {
        if (size <= kMaxSmallSize) {
            deallocate_to_pool(ptr, size);
        } else {
            ::operator delete(ptr);
        }
    }
};
```

### 7.3 内存安全检查

```cpp
// 使用地址Sanitizer检测内存问题
// 编译时启用：-fsanitize=address -fsanitize=leak -fsanitize=undefined

// 敏感数据内存安全擦除
class SecureBuffer {
public:
    ~SecureBuffer() {
        // 安全擦除敏感数据
        if (data_ && size_ > 0) {
            secure_memset(data_, 0, size_);
        }
    }
    
private:
    uint8_t* data_;
    size_t size_;
};

// 安全内存操作
inline void secure_memset(void* ptr, uint8_t value, size_t size) {
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (size--) {
        *p++ = value;
    }
}
```

---

## 八、错误处理

### 8.1 错误码定义

```cpp
// 错误码命名：模块名_ERR_描述
enum : int {
    AGENTOS_OK = 0,
    
    // 通用错误 (1xxx)
    AGENTOS_ERR_GENERAL = -1000,
    AGENTOS_ERR_INVALID_ARG = -1001,
    AGENTOS_ERR_OUT_OF_MEMORY = -1002,
    AGENTOS_ERR_TIMEOUT = -1003,
    AGENTOS_ERR_NOT_FOUND = -1004,
    AGENTOS_ERR_ALREADY_EXISTS = -1005,
    AGENTOS_ERR_PERMISSION_DENIED = -1006,
    
    // 调度器错误 (2xxx)
    COG_ERR_SCHEDULER_CLOSED = -2001,
    COG_ERR_QUEUE_FULL = -2002,
    COG_ERR_TASK_CANCELLED = -2003,
    
    // 记忆层错误 (3xxx)
    MEM_ERR_ALLOCATION_FAILED = -3001,
    MEM_ERR_OUT_OF_QUOTA = -3002,
    MEM_ERR_VECTOR_INDEX_CORRUPTED = -3003
};

// 错误码转字符串
constexpr const char* agentos_strerror(int err) {
    switch (err) {
        case AGENTOS_OK: return "Success";
        case AGENTOS_ERR_INVALID_ARG: return "Invalid argument";
        case AGENTOS_ERR_OUT_OF_MEMORY: return "Out of memory";
        // ...
        default: return "Unknown error";
    }
}
```

### 8.2 错误日志

```cpp
// 错误日志宏，包含上下文信息
#define AGENTOS_LOG_ERROR(fmt, ...) \
    log_error("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

#define AGENTOS_LOG_ERROR_WITH_TRACE(fmt, ...) \
    log_error("[%s:%d] [trace_id=%s] " fmt, \
        __FILE__, __LINE__, get_current_trace_id(), ##__VA_ARGS__)

// 错误路径必须记录
int memory_alloc(size_t size, void** out_ptr) {
    void* ptr = allocate_from_pool(size);
    if (!ptr) {
        AGENTOS_LOG_ERROR_WITH_TRACE(
            "Memory allocation failed: size=%zu", size);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    *out_ptr = ptr;
    return AGENTOS_OK;
}
```

### 8.3 异常使用规范

```cpp
// AgentOS C++ 代码中，公共 API 禁止抛出异常
// 内部实现可以使用异常，但必须转换为错误码

class InternalParser {
public:
    // 内部方法可以使用异常
    ParseResult parse(const std::string& input) {
        try {
            return do_parse(input);
        } catch (const ParseError& e) {
            return ParseResult::error(e.what());
        }
    }
    
private:
    ParseResult do_parse(const std::string& input);
};

// RAII 用于异常安全
class AutoCleanup {
public:
    explicit AutoCleanup(std::function<void()> cleanup)
        : cleanup_(std::move(cleanup)) {}
    
    ~AutoCleanup() {
        if (cleanup_) {
            cleanup_();
        }
    }
    
    // 禁止拷贝和移动
    AutoCleanup(const AutoCleanup&) = delete;
    AutoCleanup& operator=(const AutoCleanup&) = delete;
    
private:
    std::function<void()> cleanup_;
};
```

---

## 九、并发编程

### 9.1 线程同步原语

```cpp
// 使用 RAII 包装的锁
class ThreadSafeQueue {
public:
    void push(Task task) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(task));
        cv_.notify_one();
    }
    
    std::optional<Task> pop(uint32_t timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                [this] { return !queue_.empty(); });
        }
        if (queue_.empty()) {
            return std::nullopt;
        }
        Task task = std::move(queue_.front());
        queue_.pop();
        return task;
    }
    
private:
    std::queue<Task> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};
```

### 9.2 原子操作

```cpp
// 使用 std::atomic 进行无锁编程
class Statistics {
public:
    void record_success() {
        success_count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    void record_failure() {
        failure_count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    double success_rate() const {
        uint64_t success = success_count_.load(std::memory_order_acquire);
        uint64_t total = success + failure_count_.load(std::memory_order_acquire);
        return total > 0 ? static_cast<double>(success) / total : 0.0;
    }
    
private:
    std::atomic<uint64_t> success_count_{0};
    std::atomic<uint64_t> failure_count_{0};
};
```

### 9.3 双系统并发模型

```cpp
// System 1: 快速路径，使用轻量级锁
class FastPathProcessor {
public:
    int process_simple_task(const Task& task) {
        std::lock_guard<std::mutex> lock(FAST_LOCK);
        // 快速处理逻辑
        return do_process(task);
    }
    
private:
    static std::mutex FAST_LOCK;
};

// System 2: 慢速路径，使用读写锁
class SlowPathProcessor {
public:
    int process_complex_task(const Task& task) {
        std::unique_lock<std::shared_mutex> lock(rw_lock_);
        // 复杂处理逻辑，可能涉及 LLM 调用
        return do_process_with_llm(task);
    }
    
    StrategyConfig get_strategy() {
        std::shared_lock<std::shared_mutex> lock(rw_lock_);
        return strategy_config_;
    }
    
private:
    mutable std::shared_mutex rw_lock_;
    StrategyConfig strategy_config_;
};
```

---

## 十、代码注释

### 10.1 Doxygen 注释风格

```cpp
/**
 * @file memory_pool.h
 * @brief 内存池接口
 * 
 * 提供固定大小内存块的分配器。适用于高频分配/释放场景，
 * 如任务调度、事件处理等。
 * 
 * @author AgentOS Team
 * @date 2026-03-25
 * @version 1.6
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see atoms/corekern/memory/
 */

/**
 * @brief 分配内存块
 * 
 * 从内存池中分配一块固定大小的内存。分配速度优于标准
 * malloc，但只能分配预定大小的内存块。
 * 
 * @param pool 内存池句柄，非 NULL
 * @param block_id 输出参数，接收分配的块 ID
 * @return 成功返回内存指针，失败返回 NULL
 * 
 * @warning 分配的内存必须使用 memory_pool_free 释放
 * @note 块 ID 用于内存释放，必须妥善保存
 * 
 * @example
 * @code
 * void* ptr = nullptr;
 * uint32_t block_id = 0;
 * int err = memory_pool_alloc(pool, &block_id, &ptr);
 * if (err != AGENTOS_OK) {
 *     log_error("Allocation failed: %s", agentos_strerror(err));
 *     return err;
 * }
 * // 使用 ptr...
 * memory_pool_free(pool, block_id);
 * @endcode
 * 
 * @see memory_pool_free()
 */
void* memory_pool_alloc(MemoryPool* pool, uint32_t* block_id, void** out_ptr);
```

### 10.2 TODO/FIXME 注释

```cpp
// TODO(author): 添加缓存淘汰策略 [P1] [#1234]
// TODO(zhangsan): 支持 NUMA-aware 分配 [P2]

// FIXME(author): 存在竞态条件 [Critical]
// FIXME(lisi): 高并发下性能下降，需要优化锁粒度 [P1]
```

---

## 十一、现代 C++ 特性

### 11.1 auto 关键字

```cpp
// 推导出复杂类型时使用 auto
auto metrics = scheduler->collect_metrics();
auto processor = std::make_unique<FastProcessor>();

// 迭代器使用 auto
for (auto it = tasks.begin(); it != tasks.end(); ++it) {
    // ...
}

// 范围 for 循环
for (const auto& task : tasks) {
    // ...
}

// 明确类型更清晰时不使用 auto
int count = tasks.size();  // 明确是整数
double ratio = success / total;  // 明确是浮点数
```

### 11.2 std::optional

```cpp
// 表示值可能不存在
std::optional<std::string> find_config(const std::string& key) {
    auto it = configs_.find(key);
    if (it != configs_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// 使用
auto manager = find_config("log_level");
if (manager.has_value()) {
    log_info("Log level: %s", manager->c_str());
} else {
    log_warn("Log level not configured, using default");
}

// 配合默认值
std::string level = find_config("log_level").value_or("INFO");
```

### 11.3 std::variant 和 std::visit

```cpp
// 类型安全的联合
using TaskResult = std::variant<TaskSuccess, TaskFailure, TaskPending>;

void handle_result(const TaskResult& result) {
    std::visit(
        [](const auto& r) {
            using T = std::decay_t<decltype(r)>;
            if constexpr (std::is_same_v<T, TaskSuccess>) {
                log_info("Task completed: output_size=%zu", r.output.size());
            } else if constexpr (std::is_same_v<T, TaskFailure>) {
                log_error("Task failed: error=%s", r.message.c_str());
            } else if constexpr (std::is_same_v<T, TaskPending>) {
                log_debug("Task still pending");
            }
        },
        result
    );
}
```

---

## 十二、性能优化

### 12.1 零拷贝原则

```cpp
// 避免不必要的拷贝
void process_data(const ByteBuffer& data) {
    // data 是 const 引用，避免拷贝
    // 如果需要修改，传入指针
}

// 使用移动语义
ByteBuffer create_buffer() {
    ByteBuffer buffer;
    // ... 填充 buffer
    return buffer;  // 移动而非拷贝
}

// 预分配容量
std::vector<Task> tasks;
tasks.reserve(1000);  // 预分配，避免多次重新分配
```

### 12.2 缓存友好

```cpp
// 结构体布局优化：按访问频率排列
struct TaskInfo {
    uint64_t task_id;      // 最常访问
    uint32_t status;       // 次常访问
    uint64_t create_time;  // 较少访问
    std::string metadata;   // 大数据放最后，避免 cache line 污染
};

// 数据对齐
struct alignas(64) CacheLineData {
    uint64_t counters[8];  // 填满一个 cache line
};
```

---

## 十三、测试集成

### 13.1 单元测试框架

```cpp
// 使用 Google Test
#include <gtest/gtest.h>

class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.block_size = 128;
        config_.num_blocks = 100;
        pool_ = std::make_unique<MemoryPool>(config_);
    }
    
    void TearDown() override {
        pool_.reset();
    }
    
    MemoryPool::manager config_;
    std::unique_ptr<MemoryPool> pool_;
};

TEST_F(MemoryPoolTest, AllocateAndFree) {
    void* ptr = nullptr;
    uint32_t block_id = 0;
    
    int err = pool_->allocate(&block_id, &ptr);
    ASSERT_EQ(AGENTOS_OK, err);
    ASSERT_NE(nullptr, ptr);
    
    err = pool_->free(block_id);
    ASSERT_EQ(AGENTOS_OK, err);
}

TEST_F(MemoryPoolTest, ExhaustPool) {
    std::vector<std::pair<uint32_t, void*>> allocations;
    
    for (size_t i = 0; i < 100; ++i) {
        void* ptr = nullptr;
        uint32_t block_id = 0;
        int err = pool_->allocate(&block_id, &ptr);
        if (err == AGENTOS_ERR_OUT_OF_MEMORY) {
            break;
        }
        allocations.emplace_back(block_id, ptr);
    }
    
    // 清理
    for (auto& [id, ptr] : allocations) {
        pool_->free(id);
    }
}
```

---
## 十四、AgentOS 模块 C++ 编码示例

### 14.1 Atoms（原子层）C++ 编码
Atoms模块实现微内核核心功能，要求最高级别的性能和可靠性：

#### 14.1.1 内存管理器（映射原则：M-3 拓扑优化）
```cpp
/**
 * @brief NUMA感知内存分配器 - 体现工程观（E-3）和系统观（S-1）原则
 * 
 * 基于五维正交系统设计：
 * - 系统观（S-1）：垂直分层，作为内核基础设施
 * - 内核观（K-1）：最小特权，仅提供基本分配原语
 * - 工程观（E-3）：资源确定性，保证分配时间上限
 * - 设计美学（A-1）：接口简洁，命名优雅
 * 
 * @see memoryrovol.md 中的记忆进化算法
 */
class NumaAwareAllocator {
public:
    /**
     * @brief 分配NUMA优化内存（映射原则：M-3）
     * 
     * 根据CPU拓扑选择最优内存节点，减少跨节点访问延迟。
     * 集成持久同调分析，动态优化分配策略。
     * 
     * @param size 请求大小，自动对齐到HUGEPAGE边界
     * @param numa_node 目标NUMA节点，-1表示自动选择
     * @param flags 分配标志，见AllocFlags枚举
     * @return 分配的内存指针，失败返回nullptr
     * 
     * @note 此函数被coreloopthree调度循环频繁调用，必须极致优化
     * @thread_safe 线程安全，内部使用细粒度锁
     */
    void* allocate_numa(size_t size, int numa_node = -1, 
                        AllocFlags flags = AllocFlags::kDefault) {
        // 规则3-1：内存申请校验
        if (size == 0 || size > kMaxNumaAllocSize) {
            AGENTOS_LOG_ERROR("Invalid NUMA allocation size: %zu", size);
            return nullptr;
        }
        
        // 规则2-3：防止整数溢出（对齐计算）
        size_t aligned_size = align_up(size, kHugepageSize);
        if (aligned_size < size) {
            AGENTOS_LOG_ERROR("Alignment overflow: size=%zu", size);
            return nullptr;
        }
        
        // NUMA节点信任边界验证
        if (numa_node < -1 || numa_node >= numa::num_configured_nodes()) {
            AGENTOS_LOG_ERROR("Invalid NUMA node: %d", numa_node);
            return nullptr;
        }
        
        // 实际分配逻辑...
    }
    
private:
    // 遵循类设计规范：简单类型在前，复杂类型在后
    std::atomic<uint64_t> total_allocated_{0};
    std::atomic<uint64_t> allocation_count_{0};
    std::unique_ptr<NumaPool> pools_[kMaxNumaNodes];
    mutable std::shared_mutex pool_mutex_;  // 读写锁保护池访问
};
```

#### 14.1.2 任务调度器（映射原则：C-2 认知优化）
```cpp
/**
 * @brief 双系统任务调度器 - 体现认知观（C-1, C-2）和工程观（E-2）原则
 * 
 * 实现System 1（快速路径）和System 2（慢速路径）双系统架构：
 * - System 1：简单任务，直接执行
 * - System 2：复杂任务，深度分析后执行
 * 
 * @see coreloopthree.md 中的三循环架构
 * @see microkernel.md 中的任务管理原语
 */
class DualSystemScheduler : public SchedulerBase {
public:
    // 使用现代C++特性：enum class 类型安全
    enum class SystemSelection {
        kAuto,      // 自动选择
        kSystem1,   // 强制使用System 1
        kSystem2    // 强制使用System 2
    };
    
    /**
     * @brief 提交任务到双系统调度器
     * 
     * 根据任务复杂度自动选择执行系统，支持运行时策略切换。
     * 
     * @param task 任务对象，使用移动语义避免拷贝
     * @param selection 系统选择策略
     * @return 任务ID，失败返回std::nullopt
     * 
     * @performance 关键路径，必须高效
     */
    std::optional<TaskId> submit(Task&& task, 
                                 SystemSelection selection = SystemSelection::kAuto) {
        // 参数验证（规则2-1）
        if (!task.valid()) {
            AGENTOS_LOG_ERROR("Invalid task submission");
            return std::nullopt;
        }
        
        // 双系统选择逻辑
        SystemSelection target = selection;
        if (target == SystemSelection::kAuto) {
            target = select_system_based_on_complexity(task);
        }
        
        // 根据系统选择提交
        if (target == SystemSelection::kSystem1) {
            return submit_to_system1(std::move(task));
        } else {
            return submit_to_system2(std::move(task));
        }
    }
};
```

### 14.2 daemon（守护层）C++ 编码
Backs模块作为系统服务，强调可靠性和可观测性：

#### 14.2.1 IPC服务守护进程（映射原则：E-3 通信基础设施）
```cpp
/**
 * @brief IPC守护进程 - 体现系统观（S-3）和工程观（E-2, E-4）原则
 * 
 * 实现高性能进程间通信，集成OpenTelemetry可观测性。
 * 遵循守护进程设计模式，支持优雅启停。
 * 
 * @see ipc.md 中的通信协议规范
 */
class IpcDaemon : public DaemonBase {
public:
    /**
     * @brief 处理IPC消息 - 体现防御深度（D-3）原则
     * 
     * 多层安全验证：
     * 1. 消息格式验证
     * 2. 发送者身份验证
     * 3. 权限检查
     * 4. 资源配额检查
     * 
     * @param message IPC消息，使用const引用避免拷贝
     * @return 处理结果，使用std::expected表达成功/失败
     */
    std::expected<ProcessResult, ErrorCode> 
    process_message(const IpcMessage& message) {
        // 层次1：消息格式验证
        if (!validate_message_format(message)) {
            AGENTOS_LOG_ERROR("Invalid IPC message format");
            return std::unexpected(ErrorCode::kInvalidFormat);
        }
        
        // 层次2：身份验证
        if (!authenticate_sender(message.sender())) {
            log_audit("Unauthorized IPC access attempt: sender=%s", 
                      message.sender().c_str());
            return std::unexpected(ErrorCode::kUnauthorized);
        }
        
        // 层次3：权限检查
        if (!check_permission(message.sender(), message.operation())) {
            return std::unexpected(ErrorCode::kPermissionDenied);
        }
        
        // 层次4：资源检查
        if (!check_resource_quota(message.sender())) {
            return std::unexpected(ErrorCode::kResourceExhausted);
        }
        
        // 实际处理
        return do_process_message(message);
    }
    
private:
    // 使用智能指针管理资源
    std::unique_ptr<IpcChannel> channel_;
    std::shared_ptr<MetricsCollector> metrics_;
    std::vector<std::thread> worker_threads_;
};
```

### 14.3 cupolas（安全穹顶）C++ 编码
cupolas模块实现零信任安全模型，要求形式化验证支持：

#### 14.3.1 安全能力管理器（映射原则：D-1 最小权限）
```cpp
/**
 * @brief 能力（Capability）管理器 - 体现安全工程（D-1至D-4）原则
 * 
 * 基于能力的访问控制模型，每个操作必须显式授权。
 * 支持能力传递、撤销和审计追踪。
 * 
 * @formal 关键安全属性已通过形式化验证
 * @see Security_design_guide.md 中的零信任架构
 */
class CapabilityManager {
public:
    /**
     * @brief 创建能力 - 体现最小权限（D-1）原则
     * 
     * 创建仅包含必要权限的能力对象。
     * 能力创建过程被审计日志记录。
     * 
     * @param subject 主体标识
     * @param resource 资源标识
     * @param permissions 权限位图
     * @return 能力句柄，失败返回std::nullopt
     */
    std::optional<CapabilityHandle> create_capability(
        const SubjectId& subject,
        const ResourceId& resource,
        PermissionSet permissions) {
        
        // 权限最小化：移除不必要的权限
        permissions = minimize_permissions(subject, resource, permissions);
        
        // 创建能力
        auto capability = Capability::create(subject, resource, permissions);
        if (!capability) {
            return std::nullopt;
        }
        
        // 审计日志（D-4原则）
        log_audit("Capability created: subject=%s, resource=%s, permissions=%x",
                  subject.c_str(), resource.c_str(), permissions.bits());
        
        return capability->handle();
    }
    
private:
    // 使用不可变数据结构保证线程安全
    const CapabilityStore store_;
    std::shared_mutex store_mutex_;
};
```

### 14.4 bases（公共库层）C++ 编码
Common模块提供跨层基础设施，强调通用性和性能：

#### 14.4.1 向量数据库客户端（映射原则：E-1 基础设施）
```cpp
/**
 * @brief 向量数据库客户端 - 体现工程观（E-1, E-3）和认知观（C-3）原则
 * 
 * 封装FAISS和HNSW向量索引，支持持久化存储和拓扑分析。
 * 集成HDBSCAN聚类算法，自动发现数据中的拓扑结构。
 * 
 * @see memoryrovol.md 中的记忆进化算法
 */
class VectorDbClient {
public:
    /**
     * @brief 相似性搜索 - 体现性能优化和资源确定性
     * 
     * 支持近似最近邻搜索，平衡精度和性能。
     * 实现缓存友好数据布局，优化CPU缓存利用率。
     * 
     * @param query 查询向量
     * @param k 返回结果数量
     * @param options 搜索选项
     * @return 搜索结果，使用移动语义返回
     */
    std::vector<SearchResult> search(
        const Vector& query, 
        size_t k, 
        SearchOptions options = {}) {
        
        // 预分配结果向量，避免多次分配
        std::vector<SearchResult> results;
        results.reserve(k);
        
        // 缓存友好访问模式
        auto indices = index_->search(query, k, options);
        
        // 批量获取元数据，减少随机访问
        auto metadata = batch_get_metadata(indices);
        
        // 构造结果
        for (size_t i = 0; i < indices.size(); ++i) {
            results.emplace_back(
                indices[i],
                metadata[i],
                compute_similarity(query, metadata[i].vector)
            );
        }
        
        return results;  // 移动语义返回，避免拷贝
    }
    
private:
    // 使用PImpl模式隐藏实现细节
    class Impl;
    std::unique_ptr<Impl> impl_;
};
```

---
## 十五、参考文献

1. **AgentOS 架构设计原则**: [architectural_design_principles.md](../../architecture/folder/architectural_design_principles.md)
2. **C++ Core Guidelines**: https://isocpp.github.io/CppCoreGuidelines/
3. **Google C++ Style Guide**: https://google.github.io/styleguide/cppguide.html
4. **ISO C++ Standard**: https://eel.is/c++draft/
5. **AgentOS 核心架构文档**:
   - [coreloopthree.md](../../architecture/folder/coreloopthree.md)
   - [memoryrovol.md](../../architecture/folder/memoryrovol.md)  
   - [microkernel.md](../../architecture/folder/microkernel.md)
   - [ipc.md](../../architecture/folder/ipc.md)
   - [syscall.md](../../architecture/folder/syscall.md)
   - [logging_system.md](../../architecture/folder/logging_system.md)

---

© 2026 SPHARX Ltd. All Rights Reserved.  
"From data intelligence emerges."