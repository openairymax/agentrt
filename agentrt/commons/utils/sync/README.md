# Sync — 同步原语模块

**模块路径**: `agentrt/commons/utils/sync/`
**版本**: v0.1.0

## 概述

Sync 模块提供跨平台、安全、高效的线程同步原语，是 AgentRT 并发编程的基础设施。该模块封装了互斥锁（Mutex）、递归互斥锁（Recursive Mutex）、读写锁（RWLock）、自旋锁（Spinlock）、信号量（Semaphore）、条件变量（Condition）、屏障（Barrier）和事件（Event）等完整的同步原语，支持 Windows 和 POSIX 系统，并提供了死锁检测、超时控制、统计信息和 POSIX 兼容层等功能。

## 设计目标

- **跨平台统一**：统一 API 屏蔽 Windows 和 POSIX 的线程同步差异
- **安全可靠**：内置死锁检测、超时控制、错误回调机制，确保资源确定性
- **可观测性**：提供锁统计信息，包括加锁/解锁次数、等待时间、死锁检测次数等
- **向后兼容**：提供 POSIX 风格兼容层，便于现有代码逐步迁移

## 目录结构

```
sync/
├── include/
│   ├── sync.h              # 核心层同步原语 API（不透明句柄、统计、死锁检测）
│   ├── sync_common.h       # 公共层同步原语 API（简化接口）
│   └── sync_compat.h       # POSIX 兼容层（宏和 inline 包装函数）
├── src/
│   ├── sync.c              # 核心同步模块初始化与清理
│   ├── sync_mutex.c        # 互斥锁实现
│   ├── sync_recursive_mutex.c  # 递归互斥锁实现
│   ├── sync_rwlock.c       # 读写锁实现
│   ├── sync_spinlock.c     # 自旋锁实现
│   ├── sync_semaphore.c    # 信号量实现
│   ├── sync_condition.c    # 条件变量实现
│   ├── sync_barrier.c      # 屏障实现
│   ├── sync_event.c        # 事件实现
│   ├── sync_platform.c     # 平台适配层
│   ├── sync_common.c       # 公共层同步原语实现
│   ├── sync_internal.c     # 内部辅助函数
│   ├── sync_internal.h     # 内部头文件
│   ├── sync_types.h        # 内部类型定义
│   └── sync_platform.h     # 平台适配头文件
├── test/
│   └── test_sync.c         # 单元测试
└── README.md               # 本文档
```

## 核心数据结构

### sync_type_t — 同步原语类型

| 枚举值 | 说明 |
|------|------|
| `SYNC_TYPE_MUTEX` | 互斥锁 |
| `SYNC_TYPE_RECURSIVE_MUTEX` | 递归互斥锁 |
| `SYNC_TYPE_RWLOCK` | 读写锁 |
| `SYNC_TYPE_SPINLOCK` | 自旋锁 |
| `SYNC_TYPE_SEMAPHORE` | 信号量 |
| `SYNC_TYPE_CONDITION` | 条件变量 |
| `SYNC_TYPE_BARRIER` | 屏障 |
| `SYNC_TYPE_EVENT` | 事件 |

### sync_result_t — 操作结果

| 枚举值 | 说明 |
|------|------|
| `SYNC_SUCCESS` | 操作成功 |
| `SYNC_ERROR_TIMEOUT` | 操作超时 |
| `SYNC_ERROR_DEADLOCK` | 检测到死锁 |
| `SYNC_ERROR_INVALID` | 无效参数或状态 |
| `SYNC_ERROR_MEMORY` | 内存分配失败 |
| `SYNC_ERROR_PERMISSION` | 权限不足 |
| `SYNC_ERROR_BUSY` | 资源繁忙 |
| `SYNC_ERROR_UNSUPPORTED` | 不支持的操作 |
| `SYNC_ERROR_UNKNOWN` | 未知错误 |

### sync_flag_t — 锁选项标志

| 枚举值 | 说明 |
|------|------|
| `SYNC_FLAG_SHARED` | 共享锁（读写锁） |
| `SYNC_FLAG_EXCLUSIVE` | 排他锁（读写锁） |
| `SYNC_FLAG_TRY` | 尝试获取，不阻塞 |
| `SYNC_FLAG_TIMEOUT` | 支持超时 |
| `SYNC_FLAG_RECURSIVE` | 递归锁 |
| `SYNC_FLAG_ERROR_CHECK` | 错误检查锁 |
| `SYNC_FLAG_PRIORITY_INHERIT` | 优先级继承 |
| `SYNC_FLAG_ROBUST` | 健壮锁（进程间） |

### sync_timeout_t — 超时选项

| 字段 | 类型 | 说明 |
|------|------|------|
| `timeout_ms` | `uint64_t` | 超时时间（毫秒） |
| `absolute` | `bool` | 是否为绝对时间 |

