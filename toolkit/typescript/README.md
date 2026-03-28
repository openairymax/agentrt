# AgentOS TypeScript SDK

## 概述

AgentOS TypeScript SDK 提供了一个类型安全、现代化的接口，用于与 AgentOS 系统进行交互。该 SDK 支持任务管理、记忆操作、会话管理和技能加载等功能，并提供了完整的 TypeScript 类型定义。

## 安装

### 使用 npm

```bash
npm install @agentos/sdk
```

### 使用 yarn

```bash
yarn add @agentos/sdk
```

### 使用 pnpm

```bash
pnpm add @agentos/sdk
```

## 快速开始

### 基本用法

```typescript
import { AgentOS } from '@agentos/sdk';

async function main() {
  const client = new AgentOS({
    endpoint: 'http://localhost:18789'
  });
  
  // 提交任务
  const task = await client.submitTask('分析销售数据');
  console.log(`Task ID: ${task.id}`);
  
  // 等待完成
  const result = await task.wait({ timeout: 30000 });
  console.log(`Result: ${result.output}`);
  
  // 写入记忆
  const memoryId = await client.writeMemory('用户偏好使用 TypeScript');
  console.log(`Memory ID: ${memoryId}`);
  
  // 搜索记忆
  const memories = await client.searchMemory('TypeScript 编程', { limit: 5 });
  memories.forEach(mem => {
    console.log(`- ${mem.content}`);
  });
}

main();
```

### 高级用法

```typescript
import { AgentOS } from '@agentos/sdk';

async function advancedExample() {
  const client = new AgentOS({
    endpoint: 'http://localhost:18789',
    timeout: 60000, // 60秒超时
    headers: {
      'X-Custom-Header': 'value'
    }
  });
  
  try {
    // 创建会话
    const session = await client.createSession();
    console.log(`Session ID: ${session.id}`);
    
    // 设置会话上下文
    await session.setContext('user_id', '12345');
    await session.setContext('preferences', {
      language: 'TypeScript',
      theme: 'dark'
    });
    
    // 获取会话上下文
    const userId = await session.getContext('user_id');
    const preferences = await session.getContext('preferences');
    console.log(`User ID: ${userId}`);
    console.log(`Preferences: ${JSON.stringify(preferences)}`);
    
    // 加载技能
    const skill = await client.loadSkill('browser_skill');
    
    // 获取技能信息
    const skillInfo = await skill.getInfo();
    console.log(`Skill: ${skillInfo.skillName} v${skillInfo.version}`);
    console.log(`Description: ${skillInfo.description}`);
    
    // 执行技能
    const skillResult = await skill.execute({
      url: 'https://example.com',
      timeout: 10000
    });
    
    if (skillResult.success) {
      console.log(`Skill executed successfully: ${JSON.stringify(skillResult.output)}`);
    } else {
      console.log(`Skill execution failed: ${skillResult.error}`);
    }
    
    // 关闭会话
    await session.close();
    console.log('Session closed');
    
  } catch (error) {
    console.error('Error:', error);
  }
}

advancedExample();
```

## API 文档

### AgentOS 类

#### 初始化

```typescript
constructor(manager?: ClientConfig);

interface ClientConfig {
  endpoint?: string;      // AgentOS 服务器端点，默认为 'http://localhost:18789'
  timeout?: number;       // 请求超时时间（毫秒），默认为 30000
  headers?: Record<string, string>;  // 自定义请求头
}
```

#### submitTask

```typescript
async submitTask(taskDescription: string): Promise<Task>;

// 参数:
//   taskDescription: 任务描述
// 返回:
//   Task 对象，表示提交的任务
```

#### writeMemory

```typescript
async writeMemory(content: string, metadata?: Record<string, any>): Promise<string>;

// 参数:
//   content: 记忆内容
//   metadata: 可选的元数据
// 返回:
//   记忆 ID
```

#### searchMemory

