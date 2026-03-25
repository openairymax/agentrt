# AgentOS 代码注释规范

**版本**: Doc V1.6  
**更新日期**: 2026-03-25  
**适用范围**: AgentOS 所有编程语言的代码注释  
**理论基础**: 工程控制论（反馈闭环）、系统工程（模块化）、五维正交系统（系统观、内核观、认知观、工程观、设计美学）、双系统认知理论  
**原则映射**: A-1至A-4（设计美学）、C-1至C-4（认知工程）、E-1至E-4（工程基础设施）

---

## 一、概述

### 1.1 编制目的

代码注释是软件工程中不可或缺的组成部分，是代码可读性、可维护性和协作效率的关键保障。本文档为 AgentOS 项目提供统一的代码注释规范，旨在：

1. **统一注释风格**：确保整个项目注释格式一致，降低认知负担
2. **提升代码质量**：通过规范化的注释促进开发者深入思考代码设计
3. **加速团队协作**：清晰的注释使代码意图一目了然，减少沟通成本
4. **支持自动化工具**：规范的注释可被 Doxygen、Sphinx、TypeDoc 等工具解析生成文档

### 1.2 核心理念与架构映射

AgentOS 的代码注释规范建立在"工程两论"（工程控制论与系统工程）和五维正交系统的基础之上，体现了以下核心设计理念：

#### 1.2.1 基于工程控制论的注释理念
- **反馈闭环设计**：注释构成代码的"自解释系统"，为开发者提供实时的设计意图反馈，形成"编写-注释-理解"的认知闭环
- **稳定性保障**：公共 API 注释作为系统接口的"控制契约"，确保模块间交互的稳定性和可预测性（映射原则：S-3接口稳定性）

#### 1.2.2 基于系统工程的注释理念  
- **分层抽象**：注释体系遵循五维正交系统，从系统观（架构意图）到工程观（实现细节）提供不同层次的解释
- **模块化文档**：注释与代码结构严格对应，支持"按需加载"的文档认知模式（映射原则：S-2模块化设计）

#### 1.2.3 基于双系统认知理论的注释理念
- **System 1 注释**：简单接口的注释应直观易懂，支持快速认知（快速路径）
- **System 2 注释**：复杂算法的注释需提供深度原理说明，支持深度思考（慢速路径）

#### 1.2.4 基于设计美学的注释理念
- **极简主义**（A-1原则）：注释应简洁精炼，避免冗余信息污染代码空间
- **细节关注**（A-2原则）：关键设计决策必须在注释中明确记录，传承工程智慧
- **人文关怀**（A-3原则）：注释应设身处地为读者着想，提供必要的上下文和背景
- **完美主义**（A-4原则）：注释质量应与代码质量同等要求，追求文档的完美无瑕

#### 1.2.5 注释的多重角色
- **注释即认知桥梁**：连接代码实现与设计意图，降低认知负荷
- **注释即知识传承**：记录技术决策背后的思考过程，避免知识丢失
- **注释即质量指标**：注释完整度直接反映代码的工程成熟度
- **注释即协作协议**：为团队协作提供统一的沟通语言和标准

### 1.3 适用范围

| 语言 | 注释风格 | 文档生成工具 |
|------|----------|--------------|
| C/C++ | Doxygen | Doxygen |
| Python | Google/NumPy Docstring | Sphinx |
| JavaScript/TypeScript | JSDoc/TSDoc | TypeDoc |
| Go | GoDoc | go doc |
| Rust | Rustdoc | cargo doc |

---

## 二、通用原则

### 2.1 注释的必要性判断

**必须注释的场景**：
- 公共 API 接口（函数、类、模块）
- 复杂算法或非直观逻辑
- 性能优化相关代码
- 安全敏感代码
- 平台特定代码
- 临时解决方案（TODO/FIXME）
- 已废弃接口（DEPRECATED）

**无需注释的场景**：
- 简单的 getter/setter
- 标准库调用的包装
- 代码本身已足够清晰

### 2.2 注释内容要素

完整的函数/方法注释应包含以下要素：

| 要素 | 必要性 | 说明 |
|------|--------|------|
| 简要描述 | 必须 | 一句话说明功能 |
| 详细描述 | 推荐 | 解释设计意图、算法原理 |
| 参数说明 | 必须 | 每个参数的含义、约束、单位 |
| 返回值说明 | 必须 | 返回值含义、成功/失败条件 |
| 异常说明 | 必须 | 可能抛出的异常及触发条件 |
| 线程安全 | 推荐 | 是否线程安全、并发使用注意事项 |
| 使用示例 | 推荐 | 典型用法代码示例 |
| 注意事项 | 可选 | 边界条件、性能考量、资源管理 |
| 相关接口 | 可选 | 相关函数、参考文档链接 |
| 作者/日期 | 可选 | 创建者、最后修改者、日期 |

### 2.3 注释语言规范

- **中文项目**：注释使用中文，技术术语保留英文
- **开源项目**：公共 API 注释使用英文，内部注释可使用中文
- **术语一致性**：使用项目统一术语表中的术语
- **避免冗余**：不重复代码已表达的信息

---

## 三、C/C++ 注释规范（Doxygen 风格）

### 3.1 文件头注释

