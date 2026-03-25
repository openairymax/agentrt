# AgentOS C++ 编码规范

**版本**: Doc V1.5  
**发布日期**: 2026-03-24  
**适用范围**: AgentOS 所有 C++ 代码模块  
**理论基础**: 工程两论（反馈闭环）、系统工程（层次分解）、双系统认知理论、微内核哲学

---

## 一、概述

### 1.1 编制目的

本规范为 AgentOS 项目中的 C++ 代码提供统一的编码标准。基于项目架构设计原则的四维正交体系，本规范聚焦于工程观维度，为开发者提供可操作的代码实现指南。

### 1.2 理论基础

本规范基于 AgentOS 架构设计原则的四维正交体系，聚焦于**工程观**维度：

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

### 1.3 适用范围

| 模块 | 语言标准 | 编译器要求 |
|------|----------|-----------|
| atoms/corekern | C++17 | GCC 11+ / Clang 14+ |
| backs/ | C++17 | GCC 11+ / Clang 14+ |
| openhub/ | C++17 | GCC 11+ / Clang 14+ |

### 1.4 与 AgentOS 架构的关系

本规范适用于 AgentOS 的以下层次：

| 层次 | 组件 | C++ 代码位置 | 关联原则 |
|------|------|-------------|---------|
| 原子内核 | corekern/ | atoms/corekern/ | K-1, K-2 |
| 认知层 | coreloopthree/ | atoms/cognition/ | C-1, C-2 |
| 记忆层 | memoryrovol/ | atoms/memory/ | C-3, C-4 |
| 安全层 | domes/ | atoms/domes/ | E-1, S-2 |
| 系统调用 | syscall/ | atoms/syscall/ | K-2, K-3 |
| 守护进程 | backs/ | backs/*_d/ | S-3, K-4 |

**层次纪律**（原则 S-2）:
- `atoms/` 内部模块只能通过 `corekern/` 的 IPC 机制通信
- `backs/` 守护进程只能通过 `syscalls.h` 与内核交互
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
int module_init(const ModuleConfig* config);
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
  - `domes_`: 安全层

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
    struct Config {
        std::string name;
        uint32_t max_workers;
        uint32_t default_timeout_ms;
        bool enable_retry;
    };
    
    /**
     * @brief 构造调度器
     * @param config 调度器配置
     * @throws std::invalid_argument 当配置无效时
     */
    explicit Scheduler(const Config& config);
    
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
std::unique_ptr<Config> config = std::make_unique<Config>();
std::unique_ptr<Task[]> tasks = std::make_unique<Task[]>(count);

// 共享所有权：使用 shared_ptr
std::shared_ptr<Scheduler> scheduler = std::make_shared<Scheduler>(config);

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
 * @date 2026-03-24
 * @version 1.5
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
auto config = find_config("log_level");
if (config.has_value()) {
    log_info("Log level: %s", config->c_str());
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
    
    MemoryPool::Config config_;
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

## 十四、参考文献

1. **AgentOS 架构设计原则**: [architectural_design_principles.md](../../architecture/folder/architectural_design_principles.md)
2. **C++ Core Guidelines**: https://isocpp.github.io/CppCoreGuidelines/
3. **Google C++ Style Guide**: https://google.github.io/styleguide/cppguide.html
4. **ISO C++ Standard**: https://eel.is/c++draft/

---

© 2026 SPHARX Ltd. All Rights Reserved.  
"From data intelligence emerges."