```typescript
async searchMemory(query: string, options?: { limit?: number }): Promise<Memory[]>;

// 参数:
//   query: 搜索查询
//   options:
//     limit: 返回结果的最大数量，默认为 5
// 返回:
//   Memory 对象列表
```

#### getMemory

```typescript
async getMemory(memoryId: string): Promise<Memory>;

// 参数:
//   memoryId: 记忆 ID
// 返回:
//   Memory 对象
```

#### deleteMemory

```typescript
async deleteMemory(memoryId: string): Promise<boolean>;

// 参数:
//   memoryId: 记忆 ID
// 返回:
//   如果记忆删除成功，则为 true
```

#### createSession

```typescript
async createSession(): Promise<Session>;

// 返回:
//   Session 对象
```

#### loadSkill

```typescript
async loadSkill(skillName: string): Promise<Skill>;

// 参数:
//   skillName: 技能名称
// 返回:
//   Skill 对象
```

### Task 类

#### id

```typescript
get id(): string;

// 返回:
//   任务 ID
```

#### query

```typescript
async query(): Promise<TaskStatus>;

// 返回:
//   当前任务状态
```

#### wait

```typescript
async wait(options?: { timeout?: number }): Promise<TaskResult>;

// 参数:
//   options:
//     timeout: 最大等待时间（毫秒）
// 返回:
//   任务结果
```

#### cancel

```typescript
async cancel(): Promise<boolean>;

// 返回:
//   如果任务取消成功，则为 true
```

### Session 类

#### id

```typescript
get id(): string;

// 返回:
//   会话 ID
```

#### setContext

```typescript
async setContext(key: string, value: any): Promise<boolean>;

// 参数:
//   key: 上下文键
//   value: 上下文值
// 返回:
//   如果上下文设置成功，则为 true
```

#### getContext

```typescript
async getContext(key: string): Promise<any>;

// 参数:
//   key: 上下文键
// 返回:
//   上下文值
```

#### close

```typescript
async close(): Promise<boolean>;

// 返回:
//   如果会话关闭成功，则为 true
```

### Skill 类

#### name

```typescript
get name(): string;

// 返回:
//   技能名称
```

#### execute

```typescript
async execute(parameters?: Record<string, any>): Promise<SkillResult>;

// 参数:
//   parameters: 技能参数
// 返回:
//   技能执行结果
```

#### getInfo

```typescript
async getInfo(): Promise<SkillInfo>;

// 返回:
//   技能信息
```

## 错误处理

SDK 提供了以下错误类：

- `AgentOSError`: 基础错误类
- `NetworkError`: 网络相关错误
- `HttpError`: HTTP 错误
- `TimeoutError`: 超时错误
- `TaskError`: 任务相关错误
- `MemoryError`: 记忆相关错误
- `SessionError`: 会话相关错误
- `SkillError`: 技能相关错误

## 类型定义

### TaskStatus

```typescript
enum TaskStatus {
  PENDING = 'pending',
  RUNNING = 'running',
  COMPLETED = 'completed',
  FAILED = 'failed',
  CANCELLED = 'cancelled'
}
```

### TaskResult

```typescript
interface TaskResult {
  taskId: string;
  status: TaskStatus;
  output?: string;
  error?: string;
}
```

### Memory

```typescript
interface Memory {
  memoryId: string;
  content: string;
  createdAt: string;
  metadata?: Record<string, any>;
}
```

### SkillInfo

```typescript
interface SkillInfo {
  skillName: string;
  description: string;
  version: string;
  parameters?: Record<string, any>;
}
```

### SkillResult

```typescript
interface SkillResult {
  success: boolean;
  output?: any;
  error?: string;
}
```

## 示例

### 完整示例