```c
/**
 * @file memory_manager.h
 * @brief AgentOS 内存管理器接口
 * 
 * 提供内核级内存分配、释放、监控功能。采用 slab 分配器
 * 优化小对象分配性能，支持 NUMA 感知内存布局。
 * 
 * @author AgentOS Team
 * @date 2026-03-25
 * @version 1.6
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see atoms/corekern/memory/
 */

#ifndef AGENTOS_MEMORY_MANAGER_H
#define AGENTOS_MEMORY_MANAGER_H

#include "agentos_types.h"

#ifdef __cplusplus
extern "C" {
#endif
```

### 3.2 函数注释模板

```c
/**
 * @brief 分配指定大小的内存块
 * 
 * 从内核内存池中分配一块连续内存。分配器会根据请求大小
 * 自动选择最优的分配策略：
 * - 小于 256 字节：使用 slab 分配器
 * - 256 字节 ~ 4KB：使用 buddy 分配器
 * - 大于 4KB：直接向操作系统申请
 * 
 * @param size 请求分配的字节数，必须大于 0
 * @param flags 分配标志位，见 agentos_mem_flags_t
 * @return 成功返回内存块指针，失败返回 NULL
 * 
 * @note 调用者负责释放内存（调用 agentos_mem_free）
 * @note 返回的内存块已按 16 字节对齐
 * 
 * @warning 不要在信号处理函数中调用此函数
 * 
 * @see agentos_mem_free()
 * @see agentos_mem_realloc()
 * 
 * @example
 * // 基本用法
 * void* buffer = agentos_mem_alloc(1024, AGENTOS_MEM_FLAG_ZERO);
 * if (buffer == NULL) {
 *     log_error("Memory allocation failed");
 *     return AGENTOS_ERR_NO_MEMORY;
 * }
 * // 使用 buffer...
 * agentos_mem_free(buffer);
 */
void* agentos_mem_alloc(size_t size, uint32_t flags);
```

### 3.3 结构体注释模板

```c
/**
 * @brief 任务执行上下文结构体
 * 
 * 封装任务执行所需的全部上下文信息，包括输入数据、
 * 执行配置和输出缓冲区。此结构体为不透明类型，
 * 外部代码应通过访问函数操作。
 */
typedef struct agentos_task_context {
    uint64_t task_id;           /**< 任务唯一标识符 */
    char* input_data;           /**< 输入数据指针，内部管理 */
    size_t input_size;          /**< 输入数据大小（字节） */
    char* output_buffer;        /**< 输出缓冲区指针 */
    size_t output_capacity;     /**< 输出缓冲区容量 */
    size_t output_size;         /**< 实际输出大小 */
    uint32_t flags;             /**< 执行标志位 */
    int32_t error_code;         /**< 错误码，0 表示成功 */
    struct timespec start_time; /**< 任务开始时间 */
    struct timespec end_time;   /**< 任务结束时间 */
} agentos_task_context_t;
```

### 3.4 枚举注释模板

```c
/**
 * @brief 任务状态枚举
 * 
 * 定义任务在整个生命周期中可能的状态。状态转换遵循
 * 严格的状态机规则，非法转换将被拒绝。
 * 
 * @see agentos_task_state_transition_valid()
 */
typedef enum agentos_task_state {
    AGENTOS_TASK_STATE_IDLE = 0,       /**< 空闲状态，任务已创建但未启动 */
    AGENTOS_TASK_STATE_PENDING = 1,    /**< 等待状态，等待资源或依赖任务 */
    AGENTOS_TASK_STATE_RUNNING = 2,    /**< 运行状态，正在执行 */
    AGENTOS_TASK_STATE_SUSPENDED = 3,  /**< 挂起状态，可恢复 */
    AGENTOS_TASK_STATE_COMPLETED = 4,  /**< 完成状态，成功结束 */
    AGENTOS_TASK_STATE_FAILED = 5,     /**< 失败状态，异常终止 */
    AGENTOS_TASK_STATE_CANCELLED = 6,  /**< 取消状态，用户主动取消 */
} agentos_task_state_t;
```

### 3.5 宏定义注释模板

```c
/**
 * @def AGENTOS_MAX_TASK_NAME_LEN
 * @brief 任务名称最大长度
 */
#define AGENTOS_MAX_TASK_NAME_LEN 256

/**
 * @def AGENTOS_CHECK_NULL
 * @brief 空指针检查宏
 * 
 * 检查指针是否为空，若为空则记录错误日志并返回指定错误码。
 * 此宏会自动记录文件名、行号和函数名。
 * 
 * @param ptr 待检查的指针
 * @param ret 空指针时的返回值
 * 
 * @example
 * int process_data(const char* data) {
 *     AGENTOS_CHECK_NULL(data, AGENTOS_ERR_INVALID_ARG);
 *     // 处理 data...
 *     return AGENTOS_OK;
 * }
 */
#define AGENTOS_CHECK_NULL(ptr, ret) \
    do { \
        if ((ptr) == NULL) { \
            log_error("%s:%d: Null pointer: %s", __FILE__, __LINE__, #ptr); \
            return (ret); \
        } \
    } while (0)
```

### 3.6 全局变量注释模板

```c
/**
 * @brief 全局任务调度器实例
 * 
 * 单例模式实现，在系统启动时初始化。所有任务调度操作
 * 都通过此实例进行。访问前必须确保已调用 agentos_scheduler_init()。
 * 
 * @note 线程安全：内部使用读写锁保护
 * @warning 禁止直接修改此变量，必须通过 API 操作
 */
extern agentos_scheduler_t* g_scheduler;
```

---

## 四、Python 注释规范（Google Docstring 风格）

### 4.1 模块注释模板

