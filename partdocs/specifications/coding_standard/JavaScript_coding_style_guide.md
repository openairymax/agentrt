# AgentOS JavaScript 编码规范

**版本**: Doc V1.5  
**发布日期**: 2026-03-24  
**适用范围**: AgentOS 所有 JavaScript/TypeScript 代码  
**理论基础**: 工程两论（反馈闭环）、系统工程（模块化）、双系统认知理论

---

## 一、概述

### 1.1 编制目的

本规范为 AgentOS 项目中的 JavaScript/TypeScript 代码提供统一的编码标准。基于项目架构设计原则的四维正交体系，本规范聚焦于工程观维度，为开发者提供可操作的代码实现指南。

### 1.2 理论基础

本规范基于 AgentOS 架构设计原则的四维正交体系：

- **《工程控制论》**（原则 S-1, E-2）：通过错误处理、日志、健康检查构建反馈闭环
- **《论系统工程》**（原则 S-2）：模块化、接口驱动、边界清晰
- **双系统认知理论**（原则 C-1）：TypeScript 提供编译时检查（System 2），JavaScript 提供运行时灵活（System 1）

**双系统在 JavaScript/TypeScript 中的体现**:

| 场景 | System 1（快速） | System 2（严谨） |
|------|-----------------|-----------------|
| 类型系统 | JavaScript 动态类型 | TypeScript 静态类型 |
| 错误处理 | 运行时 try-catch | 编译时类型检查 |
| 开发体验 | 快速原型开发 | 完整类型定义 |
| 性能优化 | JIT 热路径优化 | AOT 编译优化 |

### 1.3 适用范围

| 环境 | 标准 | 运行环境 |
|------|------|----------|
| Node.js | ES2022+ / TypeScript 5.0+ | Node.js 18+ |
| 浏览器 | ES2022 / TypeScript 5.0+ | ES2022 兼容浏览器 |
| SDK | ES2022 / TypeScript 5.0+ | Node.js 18+ |

---

## 二、文件组织

### 2.1 文件命名

- **TypeScript 文件**：`.ts` 扩展名，采用 `kebab-case`
- **JavaScript 文件**：`.js` 扩展名，采用 `kebab-case`
- **类型定义文件**：`.d.ts` 扩展名
- **React 组件**：`.tsx`/`.jsx` 扩展名

```
src/
├── agents/
│   ├── task-scheduler.ts
│   ├── memory-manager.ts
│   └── cognition-engine.ts
├── types/
│   ├── agent.types.ts
│   └── task.types.ts
└── utils/
    ├── error-handler.ts
    └── logger.ts
```

### 2.2 模块结构

```typescript
/**
 * @fileoverview 任务调度器模块
 *
 * 提供任务调度核心功能，包括任务提交、状态管理、
 * 优先级队列和依赖解析。
 *
 * @module agentos/scheduler
 * @author AgentOS Team
 * @version 1.5.0
 */

// 1. 导入语句
import { EventEmitter } from 'events';
import { validateTaskPlan } from '../utils/validator';
import { Logger } from '../utils/logger';

// 2. 类型定义
export interface SchedulerConfig {
    name: string;
    maxWorkers: number;
    defaultTimeout: number;
}

export type SchedulerStatus = 'idle' | 'running' | 'closed';

// 3. 常量定义
const DEFAULT_CONFIG: Required<SchedulerConfig> = {
    name: 'default',
    maxWorkers: 4,
    defaultTimeout: 30000
};

// 4. 类/函数定义
export class TaskScheduler {
    private config: SchedulerConfig;
    private status: SchedulerStatus = 'idle';
    
    constructor(config: SchedulerConfig) {
        this.config = { ...DEFAULT_CONFIG, ...config };
    }
    
    // 方法实现...
}

// 5. 导出
export default TaskScheduler;
```

---

## 三、命名规范

### 3.1 命名风格

