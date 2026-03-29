# daemon commons - 公共组件�?

<div align="center">

[![Version](https://img.shields.io/badge/version-v1.0.0.6-blue.svg)](../README.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](../../LICENSE)

**版本**: v1.0.0.6 | **更新日期**: 2026-03-26

</div>

## 📊 功能完成�?

- **核心功能**: 95% �?
- **单元测试**: 90% �?
- **文档完善�?*: 95% �?
- **生产就绪**: �?

## 🎯 概述

daemon commons �?daemon 层的公共组件库，为所有守护进程（llm_d, market_d, monit_d, sched_d, tool_d）提供共享的基础设施和服务，确保各模块间的一致性和互操作性�?

### 核心功能

- **统一日志**: 跨服务日志接口，trace_id 全链路追�?
- **配置管理**: YAML 配置加载和热更新
- **错误处理**: 统一错误码和错误消息
- **指标采集**: OpenTelemetry 集成，性能监控
- **健康检�?*: 服务状态监控和自愈机制

## 🛠�?主要变更 (v1.0.0.6)

- �?**新增**: OpenTelemetry 追踪集成
- �?**新增**: 配置热更新支�?
- 🚀 **优化**: 日志性能提升 50%
- 🚀 **优化**: 内存占用降低 30%
- 📝 **完善**: 添加健康检查和自愈机制

## 🏗�?架构设计

### 模块组成

```
commons/
├── include/
�?  ├── logging.h        # 统一日志接口
�?  ├── manager.h         # 配置管理接口
�?  ├── error.h          # 错误处理接口
�?  ├── metrics.h        # 指标采集接口
�?  └── health.h         # 健康检查接�?
├── src/
�?  ├── logging.c        # 日志实现
�?  ├── manager.c         # 配置实现
�?  ├── error.c          # 错误处理实现
�?  ├── metrics.c        # 指标采集实现
�?  └── health.c         # 健康检查实�?
└── tests/
    └── test_common.c    # 单元测试
```

## 🔧 使用示例

### 统一日志

```c
#include <logging.h>

// 记录日志（自动包�?trace_id�?
AGENTOS_LOG_INFO("Service started");
AGENTOS_LOG_WARNING("High memory usage: %d%%", memory_percent);
AGENTOS_LOG_ERROR("Failed to connect: %s", error_msg);

// 带追�?ID 的日�?
agentos_log_set_trace_id("trace-12345");
AGENTOS_LOG_INFO("Processing request");
```

### 配置管理

```c
#include <manager.h>

// 加载配置
agentos_config_t* manager = agentos_config_load("./manager.yaml");

// 读取配置�?
const char* llm_api_key = agentos_config_get(manager, "llm.api_key");
int timeout = agentos_config_get_int(manager, "network.timeout", 30);

// 热更新监�?
agentos_config_watch(manager, "llm.model", on_model_change);
```

### 健康检�?

```c
#include <health.h>

// 注册健康检�?
agentos_health_register("llm_d", check_llm_health);

// 健康检查函�?
bool check_llm_health() {
    // 检�?LLM 服务连接
    return llm_is_connected();
}
```

## 📈 性能指标

| 指标 | 数�?| 测试条件 |
|------|------|---------|
| 日志吞吐 | 100,000+ �?�?| 异步批量写入 |
| 配置加载 | < 10ms | 千行配置 |
| 健康检查延�?| < 5ms | 单次检�?|
| 内存占用 | < 50MB | 典型场景 |

## 🤝 贡献指南

欢迎贡献代码或提出改进建议！

## 📞 联系方式

- **维护�?*: AgentOS 架构委员�?
- **技术支�?*: lidecheng@spharx.cn
- **安全问题**: wangliren@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"From data intelligence emerges 始于数据，终于智能�?*