```python
"""AgentOS 任务调度模块。

本模块提供任务调度核心功能，包括任务提交、状态管理、
优先级队列和依赖解析。调度器采用双系统架构：
- System 1：快速路径，处理简单任务
- System 2：深度路径，处理复杂任务

典型用法:
    from agentos.scheduler import TaskScheduler
    
    scheduler = TaskScheduler()
    task_id = scheduler.submit(task_plan)
    result = scheduler.wait(task_id)

属性:
    MAX_PRIORITY (int): 最大优先级值，默认为 10
    DEFAULT_TIMEOUT (float): 默认超时时间（秒），默认为 30.0

作者:
    AgentOS Team

版本:
    1.6.0
"""

from typing import Optional, List, Dict, Any
from dataclasses import dataclass
import logging

MAX_PRIORITY: int = 10
DEFAULT_TIMEOUT: float = 30.0
```

### 4.2 类注释模板

```python
class TaskScheduler:
    """任务调度器，管理任务的生命周期和执行。
    
    调度器负责接收任务请求、解析依赖关系、分配执行资源，
    并跟踪任务状态。支持优先级队列、超时控制和错误重试。
    
    调度器是线程安全的，可在多线程环境中使用。
    
    属性:
        name (str): 调度器名称，用于日志标识
        max_workers (int): 最大工作线程数
        queue_size (int): 当前队列长度
        
    示例:
        >>> scheduler = TaskScheduler(name="main", max_workers=4)
        >>> task_id = scheduler.submit(plan)
        >>> result = scheduler.wait(task_id, timeout=60)
        >>> print(result.status)
        'completed'
    
    注意:
        使用完毕后应调用 shutdown() 方法释放资源，
        或使用上下文管理器自动管理生命周期。
    """
    
    def __init__(self, name: str = "default", max_workers: int = 4) -> None:
        """初始化任务调度器。
        
        Args:
            name: 调度器名称，用于日志和监控标识
            max_workers: 最大工作线程数，必须为正整数
            
        Raises:
            ValueError: 当 max_workers <= 0 时抛出
        """
        pass
```

### 4.3 方法注释模板

```python
def submit(
    self,
    plan: TaskPlan,
    priority: int = 5,
    timeout: Optional[float] = None,
    tags: Optional[Dict[str, str]] = None,
) -> str:
    """提交任务计划到调度队列。
    
    将任务计划加入调度队列，返回任务 ID 用于后续查询。
    调度器会根据优先级和依赖关系决定执行顺序。
    
    Args:
        plan: 任务计划对象，包含执行步骤和参数
        priority: 任务优先级，1-10，数值越大优先级越高
        timeout: 任务超时时间（秒），None 表示使用默认值
        tags: 任务标签字典，用于分类和过滤
        
    Returns:
        str: 任务唯一标识符，格式为 "task-{uuid}"
        
    Raises:
        ValueError: 当 priority 不在 1-10 范围内
        TypeError: 当 plan 不是 TaskPlan 类型
        SchedulerError: 当调度器已关闭或队列已满
        
    Example:
        >>> plan = TaskPlan(steps=[...])
        >>> task_id = scheduler.submit(
        ...     plan,
        ...     priority=8,
        ...     timeout=120.0,
        ...     tags={"env": "production", "team": "ml"}
        ... )
        >>> print(task_id)
        'task-a1b2c3d4-e5f6-7890-abcd-ef1234567890'
        
    Note:
        - 提交后任务进入 PENDING 状态
        - 高优先级任务可能抢占正在执行的低优先级任务
        - 超时从任务开始执行时计算，不包括排队时间
    """
    pass
```

### 4.4 函数注释模板（独立函数）

```python
def parse_task_dag(
    plan: TaskPlan,
    validate: bool = True,
) -> Tuple[Dict[str, TaskNode], List[str]]:
    """解析任务计划为有向无环图（DAG）。
    
    将任务计划中的步骤解析为节点和边，构建执行依赖图。
    支持并行步骤、条件分支和循环结构。
    
    Args:
        plan: 任务计划对象
        validate: 是否验证 DAG 的有效性（无环、连通）
        
    Returns:
        Tuple[Dict[str, TaskNode], List[str]]:
            - 节点字典：键为节点 ID，值为 TaskNode 对象
            - 拓扑排序结果：节点 ID 列表，按执行顺序排列
            
    Raises:
        DAGValidationError: 当 validate=True 且检测到环时
        
    Example:
        >>> nodes, order = parse_task_dag(plan)
        >>> print(order)
        ['step_1', 'step_2', 'step_3']
        
    See Also:
        - execute_dag: 执行 DAG
        - visualize_dag: 可视化 DAG
    """
    pass
```

### 4.5 数据类注释模板

```python
@dataclass
class TaskPlan:
    """任务计划数据类。
    
    封装任务执行的完整计划，包括步骤序列、参数配置
    和执行策略。
    
    Attributes:
        id: 计划唯一标识符
        name: 计划名称，用于日志和监控
        steps: 执行步骤列表
        strategy: 执行策略（sequential/parallel/adaptive）
        max_retries: 最大重试次数
        created_at: 创建时间戳
        
    Example:
        >>> plan = TaskPlan(
        ...     id="plan-001",
        ...     name="data-processing",
        ...     steps=[Step(...), Step(...)],
        ...     strategy="parallel"
        ... )
    """
    
    id: str
    name: str
    steps: List["Step"]
    strategy: str = "sequential"
    max_retries: int = 3
    created_at: Optional[float] = None
```

---

## 五、JavaScript/TypeScript 注释规范（JSDoc/TSDoc 风格）