| 类型 | 风格 | 示例 |
|------|------|------|
| 变量/函数 | camelCase | `taskId`, `submitTask()`, `getTaskStatus()` |
| 类/接口/类型 | PascalCase | `class TaskScheduler`, `interface TaskConfig` |
| 常量 | UPPER_SNAKE_CASE | `MAX_RETRY_COUNT`, `DEFAULT_TIMEOUT` |
| 枚举成员 | camelCase | `TaskStatus.Idle`, `Priority.Normal` |
| 文件名 | kebab-case | `task-scheduler.ts`, `memory-manager.js` |
| 目录名 | kebab-case | `task-scheduler/`, `memory-manager/` |

### 3.2 命名示例

```typescript
// 正确的命名
const MAX_TASK_NAME_LENGTH = 256;
const taskRegistry = new Map<string, Task>();
const taskId = 'task-001';

enum TaskPriority {
    Low = 1,
    Normal = 5,
    High = 8,
    Critical = 10
}

interface TaskConfig {
    name: string;
    priority: TaskPriority;
}

// 错误的命名
const maxTaskNameLength = 256;  // 常量应该 UPPER_SNAKE_CASE
const TaskRegistry = new Map();  // 变量应该 camelCase
class task_scheduler {}  // 类名应该 PascalCase
```

---

## 四、类型设计

### 4.1 类型别名

```typescript
// 基本类型别名
type TaskId = string;
type Timestamp = number;
type ByteBuffer = Uint8Array;

// 函数类型
type TaskHandler<TInput, TOutput> = (input: TInput) => Promise<TOutput>;
type ErrorHandler = (error: Error) => void;

// 回调类型
type CompletionCallback = (result: TaskResult) => void;
type ProgressCallback = (progress: number) => void;
```

### 4.2 接口定义

```typescript
/**
 * 任务计划接口
 */
export interface TaskPlan {
    /** 任务唯一标识符 */
    readonly id: TaskId;
    
    /** 任务名称 */
    name: string;
    
    /** 执行步骤列表 */
    steps: readonly TaskStep[];
    
    /** 执行策略 */
    strategy: 'sequential' | 'parallel' | 'adaptive';
    
    /** 最大重试次数 */
    maxRetries: number;
    
    /** 创建时间戳 */
    readonly createdAt: Timestamp;
}

/**
 * 任务步骤接口
 */
export interface TaskStep {
    /** 步骤 ID */
    readonly id: string;
    
    /** 步骤名称 */
    name: string;
    
    /** 步骤处理器 */
    handler: TaskHandler<unknown, unknown>;
    
    /** 依赖的步骤 ID */
    dependencies?: readonly string[];
    
    /** 超时时间（毫秒） */
    timeout?: number;
}
```

### 4.3 枚举

```typescript
/**
 * 任务状态枚举
 */
export const enum TaskStatus {
    /** 空闲状态 */
    Idle = 'idle',
    
    /** 等待状态 */
    Pending = 'pending',
    
    /** 运行状态 */
    Running = 'running',
    
    /** 完成状态 */
    Completed = 'completed',
    
    /** 失败状态 */
    Failed = 'failed',
    
    /** 取消状态 */
    Cancelled = 'cancelled'
}

/**
 * 任务优先级枚举
 */
export const enum TaskPriority {
    Low = 1,
    Normal = 5,
    High = 8,
    Critical = 10
}
```

---

## 五、函数设计

### 5.1 函数签名规范

```typescript
/**
 * 提交任务计划
 *
 * 将任务计划加入调度队列，返回任务 ID 用于后续查询。
 * 调度器会根据优先级和依赖关系决定执行顺序。
 *
 * @param plan - 任务计划对象
 * @param options - 提交选项
 * @returns 任务 ID
 *
 * @throws {ValidationError} 当计划格式无效时
 * @throws {SchedulerClosedError} 当调度器已关闭时
 *
 * @example
 * ```typescript
 * const taskId = await scheduler.submit({
 *     name: 'data-processing',
 *     steps: [...],
 *     strategy: 'parallel'
 * });
 * console.log(`Task submitted: ${taskId}`);
 * ```
 */
async submit(
    plan: TaskPlan,
    options?: SubmitOptions
): Promise<string> {
    // 实现...
}
```

