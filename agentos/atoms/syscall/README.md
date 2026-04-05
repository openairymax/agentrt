# AgentOS Syscall - 系统调用接口

<div align="center">

[![Version](https://img.shields.io/badge/version-v1.0.0.7-blue.svg)](../../README.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](../../../LICENSE)

**版本**: v1.0.0.7 | **最后更新**: 2026-04-03

</div>

## ✅ 完成状态

- **核心功能**: 100% ✅
- **单元测试**: 100% ✅
- **文档覆盖**: 100% ✅
- **代码审查**: ✅

## 📖 概述

Syscall（系统调用）是用户态程序与内核之间的**唯一通信通道**，防止其他模块（如 `agentos/daemon/`）绕过 syscall 直接调用内核内部函数。

本模块实现完整的系统调用接口，提供任务管理、内存操作、会话管理、可观测性、Agent 管理等核心功能。

## 🏗️ 架构设计

### 核心接口分类

| 类别 | 接口 | 说明 |
|------|------|------|
| **任务管理** | `agentos_sys_task_submit/query/wait/cancel` | 任务全生命周期管理 |
| **内存管理** | `agentos_sys_memory_write/search/get/delete` | 记忆 CRUD 操作 |
| **会话管理** | `agentos_sys_session_create/get/close/list` | 多轮对话会话管理 |
| **可观测性** | `agentos_sys_telemetry_metrics/traces` | 指标采集和链路追踪 |
| **Agent 管理** | `agentos_sys_agent_register/invoke/terminate` | Agent 生命周期管理 |

### 设计原则

- **唯一通道**: 用户态与内核通信的唯一路径
- **接口规范**: 所有 API 通过 Doxygen 规范注释
- **线程安全**: 所有系统调用保证线程安全
- **错误处理**: 统一错误码和错误信息

## 🚀 主要功能 (v1.0.0.6)

- ✅ **完整实现**: 完整系统调用接口实现，包括任务/内存/会话/可观测性/Agent 等
- ✅ **文档完善**: Doxygen 规范注释覆盖所有公开 API
- 🚀 **优化**: 性能优化和统一错误处理机制
- 🚀 **优化**: 支持跨进程 IPC 调用封装
- 📚 **文档**: 接口文档和使用示例

## 📦 依赖

- AgentOS 微内核 (`corekern`)
- AgentOS 公共库 (`commons`)
- cJSON (>=1.7.15)

## 🔧 编译

```bash
mkdir build && cd build
cmake ../agentos/atoms/syscall -DBUILD_TESTS=ON
make -j4
```

## 🔗 集成

syscall 模块是 AgentOS 的标准接口层，所有用户态程序通过 syscall 与内核交互：

```c
// 提交任务
const char* input = "{\"task\": \"analyze_data\"}";
char* output = NULL;
agentos_error_t err = agentos_sys_task_submit(input, strlen(input), 5000, &output);
if (err == AGENTOS_SUCCESS) {
    printf("Task output: %s\n", output);
    agentos_sys_free(output);
}

// 查询任务状态
int status = 0;
err = agentos_sys_task_query(task_id, &status);
```

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"系统调用，用户态与内核的唯一通道。"*