### 5.1 文件头注释

```typescript
/**
 * @fileoverview AgentOS 任务调度模块
 * 
 * 提供任务调度核心功能，包括任务提交、状态管理、
 * 优先级队列和依赖解析。
 * 
 * @module agentos/scheduler
 * @author AgentOS Team
 * @version 1.6.0
 * @license Apache-2.0
 * 
 * @example
 * import { TaskScheduler } from 'agentos/scheduler';
 * 
 * const scheduler = new TaskScheduler({ maxWorkers: 4 });
 * const taskId = await scheduler.submit(plan);
 * const result = await scheduler.wait(taskId);
 */

import { TaskPlan, TaskResult, TaskStatus } from './types';
```

### 5.2 类注释模板

```typescript
/**
 * 任务调度器，管理任务的生命周期和执行。
 * 
 * 调度器负责接收任务请求、解析依赖关系、分配执行资源，
 * 并跟踪任务状态。支持优先级队列、超时控制和错误重试。
 * 
 * @template T - 任务输入数据类型
 * @template R - 任务输出数据类型
 * 
 * @example
 * const scheduler = new TaskScheduler<TaskInput, TaskOutput>({
 *   name: 'main',
 *   maxWorkers: 4
 * });
 * 
 * const taskId = await scheduler.submit(plan);
 * const result = await scheduler.wait(taskId);
 * console.log(result.status); // 'completed'
 */
export class TaskScheduler<T = unknown, R = unknown> {
    /**
     * 创建任务调度器实例。
     * 
     * @param config - 调度器配置选项
     * @param config.name - 调度器名称，用于日志标识
     * @param config.maxWorkers - 最大工作线程数
     * @param config.defaultTimeout - 默认超时时间（毫秒）
     * 
     * @throws {Error} 当 maxWorkers <= 0 时抛出
     */
    constructor(config: SchedulerConfig) {
        // 实现...
    }
}
```

### 5.3 方法注释模板

```typescript
/**
 * 提交任务计划到调度队列。
 * 
 * 将任务计划加入调度队列，返回任务 ID 用于后续查询。
 * 调度器会根据优先级和依赖关系决定执行顺序。
 * 
 * @param plan - 任务计划对象
 * @param options - 提交选项
 * @param options.priority - 任务优先级（1-10），默认为 5
 * @param options.timeout - 超时时间（毫秒），默认使用调度器配置
 * @param options.tags - 任务标签，用于分类和过滤
 * 
 * @returns Promise 解析为任务 ID 字符串
 * 
 * @throws {ValidationError} 当 priority 不在 1-10 范围内
 * @throws {TypeError} 当 plan 格式不正确
 * @throws {SchedulerError} 当调度器已关闭或队列已满
 * 
 * @example
 * const taskId = await scheduler.submit(plan, {
 *   priority: 8,
 *   timeout: 120000,
 *   tags: { env: 'production', team: 'ml' }
 * });
 * console.log(taskId); // 'task-a1b2c3d4-...'
 */
async submit(
    plan: TaskPlan<T>,
    options?: SubmitOptions
): Promise<string> {
    // 实现...
}
```

### 5.4 接口注释模板

```typescript
/**
 * 调度器配置接口。
 * 
 * 定义创建调度器实例所需的所有配置选项。
 */
export interface SchedulerConfig {
    /** 调度器名称，用于日志和监控标识 */
    name: string;
    
    /** 最大工作线程数，默认为 CPU 核心数 */
    maxWorkers?: number;
    
    /** 默认任务超时时间（毫秒），默认为 30000 */
    defaultTimeout?: number;
    
    /** 是否启用任务重试，默认为 true */
    enableRetry?: boolean;
    
    /** 最大重试次数，默认为 3 */
    maxRetries?: number;
    
    /** 任务完成回调函数 */
    onComplete?: (taskId: string, result: TaskResult<R>) => void;
    
    /** 任务失败回调函数 */
    onError?: (taskId: string, error: Error) => void;
}
```

### 5.5 类型别名注释模板

```typescript
/**
 * 任务状态类型。
 * 
 * 定义任务在整个生命周期中可能的状态值。
 * - 'idle': 空闲状态，任务已创建但未启动
 * - 'pending': 等待状态，等待资源或依赖任务
 * - 'running': 运行状态，正在执行
 * - 'completed': 完成状态，成功结束
 * - 'failed': 失败状态，异常终止
 * - 'cancelled': 取消状态，用户主动取消
 */
export type TaskStatus = 
    | 'idle' 
    | 'pending' 
    | 'running' 
    | 'completed' 
    | 'failed' 
    | 'cancelled';

/**
 * 任务处理函数类型。
 */
export type TaskHandler<T, R> = (input: T) => Promise<R>;
```

### 5.6 枚举注释模板

```typescript
/**
 * 任务优先级枚举。
 * 
 * 定义预定义的任务优先级级别。
 */
export const enum TaskPriority {
    /** 最低优先级，后台任务 */
    LOW = 1,
    
    /** 正常优先级，默认值 */
    NORMAL = 5,
    
    /** 高优先级，重要任务 */
    HIGH = 8,
    
    /** 最高优先级，紧急任务 */
    CRITICAL = 10
}
```

### 5.7 常量注释模板

```typescript
/**
 * 默认任务超时时间（毫秒）。
 */
export const DEFAULT_TIMEOUT_MS = 30000;

/**
 * 任务名称最大长度。
 */
export const MAX_TASK_NAME_LENGTH = 256;

/**
 * 支持的任务状态列表。
 */
export const VALID_TASK_STATUSES: readonly TaskStatus[] = [
    'idle',
    'pending',
    'running',
    'completed',
    'failed',
    'cancelled'
] as const;
```