### sync_attr_t — 锁属性

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `sync_type_t` | 锁类型 |
| `flags` | `uint32_t` | 标志位 |
| `name` | `const char *` | 锁名称（用于调试） |
| `context` | `void *` | 用户上下文 |

### sync_stats_t — 锁统计信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `lock_count` | `size_t` | 加锁次数 |
| `unlock_count` | `size_t` | 解锁次数 |
| `wait_count` | `size_t` | 等待次数 |
| `timeout_count` | `size_t` | 超时次数 |
| `deadlock_count` | `size_t` | 死锁检测次数 |
| `total_wait_time_ms` | `uint64_t` | 总等待时间（毫秒） |
| `max_wait_time_ms` | `uint64_t` | 最大等待时间（毫秒） |

### sync_deadlock_info_t — 死锁检测信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `thread_count` | `size_t` | 涉及线程数 |
| `lock_count` | `size_t` | 涉及锁数量 |
| `detection_time` | `uint64_t` | 检测时间戳 |
| `thread_names` | `char **` | 线程名称数组 |
| `lock_names` | `char **` | 锁名称数组 |

## 接口说明

### 模块生命周期

| 函数 | 说明 |
|------|------|
| `sync_init(error_callback, context)` | 初始化同步模块，注册错误回调 |
| `sync_cleanup()` | 清理同步模块 |

### 互斥锁（Mutex）

| 函数 | 说明 |
|------|------|
| `sync_mutex_create(mutex, attr)` | 创建互斥锁 |
| `sync_mutex_free(mutex)` | 销毁互斥锁 |
| `sync_mutex_lock_ex(mutex, timeout)` | 加锁（支持超时），返回 `sync_result_t` |
| `sync_mutex_try_lock(mutex)` | 尝试加锁，不阻塞 |
| `sync_mutex_unlock_ex(mutex)` | 解锁 |

### 递归互斥锁（Recursive Mutex）

| 函数 | 说明 |
|------|------|
| `sync_recursive_mutex_create(mutex, attr)` | 创建递归互斥锁 |
| `sync_recursive_mutex_free(mutex)` | 销毁递归互斥锁 |
| `sync_recursive_mutex_lock_ex(mutex, timeout)` | 加锁（支持超时和递归） |
| `sync_recursive_mutex_unlock_ex(mutex)` | 解锁 |
| `sync_recursive_mutex_get_count(mutex, count)` | 获取递归计数 |

### 读写锁（RWLock）

| 函数 | 说明 |
|------|------|
| `sync_rwlock_create(rwlock, attr)` | 创建读写锁 |
| `sync_rwlock_free(rwlock)` | 销毁读写锁 |
| `sync_rwlock_read_lock_ex(rwlock, timeout)` | 获取读锁（共享锁） |
| `sync_rwlock_try_read_lock(rwlock)` | 尝试获取读锁 |
| `sync_rwlock_write_lock_ex(rwlock, timeout)` | 获取写锁（排他锁） |
| `sync_rwlock_try_write_lock(rwlock)` | 尝试获取写锁 |
| `sync_rwlock_unlock_ex(rwlock)` | 解锁读写锁 |

### 自旋锁（Spinlock）

| 函数 | 说明 |
|------|------|
| `sync_spinlock_create(spinlock, attr)` | 创建自旋锁 |
| `sync_spinlock_free(spinlock)` | 销毁自旋锁 |
| `sync_spinlock_lock_ex(spinlock)` | 加锁自旋锁 |
| `sync_spinlock_try_lock(spinlock)` | 尝试加锁自旋锁 |
| `sync_spinlock_unlock_ex(spinlock)` | 解锁自旋锁 |

### 信号量（Semaphore）

| 函数 | 说明 |
|------|------|
| `sync_semaphore_create(semaphore, init_val, max_val, attr)` | 创建信号量 |
| `sync_semaphore_free(semaphore)` | 销毁信号量 |
| `sync_semaphore_wait_ex(semaphore, timeout)` | 等待信号量（P 操作） |
| `sync_semaphore_try_wait(semaphore)` | 尝试等待信号量 |
| `sync_semaphore_post_ex(semaphore)` | 发布信号量（V 操作） |
| `sync_semaphore_get_value(semaphore, value)` | 获取信号量当前值 |

### 条件变量（Condition）

| 函数 | 说明 |
|------|------|
| `sync_condition_create(condition, attr)` | 创建条件变量 |
| `sync_condition_free(condition)` | 销毁条件变量 |
| `sync_condition_wait_ex(condition, mutex, timeout)` | 等待条件变量（原子释放 mutex 并阻塞） |
| `sync_condition_signal_ex(condition)` | 唤醒一个等待线程 |
| `sync_condition_broadcast_ex(condition)` | 唤醒所有等待线程 |