### 5.2 参数验证

```typescript
/**
 * 验证任务计划
 */
export function validateTaskPlan(plan: unknown): ValidationResult {
    // 1. 基本类型检查
    if (plan === null || plan === undefined) {
        return { valid: false, error: 'Plan cannot be null or undefined' };
    }
    
    // 2. 类型断言
    if (typeof plan !== 'object') {
        return { valid: false, error: 'Plan must be an object' };
    }
    
    const taskPlan = plan as Partial<TaskPlan>;
    
    // 3. 必填字段验证
    if (!taskPlan.id || typeof taskPlan.id !== 'string') {
        return { valid: false, error: 'Plan must have a valid id' };
    }
    
    // 4. 长度验证
    if (taskPlan.id.length > MAX_TASK_ID_LENGTH) {
        return { valid: false, error: `Task ID too long: ${taskPlan.id.length}` };
    }
    
    // 5. 枚举值验证
    if (taskPlan.strategy && !['sequential', 'parallel', 'adaptive'].includes(taskPlan.strategy)) {
        return { valid: false, error: `Invalid strategy: ${taskPlan.strategy}` };
    }
    
    return { valid: true };
}
```

### 5.3 返回值约定

```typescript
/**
 * 使用 Result 类型处理错误
 */
export type Result<T, E = Error> =
    | { success: true; value: T }
    | { success: false; error: E };

/**
 * 异步操作结果
 */
export interface AsyncResult<T> {
    /** 是否成功 */
    readonly success: boolean;
    /** 成功时的值 */
    readonly value?: T;
    /** 失败时的错误 */
    readonly error?: Error;
}

/**
 * 任务提交结果
 */
export interface SubmitResult {
    /** 任务 ID */
    taskId: string;
    /** 提交时间戳 */
    submittedAt: Timestamp;
    /** 预计开始时间 */
    estimatedStartTime?: Timestamp;
}
```

---

## 六、类设计

### 6.1 类结构模板

```typescript
/**
 * 任务调度器
 *
 * 负责管理任务的生命周期和执行。调度器采用双系统架构，
 * System 1 处理简单任务，System 2 处理复杂任务。
 *
 * @example
 * ```typescript
 * const scheduler = new TaskScheduler({
 *     name: 'main',
 *     maxWorkers: 4
 * });
 *
 * const taskId = await scheduler.submit(plan);
 * const result = await scheduler.wait(taskId);
 * ```
 */
export class TaskScheduler extends EventEmitter {
    /** 调度器配置 */
    private readonly config: Readonly<Required<SchedulerConfig>>;
    
    /** 当前状态 */
    private status: SchedulerStatus = 'idle';
    
    /** 任务注册表 */
    private readonly tasks: Map<TaskId, TaskContext> = new Map();
    
    /** 工作线程池 */
    private readonly workerPool: WorkerPool;
    
    /**
     * 构造函数
     */
    public constructor(config: SchedulerConfig) {
        super();
        this.config = Object.freeze({ ...DEFAULT_CONFIG, ...config });
        this.workerPool = new WorkerPool(this.config.maxWorkers);
        
        // 设置事件处理器...
    }
    
    /**
     * 提交任务
     */
    public async submit(plan: TaskPlan): Promise<string> {
        // 实现...
    }
    
    /**
     * 等待任务完成
     */
    public async wait(taskId: string, timeout?: number): Promise<TaskResult> {
        // 实现...
    }
    
    /**
     * 关闭调度器
     */
    public async shutdown(): Promise<void> {
        // 实现...
    }
}
```

### 6.2 私有成员命名