---

## 六、Go 注释规范（GoDoc 风格）

### 6.1 包注释模板

```go
// Package scheduler 提供任务调度核心功能。
//
// 本包实现任务调度器，支持任务提交、状态管理、优先级队列
// 和依赖解析。调度器采用双系统架构：
//   - System 1：快速路径，处理简单任务
//   - System 2：深度路径，处理复杂任务
//
// 典型用法:
//
//	scheduler := scheduler.New(scheduler.Config{MaxWorkers: 4})
//	taskID, err := scheduler.Submit(plan)
//	if err != nil {
//	    log.Fatal(err)
//	}
//	result, err := scheduler.Wait(taskID)
//
// 调度器是线程安全的，可在多个 goroutine 中并发使用。
//
// 注意事项:
//   - 使用完毕后应调用 Shutdown() 方法释放资源
//   - 任务超时从开始执行时计算，不包括排队时间
package scheduler
```

### 6.2 函数注释模板

```go
// Submit 提交任务计划到调度队列。
//
// 将任务计划加入调度队列，返回任务 ID 用于后续查询。
// 调度器会根据优先级和依赖关系决定执行顺序。
//
// 参数:
//   - plan: 任务计划对象，不能为 nil
//   - opts: 提交选项，可为 nil 使用默认值
//
// 返回:
//   - string: 任务唯一标识符
//   - error: 错误信息，可能的错误包括：
//   - ErrInvalidPlan: 计划格式无效
//   - ErrSchedulerClosed: 调度器已关闭
//   - ErrQueueFull: 队列已满
//
// 示例:
//
//	taskID, err := scheduler.Submit(plan, scheduler.WithPriority(8))
//	if err != nil {
//	    return fmt.Errorf("submit failed: %w", err)
//	}
//	fmt.Println("Task ID:", taskID)
//
// 注意: 提交后任务进入 Pending 状态，需要调用 Wait 或 Poll 查询结果。
func (s *Scheduler) Submit(plan *TaskPlan, opts ...SubmitOption) (string, error) {
    // 实现...
}
```

### 6.3 类型注释模板

```go
// Scheduler 任务调度器，管理任务的生命周期和执行。
//
// 调度器负责接收任务请求、解析依赖关系、分配执行资源，
// 并跟踪任务状态。Scheduler 是线程安全的。
//
// 零值不可用，必须使用 New 创建实例。
type Scheduler struct {
    name       string        // 调度器名称
    maxWorkers int           // 最大工作 goroutine 数
    queue      *priorityQueue // 优先级队列
    workers    *workerPool   // 工作池
    mu         sync.RWMutex  // 保护内部状态
    closed     bool          // 是否已关闭
}

// TaskPlan 任务计划，封装任务执行的完整计划。
type TaskPlan struct {
    ID         string        // 计划唯一标识符
    Name       string        // 计划名称
    Steps      []*TaskStep   // 执行步骤列表
    Strategy   Strategy      // 执行策略
    MaxRetries int           // 最大重试次数
    CreatedAt  time.Time     // 创建时间
}
```

### 6.4 接口注释模板

```go
// Executor 任务执行器接口。
//
// 定义任务执行的标准接口，不同的执行器实现可以支持
// 不同的执行环境（本地、容器、远程）。
type Executor interface {
    // Execute 执行单个任务步骤。
    //
    // 参数:
    //   - ctx: 上下文，用于取消和超时控制
    //   - step: 任务步骤定义
    //
    // 返回:
    //   - *StepResult: 步骤执行结果
    //   - error: 执行错误
    Execute(ctx context.Context, step *TaskStep) (*StepResult, error)
    
    // Name 返回执行器名称。
    Name() string
    
    // Close 关闭执行器，释放资源。
    Close() error
}
```

### 6.5 常量注释模板

```go
const (
    // DefaultMaxWorkers 默认最大工作 goroutine 数。
    DefaultMaxWorkers = 4
    
    // DefaultTimeout 默认任务超时时间（秒）。
    DefaultTimeout = 30
    
    // MaxTaskNameLength 任务名称最大长度。
    MaxTaskNameLength = 256
)
```

---

## 七、Rust 注释规范（Rustdoc 风格）

### 7.1 模块注释模板

```rust
//! AgentOS 任务调度模块
//!
//! 提供任务调度核心功能，包括任务提交、状态管理、
//! 优先级队列和依赖解析。调度器采用双系统架构：
//! - System 1：快速路径，处理简单任务
//! - System 2：深度路径，处理复杂任务
//!
//! # 典型用法
//!
//! ```
//! use agentos_scheduler::{Scheduler, TaskPlan};
//!
//! let scheduler = Scheduler::new(Config::default());
//! let task_id = scheduler.submit(plan)?;
//! let result = scheduler.wait(task_id)?;
//! println!("Status: {:?}", result.status);
//! # Ok::<(), Error>(())
//! ```
//!
//! # 线程安全
//!
//! 所有公共接口均为线程安全，可在多线程环境中使用。

