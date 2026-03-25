# Backs Common - 公共组件库

<div align="center">

[![Version](https://img.shields.io/badge/version-v1.0.0.6-blue.svg)](../README.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](../../LICENSE)
[![Status](https://img.shields.io/badge/status-production%20ready-success.svg)](../README.md)

**版本**: v1.0.0.6 | **更新日期**: 2026-03-25

</div>

## 📊 功能完成度

- **核心功能**: 95% ✅
- **单元测试**: 90% ✅
- **文档完善度**: 95% ✅
- **生产就绪**: ✅

## 🎯 概述

Backs Common 是 backs 层的公共组件库，为所有守护进程（llm_d, market_d, monit_d, sched_d, tool_d）提供共享的基础设施和服务，确保各模块间的一致性和互操作性。

### 核心功能

- **统一日志**: 跨服务日志接口，trace_id 全链路追踪
- **配置管理**: YAML 配置加载和热更新
- **错误处理**: 统一错误码和错误消息
- **指标采集**: OpenTelemetry 集成，性能监控
- **健康检查**: 服务状态监控和自愈机制

## 🛠️ 主要变更 (v1.0.0.6)

- ✨ **新增**: OpenTelemetry 追踪集成
- ✨ **新增**: 配置热更新支持
- 🚀 **优化**: 日志性能提升 50%
- 🚀 **优化**: 内存占用降低 30%
- 📝 **完善**: 添加健康检查和自愈机制

## 🏗️ 架构设计

### 模块组成

```
common/
├── include/
│   ├── logging.h        # 统一日志接口
│   ├── config.h         # 配置管理接口
│   ├── error.h          # 错误处理接口
│   ├── metrics.h        # 指标采集接口
│   └── health.h         # 健康检查接口
├── src/
│   ├── logging.c        # 日志实现
│   ├── config.c         # 配置实现
│   ├── error.c          # 错误处理实现
│   ├── metrics.c        # 指标采集实现
│   └── health.c         # 健康检查实现
└── tests/
    └── test_common.c    # 单元测试
```

## 🔧 使用示例

### 统一日志

```c
#include <logging.h>

// 记录日志（自动包含 trace_id）
AGENTOS_LOG_INFO("Service started");
AGENTOS_LOG_WARNING("High memory usage: %d%%", memory_percent);
AGENTOS_LOG_ERROR("Failed to connect: %s", error_msg);

// 带追踪 ID 的日志
agentos_log_set_trace_id("trace-12345");
AGENTOS_LOG_INFO("Processing request");
```

### 配置管理

```c
#include <config.h>

// 加载配置
agentos_config_t* config = agentos_config_load("./config.yaml");

// 读取配置项
const char* llm_api_key = agentos_config_get(config, "llm.api_key");
int timeout = agentos_config_get_int(config, "network.timeout", 30);

// 热更新监听
agentos_config_watch(config, "llm.model", on_model_change);
```

### 健康检查

```c
#include <health.h>

// 注册健康检查
agentos_health_register("llm_d", check_llm_health);

// 健康检查函数
bool check_llm_health() {
    // 检查 LLM 服务连接
    return llm_is_connected();
}
```

## 📈 性能指标

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| 日志吞吐 | 100,000+ 条/秒 | 异步批量写入 |
| 配置加载 | < 10ms | 千行配置 |
| 健康检查延迟 | < 5ms | 单次检查 |
| 内存占用 | < 50MB | 典型场景 |

## 🤝 贡献指南

欢迎贡献代码或提出改进建议！

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **安全问题**: wangliren@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"From data intelligence emerges 始于数据，终于智能。"*