```typescript
class TaskExecutor {
    // 使用下划线前缀标记私有成员
    private _isRunning: boolean = false;
    private _taskQueue: Task[] = [];
    
    // 公开访问器
    public get isRunning(): boolean {
        return this._isRunning;
    }
    
    // 或者使用 # 前缀（ES2022 私有字段）
    #cancellationToken: CancellationToken | null = null;
    
    public execute(task: Task): Promise<TaskResult> {
        // 实现...
    }
}
```

---

## 七、错误处理

### 7.1 错误类定义

```typescript
/**
 * AgentOS 错误基类
 */
export class AgentOSError extends Error {
    public readonly code: string;
    public readonly timestamp: number;
    
    constructor(message: string, code: string) {
        super(message);
        this.name = this.constructor.name;
        this.code = code;
        this.timestamp = Date.now();
        Error.captureStackTrace(this, this.constructor);
    }
}

/**
 * 调度器错误
 */
export class SchedulerError extends AgentOSError {
    public constructor(message: string) {
        super(message, 'SCHEDULER_ERROR');
    }
}

/**
 * 调度器已关闭错误
 */
export class SchedulerClosedError extends SchedulerError {
    public constructor() {
        super('Scheduler is closed');
        this.name = 'SchedulerClosedError';
    }
}

/**
 * 任务不存在错误
 */
export class TaskNotFoundError extends SchedulerError {
    public constructor(taskId: string) {
        super(`Task not found: ${taskId}`);
        this.name = 'TaskNotFoundError';
    }
}

/**
 * 验证错误
 */
export class ValidationError extends AgentOSError {
    public constructor(message: string) {
        super(message, 'VALIDATION_ERROR');
    }
}
```

### 7.2 错误处理模式

```typescript
/**
 * 使用 Result 类型处理错误
 */
async function submitTask(
    plan: TaskPlan
): Promise<Result<string, SchedulerError>> {
    try {
        // 验证计划
        const validation = validateTaskPlan(plan);
        if (!validation.valid) {
            return { success: false, error: new ValidationError(validation.error!) };
        }
        
        // 提交任务
        const taskId = await scheduler.submit(plan);
        return { success: true, value: taskId };
        
    } catch (error) {
        if (error instanceof SchedulerError) {
            return { success: false, error };
        }
        return { success: false, error: new SchedulerError(`Unexpected error: ${error}`) };
    }
}

/**
 * 使用 try-catch
 */
async function processTask(taskId: string): Promise<TaskResult> {
    try {
        const task = await taskRegistry.get(taskId);
        if (!task) {
            throw new TaskNotFoundError(taskId);
        }
        
        return await task.execute();
        
    } catch (error) {
        if (error instanceof AgentOSError) {
            logger.error(`Task ${taskId} failed: ${error.message}`, { code: error.code });
            throw error;
        }
        logger.error(`Unexpected error processing task ${taskId}`, error);
        throw new SchedulerError('Internal error');
    }
}
```

---

## 八、异步编程

### 8.1 Promise 使用

```typescript
/**
 * 异步任务执行
 */
async function executeTask(task: Task): Promise<TaskResult> {
    const startTime = Date.now();
    
    try {
        const result = await task.execute();
        return {
            taskId: task.id,
            status: 'completed',
            result,
            duration: Date.now() - startTime
        };
    } catch (error) {
        return {
            taskId: task.id,
            status: 'failed',
            error: error instanceof Error ? error.message : String(error),
            duration: Date.now() - startTime
        };
    }
}

/**
 * 并行执行多个任务
 */
async function executeParallel(tasks: Task[]): Promise<TaskResult[]> {
    return Promise.all(tasks.map(executeTask));
}

/**
 * 带超时的 Promise
 */
function withTimeout<T>(
    promise: Promise<T>,
    timeoutMs: number,
    errorMessage: string = 'Operation timed out'
): Promise<T> {
    return Promise.race([
        promise,
        new Promise<T>((_, reject) =>
            setTimeout(() => reject(new Error(errorMessage)), timeoutMs)
        )
    ]);
}
```

### 8.2 async/await 模式