pub mod scheduler;
pub mod task;
pub mod error;
```

### 7.2 结构体注释模板

```rust
/// 任务调度器，管理任务的生命周期和执行。
///
/// 调度器负责接收任务请求、解析依赖关系、分配执行资源，
/// 并跟踪任务状态。
///
/// # 线程安全
///
/// `Scheduler` 是 `Send` 和 `Sync`，可在多线程间共享。
///
/// # 示例
///
/// ```
/// use agentos_scheduler::{Scheduler, Config};
///
/// let scheduler = Scheduler::new(Config {
///     max_workers: 4,
///     default_timeout: Duration::from_secs(30),
/// });
///
/// let task_id = scheduler.submit(plan)?;
/// let result = scheduler.wait(task_id)?;
/// ```
///
/// # Panics
///
/// 不会 panic，所有错误通过 `Result` 返回。
pub struct Scheduler {
    name: String,
    max_workers: usize,
    queue: PriorityQueue,
    workers: WorkerPool,
}
```

### 7.3 方法注释模板

```rust
impl Scheduler {
    /// 创建新的调度器实例。
    ///
    /// # 参数
    ///
    /// * `config` - 调度器配置
    ///
    /// # 返回
    ///
    /// 成功返回调度器实例。
    ///
    /// # 错误
    ///
    /// 当 `config.max_workers` 为 0 时返回 `Error::InvalidConfig`。
    ///
    /// # 示例
    ///
    /// ```
    /// let scheduler = Scheduler::new(Config {
    ///     name: "main".into(),
    ///     max_workers: 4,
    ///     ..Default::default()
    /// })?;
    /// ```
    pub fn new(config: Config) -> Result<Self, Error> {
        // 实现...
    }
    
    /// 提交任务计划到调度队列。
    ///
    /// 将任务计划加入调度队列，返回任务 ID 用于后续查询。
    ///
    /// # 参数
    ///
    /// * `plan` - 任务计划对象
    /// * `options` - 提交选项（可选）
    ///
    /// # 返回
    ///
    /// 成功返回任务 ID 字符串。
    ///
    /// # 错误
    ///
    /// * `Error::InvalidPlan` - 计划格式无效
    /// * `Error::SchedulerClosed` - 调度器已关闭
    /// * `Error::QueueFull` - 队列已满
    ///
    /// # 示例
    ///
    /// ```
    /// let task_id = scheduler.submit(plan, SubmitOptions {
    ///     priority: Priority::High,
    ///     timeout: Some(Duration::from_secs(120)),
    /// })?;
    /// println!("Task ID: {}", task_id);
    /// ```
    pub fn submit(
        &self,
        plan: TaskPlan,
        options: Option<SubmitOptions>,
    ) -> Result<String, Error> {
        // 实现...
    }
}
```

### 7.4 枚举注释模板

```rust
/// 任务状态枚举。
///
/// 定义任务在整个生命周期中可能的状态。
///
/// # 状态转换
///
/// ```text
/// Idle -> Pending -> Running -> Completed/Failed/Cancelled
///                ↘ Suspended ↗
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TaskStatus {
    /// 空闲状态，任务已创建但未启动。
    Idle,
    
    /// 等待状态，等待资源或依赖任务。
    Pending,
    
    /// 运行状态，正在执行。
    Running,
    
    /// 挂起状态，可恢复。
    Suspended,
    
    /// 完成状态，成功结束。
    Completed,
    
    /// 失败状态，异常终止。
    Failed,
    
    /// 取消状态，用户主动取消。
    Cancelled,
}
```

### 7.5 trait 注释模板

```rust
/// 任务执行器 trait。
///
/// 定义任务执行的标准接口，不同的实现可以支持
/// 不同的执行环境（本地、容器、远程）。
///
/// # 实现要求
///
/// 实现必须是 `Send` 和 `Sync`，以支持多线程环境。
///
/// # 示例
///
/// ```
/// struct LocalExecutor;
///
/// impl Executor for LocalExecutor {
///     fn execute(&self, step: &TaskStep) -> Result<StepResult, Error> {
///         // 本地执行逻辑
///         Ok(StepResult::default())
///     }
/// }
/// ```
pub trait Executor: Send + Sync {
    /// 执行单个任务步骤。
    ///
    /// # 参数
    ///
    /// * `step` - 任务步骤定义
    ///
    /// # 返回
    ///
    /// 成功返回步骤执行结果。
    fn execute(&self, step: &TaskStep) -> Result<StepResult, Error>;
    
    /// 返回执行器名称。
    fn name(&self) -> &str;
}
```

---

## 八、特殊场景注释

### 8.1 TODO 注释

```c
// TODO(author): 简短描述 [优先级] [关联Issue]
// TODO(zhangsan): 添加任务取消功能 [P1] [#1234]
```

```python
# TODO(author): 简短描述 [优先级]
# TODO(zhangsan): 实现任务重试逻辑 [P2]
```

```typescript
// TODO(author): 简短描述 [优先级]
// TODO(zhangsan): 添加任务超时处理 [P1]
```

### 8.2 FIXME 注释

```c
// FIXME(author): 问题描述 [严重程度]
// FIXME(lisi): 并发访问时存在竞态条件 [Critical]
```

```python
# FIXME(author): 问题描述 [严重程度]
# FIXME(lisi): 内存泄漏，需要手动释放资源 [High]
```

### 8.3 DEPRECATED 注释

```c
/**
 * @deprecated 自版本 1.5.0 起废弃，请使用 agentos_task_submit_v2()
 * @see agentos_task_submit_v2()
 * 
 * 此函数将在版本 2.0.0 中移除。迁移指南：
 * - 将 task_context_t 替换为 task_config_t
 * - 使用新的错误处理机制
 */