### 屏障（Barrier）

| 函数 | 说明 |
|------|------|
| `sync_barrier_create(barrier, count, attr)` | 创建屏障 |
| `sync_barrier_free(barrier)` | 销毁屏障 |
| `sync_barrier_wait_ex(barrier, timeout)` | 等待屏障（所有线程到达后释放） |
| `sync_barrier_reset(barrier, new_count)` | 重置屏障 |

### 事件（Event）

| 函数 | 说明 |
|------|------|
| `sync_event_create(event, manual_reset, init_state, attr)` | 创建事件 |
| `sync_event_free(event)` | 销毁事件 |
| `sync_event_set_ex(event)` | 设置事件为有信号状态 |
| `sync_event_reset(event)` | 重置事件为无信号状态 |
| `sync_event_wait_ex(event, timeout)` | 等待事件 |

### 统计与诊断

| 函数 | 说明 |
|------|------|
| `sync_get_stats(lock, stats)` | 获取锁统计信息 |
| `sync_reset_stats(lock)` | 重置锁统计信息 |
| `sync_check_deadlock(info, max_info_size)` | 检查死锁 |
| `sync_set_name(lock, name)` | 设置锁名称 |
| `sync_get_name(lock)` | 获取锁名称 |
| `sync_get_type(lock, lock_type)` | 获取锁类型 |
| `sync_get_thread_id()` | 获取当前线程 ID |

### 原子操作

| 函数 | 说明 |
|------|------|
| `sync_atomic_cas(ptr, expected, desired)` | 比较并交换 |
| `sync_atomic_add(ptr, value)` | 原子增加，返回增加前的值 |
| `sync_atomic_sub(ptr, value)` | 原子减少，返回减少前的值 |
| `sync_atomic_load(ptr)` | 原子读取 |
| `sync_atomic_store(ptr, value)` | 原子存储 |

### 工具函数

| 函数 | 说明 |
|------|------|
| `sync_sleep(ms)` | 线程休眠（毫秒） |
| `sync_get_timestamp_ms()` | 获取当前时间戳（毫秒） |
| `sync_set_option(lock, option, value)` | 设置锁选项 |
| `sync_get_option(lock, option, value)` | 获取锁选项 |

### 公共层 API（sync_common.h）

| 函数 | 说明 |
|------|------|
| `sync_mutex_init(mutex)` | 初始化互斥锁 |
| `sync_mutex_destroy(mutex)` | 销毁互斥锁 |
| `sync_mutex_lock(mutex)` | 加锁互斥锁 |
| `sync_mutex_unlock(mutex)` | 解锁互斥锁 |
| `sync_mutex_trylock(mutex)` | 尝试加锁互斥锁 |
| `sync_cond_init(cond)` | 初始化条件变量 |
| `sync_cond_destroy(cond)` | 销毁条件变量 |
| `sync_cond_wait(cond, mutex)` | 等待条件变量 |
| `sync_cond_timedwait(cond, mutex, timeout_ms)` | 带超时的等待条件变量 |
| `sync_cond_signal(cond)` | 唤醒一个等待线程 |
| `sync_cond_broadcast(cond)` | 唤醒所有等待线程 |
| `sync_sem_init(sem, value)` | 初始化信号量 |
| `sync_sem_destroy(sem)` | 销毁信号量 |
| `sync_sem_wait(sem)` | 等待信号量 |
| `sync_sem_timedwait(sem, timeout_ms)` | 带超时的等待信号量 |
| `sync_sem_trywait(sem)` | 尝试等待信号量 |
| `sync_sem_post(sem)` | 释放信号量 |
| `sync_sem_getvalue(sem, value)` | 获取信号量当前值 |
| `sync_rwlock_init(rwlock)` | 初始化读写锁 |
| `sync_rwlock_destroy(rwlock)` | 销毁读写锁 |
| `sync_rwlock_rdlock(rwlock)` | 读加锁 |
| `sync_rwlock_wrlock(rwlock)` | 写加锁 |
| `sync_rwlock_tryrdlock(rwlock)` | 尝试读加锁 |
| `sync_rwlock_trywrlock(rwlock)` | 尝试写加锁 |
| `sync_rwlock_unlock(rwlock)` | 解锁读写锁 |

### POSIX 兼容层（sync_compat.h）

兼容层提供 `AGENTRT_MUTEX_*`、`AGENTRT_COND_*`、`AGENTRT_RWLOCK_*`、`AGENTRT_SEM_*` 系列宏，可直接替换 `pthread_mutex_*`、`pthread_cond_*`、`sem_*` 等标准 API。

## 使用示例