```typescript
/**
 * 顺序执行任务
 */
async function executeSequential(tasks: Task[]): Promise<TaskResult[]> {
    const results: TaskResult[] = [];
    
    for (const task of tasks) {
        const result = await executeTask(task);
        results.push(result);
        
        // 如果任务失败，可以选择停止
        if (result.status === 'failed' && task.critical) {
            break;
        }
    }
    
    return results;
}

/**
 * 并行执行并限制并发数
 */
async function executeWithConcurrency(
    tasks: Task[],
    concurrency: number
): Promise<TaskResult[]> {
    const results: TaskResult[] = [];
    const executing: Promise<void>[] = [];
    
    for (const task of tasks) {
        const promise = executeTask(task).then(result => {
            results.push(result);
        });
        
        executing.push(promise);
        
        if (executing.length >= concurrency) {
            await Promise.race(executing);
            executing.splice(
                executing.findIndex(p => p === promise),
                1
            );
        }
    }
    
    await Promise.all(executing);
    return results;
}
```

---

## 九、模块与导入

### 9.1 导入语句顺序

```typescript
// 1. Node.js 内置模块
import { EventEmitter } from 'events';
import { readFile, writeFile } from 'fs/promises';
import { join } from 'path';

// 2. 外部包
import express, { Request, Response } from 'express';
import { z } from 'zod';
import { Logger } from 'winston';

// 3. 项目内部模块（相对于当前文件）
import { TaskScheduler } from './task-scheduler';
import { TaskPlan, TaskResult } from '../types/task.types';
import { ValidationError } from '../errors';
import { DEFAULT_CONFIG } from '../constants';

// 4. 类型导入（始终使用 type 导入）
import type { Config, Options } from '../types';
import type { Logger } from 'winston';
```

### 9.2 导出模式

```typescript
// named export（命名导出）
export const MAX_RETRY_COUNT = 3;
export interface TaskConfig { ... }
export function submitTask() { ... }

// re-export（重新导出）
export { TaskScheduler } from './task-scheduler';
export type { TaskPlan, TaskResult } from '../types';

// default export（默认导出）
export default class TaskScheduler { ... }

// Barrel 导出（index.ts）
export { TaskScheduler } from './task-scheduler';
export { MemoryManager } from './memory-manager';
export { CognitionEngine } from './cognition-engine';
```

---

## 十、注释规范

### 10.1 JSDoc 注释

```typescript
/**
 * @fileoverview 任务调度器模块
 *
 * 提供任务调度核心功能，包括任务提交、状态管理、
 * 优先级队列和依赖解析。调度器采用双系统架构：
 * - System 1：快速路径，处理简单任务
 * - System 2：深度路径，处理复杂任务
 *
 * @module agentos/scheduler
 */

/**
 * 提交任务计划
 *
 * 将任务计划加入调度队列，返回任务 ID 用于后续查询。
 * 调度器会根据优先级和依赖关系决定执行顺序。
 *
 * @param plan - 任务计划对象，非空
 * @param options - 提交选项，可选
 * @returns Promise 解析为任务 ID
 *
 * @throws {ValidationError} 当计划格式无效
 * @throws {SchedulerClosedError} 当调度器已关闭
 * @throws {QueueFullError} 当队列已满
 *
 * @example
 * ```typescript
 * const taskId = await scheduler.submit({
 *     name: 'data-processing',
 *     steps: [
 *         { id: 'step1', name: 'Load Data', handler: loadData },
 *         { id: 'step2', name: 'Process', handler: processData, dependencies: ['step1'] }
 *     ],
 *     strategy: 'sequential'
 * });
 * ```
 *
 * @see {@link TaskScheduler.wait} 获取任务结果
 * @see {@link TaskScheduler.cancel} 取消任务
 */
async submit(
    plan: TaskPlan,
    options?: SubmitOptions
): Promise<string> {
    // 实现...
}
```

### 10.2 TSDoc 类型注释