AGENTOS_DEPRECATED("Use agentos_task_submit_v2() instead")
int agentos_task_submit(task_context_t* ctx);
```

```python
def submit_task(plan: TaskPlan) -> str:
    """提交任务（已废弃）。
    
    .. deprecated:: 1.5.0
        请使用 :meth:`TaskScheduler.submit` 代替。
        此方法将在 2.0.0 版本中移除。
    
    Args:
        plan: 任务计划
        
    Returns:
        任务 ID
        
    Migration:
        旧代码: submit_task(plan)
        新代码: scheduler.submit(plan)
    """
    warnings.warn(
        "submit_task is deprecated, use TaskScheduler.submit",
        DeprecationWarning,
        stacklevel=2
    )
    return self._scheduler.submit(plan)
```

```typescript
/**
 * @deprecated 自版本 1.5.0 起废弃
 * 
 * 请使用 `TaskScheduler.submit()` 代替。
 * 此方法将在 2.0.0 版本中移除。
 * 
 * @see TaskScheduler.submit
 */
submitTask(plan: TaskPlan): string {
    console.warn('submitTask is deprecated, use scheduler.submit()');
    return this.submit(plan);
}
```

### 8.4 HACK 注释

```c
// HACK(author): 临时解决方案，原因说明
// HACK(wangwu): 绕过第三方库的 bug，等待上游修复
// 相关 Issue: https://github.com/vendor/lib/issues/456
```

### 8.5 NOTE 注释

```c
// NOTE: 重要说明，需要特别注意的点
// NOTE: 此处必须使用 volatile，因为会被信号处理函数修改
```

### 8.6 SECURITY 注释

```c
// SECURITY: 安全相关说明
// SECURITY: 此函数处理用户输入，必须进行严格的参数验证
// 参考: OWASP Input Validation Cheat Sheet
```

---

## 九、注释质量检查

### 9.1 自动化检查工具

| 语言 | 工具 | 配置文件 |
|------|------|----------|
| C/C++ | Doxygen + clang-tidy | Doxyfile, .clang-tidy |
| Python | pydocstyle, flake8-docstrings | .pydocstyle, setup.cfg |
| JavaScript/TypeScript | ESLint + TSDoc | .eslintrc.js |
| Go | golint, go vet | - |
| Rust | cargo doc, clippy | Cargo.toml |

### 9.2 检查规则

**必须检查项**：
- 公共 API 是否有完整注释
- 参数说明是否完整
- 返回值说明是否准确
- 异常说明是否完整
- 使用示例是否可运行

**建议检查项**：
- 注释是否解释"为什么"
- 注释是否与代码同步
- 注释语言是否一致
- 注释格式是否规范

### 9.3 CI 集成示例

```yaml
# .github/workflows/doc-check.yml
name: Documentation Check

on: [push, pull_request]

jobs:
  check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Check C/C++ Documentation
        run: |
          doxygen Doxyfile
          if [ -s doxygen_warnings.txt ]; then
            echo "Documentation warnings found:"
            cat doxygen_warnings.txt
            exit 1
          fi
          
      - name: Check Python Documentation
        run: |
          pip install pydocstyle flake8-docstrings
          pydocstyle --add-ignore=D100,D104 agentos/
          
      - name: Check TypeScript Documentation
        run: |
          npm install
          npm run lint:docs
```

---

## 十、最佳实践

### 10.1 注释编写原则

1. **及时更新**：代码修改时同步更新注释
2. **避免冗余**：不重复代码已表达的信息
3. **保持简洁**：注释应精炼，避免过度描述
4. **使用示例**：复杂接口必须提供使用示例
5. **说明边界**：明确说明参数边界、返回值边界

### 10.2 常见错误

| 错误类型 | 错误示例 | 正确做法 |
|----------|----------|----------|
| 无用注释 | `i++; // i 加 1` | 删除注释 |
| 过时注释 | 注释描述与代码不符 | 同步更新注释 |
| 冗余注释 | 重复参数类型说明 | 使用类型系统表达 |
| 误导注释 | 注释与实际行为矛盾 | 修正注释或代码 |
| 过长注释 | 注释比代码还长 | 重构代码或提取文档 |

### 10.3 注释模板快速参考

```
C/C++ 函数:
/**
 * @brief 简要描述
 * @param param_name [in/out/in-out] 描述
 * @return 返回值描述
 * @note 注意事项
 * @example 使用示例
 */

Python 函数:
def func(arg: Type) -> ReturnType:
    """简要描述。
    
    详细描述...
    
    Args:
        arg: 参数描述
        
    Returns:
        返回值描述
        
    Raises:
        ErrorType: 异常描述
        
    Example:
        >>> func(value)
        result
    """

TypeScript 函数:
/**
 * 简要描述。
 * 
 * @param arg - 参数描述
 * @returns 返回值描述
 * @throws {ErrorType} 异常描述
 * 
 * @example
 * const result = func(value);
 */

Go 函数:
// FuncName 简要描述。
//
// 详细描述...
//
// 参数:
//   - arg: 参数描述
//
// 返回:
//   - 返回值描述
//   - error: 错误描述
//
// 示例:
//   result, err := FuncName(arg)

Rust 函数:
/// 简要描述。
///
/// 详细描述...
///
/// # 参数
///
/// * `arg` - 参数描述
///
/// # 返回
///
/// 返回值描述
///
/// # 错误
///
/// 错误描述
///
/// # 示例
///
/// ```
/// let result = func(arg)?;
/// ```
```

---
## 十一、AgentOS 特定模块注释指南

AgentOS 的四层垂直架构和核心模块对代码注释提出了特定要求。以下指南确保注释与系统架构保持高度一致：

### 11.1 Atoms（原子层）注释规范
Atoms 模块实现微内核核心功能，注释要求最高级别严谨性：

#### 11.1.1 内存管理（Memory Manager）
```c
/**
 * @brief 分配 NUMA 感知的内存块（映射原则：M-3 拓扑优化）
 * 
 * 根据 CPU 拓扑结构分配最优位置的内存，减少跨节点访问延迟。
 * 使用持久同调分析识别内存访问模式，动态调整分配策略。
 * 
 * @param size 请求大小，自动对齐到 HUGEPAGE 边界
 * @param numa_node 目标 NUMA 节点，-1 表示自动选择
 * @param flags 分配标志，见 AGENTOS_MEM_FLAGS
 * 
 * @note 此函数被 coreloopthree 的核心调度循环频繁调用
 * @see memoryrovol.md 中的内存进化机制
 * @see atoms/corekern/memory/numa_allocator.c
 */
