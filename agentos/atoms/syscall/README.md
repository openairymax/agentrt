# Syscall — 系统调用层

**路径**: `agentos/atoms/syscall/`

Syscall 层是 AgentOS 中**用户态与内核态之间的唯一通道**，提供统一的系统调用抽象接口。它将底层硬件的异构性和操作系统的差异性封装为标准的 API 集合，确保上层模块能以统一的方式访问系统资源。

---

## 设计原则

- **统一抽象**: 所有系统资源通过统一的调用接口访问
- **线程安全**: 所有接口设计为可重入、线程安全
- **最小权限**: 每个调用携带调用方身份，内核进行权限校验
- **零拷贝**: 核心数据路径避免不必要的数据复制
- **异步兼容**: 同步接口与异步回调机制并存
- **跨语言 FFI**: C ABI 导出，支持跨语言调用

---

## 接口体系

Syscall 层提供 5 类标准系统调用接口：

### 1. 任务调用 (Task Calls)

任务生命周期管理和调度控制。

| 接口 | 功能 | 参数 |
|------|------|------|
| `task_create` | 创建新任务 | 入口函数、栈大小、优先级 |
| `task_schedule` | 调度任务执行 | 任务 ID、调度策略 |
| `task_suspend` | 挂起任务 | 任务 ID |
| `task_resume` | 恢复任务 | 任务 ID |
| `task_terminate` | 终止任务 | 任务 ID、退出码 |
| `task_yield` | 主动让出 CPU | 无 |
| `task_setprio` | 设置任务优先级 | 任务 ID、优先级 |

### 2. 内存调用 (Memory Calls)

内存分配、映射和释放。

| 接口 | 功能 | 参数 |
|------|------|------|
| `mem_alloc` | 分配内存 | 大小、对齐方式 |
| `mem_free` | 释放内存 | 指针 |
| `mem_map` | 映射虚拟内存 | 物理地址、大小、权限 |
| `mem_unmap` | 解除映射 | 虚拟地址 |
| `mem_share` | 创建共享内存 | 大小、键值 |
| `mem_protect` | 修改内存保护属性 | 地址、大小、权限 |

### 3. 会话调用 (Session Calls)

会话管理和上下文维护。

| 接口 | 功能 | 参数 |
|------|------|------|
| `session_create` | 创建新会话 | 会话类型、配置 |
| `session_destroy` | 销毁会话 | 会话 ID |
| `session_get` | 获取会话信息 | 会话 ID |
| `session_set` | 设置会话属性 | 会话 ID、键值对 |

### 4. 遥测调用 (Telemetry Calls)

系统监控、日志和性能数据采集。

| 接口 | 功能 | 参数 |
|------|------|------|
| `telemetry_log` | 写入日志 | 级别、消息、标签 |
| `telemetry_metric` | 上报指标 | 指标名、值、标签 |
| `telemetry_trace` | 创建追踪 | 操作名、父 Span ID |
| `telemetry_span_end` | 结束追踪 | Span ID |

### 5. 代理调用 (Agent Calls)

智能体代理相关的特定调用。

| 接口 | 功能 | 参数 |
|------|------|------|
| `agent_register` | 注册智能体 | 名称、能力声明 |
| `agent_unregister` | 注销智能体 | 代理 ID |
| `agent_discover` | 发现智能体 | 能力过滤条件 |
| `agent_invoke` | 调用智能体能力 | 代理 ID、能力名称、参数 |
| `agent_cancel` | 取消调用 | 调用 ID |

---

## 调用流程

```
调用方 → 用户态 API → Syscall 入口 → 权限检查 → 参数校验 → 内核处理 → 返回结果
```

```c
// 调用示例: 创建任务
syscall_ret_t ret = syscall_invoke(SYSCALL_TASK_CREATE, 3,
    (uint64_t)entry_func,   // arg1: 入口函数
    (uint64_t)stack_size,   // arg2: 栈大小
    (uint64_t)priority      // arg3: 优先级
);
```

---

## 安全机制

Syscall 层实现了多层安全防护：

| 安全层 | 说明 |
|--------|------|
| 参数校验 | 对所有输入参数进行边界检查和类型验证 |
| 权限检查 | 调用方身份验证，基于能力模型的访问控制 |
| 审计日志 | 所有系统调用记录审计日志，支持事后追溯 |
| 速率限制 | 防止 DoS 攻击的调用频率控制 |

---

## 与相关模块的关系

- **CoreKern**: Syscall 是 CoreKern 暴露给上层的能力接口
- **CoreLoopThree**: 通过 Syscall 获取系统资源和执行系统操作
- **TaskFlow**: 通过 Syscall 的任务调用管理任务生命周期
- **Daemon 服务**: 各守护进程通过 Syscall 访问系统级能力

---

© 2026 SPHARX Ltd. All Rights Reserved.
