Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS API 参考文档

**版本**: Doc V1.7  
**最后更新**: 2026-03-31  
**状态**: 🟢 生产就绪  
**作者**: LirenWang

---

## 🎯 概述

AgentOS 提供多层次的 API 接口，从底层的系统调用到高级的 SDK 封装，满足不同开发场景的需求。所有 API 遵循统一的契约化设计原则，确保接口的稳定性、安全性和可观测性。API 设计完全基于五维正交原则体系，将架构设计哲学深度融入接口定义之中。

### API 层次结构

```
┌─────────────────────────────────────────┐
│          应用层 API (SDK)                │
│  • Go SDK • Python SDK • Rust SDK       │
│  • TypeScript SDK • 高级抽象接口         │
├─────────────────────────────────────────┤
│          系统调用 API (Syscall)          │
│  • 任务管理 • 记忆管理 • 会话管理         │
│  • 可观测性 • Agent 管理                 │
├─────────────────────────────────────────┤
│          内核 API (Core)                 │
│  • IPC Binder • 内存管理 • 任务调度       │
│  • 时间服务 • 安全穹顶接口               │
└─────────────────────────────────────────┘
```

## 🔬 API 设计哲学：五维正交原则体系

AgentOS API 设计基于五维正交系统，每个维度对应一类核心设计原则，共同构成 API 设计的完整哲学体系：

| 维度 | 核心问题 | API 设计体现 | 典型实现 |
|------|----------|-------------|---------|
| **系统观 (System)** | API 如何体现复杂系统的反馈闭环和层次分解？ | 统一的错误码体系、状态反馈机制、分层抽象设计 | 错误码映射、状态查询接口、API 层次结构 |
| **内核观 (Kernel)** | API 如何体现微内核的极简和契约化？ | 极简的 C 函数接口、严格的参数契约、清晰的线程安全声明 | 系统调用函数签名、所有权语义注解 |
| **认知观 (Cognition)** | API 如何支持双系统认知协同？ | System 1 快速路径接口、System 2 深度分析接口、记忆卷载机制 | 快速任务提交 API、语义检索 API、记忆进化接口 |
| **工程观 (Engineering)** | API 如何保证可维护性、安全性和可观测性？ | 安全内生设计、统一的可观测性集成、跨平台一致性 | 权限检查、结构化日志、指标收集、多平台适配 |
| **设计美学 (Aesthetics)** | API 如何做到既正确又优雅？ | 一致的命名规范、清晰的文档结构、人性化的错误信息 | 统一的前缀规则、自动文档生成、用户友好的错误提示 |

### 五维正交原则在 API 中的具体体现

#### 1. 系统观 → 反馈闭环与层次分解
- **反馈机制**: 所有操作返回明确状态码，支持增量结果查询
- **层次分离**: 内核 API、系统调用、SDK 三层职责清晰分离
- **状态管理**: 统一的资源状态机，避免状态不一致

#### 2. 内核观 → 接口契约与极简设计
- **契约化设计**: 每个 API 有明确的输入输出契约
- **极简主义**: 仅暴露必要的参数，避免功能膨胀
- **服务隔离**: 模块化接口设计，降低耦合度

#### 3. 认知观 → 双系统协同与记忆进化
- **双系统路径**: System 1 快速接口（<10ms）、System 2 深度接口（<500ms）
- **记忆卷载**: 支持 L1→L4 逐层记忆操作和查询
- **注意力机制**: 可配置的注意力分配策略接口

#### 4. 工程观 → 安全内生与可观测性
- **安全内生**: 权限检查、输入净化、审计追踪内置到每个 API
- **可观测性**: 自动的日志、指标、追踪数据生成
- **错误可追溯**: 完整的错误上下文和调用链信息

#### 5. 设计美学 → 优雅的用户体验
- **一致性原则**: 跨语言 SDK 接口命名和结构一致
- **文档即代码**: API 文档与代码实现同步更新
- **人性化设计**: 有意义的错误信息，完整的示例代码

---


## 📚 文档索引