void* atoms_mem_alloc_numa(size_t size, int numa_node, uint32_t flags);
```

#### 11.1.2 任务调度（Task Scheduler）
```c
/**
 * @brief 提交任务到双系统调度器（映射原则：C-2 认知优化）
 * 
 * 根据任务复杂度自动选择执行路径：
 * - System 1 路径：简单任务，直接执行
 * - System 2 路径：复杂任务，深度分析后执行
 * 
 * @note 与 microkernel.md 中描述的调度策略严格对应
 * @see coreloopthree.md 中的三循环架构
 */
```

### 11.2 Backs（守护层）注释规范
Backs 模块作为系统服务守护进程，注释需强调可靠性和监控：

#### 11.2.1 IPC 通信服务
```python
class IPCBridge:
    """IPC 通信桥梁，连接 Atoms 与 Domes（映射原则：E-3 通信基础设施）
    
    实现基于共享内存和信号量的高性能进程间通信。
    集成 OpenTelemetry 监控，实时追踪消息流。
    
    Attributes:
        shm_id: 共享内存标识符
        sem_key: 信号量键值
        metrics: 通信指标收集器
        
    Note:
        - 必须与 ipc.md 中的通信协议保持一致
        - 异常处理需记录到 syslog 并触发告警
    """
```

### 11.3 Domes（安全域层）注释规范
Domes 模块处理安全敏感操作，注释必须包含完整的安全考量：

#### 11.3.1 安全上下文管理
```typescript
/**
 * 安全上下文管理器，实现零信任访问控制（映射原则：D-2 安全隔离）
 * 
 * 基于能力（Capability）的安全模型，每个操作必须显式授权。
 * 使用形式化验证确保访问控制策略的正确性。
 * 
 * @security 此组件处理用户身份和权限，必须防御提权攻击
 * @see Security_design_guide.md 中的零信任架构
 */
```

### 11.4 Common（公共库层）注释规范
Common 模块提供跨层通用功能，注释需强调通用性和可重用性：

#### 11.4.1 公共数据结构
```go
// VectorDB 向量数据库客户端，提供 FAISS 和 HNSW 支持。
//
// 封装向量索引的创建、查询和更新操作，支持持久化存储。
// 集成 HDBSCAN 聚类算法，自动发现数据中的拓扑结构。
//
// 注意:
//   - 此客户端被 Atoms、Backs、Domes 三层共同使用
//   - 性能关键路径，避免不必要的内存分配
//   - 错误处理必须向上层传递完整的上下文信息
//
// 参考:
//   - memoryrovol.md 中的记忆进化算法
//   - 持久同调在拓扑数据分析中的应用
```

### 11.5 跨模块交互注释
重要跨模块接口必须在注释中明确标注交互关系：

```c
/**
 * @brief 核心三循环与微内核的集成点（关键架构接口）
 * 
 * 此函数是 coreloopthree 调度循环与 microkernel 任务管理的桥梁。
 * 涉及模块：atoms/corekern/scheduler/, atoms/corekern/task/
 * 
 * @architecture 此接口体现了 S-1 垂直分层的设计原则
 * @performance 被每秒调用数千次，必须高度优化
 * @security 必须验证调用者身份，防止非法调度请求
 */
```

---
## 十二、参考资料

1. **Doxygen 官方文档**: https://www.doxygen.nl/manual/
2. **Google Python Style Guide**: https://google.github.io/styleguide/pyguide.html
3. **TypeScript Documentation**: https://www.typescriptlang.org/docs/
4. **Effective Go**: https://golang.org/doc/effective_go
5. **Rust API Guidelines**: https://rust-lang.github.io/api-guidelines/
6. **AgentOS 架构设计原则**: [architectural_design_principles.md](../../architecture/folder/architectural_design_principles.md)
7. **AgentOS 统一术语表**: [TERMINOLOGY.md](../TERMINOLOGY.md)
8. **AgentOS 核心架构文档**: 
   - [coreloopthree.md](../../architecture/folder/coreloopthree.md)
   - [memoryrovol.md](../../architecture/folder/memoryrovol.md)
   - [microkernel.md](../../architecture/folder/microkernel.md)
   - [ipc.md](../../architecture/folder/ipc.md)
   - [syscall.md](../../architecture/folder/syscall.md)
   - [logging_system.md](../../architecture/folder/logging_system.md)

---

© 2026 SPHARX Ltd. All Rights Reserved.  
"From data intelligence emerges."