```typescript
import { AgentOS, TaskStatus } from '@agentos/sdk';
import { AgentOSError, NetworkError, TimeoutError } from '@agentos/sdk';

async function completeExample() {
  try {
    // 初始化客户端
    const client = new AgentOS({
      endpoint: 'http://localhost:18789'
    });
    
    console.log('=== Task Management ===');
    // 提交任务
    const task = await client.submitTask('分析2026年第一季度销售数据');
    console.log(`Task ID: ${task.id}`);
    
    // 等待任务完成
    const taskResult = await task.wait({ timeout: 60000 });
    if (taskResult.status === TaskStatus.COMPLETED) {
      console.log(`Task completed successfully: ${taskResult.output}`);
    } else if (taskResult.status === TaskStatus.FAILED) {
      console.log(`Task failed: ${taskResult.error}`);
    } else if (taskResult.status === TaskStatus.CANCELLED) {
      console.log('Task was cancelled');
    }
    
    console.log('\n=== Memory Management ===');
    // 写入记忆
    const memoryId1 = await client.writeMemory('用户喜欢使用 TypeScript 进行前端开发');
    const memoryId2 = await client.writeMemory('用户对 React 和 Vue 框架有经验');
    console.log(`Created memories: ${memoryId1}, ${memoryId2}`);
    
    // 搜索记忆
    const memories = await client.searchMemory('TypeScript 框架');
    console.log(`Found ${memories.length} memories:`);
    memories.forEach((mem, index) => {
      console.log(`${index + 1}. ${mem.content}`);
    });
    
    // 获取记忆
    const memory = await client.getMemory(memoryId1);
    console.log(`Memory content: ${memory.content}`);
    
    // 删除记忆
    const deleted = await client.deleteMemory(memoryId2);
    console.log(`Memory deleted: ${deleted}`);
    
    console.log('\n=== Session Management ===');
    // 创建会话
    const session = await client.createSession();
    console.log(`Session ID: ${session.id}`);
    
    // 设置会话上下文
    await session.setContext('user_id', '67890');
    await session.setContext('language', 'TypeScript');
    
    // 获取会话上下文
    const userId = await session.getContext('user_id');
    const language = await session.getContext('language');
    console.log(`User ID: ${userId}`);
    console.log(`Language: ${language}`);
    
    // 关闭会话
    const closed = await session.close();
    console.log(`Session closed: ${closed}`);
    
    console.log('\n=== Skill Management ===');
    // 加载技能
    const skill = await client.loadSkill('browser_skill');
    console.log(`Loaded skill: ${skill.name}`);
    
    // 获取技能信息
    const skillInfo = await skill.getInfo();
    console.log(`Skill version: ${skillInfo.version}`);
    console.log(`Skill description: ${skillInfo.description}`);
    
    // 执行技能
    const skillResult = await skill.execute({
      url: 'https://example.com',
      actions: ['extract_title', 'extract_links']
    });
    
    if (skillResult.success) {
      console.log('Skill executed successfully:');
      console.log(JSON.stringify(skillResult.output, null, 2));
    } else {
      console.log(`Skill execution failed: ${skillResult.error}`);
    }
    
  } catch (error) {
    if (error instanceof NetworkError) {
      console.error('Network error:', error.message);
    } else if (error instanceof TimeoutError) {
      console.error('Timeout error:', error.message);
    } else if (error instanceof AgentOSError) {
      console.error('AgentOS error:', error.message);
    } else {
      console.error('Unknown error:', error);
    }
  }
}

completeExample();
```

## 浏览器使用

AgentOS TypeScript SDK 也可以在浏览器环境中使用：

```typescript
// 在浏览器中使用
import { AgentOS } from '@agentos/sdk';

async function browserExample() {
  const client = new AgentOS({
    endpoint: 'http://localhost:18789'
  });
  
  try {
    // 提交任务
    const task = await client.submitTask('分析数据');
    document.getElementById('taskId').textContent = task.id;
    
    // 等待任务完成
    const result = await task.wait();
    document.getElementById('result').textContent = result.output || result.error;
    
  } catch (error) {
    document.getElementById('error').textContent = error.message;
  }
}

document.getElementById('submit').addEventListener('click', browserExample);
```

## 版本信息

- **版本**: 1.0.0.5
- **最后更新**: 2026-03-21
- **许可证**: Apache License 2.0
