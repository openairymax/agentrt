# Syscall — 系统调用层

`agentos/atoms/syscall/`

**版本**: v0.0.5 | **API 版本**: 1.0.0

---

## 概述

Syscall 层是 AgentOS 中**用户态与内核态之间的唯一通道**，提供统一的系统调用抽象接口。它将底层硬件的异构性和操作系统的差异性封装为标准的 API 集合，确保上层模块能以统一的方式访问系统资源。Syscall 层不仅提供 5 类标准系统调用（任务、内存、会话、遥测、代理），还包含 Skill 管理子系统、沙箱隔离机制、熔断器和速率限制器，构成完整的安全防护体系。

Syscall 层以 C11 标准实现，所有接口设计为线程安全、可重入，通过 C ABI 导出支持跨语言 FFI 调用。调用流程遵循严格的权限检查和参数校验，所有调用记录审计日志。

---

## 设计原则

| 原则 | 说明 |
|------|------|
| **统一抽象** | 所有系统资源通过统一的调用接口访问 |
| **线程安全** | 所有接口设计为可重入、线程安全 |
| **最小权限** | 每个调用携带调用方身份，内核进行权限校验 |
| **零拷贝** | 核心数据路径避免不必要的数据复制 |
| **异步兼容** | 同步接口与异步回调机制并存 |
| **跨语言 FFI** | C ABI 导出，支持跨语言调用 |
| **安全防护** | 沙箱隔离、熔断器、速率限制三重保护 |

---

## 目录结构

```
syscall/
├── include/
│   └── syscalls.h                # 系统调用公共接口
├── src/
│   ├── syscall_entry.c           # 系统调用入口（初始化/清理）
│   ├── syscall_table.c           # 系统调用分发表
│   ├── task.c                    # 任务管理调用实现
│   ├── memory.c                  # 内存管理调用实现
│   ├── session.c                 # 会话管理调用实现
│   ├── telemetry.c               # 遥测调用实现
│   ├── agent.c                   # Agent 管理调用实现
│   ├── skill.c                   # Skill 管理调用实现
│   ├── sandbox.c                 # 沙箱隔离核心
│   ├── sandbox_permission.c      # 沙箱权限管理
│   ├── sandbox_quota.c           # 沙箱配额管理
│   ├── sandbox_utils.c           # 沙箱工具函数
│   ├── sandbox_internal.h        # 沙箱内部头文件
│   ├── sandbox_permission.h      # 沙箱权限头文件
│   ├── sandbox_quota.h           # 沙箱配额头文件
│   ├── sandbox_utils.h           # 沙箱工具头文件
│   ├── circuit_breaker.c         # 熔断器实现
│   └── rate_limiter.c            # 速率限制器实现
├── tests/
│   ├── test_syscalls.c           # 系统调用基础测试
│   ├── test_syscall_extended.c   # 扩展测试（沙箱/熔断/限流）
│   └── CMakeLists.txt
└── CMakeLists.txt
```

---

## 接口体系

### 1. 任务管理调用

| 接口 | 功能 | 参数 |
|------|------|------|
| `agentos_sys_task_submit()` | 提交任务到内核 | 输入数据（JSON）、超时时间 |
| `agentos_sys_task_query()` | 查询任务状态 | 任务 ID |
| `agentos_sys_task_wait()` | 等待任务完成 | 任务 ID、超时时间 |
| `agentos_sys_task_cancel()` | 取消任务 | 任务 ID |

### 2. 内存管理调用

| 接口 | 功能 | 参数 |
|------|------|------|
| `agentos_sys_memory_write()` | 写入记忆数据 | 数据指针、长度、元数据（JSON） |
| `agentos_sys_memory_search()` | 搜索记忆 | 查询字符串、返回数量限制 |
| `agentos_sys_memory_get()` | 获取记忆记录 | 记录 ID |
| `agentos_sys_memory_delete()` | 删除记忆记录 | 记录 ID |

### 3. 会话管理调用

| 接口 | 功能 | 参数 |
|------|------|------|
| `agentos_sys_session_create()` | 创建会话 | 元数据（JSON，可为 NULL） |
| `agentos_sys_session_get()` | 获取会话信息 | 会话 ID |
| `agentos_sys_session_close()` | 关闭会话 | 会话 ID |
| `agentos_sys_session_list()` | 列出所有会话 | — |
| `agentos_sys_session_get_persist_status()` | 获取会话持久化状态 | 会话 ID |

**会话持久化状态**:

| 状态 | 说明 |
|------|------|
| `SESSION_PERSIST_UNKNOWN` | 未知状态 |
| `SESSION_PERSIST_PENDING` | 等待持久化 |
| `SESSION_PERSIST_SUCCESS` | 持久化成功 |
| `SESSION_PERSIST_FAILED` | 持久化失败 |
| `SESSION_PERSIST_DISABLED` | 持久化禁用 |