### 1. 系统调用 API (Syscall)

系统调用是用户态服务与内核之间的标准化接口，所有守护进程必须通过系统调用与内核交互。

| 模块 | 文档 | 状态 |
|------|------|------|
| **任务管理** | [syscalls/task.md](syscalls/task.md) | ✅ 生产就绪 |
| **记忆管理** | [syscalls/memory.md](syscalls/memory.md) | ✅ 生产就绪 |
| **会话管理** | [syscalls/session.md](syscalls/session.md) | ✅ 生产就绪 |
| **可观测性** | [syscalls/telemetry.md](syscalls/telemetry.md) | ✅ 生产就绪 |
| **Agent 管理** | [syscalls/agent.md](syscalls/agent.md) | ✅ 生产就绪 |

### 2. 多语言 SDK

AgentOS 提供原生多语言 SDK，支持快速集成和开发。

| 语言 | 文档 | 状态 | 特性 |
|------|------|------|------|
| **Python SDK** | [agentos/toolkit/python/README.md](agentos/toolkit/python/README.md) | ✅ 生产就绪 | 易用性、异步支持、类型注解 |
| **Rust SDK** | [agentos/toolkit/rust/README.md](agentos/toolkit/rust/README.md) | ✅ 生产就绪 | 内存安全、零成本抽象、FFI 优化 |
| **Go SDK** | [agentos/toolkit/go/README.md](agentos/toolkit/go/README.md) | ✅ 生产就绪 | 高性能、并发安全、完整接口 |
| **TypeScript SDK** | [agentos/toolkit/typescript/README.md](agentos/toolkit/typescript/README.md) | ✅ 生产就绪 | 完整类型支持、前端集成、异步API |

### 3. 核心算法与实现

AgentOS API 背后是高效的核心算法支撑，涵盖文档处理、搜索索引、质量验证和性能优化等关键领域。

| 模块 | 文档 | 状态 | 说明 |
|------|------|------|------|
| **算法实现文档** | [algorithms/README.md](algorithms/README.md) | ✅ 生产就绪 | 详细的核心算法设计与实现逻辑 |

### 4. 内核 API (Core)

内核 API 提供底层系统服务，通常由系统调用层封装后暴露给用户态。详细文档请参阅架构设计文档。

| 模块 | 架构文档 | 状态 |
|------|----------|------|
| **IPC Binder** | [../architecture/ipc.md](../architecture/ipc.md) | ✅ 生产就绪 |
| **内存管理** | [../architecture/memoryrovol.md](../architecture/memoryrovol.md) | ✅ 生产就绪 |
| **任务调度** | [../architecture/coreloopthree.md](../architecture/coreloopthree.md) | ✅ 生产就绪 |
| **微内核** | [../architecture/microkernel.md](../architecture/microkernel.md) | ✅ 生产就绪 |
| **安全穹顶** | [../architecture/ARCHITECTURAL_PRINCIPLES.md](../architecture/ARCHITECTURAL_PRINCIPLES.md) | ✅ 生产就绪 |

---

## 🔧 API 设计原则

### 1. 契约化设计

所有 API 遵循严格的契约化设计，包含完整的元数据声明：

```c
/**
 * @brief 提交任务到调度队列
 * 
 * 将任务描述转换为任务图，并加入调度队列。
 * 任务 ID 通过 out_id 返回，用于后续查询和取消。
 * 
 * @param description UTF-8 编码的任务描述
 * @param out_id 输出参数，返回任务 ID
 * @return 0 成功；-EINVAL 参数错误；-ENOMEM 内存不足
 * 
 * @ownership out_id 由调用者负责释放
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_task_cancel(), agentos_task_query()
 */
AGENTOS_API int agentos_task_submit(const char* description, uint64_t* out_id);
```

### 2. 错误处理

统一的错误码体系和错误传播机制：

