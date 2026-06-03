# Toolkit TypeScript — AgentOS TypeScript SDK

**模块路径**: `agentos/toolkit/typescript/`
**版本**: v0.1.0 (SDK v0.1.0)

## 概述

AgentOS TypeScript SDK 提供基于 TypeScript 的 AgentOS 系统编程接口，适用于 Node.js 和浏览器环境。SDK 采用模块化设计，包含客户端层、业务模块层（Task/Memory/Session/Skill）、系统调用绑定、遥测和插件系统，与 Python/Go/Rust SDK 保持 API 一致性。提供完整的类型定义和 ES Module 支持。

## 目录结构

```
typescript/
├── src/
│   ├── index.ts                # 模块入口，导出所有公共 API
│   ├── agentos.ts              # AgentOS 主类 / createAgentOS 工厂
│   ├── manager.ts              # 配置管理（ConfigOption/环境变量）
│   ├── config.ts               # 配置类型定义
│   ├── errors.ts               # 错误定义与错误码
│   ├── protocol.ts             # 协议处理
│   ├── syscall.ts              # 系统调用绑定
│   ├── telemetry.ts            # OpenTelemetry 遥测
│   ├── plugin.ts               # 插件系统
│   ├── task.ts                 # Task 领域模型
│   ├── memory.ts               # Memory 领域模型
│   ├── session.ts              # Session 领域模型
│   ├── skill.ts                # Skill 领域模型
│   ├── client/
│   │   ├── index.ts            # 客户端导出
│   │   ├── client.ts           # Client/APIClient 实现
│   │   └── mock.ts             # MockClient 测试客户端
│   ├── modules/
│   │   ├── index.ts            # 模块导出
│   │   ├── base_manager.ts     # BaseManager 基类
│   │   ├── task.ts             # TaskManager
│   │   ├── memory.ts           # MemoryManager / MemoryWriteItem
│   │   ├── session.ts          # SessionManager
│   │   └── skill.ts            # SkillManager / SkillExecuteRequest
│   ├── types/
│   │   ├── index.ts            # 类型导出
│   │   ├── enums.ts            # 枚举类型
│   │   ├── models.ts           # 领域模型
│   │   └── requests.ts         # 请求/响应类型
│   └── utils/
│       ├── index.ts            # 工具函数导出
│       ├── helpers.ts          # 通用工具函数
│       └── logger.ts           # 日志工具
├── tests/                      # 测试套件
│   ├── base_manager.test.ts    # BaseManager 测试
│   ├── client.test.ts          # 客户端测试
│   ├── config.test.ts          # 配置测试
│   ├── helpers.test.ts         # 工具函数测试
│   ├── logger.test.ts          # 日志测试
│   ├── syscall.test.ts         # 系统调用测试
│   ├── telemetry.test.ts       # 遥测测试
│   ├── plugin.test.ts          # 插件测试
│   ├── benchmark_performance.test.ts  # 性能基准测试
│   └── test_comprehensive.test.ts     # 综合测试
├── package.json                # NPM 配置
├── tsconfig.json               # TypeScript 配置
└── README.md                   # 本文件
```

## 核心组件

### AgentOS 主类

```typescript
import { AgentOS, createAgentOS } from 'agentos';

const client = new AgentOS({
    endpoint: 'http://localhost:18789',
    timeout: 30,
    apiKey: 'your-api-key',
});

const client2 = createAgentOS({
    endpoint: 'http://localhost:18789',
});
```

### 业务模块层

| 管理器 | 说明 | 核心方法 |
|--------|------|----------|
| `TaskManager` | 任务管理 | submit/get/cancel/list/wait |
| `MemoryManager` | 记忆管理 | write/read/search/delete/list |
| `SessionManager` | 会话管理 | create/get/close/list |
| `SkillManager` | 技能管理 | load/execute/unload/list |

### 类型系统

#### 枚举类型 (`types/enums.ts`)

| 枚举 | 值 |
|------|-----|
| `TaskStatus` | PENDING/RUNNING/COMPLETED/FAILED/CANCELLED |
| `MemoryLayer` | L1/L2/L3/L4 |
| `SessionStatus` | ACTIVE/EXPIRED/CLOSED |
| `SkillStatus` | LOADED/EXECUTING/COMPLETED/FAILED |
| `SpanStatus` | OK/ERROR/UNSET |

