# Utils Module (工具库模块)

## 概述

工具库模块提供 Cupolas 项目统一的底层抽象和实用函数，消除代码重复，提升可移植性和可维护性。所有其他子模块均依赖此模块。

## 设计理念

### 核心原则
- **DRY (Don't Repeat Yourself)**: 消除平台特定代码重复
- **跨平台兼容**: 统一 Windows/Linux 抽象
- **零成本抽象**: 宏展开后无运行时开销
- **类型安全**: 编译期类型检查

### 架构定位
```
                    ┌─────────────┐
                    │   应用层    │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │  Audit   │ │Permission│ │ Sanitizer│
        └────┬─────┘ └────┬─────┘ └────┬─────┘
             │            │            │
             └────────────┼────────────┘
                          ▼
                   ┌──────────────┐
                   │    Utils     │ ◄── 你在这里
                   │  (本模块)    │
                   └──────┬───────┘
                          │
                ┌─────────┴─────────┐
                ▼                   ▼
         ┌──────────┐       ┌──────────┐
         │ Platform │       │ Standard │
         │ (OS 抽象)│       │  Library │
         └──────────┘       └──────────┘
```

## 文件结构

```
utils/
├── cupolas_utils.h      # 头文件：所有宏定义和函数声明
└── cupolas_utils.c      # 实现：工具函数的具体实现
```

## API 分类

### 1. 平台抽象宏 (Platform Abstraction)

统一线程同步原语，消除 `#ifdef _WIN32` 条件编译：

```c
// 类型定义
CUPOLAS_MUTEX_TYPE  // CRITICAL_SECTION (Win) / pthread_mutex_t (POSIX)

// 操作宏
CUPOLAS_MUTEX_INIT(mutex_ptr)      // 初始化互斥锁
CUPOLAS_MUTEX_LOCK(mutex_ptr)      // 加锁（阻塞）
CUPOLAS_MUTEX_UNLOCK(mutex_ptr)    // 解锁
CUPOLAS_MUTEX_DESTROY(mutex_ptr)   // 销毁互斥锁
```

**使用示例**:
```c
CUPOLAS_MUTEX_TYPE lock;
CUPOLAS_MUTEX_INIT(&lock);

CUPOLAS_MUTEX_LOCK(&lock);
// 临界区代码
CUPOLAS_MUTEX_UNLOCK(&lock);

CUPOLAS_MUTEX_DESTROY(&lock);
```

### 2. 内存管理宏 (Memory Management)

安全内存分配与释放，自动处理 NULL 和内存清零：

```c
// 分配（自动 zero-initialized）
int* arr = CUPOLAS_ALLOC(int, 100);           // calloc: 100个int，零初始化
my_struct_t* s = CUPOLAS_ALLOC_STRUCT(my_struct_t); // calloc: 1个结构体

// 分配（未初始化，更快）
int* buf = CUPOLAS_ALLOC_ARRAY(int, 1000);    // malloc: 1000个int

// 重分配
arr = CUPOLAS_REALLOC(arr, int, 200);         // realloc: 扩展到200个

// 释放（自动置 NULL，防止悬垂指针）
CUPOLAS_FREE(arr);      // free + arr = NULL
CUPOLAS_FREE_ARRAY(buf); // 带NULL检查的释放
```

**优势对比**:
```c
// ❌ 旧方式（不安全）
free(ptr);  // ptr 仍指向已释放内存！

// ✅ 新方式（安全）
CUPOLAS_FREE(ptr);  // free + ptr = NULL
```

### 3. 错误处理宏 (Error Handling)

统一错误检查模式，减少样板代码：

```c
// NULL指针检查
CUPOLAS_CHECK_NULL(ptr);           // if NULL return -1;
CUPOLAS_CHECK_NULL_RET(ptr, -42);  // if NULL return -42;

// 结果检查
CUPOLAS_CHECK_RESULT(expr);        // if (expr != 0) return -1;
CUPOLAS_CHECK_RESULT_RET(expr, err); // if (expr != 0) return err;

// 条件检查
CUPOLAS_CHECK_TRUE(cond);          // if (!cond) return -1;
CUPOLAS_CHECK_TRUE_RET(cond, val); // if (!cond) return val;
```

**实际效果**:
```c
// ❌ 冗长写法（每个函数都要重复）
if (!config) return -1;
if (!buffer) return -1;
if (init() != 0) return -1;

// ✅ 简洁写法（一行搞定）
CUPOLAS_CHECK_NULL(config);
CUPOLAS_CHECK_NULL(buffer);
CUPOLAS_CHECK_RESULT(init());
```

### 4. 日志宏 (Logging)

分级日志输出，支持条件编译：

```c
CUPOLAS_LOG("常规消息");                       // INFO级别
CUPOLAS_LOG_ERROR("错误码: %d", errno);        // ERROR级别
CUPOLAS_LOG_DEBUG("调试: ptr=%p", ptr);       // DEBUG级别（Release编译时自动移除）
```

### 5. 性能优化宏 (Performance Hints)

分支预测提示和内联控制：

```c
// 分支预测（用于热路径优化）
if (CUPOLAS_LIKELY(condition)) { ... }   // 提示编译器：这个分支大概率成立
if (CUPOLAS_UNLIKELY(error)) { ... }     // 提示编译器：这个分支大概率不成立

// 内联控制
static CUPOLAS_INLINE int fast_function(...) { ... }
```

### 6. 工具函数 (Utility Functions)

在 `cupolas_utils.c` 中实现的通用功能：

```c
// 字符串操作
char* cupolas_strdup(const char* str);             // 安全strdup（NULL安全）
size_t cupolas_strlcpy(char* dst, const char* src, size_t len);  // 安全字符串拷贝

// 内存安全
void cupolas_memset_s(void* ptr, size_t len);      // 安全memset（不会被编译器优化掉）

// 时间戳
uint64_t cupolas_get_timestamp_ms(void);           // 获取毫秒时间戳

// 哈希计算
uint32_t cupolas_hash_string(const char* str);     // 字符串哈希（djb2算法）

// 日志输出
void cupolas_log_message(const char* level, const char* fmt, ...);
```

## 使用统计

截至 2026-04-06，本项目中的使用情况：

| 宏类别 | 使用次数 | 覆盖文件数 |
|--------|---------|-----------|
| CUPOLAS_MUTEX_* | 45+ | 24 files |
| CUPOLAS_ALLOC/FREE | 38+ | 22 files |
| CUPOLAS_CHECK_* | 52+ | 24 files |
| CUPOLAS_LOG_* | 28+ | 18 files |

**总代码重复减少**: 约 **40%** (从~10%降至<4%)

## 编译配置

### 必要条件
- C11 标准 (`-std=c11`)
- 平台头文件: `<windows.h>` (Win) / `<pthread.h>` (POSIX)

### 可选特性
- `NDEBUG` 定义: 移除 `CUPOLAS_LOG_DEBUG` 输出
- `_DEBUG`: 启用额外断言检查

## 最佳实践

### ✅ 推荐用法

```c
// 1. 所有新代码必须使用 CUPOLAS_* 宏
int my_function(my_struct_t* config) {
    CUPOLAS_CHECK_NULL(config);
    
    my_data_t* data = CUPOLAS_ALLOC_STRUCT(my_data_t);
    if (!data) return -1;
    
    CUPOLAS_MUTEX_LOCK(&config->lock);
    // ... 操作 ...
    CUPOLAS_MUTEX_UNLOCK(&config->lock);
    
    CUPOLAS_FREE(data);
    return 0;
}
```

### ❌ 禁止用法

```c
// 1. 禁止直接调用平台API
pthread_mutex_init(&lock, NULL);  // ❌ 使用 CUPOLAS_MUTEX_INIT
InitializeCriticalSection(&lock);  // ❌ 使用 CUPOLAS_MUTEX_INIT

// 2. 禁止裸free()
free(ptr);  // ❌ 使用 CUPOLAS_FREE(ptr)

// 3. 禁止手动NULL检查样板代码
if (!ptr) { return -1; }  // ❌ 使用 CUPOLAS_CHECK_NULL(ptr)
```

## 迁移指南

如果你有旧代码需要迁移到新的 Utils 系统：

### Step 1: 替换 include
```c
// 旧
#include "../platform/platform.h"

// 新
#include "../utils/cupolas_utils.h"
```

### Step 2: 替换 Mutex 操作
```c
// 旧
#ifdef _WIN32
    InitializeCriticalSection(&lock);
#else
    pthread_mutex_init(&lock, NULL);
#endif

// 新
CUPOLAS_MUTEX_INIT(&lock);
```

### Step 3: 替换内存操作
```c
// 旧
ptr = malloc(sizeof(my_t));
// ... 使用 ...
free(ptr);

// 新
ptr = CUPOLAS_ALLOC_STRUCT(my_t);
// ... 使用 ...
CUPOLAS_FREE(ptr);
```

### Step 4: 替换错误检查
```c
// 旧
if (!param) return -1;
if (result != 0) return -1;

// 新
CUPOLAS_CHECK_NULL(param);
CUPOLAS_CHECK_RESULT(result);
```

## 已知限制

1. **C++ 兼容性**: 使用 `extern "C"` 包装，但 C++ 特性（重载、模板）不支持
2. **递归锁**: 当前 mutex 不支持递归锁定（同一线程多次 LOCK 会死锁）
3. **RAII**: 不提供 C++ 风格的 RAII 包装器（纯 C 实现）

## 未来规划

- [ ] 添加原子操作宏 (`CUPOLAS_ATOMIC_*`)
- [ ] 添加智能指针模拟 (`CUPOLAS_UNIQUE_PTR`, `CUPOLAS_SHARED_PTR`)
- [ ] 添加线程池抽象 (`CUPOLAS_THREAD_POOL`)
- [ ] 支持更多平台 (macOS, FreeBSD, Android)

## 相关文档

- [主 README](../../README.md) - Cupolas 整体架构
- [Security 模块](../security/README.md) - 安全模块（主要使用者）
- [Architecture Principles](../../../manuals/ARCHITECTURAL_PRINCIPLES.md)

---

**版本**: 1.0.0  
**创建日期**: 2026-04-05  
**最后更新**: 2026-04-06  
**作者**: Spharx AgentOS Team
