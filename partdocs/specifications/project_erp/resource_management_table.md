# AgentOS 资源管理表

**版本**: Doc V1.5

## 1. 概述

本文档提供了AgentOS中所有需要管理的资源及其释放责任的详细说明。资源管理是确保系统稳定性和防止资源泄漏的关键部分，所有开发者必须严格遵循本文档中的资源管理规范。

## 2. 资源管理原则

- **所有权明确**：每个资源必须有明确的所有权归属
- **释放责任**：资源的创建者或接收者必须负责释放资源
- **生命周期管理**：资源的创建、使用和释放必须有清晰的生命周期管理
- **错误处理**：在资源获取失败时，必须正确处理已分配的资源
- **线程安全**：资源管理操作必须考虑线程安全性

## 3. 资源管理表

### 3.1 核心循环模块 (coreloopthree)

| 资源类型 | 创建函数 | 释放函数 | 所有权转移 | 线程安全 | 备注 |
|---------|---------|---------|-----------|---------|------|
| `agentos_core_loop_t` | `agentos_loop_create()` | `agentos_loop_destroy()` | 是 | 否 | 核心循环实例 |
| `agentos_cognition_engine_t` | `agentos_cognition_create()` | `agentos_cognition_destroy()` | 是 | 否 | 认知引擎实例 |
| `agentos_execution_engine_t` | `agentos_execution_create()` | `agentos_execution_destroy()` | 是 | 否 | 执行引擎实例 |
| `agentos_memory_engine_t` | `agentos_memory_create()` | `agentos_memory_destroy()` | 是 | 否 | 记忆引擎实例 |
| `agentos_compensation_t` | `agentos_compensation_create()` | `agentos_compensation_destroy()` | 是 | 否 | 补偿事务管理器 |
<!-- From data intelligence emerges. by spharx -->
| `char*` (任务ID) | `agentos_loop_submit()` | 调用者释放 | 是 | 是 | 使用 `free()` 释放 |
| `char*` (JSON结果) | `agentos_loop_wait()` | 调用者释放 | 是 | 是 | 使用 `free()` 释放 |
| `char*` (健康状态JSON) | `agentos_cognition_health_check()` | 调用者释放 | 是 | 是 | 使用 `free()` 释放 |
| `char*` (健康状态JSON) | `agentos_execution_health_check()` | 调用者释放 | 是 | 是 | 使用 `free()` 释放 |
| `char*` (健康状态JSON) | `agentos_memory_health_check()` | 调用者释放 | 是 | 是 | 使用 `free()` 释放 |
| `agentos_memory_result_t` | `agentos_memory_query()` | `agentos_memory_result_free()` | 是 | 是 | 记忆查询结果 |
| `agentos_memory_record_t` | `agentos_memory_get()` | `agentos_memory_record_free()` | 是 | 是 | 记忆记录 |
| `char*` (记录ID) | `agentos_memory_write()` | 调用者释放 | 是 | 是 | 使用 `free()` 释放 |
| `char**` (补偿动作) | `agentos_compensation_get_human_queue()` | 调用者释放 | 是 | 否 | 包括数组本身和数组中的每个元素 |

### 3.2 系统调用模块 (syscall)

| 资源类型 | 创建函数 | 释放函数 | 所有权转移 | 线程安全 | 备注 |
|---------|---------|---------|-----------|---------|------|
| `agentos_syscall_table_t` | `agentos_syscall_table_create()` | `agentos_syscall_table_destroy()` | 是 | 否 | 系统调用表 |
| `agentos_syscall_entry_t` | `agentos_syscall_entry_create()` | `agentos_syscall_entry_destroy()` | 是 | 否 | 系统调用条目 |

### 3.3 记忆管理模块 (memoryrovol)

| 资源类型 | 创建函数 | 释放函数 | 所有权转移 | 线程安全 | 备注 |
|---------|---------|---------|-----------|---------|------|
| `agentos_memoryrovol_t` | `agentos_memoryrovol_create()` | `agentos_memoryrovol_destroy()` | 是 | 否 | 记忆卷管理实例 |
| `agentos_memoryrovol_segment_t` | `agentos_memoryrovol_segment_create()` | `agentos_memoryrovol_segment_destroy()` | 是 | 否 | 记忆卷段 |

### 3.4 安全域模块 (domes)