### 4. 遥测调用

| 接口 | 功能 | 参数 |
|------|------|------|
| `agentos_sys_telemetry_metrics()` | 获取系统指标 | — |
| `agentos_sys_telemetry_traces()` | 获取链路追踪 | 追踪 ID |

### 5. Agent 管理调用

| 接口 | 功能 | 参数 |
|------|------|------|
| `agentos_sys_agent_spawn()` | 创建 Agent 实例 | Agent 规格（JSON） |
| `agentos_sys_agent_terminate()` | 销毁 Agent 实例 | Agent ID |
| `agentos_sys_agent_invoke()` | 调用 Agent 执行任务 | Agent ID、输入数据 |
| `agentos_sys_agent_list()` | 列出所有 Agent | — |

### 6. Skill 管理调用

| 接口 | 功能 | 参数 |
|------|------|------|
| `agentos_sys_skill_install()` | 安装技能 | 技能 URL（file:// 或 http://） |
| `agentos_sys_skill_execute()` | 执行技能 | 技能 ID、输入数据 |
| `agentos_sys_skill_list()` | 列出已安装技能 | — |
| `agentos_sys_skill_uninstall()` | 卸载技能 | 技能 ID |

### 7. 辅助函数

| 接口 | 功能 |
|------|------|
| `agentos_sys_free()` | 释放系统调用分配的内存 |
| `agentos_syscalls_init()` | 初始化系统调用层 |
| `agentos_syscalls_cleanup()` | 清理系统调用层资源 |

---

## 安全机制

Syscall 层实现了四层安全防护：

### 沙箱隔离 (Sandbox)

沙箱子系统为每个调用提供隔离执行环境：

| 组件 | 源文件 | 功能 |
|------|--------|------|
| 沙箱核心 | `sandbox.c` | 沙箱创建、销毁、执行隔离 |
| 权限管理 | `sandbox_permission.c` | 文件/网络/系统调用权限控制 |
| 配额管理 | `sandbox_quota.c` | CPU/内存/时间配额限制 |
| 工具函数 | `sandbox_utils.c` | 沙箱辅助工具 |

### 熔断器 (Circuit Breaker)

`circuit_breaker.c` 实现自动熔断和恢复机制，防止级联故障：

- **关闭状态**: 正常调用
- **打开状态**: 熔断中，直接返回错误
- **半开状态**: 试探性放行，检测恢复

### 速率限制器 (Rate Limiter)

`rate_limiter.c` 实现调用频率控制，防止 DoS 攻击：

- 令牌桶算法
- 可配置的速率和突发容量
- 按调用方身份独立限流

### 审计日志

所有系统调用记录审计日志，支持事后追溯。

---

## 调用流程

```
调用方 → 用户态 API → Syscall 入口 → 权限检查 → 参数校验 → 沙箱检查 → 内核处理 → 返回结果
                                    ↓
                              速率限制检查
                                    ↓
                              熔断器检查
```

```c
agentos_error_t ret = agentos_sys_task_submit(
    input_json, input_len, timeout_ms, &output);
```

---

## 依赖关系

| 依赖项 | 来源 | 用途 |
|--------|------|------|
| CoreKern | atoms/corekern | 微内核 IPC 和调度能力 |
| commons | agentos/commons | 统一类型和平台抽象 |
| agentos_types.h | commons | 统一类型定义 |

---

## 构建说明

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

cmake -B build -DBUILD_TESTS=ON
ctest --test-dir build -R syscall
```

---

## 使用示例

```c
#include "syscalls.h"

int main(void) {
    agentos_syscalls_init();

    char *session_id;
    agentos_sys_session_create(NULL, &session_id);

    char *output;
    agentos_sys_task_submit(
        "{\"action\":\"analyze\",\"target\":\"document.pdf\"}",
        44, 30000, &output);
    agentos_sys_free(output);

    char *record_id;
    agentos_sys_memory_write("sample data", 11,
                             "{\"type\":\"text\"}", &record_id);
    agentos_sys_free(record_id);

    char *agent_id;
    agentos_sys_agent_spawn(
        "{\"role\":\"analyst\",\"model\":\"gpt-4\"}", &agent_id);
    agentos_sys_free(agent_id);

    agentos_sys_session_close(session_id);
    agentos_sys_free(session_id);
    agentos_syscalls_cleanup();
    return 0;
}
```

---

## 与相关模块的关系

- **CoreKern**: Syscall 是 CoreKern 暴露给上层的能力接口
- **CoreLoopThree**: 通过 Syscall 获取系统资源和执行系统操作
- **TaskFlow**: 通过 Syscall 的任务调用管理任务生命周期
- **Daemon 服务**: 各守护进程通过 Syscall 访问系统级能力

---

© 2026 SPHARX Ltd. All Rights Reserved.