```typescript
/**
 * 任务计划
 *
 * @template T - 任务输入数据类型
 * @template R - 任务输出数据类型
 */
export interface TaskPlan<T = unknown, R = unknown> {
    /** 任务唯一标识符 */
    readonly id: string;
    
    /** 任务名称，用于日志和监控 */
    name: string;
    
    /** 执行步骤列表 */
    steps: readonly TaskStep[];
    
    /** 输入数据 */
    input: T;
    
    /** 执行策略 */
    strategy: 'sequential' | 'parallel' | 'adaptive';
}
```

---

## 十一、类型安全

### 11.1 严格模式

```json
// tsconfig.json
{
    "compilerOptions": {
        "strict": true,
        "noImplicitAny": true,
        "strictNullChecks": true,
        "strictFunctionTypes": true,
        "strictBindCallApply": true,
        "strictPropertyInitialization": true,
        "noImplicitThis": true,
        "useUnknownInCatchVariables": true,
        "alwaysStrict": true
    }
}
```

### 11.2 类型守卫

```typescript
/**
 * 类型守卫：检查是否为 AgentOS 错误
 */
function isAgentOSError(error: unknown): error is AgentOSError {
    return error instanceof AgentOSError;
}

/**
 * 类型守卫：检查任务计划
 */
function isValidTaskPlan(plan: unknown): plan is TaskPlan {
    if (plan === null || typeof plan !== 'object') {
        return false;
    }
    const p = plan as Record<string, unknown>;
    return (
        typeof p.id === 'string' &&
        typeof p.name === 'string' &&
        Array.isArray(p.steps)
    );
}

/**
 * 使用类型守卫
 */
async function handleError(error: unknown): Promise<void> {
    if (isAgentOSError(error)) {
        logger.error(`AgentOS error: ${error.code} - ${error.message}`);
        // error.code 是确定存在的
    } else if (error instanceof Error) {
        logger.error(`Unexpected error: ${error.message}`);
    }
}
```

---

## 十二、测试集成

### 12.1 Jest 测试示例

```typescript
/**
 * @fileoverview 任务调度器测试
 */

import { describe, it, expect, beforeEach, afterEach, jest } from '@jest/globals';
import { TaskScheduler } from '../task-scheduler';
import { TaskPlan, TaskStatus } from '../types';

describe('TaskScheduler', () => {
    let scheduler: TaskScheduler;
    
    beforeEach(() => {
        scheduler = new TaskScheduler({
            name: 'test',
            maxWorkers: 2
        });
    });
    
    afterEach(async () => {
        await scheduler.shutdown();
    });
    
    describe('submit', () => {
        it('should submit a task and return task id', async () => {
            const plan: TaskPlan = {
                id: 'task-001',
                name: 'test-task',
                steps: [],
                strategy: 'sequential'
            };
            
            const taskId = await scheduler.submit(plan);
            
            expect(taskId).toBe('task-001');
        });
        
        it('should throw ValidationError for invalid plan', async () => {
            const invalidPlan = { name: 'test' } as TaskPlan;
            
            await expect(scheduler.submit(invalidPlan))
                .rejects.toThrow('id');
        });
    });
    
    describe('wait', () => {
        it('should return task result after completion', async () => {
            const plan: TaskPlan = {
                id: 'task-002',
                name: 'completed-task',
                steps: [],
                strategy: 'sequential'
            };
            
            await scheduler.submit(plan);
            const result = await scheduler.wait('task-002', 5000);
            
            expect(result.status).toBe(TaskStatus.Completed);
        });
    });
});
```

---

## 十三、参考文献

1. **AgentOS 架构设计原则**: [architectural_design_principles.md](../../architecture/folder/architectural_design_principles.md)
2. **Google TypeScript Style Guide**: https://google.github.io/styleguide/tsguide.html
3. **TypeScript Documentation**: https://www.typescriptlang.org/docs/
4. **Airbnb JavaScript Style Guide**: https://github.com/airbnb/javascript

---

© 2026 SPHARX Ltd. All Rights Reserved.  
"From data intelligence emerges."