| 资源类型 | 创建函数 | 释放函数 | 所有权转移 | 线程安全 | 备注 |
|---------|---------|---------|-----------|---------|------|
| `agentos_domes_t` | `agentos_domes_create()` | `agentos_domes_destroy()` | 是 | 否 | 安全域实例 |
| `agentos_dome_t` | `agentos_dome_create()` | `agentos_dome_destroy()` | 是 | 否 | 单个安全域 |

### 3.5 动态模块 (dynamic)

| 资源类型 | 创建函数 | 释放函数 | 所有权转移 | 线程安全 | 备注 |
|---------|---------|---------|-----------|---------|------|
| `agentos_dynamic_t` | `agentos_dynamic_create()` | `agentos_dynamic_destroy()` | 是 | 否 | 动态模块实例 |

## 4. 资源管理最佳实践

### 4.1 内存分配与释放

- 始终使用 `malloc()`/`calloc()`/`realloc()` 分配内存
- 始终使用 `free()` 释放内存
- 在分配多个资源时，使用 `goto` 语句或类似机制确保在出错时正确释放所有已分配的资源
- 避免内存泄漏，确保每个 `malloc()` 都有对应的 `free()`

### 4.2 资源生命周期管理

- 资源的创建和释放应该在同一逻辑层级
- 避免跨模块的资源所有权转移，除非有明确的文档说明
- 使用 RAII (Resource Acquisition Is Initialization) 模式管理资源生命周期
- 在资源不再需要时立即释放，避免资源长时间占用

### 4.3 错误处理中的资源管理

- 在错误处理路径中，确保释放所有已分配的资源
- 使用统一的错误处理模式，确保资源释放的一致性
- 记录资源释放失败的情况，便于调试和问题定位

### 4.4 线程安全的资源管理

- 对于共享资源，使用适当的同步机制（如互斥锁、条件变量等）
- 避免在持有锁的情况下进行可能阻塞的操作
- 确保资源的创建和释放操作是线程安全的
- 考虑使用无锁数据结构和原子操作提高性能

## 5. 资源管理示例

### 5.1 正确的资源管理示例

```c
// 创建核心循环
agentos_core_loop_t* loop = NULL;
agentos_error_t err = agentos_loop_create(NULL, &loop);
if (err != AGENTOS_SUCCESS) {
    // 错误处理
    return err;
}

// 使用核心循环
char* task_id = NULL;
err = agentos_loop_submit(loop, "Hello World", 11, &task_id);
if (err != AGENTOS_SUCCESS) {
    // 释放已分配的资源
    agentos_loop_destroy(loop);
    return err;
}

// 等待任务完成
char* result = NULL;
size_t result_len = 0;
err = agentos_loop_wait(loop, task_id, 10000, &result, &result_len);

// 释放任务ID
free(task_id);

if (err != AGENTOS_SUCCESS) {
    // 释放已分配的资源
    agentos_loop_destroy(loop);
    return err;
}

// 使用结果
printf("Task result: %s\n", result);

// 释放结果
free(result);

// 释放核心循环
agentos_loop_destroy(loop);
```

### 5.2 错误处理中的资源管理

```c
// 分配多个资源
void* resource1 = malloc(sizeof(resource1));
if (!resource1) {
    return AGENTOS_ERROR_OUT_OF_MEMORY;
}

void* resource2 = malloc(sizeof(resource2));
if (!resource2) {
    // 释放已分配的资源
    free(resource1);
    return AGENTOS_ERROR_OUT_OF_MEMORY;
}

// 使用资源
// ...

// 释放所有资源
free(resource1);
free(resource2);
```

## 6. 资源管理检查清单

在开发和代码审查过程中，使用以下检查清单确保资源管理的正确性：

- [ ] 每个资源都有明确的创建和释放函数
- [ ] 资源的所有权转移有明确的文档说明
- [ ] 错误处理路径中正确释放所有已分配的资源
- [ ] 资源的使用和释放是线程安全的
- [ ] 避免资源泄漏和双重释放
- [ ] 遵循本文档中的资源管理规范

## 7. 结论

资源管理是AgentOS系统稳定性和可靠性的关键组成部分。所有开发者必须严格遵循本文档中的资源管理规范，确保资源的正确创建、使用和释放。通过统一的资源管理策略，可以减少内存泄漏、资源竞争等问题，提高系统的整体质量和可维护性。