#### 领域模型 (`types/models.ts`)

| 模型 | 字段 |
|------|------|
| `Task` | taskId/description/status/result/createdAt/updatedAt |
| `TaskResult` | success/output/error/metrics |
| `Memory` | memoryId/content/createdAt/metadata |
| `MemorySearchResult` | memory/score/highlight |
| `Session` | sessionId/status/createdAt/metadata |
| `Skill` | skillId/name/status/capabilities |
| `SkillResult` | success/output/error/executionTime |

### 配置管理

```typescript
import {
    newConfig, newConfigFromEnv, defaultConfig,
    withEndpoint, withTimeout, withMaxRetries,
    withAPIKey, withUserAgent, withDebug,
    withLogLevel, withMaxConnections, withHeaders,
    validateConfig, cloneConfig, mergeConfig,
} from 'agentos';

const config = newConfig(
    withEndpoint('http://localhost:18789'),
    withTimeout(30000),
    withAPIKey('your-key'),
    withDebug(true),
);

const envConfig = newConfigFromEnv();
```

### 系统调用绑定

| 类型 | 说明 |
|------|------|
| `SyscallBinding` | 系统调用绑定接口 |
| `HttpSyscallBinding` | HTTP 系统调用实现 |
| `TaskSyscall` / `MemorySyscall` / `SessionSyscall` / `SkillSyscall` / `AgentSyscall` | 各模块系统调用 |

## 接口说明

### TaskManager

```typescript
const taskMgr = client.taskManager();

const task = await taskMgr.submit('analyze data');
const result = await taskMgr.wait(task.id, 30000);
const tasks = await taskMgr.list({ limit: 10 });
await taskMgr.cancel(taskId);
```

### MemoryManager

```typescript
const memoryMgr = client.memoryManager();

const memoryId = await memoryMgr.write('content', { tag: 'important' });
const memories = await memoryMgr.search('query', 5);
const memory = await memoryMgr.read(memoryId);
await memoryMgr.delete(memoryId);
```

### SessionManager

```typescript
const sessionMgr = client.sessionManager();

const session = await sessionMgr.create();
const existing = await sessionMgr.get(sessionId);
await sessionMgr.close(sessionId);
```

### SkillManager

```typescript
const skillMgr = client.skillManager();

const skill = await skillMgr.load('browser-skill');
const result = await skillMgr.execute({
    skillId: skill.id,
    params: { url: 'https://example.com' },
});
await skillMgr.unload(skill.id);
```

## 依赖关系

- **Node.js**: >= 18.0.0
- **核心依赖**: node-fetch, events
- **开发依赖**: typescript, jest, ts-jest, @types/node
- **运行时**: 支持 Node.js 和浏览器环境

## 构建说明

```bash
# 安装依赖
npm install

# 编译 TypeScript
npm run build

# 运行测试
npm test

# 运行特定测试
npm test -- --testPathPattern=client

# 运行基准测试
npm run test:benchmark

# 代码检查
npm run lint
```

## 使用示例

### 基本使用

```typescript
import { AgentOS } from 'agentos';

const client = new AgentOS({ endpoint: 'http://localhost:18789' });

const task = await client.taskManager().submit('Generate a report');
const result = await client.taskManager().wait(task.id, 60000);
console.log('Result:', result.output);

await client.close();
```

### 使用工厂函数

```typescript
import { createAgentOS } from 'agentos';

const client = createAgentOS({
    endpoint: 'http://localhost:18789',
    apiKey: process.env.AGENTOS_API_KEY,
    timeout: 30000,
});

const memoryId = await client.memoryManager().write('Important data');
const results = await client.memoryManager().search('data', 5);
```

### 使用系统调用

```typescript
import { HttpSyscallBinding, TaskSyscall } from 'agentos';

const binding = new HttpSyscallBinding('http://localhost:18789');
const taskSyscall = new TaskSyscall(binding);

const task = await taskSyscall.submit({ description: 'Process data' });
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
