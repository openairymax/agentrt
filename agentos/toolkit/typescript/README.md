# TypeScript SDK

`toolkit/typescript/` 是 AgentOS 的 TypeScript 语言 SDK，提供完整的 AgentOS 客户端功能。

## 版本

当前版本: **v1.0.0.9**

## 安装

```bash
cd toolkit/typescript
npm install
npm run build
```

## 结构

```
typescript/
├── src/                      # 源代码
│   ├── client/              # HTTP 客户端
│   ├── modules/             # 模块管理器
│   │   ├── task.ts         # 任务管理
│   │   ├── session.ts      # 会话管理
│   │   ├── memory.ts       # 内存管理
│   │   └── skill.ts        # 技能管理
│   ├── types/              # 类型定义
│   ├── utils/              # 工具函数
│   ├── agent.ts            # Agent 核心
│   ├── syscall.ts          # 系统调用接口
│   ├── protocol.ts         # 协议实现
│   └── index.ts            # 入口
├── tests/                    # 测试用例
├── package.json              # 项目配置
├── tsconfig.json             # TypeScript 配置
├── jest.config.js            # Jest 配置
└── README.md                 # 本文件
```

## 使用示例

```typescript
import { AgentOSClient } from '@agentos/sdk';

// 创建客户端
const client = new AgentOSClient({
    baseURL: 'http://localhost:8080'
});

// 创建 Agent
const agent = await client.createAgent('agent-001', 'example-agent');
console.log(`Agent created: ${agent.id}`);

// 提交任务
const task = await client.createTask(agent.id, 'example_task');
console.log(`Task created: ${task.id}`);

// 查询任务状态
const status = await client.getTaskStatus(task.id);
console.log(`Status: ${status}`);
```

## 运行测试

```bash
cd toolkit/typescript
npm test
```

---

*AgentOS Toolkit — TypeScript SDK*