```c
// 错误码定义
#define AGENTOS_SUCCESS      0      // 成功
#define AGENTOS_EINVAL       -1     // 无效参数
#define AGENTOS_ENOMEM       -2     // 内存不足
#define AGENTOS_ENOENT       -3     // 不存在
#define AGENTOS_EPERM        -4     // 权限不足
#define AGENTOS_ETIMEDOUT    -5     // 超时
#define AGENTOS_EAGAIN       -6     // 重试
#define AGENTOS_ENOTSUP      -7     // 不支持

// 错误描述函数
const char* agentos_strerror(int errcode);
```

### 3. 线程安全与所有权

- **线程安全**: 所有公共 API 必须标注线程安全性
- **所有权语义**: 明确资源分配和释放责任
- **引用计数**: 复杂对象使用引用计数管理生命周期

### 4. 可观测性集成

所有 API 自动集成可观测性支持：

- **结构化日志**: 统一日志格式，TraceID 贯穿
- **指标收集**: 计数器、仪表、直方图
- **链路追踪**: OpenTelemetry 集成，全链路追踪

---

## 🚀 快速开始

### 使用 Go SDK

```go
package main

import (
    "context"
    "fmt"
    "log"
    
    "github.com/spharx/agentos-go"
)

func main() {
    // 初始化客户端
    client, err := agentos.NewClient(context.Background(), agentos.manager{
        Endpoint: "unix:///var/run/agentos.sock",
    })
    if err != nil {
        log.Fatal(err)
    }
    defer client.Close()
    
    // 提交任务
    taskID, err := client.Task.Submit(context.Background(), &agentos.TaskRequest{
        Description: "分析财务报表并生成摘要",
    })
    if err != nil {
        log.Fatal(err)
    }
    
    fmt.Printf("任务已提交，ID: %s\n", taskID)
}
```

### 使用 Python SDK

```python
import agentos

# 初始化客户端
client = agentos.Client(endpoint="unix:///var/run/agentos.sock")

# 提交任务
task_id = client.task.submit(
    description="分析财务报表并生成摘要"
)

print(f"任务已提交，ID: {task_id}")

# 等待任务完成
result = client.task.wait(task_id, timeout=30)
print(f"任务结果: {result}")
```

### 使用系统调用 (C API)

```c
#include <agentos.h>
#include <stdio.h>

int main() {
    // 初始化 AgentOS 核心
    int ret = agentos_core_init();
    if (ret != 0) {
        fprintf(stderr, "初始化失败: %s\n", agentos_strerror(ret));
        return 1;
    }
    
    // 提交任务
    uint64_t task_id;
    ret = agentos_task_submit("分析财务报表并生成摘要", &task_id);
    if (ret != 0) {
        fprintf(stderr, "任务提交失败: %s\n", agentos_strerror(ret));
        return 1;
    }
    
    printf("任务已提交，ID: %llu\n", task_id);
    
    // 清理
    agentos_core_shutdown();
    return 0;
}
```

---

## 📊 API 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM, Linux 6.5):

| API 类别 | 延迟 | 吞吐量 | 测试条件 |
|---------|------|--------|---------|
| **系统调用** | < 10μs | 100,000+ ops/s | 本地 IPC，小消息 |
| **Go SDK** | < 100μs | 50,000+ ops/s | 序列化 + 网络 |
| **Python SDK** | < 200μs | 30,000+ ops/s | 序列化 + 网络 |
| **Rust SDK** | < 50μs | 80,000+ ops/s | 零拷贝优化 |
| **TypeScript SDK** | < 300μs | 20,000+ ops/s | HTTP/WebSocket |

---

## 🔍 API 版本管理

AgentOS 使用语义化版本控制管理 API 兼容性：

### 版本号格式: MAJOR.MINOR.PATCH.BUILD

- **MAJOR**: 不兼容的 API 变更
- **MINOR**: 向后兼容的功能性新增
- **PATCH**: 向后兼容的问题修复
- **BUILD**: 构建编号，不影响 API

### 兼容性保证