```c
#include "sync.h"

/* === 模块初始化 === */
sync_init(NULL, NULL);

/* === 互斥锁 === */
sync_mutex_t mutex;
sync_mutex_create(&mutex, NULL);

sync_mutex_lock_ex(mutex, NULL);  // 无限等待
// ... 临界区代码 ...
sync_mutex_unlock_ex(mutex);

sync_mutex_free(mutex);

/* === 读写锁 === */
sync_rwlock_t rwlock;
sync_rwlock_create(&rwlock, NULL);

// 多个读线程可同时持有读锁
sync_rwlock_read_lock_ex(rwlock, NULL);
// ... 读取共享数据 ...
sync_rwlock_unlock_ex(rwlock);

// 写锁是排他的
sync_rwlock_write_lock_ex(rwlock, NULL);
// ... 修改共享数据 ...
sync_rwlock_unlock_ex(rwlock);

sync_rwlock_free(rwlock);

/* === 条件变量（生产者-消费者模式） === */
sync_mutex_t cv_mutex;
sync_condition_t cv;
sync_mutex_create(&cv_mutex, NULL);
sync_condition_create(&cv, NULL);

// 消费者线程
sync_mutex_lock_ex(cv_mutex, NULL);
while (!data_ready) {
    sync_condition_wait_ex(cv, cv_mutex, NULL);
}
// ... 消费数据 ...
sync_mutex_unlock_ex(cv_mutex);

// 生产者线程
sync_mutex_lock_ex(cv_mutex, NULL);
// ... 生产数据 ...
data_ready = true;
sync_condition_signal_ex(cv);  // 或 broadcast_ex 唤醒所有
sync_mutex_unlock_ex(cv_mutex);

sync_condition_free(cv);
sync_mutex_free(cv_mutex);

/* === 信号量（资源池控制） === */
sync_semaphore_t sem;
sync_semaphore_create(&sem, 5, 5, NULL);  // 最多 5 个并发访问

sync_semaphore_wait_ex(&sem, NULL);         // 获取资源
// ... 使用资源 ...
sync_semaphore_post_ex(&sem);               // 归还资源

sync_semaphore_free(sem);

/* === 屏障（并行计算同步） === */
sync_barrier_t barrier;
sync_barrier_create(&barrier, 4, NULL);  // 4 个线程参与

// 每个工作线程执行
// ... 并行计算 ...
sync_barrier_wait_ex(barrier, NULL);  // 等待所有线程到达
// ... 所有线程继续执行 ...

sync_barrier_free(barrier);

/* === 超时控制 === */
sync_timeout_t timeout = { .timeout_ms = 5000, .absolute = false };
sync_result_t result = sync_mutex_lock_ex(mutex, &timeout);
if (result == SYNC_ERROR_TIMEOUT) {
    printf("Failed to acquire lock within 5 seconds\n");
}

/* === 锁统计 === */
sync_stats_t stats;
sync_get_stats(mutex, &stats);
printf("Lock count: %zu, Max wait: %lu ms\n",
       stats.lock_count, stats.max_wait_time_ms);

/* === 死锁检测 === */
sync_deadlock_info_t info;
if (sync_check_deadlock(&info, sizeof(info)) == SYNC_ERROR_DEADLOCK) {
    printf("Deadlock detected: %zu threads, %zu locks\n",
           info.thread_count, info.lock_count);
}

/* === 模块清理 === */
sync_cleanup();
```

## 死锁检测机制

| 特性 | 说明 |
|------|------|
| 检测算法 | 基于资源分配图（Resource Allocation Graph）的循环等待检测 |
| 检测时机 | 主动调用 `sync_check_deadlock()` 时执行 |
| 输出信息 | 涉及线程名称、锁名称、检测时间戳 |
| 处理策略 | 检测到死锁后返回 `SYNC_ERROR_DEADLOCK`，由调用者决定处理方式 |

## 平台差异

| 特性 | Linux | Windows | macOS |
|------|-------|---------|-------|
| 互斥锁 | pthread_mutex | SRWLock / CRITICAL_SECTION | pthread_mutex |
| 条件变量 | pthread_cond | CONDITION_VARIABLE | pthread_cond |
| 信号量 | sem_t | Semaphore | sem_t |
| 读写锁 | pthread_rwlock | SRWLock | pthread_rwlock |
| 自旋锁 | pthread_spinlock | 原子操作 + Yield | pthread_spinlock |
| 屏障 | pthread_barrier | 自定义实现 | pthread_barrier |
| 事件 | 自定义（条件变量 + 标志） | Event | 自定义（条件变量 + 标志） |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `stdbool.h` | 布尔类型支持 |
| `stddef.h` | `size_t` 等类型 |
| `stdint.h` | 固定宽度整数类型 |
| `pthread`（POSIX）/ `Windows API`（Windows） | 底层线程 API |

---

© 2026 SPHARX Ltd. All Rights Reserved.