| 版本范围 | 兼容性保证 |
|---------|-----------|
| **同一 MAJOR 版本** | 完全向后兼容 |
| **MINOR 版本升级** | 新增功能，不破坏现有接口 |
| **PATCH 版本升级** | 仅修复问题，不改变行为 |

### 弃用策略

1. **警告阶段**: 标记为 `deprecated`，继续工作但输出警告
2. **过渡阶段**: 提供替代接口，旧接口仍可用
3. **移除阶段**: 在下一个 MAJOR 版本中移除

---

## 🛡️ API 安全

### 1. 权限控制

所有 API 调用经过安全穹顶的权限检查：

```yaml
# 权限规则示例 (agentos/manager/security/policy.yaml)
rules:
  - action: "task.submit"
    resource: "task:*"
    effect: "allow"
    conditions:
      - field: "user.role"
        operator: "in"
        value: ["developer", "admin"]
```

### 2. 输入净化

所有用户输入经过净化处理：

```c
// 自动净化用户输入
char* sanitized_input = domes_sanitize(user_input, SANITIZE_LEVEL_STRICT);
if (!sanitized_input) {
    return AGENTOS_EPERM;  // 输入包含危险内容
}
```

### 3. 审计追踪

所有敏感操作记录审计日志：

```c
// 自动记录审计日志
domes_audit_record(AUDIT_ACTION_TASK_SUBMIT, {
    .task_id = task_id,
    .user_id = current_user,
    .timestamp = agentos_time_now_ns(),
});
```

---

## 📝 API 文档生成

### 自动生成文档

```bash
# 生成 C API 文档
doxygen Doxyfile

# 生成 Go SDK 文档
cd agentos/toolkit/go
godoc -http=:6060

# 生成 Python SDK 文档
cd agentos/toolkit/python
pdoc --html agentos -o docs/

# 生成 Rust SDK 文档
cd agentos/toolkit/rust
cargo doc --no-deps --open

# 生成 TypeScript SDK 文档
cd agentos/toolkit/typescript
npm run docs
```

### 文档验证

```bash
# 验证 API 契约
python scripts/validate_api_contract.py

# 测试示例代码
python scripts/test_examples.py

# 检查死链接
python scripts/check_links.py
```

---

## 🤝 贡献 API

### 新增 API 流程

1. **设计提案**: 提交 API 设计文档，说明用途、接口、契约
2. **架构评审**: 架构委员会评审设计是否符合原则
3. **实现验证**: 通过契约测试验证接口兼容性
4. **文档编写**: 编写完整的 API 文档和示例
5. **集成测试**: 通过集成测试确保功能正确

### API 模板

```c
/**
 * @brief [功能描述]
 * 
 * [详细说明]
 * 
 * @param [参数1] [参数描述]
 * @param [参数2] [参数描述]
 * @param out_result [输出参数描述]
 * @return 0 成功；错误码 失败
 * 
 * @ownership [所有权说明]
 * @threadsafe [线程安全性]
 * @reentrant [可重入性]
 * @see [相关函数]
 * @note [注意事项]
 */
AGENTOS_API int agentos_module_function(
    [参数类型] [参数名],
    [输出参数类型]* out_result
);
```

---

## 📞 支持与反馈

### 问题报告

- **GitHub Issues**: [API 问题报告](https://github.com/SpharxTeam/AgentOS/issues)
- **邮件列表**: api-support@agentos.io
- **Discord**: #api-support 频道

### 技术支持

- **文档问题**: docs@agentos.io
- **安全漏洞**: security@agentos.io
- **API 设计咨询**: architecture@agentos.io

---

## 📚 相关文档

- [架构设计原则](../architecture/ARCHITECTURAL_PRINCIPLES.md)
- [系统调用规范](../architecture/folder/syscall.md)
- [编码规范](../specifications/coding_standard/C_coding_style_guide.md)
- [术语表](../specifications/TERMINOLOGY.md)

---

**最后更新**: 2026-03-31  
**维护者**: AgentOS API 委员会 (作者: LirenWang)

---

© 2026 SPHARX Ltd. All Rights Reserved.  
*"From data intelligence emerges."*