# AgentOS 服务管理框架设计 (Phase 3)

<div align="center">

**AgentOS 服务管理框架设计文档**  
*第三阶段"系统完善"核心设计 - 服务框架完善*

[![Phase](https://img.shields.io/badge/Phase-3%3A系统完善-blue.svg)](https://gitee.com/spharx/agentos)  
[![Status](https://img.shields.io/badge/Status-设计完成-green.svg)](https://gitee.com/spharx/agentos)  
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)

*"设计先行，渐进实施" - 为 AgentOS daemon 模块提供统一、可扩展的服务管理框架*

</div>

---

## 📋 文档导航

- [设计概述](#-设计概述) - 设计目标与原则
- [现有设计分析](#-现有设计分析) - svc_common.h 架构评估
- [架构设计方案](#-架构设计方案) - 改进的框架设计
- [核心接口定义](#-核心接口定义) - API 接口规范
- [渐进实施计划](#-渐进实施计划) - 分阶段实现策略
- [技术选型与约束](#-技术选型与约束) - 技术与架构约束
- [附录](#-附录) - 相关文档与参考资料

---

## 🎯 设计概述

### 1.1 设计背景

AgentOS daemon 模块目前包含 6 个核心服务（gateway_d、llm_d、tool_d、market_d、sched_d、monit_d），每个服务独立进程运行，通过 HTTP/REST 进行通信。虽然现有架构实现了服务隔离和微服务原则，但缺乏统一的服务管理框架，导致：

1. **服务生命周期管理不统一**：各服务自行实现启动/停止逻辑
2. **服务发现机制缺失**：服务间通过硬编码地址通信
3. **监控和诊断困难**：缺乏统一的服务健康检查和统计收集
4. **配置管理分散**：各服务使用独立的配置文件格式

### 1.2 设计目标

| 目标 | 描述 | 对应原则 |
|------|------|----------|
| **统一服务管理** | 标准化的服务生命周期管理接口 | K-2 接口契约化 |
| **服务发现与注册** | 动态的服务注册、发现和负载均衡 | K-4 可插拔原则 |
| **健康监控** | 统一的健康检查、指标收集和告警 | E-2 可观测性 |
| **配置集中化** | 统一的配置管理和动态更新 | E-7 配置管理 |
| **故障恢复** | 自动故障检测、恢复和容错 | K-3 服务隔离 |

### 1.3 设计原则

1. **渐进兼容**：保持对现有服务的向后兼容，逐步迁移
2. **无侵入性**：服务框架不强制改变现有服务实现
3. **可扩展性**：支持新服务类型的动态注册和管理
4. **跨平台支持**：确保框架在 Linux、macOS、Windows 上的一致性
5. **性能优先**：框架开销最小化，不影响核心业务逻辑

---

## 📊 现有设计分析

### 2.1 svc_common.h 架构评估

`svc_common.h` 已经提供了优秀的基础设计，包含：

#### ✅ 优点
- **完整的生命周期管理**：创建、初始化、启动、停止、暂停、恢复、销毁
- **状态机清晰**：8种服务状态，明确的转换逻辑
- **统计和监控**：请求计数、成功率、响应时间等完整统计
- **健康检查机制**：标准化的健康检查接口
- **服务注册表**：进程内服务发现和注册

#### ⚠️ 局限性
- **进程内注册表**：`agentos_service_register/find` 仅支持进程内服务发现
- **实现不完整**：API 定义存在，但实际使用率低
- **配置管理简单**：仅支持静态配置，缺少动态更新
- **缺少故障恢复**：没有自动重启、降级机制
- **缺乏服务间通信**：缺少标准化的服务间通信协议

### 2.2 现有服务架构分析

当前 daemon 架构：
```
┌─────────────────────────────────────────────┐
│             服务层 (daemon)                  │
│                                             │
│  ┌──────────┐  HTTP  ┌──────────┐  HTTP    │
│  │ gateway_d│◄──────►│   llm_d  │◄──────►  │
│  └──────────┘        └──────────┘          │
│                                             │
│  ┌──────────┐        ┌──────────┐          │
│  │  tool_d  │        │ market_d │          │
│  └──────────┘        └──────────┘          │
│                                             │
│  ┌──────────┐        ┌──────────┐          │
│  │  sched_d │        │  monit_d │          │
│  └──────────┘        └──────────┘          │
└─────────────────────────────────────────────┘
```

**通信模式**：
- 进程间：HTTP/REST（硬编码端口）
- 进程内：无统一框架
- 与内核：通过 `svc_rpc_call()` 进行 RPC

---

## 🏗️ 架构设计方案

### 3.1 整体架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       服务管理框架 (Service Management Framework)            │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      服务注册中心 (Service Registry)                  │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐                   │   │
│  │  │ 服务发现API  │ │ 健康检查API  │ │ 负载均衡API  │                   │   │
│  │  └─────────────┘ └─────────────┘ └─────────────┘                   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      服务生命周期管理器 (Service Lifecycle Manager)   │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐                   │   │
│  │  │ 启动/停止    │ │ 暂停/恢复    │ │ 状态监控    │                   │   │
│  │  └─────────────┘ └─────────────┘ └─────────────┘                   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      配置管理器 (Configuration Manager)              │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐                   │   │
│  │  │ 配置加载     │ │ 热更新       │ │ 版本管理     │                   │   │
│  │  └─────────────┘ └─────────────┘ └─────────────┘                   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      故障恢复管理器 (Fault Recovery Manager)         │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐                   │   │
│  │  │ 健康检查     │ │ 自动重启     │ │ 降级策略     │                   │   │
│  │  └─────────────┘ └─────────────┘ └─────────────┘                   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 核心组件设计

#### 3.2.1 跨进程服务注册中心

**设计目标**：解决 `svc_common.h` 中进程内注册表的局限性

**方案**：
- **中央注册服务**：扩展 `market_d` 或创建新的 `registry_d` 服务
- **服务元数据**：包含服务名称、版本、端点、健康状态、能力、负载
- **注册协议**：HTTP + 心跳机制（每30秒）
- **服务发现**：支持按名称、类型、标签查询

**接口扩展**：
```c
// 扩展 svc_common.h 的注册接口
AGENTOS_API agentos_error_t agentos_service_register_global(
    agentos_service_t service,
    const char* registry_url
);

AGENTOS_API agentos_service_t* agentos_service_discover(
    const char* service_type,
    const char* tags,
    size_t* count
);
```

#### 3.2.2 配置管理增强

**设计目标**：统一的配置管理，支持热更新

**方案**：
- **配置仓库**：集中式配置存储（YAML/JSON）
- **配置版本化**：支持配置回滚和历史记录
- **热更新机制**：配置变更时通知服务重新加载
- **环境感知**：支持开发、测试、生产环境配置

#### 3.2.3 故障恢复机制

**设计目标**：提高服务可用性，自动处理故障

**方案**：
- **健康检查策略**：分层健康检查（进程级、服务级、业务级）
- **自动重启**：服务崩溃时自动重启（带退避策略）
- **降级策略**：服务不可用时提供降级响应
- **故障转移**：多实例服务的故障转移

### 3.3 通信协议设计

#### 3.3.1 服务间通信协议

**当前问题**：HTTP/REST 硬编码，缺乏协议抽象

**改进方案**：
```c
// 统一的通信客户端接口
typedef struct {
    agentos_error_t (*call)(
        const char* service_name,
        const char* method,
        const char* params_json,
        char** response_json,
        uint32_t timeout_ms
    );
    agentos_error_t (*stream)(
        const char* service_name,
        const char* method,
        const char* params_json,
        agentos_stream_callback_t callback,
        void* user_data
    );
} agentos_service_client_t;

// 支持多种协议后端
enum {
    AGENTOS_PROTO_HTTP = 0,
    AGENTOS_PROTO_GRPC,
    AGENTOS_PROTO_IPC,    // Unix Domain Socket
    AGENTOS_PROTO_MEMORY  // 进程内通信
};
```

---

## 🔌 核心接口定义

### 4.1 扩展的服务管理接口（基于 svc_common.h）

```c
// 服务注册中心客户端
AGENTOS_API agentos_error_t agentos_registry_init(const char* registry_url);
AGENTOS_API agentos_error_t agentos_registry_register(
    agentos_service_t service,
    const agentos_service_metadata_t* metadata
);
AGENTOS_API agentos_error_t agentos_registry_deregister(
    agentos_service_t service
);
AGENTOS_API agentos_service_metadata_t* agentos_registry_discover(
    const char* service_type,
    const char* filter_tags,
    size_t* result_count
);
AGENTOS_API void agentos_registry_cleanup(void);

// 配置管理
AGENTOS_API agentos_error_t agentos_config_load(
    const char* service_name,
    agentos_config_t** config
);
AGENTOS_API agentos_error_t agentos_config_watch(
    const char* service_name,
    agentos_config_change_callback_t callback,
    void* user_data
);
AGENTOS_API void agentos_config_free(agentos_config_t* config);

// 故障恢复
AGENTOS_API agentos_error_t agentos_service_monitor_start(
    agentos_service_t service,
    const agentos_monitor_config_t* config
);
AGENTOS_API agentos_error_t agentos_service_monitor_stop(
    agentos_service_t service
);
AGENTOS_API agentos_error_t agentos_service_set_degradation_handler(
    agentos_service_t service,
    agentos_degradation_handler_t handler,
    void* user_data
);
```

### 4.2 服务元数据结构

```c
typedef struct {
    char name[64];                    // 服务名称
    char version[32];                 // 服务版本
    char endpoint[256];               // 服务端点（URL）
    char service_type[32];            // 服务类型（llm、tool、market等）
    char tags[256];                   // 标签（逗号分隔）
    agentos_svc_state_t state;        // 服务状态
    uint32_t capabilities;            // 能力标志
    uint32_t current_load;            // 当前负载（0-100）
    uint64_t last_heartbeat;          // 最后心跳时间
    bool healthy;                     // 健康状态
    uint32_t instance_id;             // 实例ID（用于多实例）
} agentos_service_metadata_t;
```

### 4.3 配置数据结构

```c
typedef struct {
    char* raw_config;                 // 原始配置内容
    size_t config_size;               // 配置大小
    uint64_t version;                 // 配置版本
    time_t last_modified;             // 最后修改时间
    char checksum[65];                // SHA-256 校验和
} agentos_config_t;
```

---

## 🗓️ 渐进实施计划

### 5.1 Phase 3.1：基础框架完善（2周）

**目标**：激活现有 `svc_common.h` 设计，为现有服务添加基本管理

**任务**：
1. **实现 svc_common.c**：实现 `svc_common.h` 中定义的核心接口
2. **服务适配层**：为每个现有服务创建适配器，实现 `agentos_svc_interface_t`
3. **进程内注册表**：实现进程内服务注册和发现
4. **基础健康检查**：添加简单的进程健康检查
5. **统一日志集成**：集成到现有日志系统

**交付物**：
- `daemon/common/src/svc_common.c` 实现
- 6个服务的适配器实现
- 服务管理命令行工具原型

### 5.2 Phase 3.2：跨进程服务发现（2周）

**目标**：实现跨进程服务注册和发现

**任务**：
1. **注册中心服务**：创建 `registry_d` 或扩展 `market_d`
2. **服务注册协议**：定义和实现注册协议（HTTP + 心跳）
3. **服务发现客户端**：实现服务发现客户端库
4. **负载均衡策略**：实现基本的负载均衡（轮询、最少连接）
5. **配置管理基础**：集中式配置存储设计

**交付物**：
- 注册中心服务实现
- 服务发现客户端库
- 配置仓库原型

### 5.3 Phase 3.3：高级管理功能（2周）

**目标**：添加配置管理、故障恢复等高级功能

**任务**：
1. **配置热更新**：实现配置动态加载和热更新
2. **健康检查增强**：分层健康检查（进程、服务、业务）
3. **故障恢复机制**：自动重启、降级策略
4. **监控和告警**：集成到 `monit_d` 监控系统
5. **性能优化**：框架性能优化和基准测试

**交付物**：
- 配置热更新机制
- 故障恢复管理器
- 性能基准测试报告

### 5.4 Phase 3.4：生产就绪（2周）

**目标**：完善、测试和文档化

**任务**：
1. **完整测试覆盖**：单元测试、集成测试、压力测试
2. **文档完善**：API文档、用户指南、部署指南
3. **安全审查**：安全漏洞扫描和修复
4. **性能调优**：生产环境性能调优
5. **向后兼容**：确保与现有服务的完全兼容

**交付物**：
- 完整的测试套件
- 完善的文档体系
- 生产就绪的服务管理框架

---

## ⚙️ 技术选型与约束

### 6.1 技术约束

| 约束类别 | 具体约束 | 解决方案 |
|----------|----------|----------|
| **平台兼容性** | Linux/macOS/Windows 跨平台支持 | 使用 `platform.h` 抽象层 |
| **性能要求** | 框架开销 < 1% CPU、< 10MB 内存 | 轻量级实现，按需加载 |
| **向后兼容** | 现有服务无需修改即可使用 | 适配器模式，可选集成 |
| **安全性** | 服务间通信需加密认证 | TLS 支持，JWT 认证 |
| **可观测性** | 完整的监控和日志 | 集成 `monit_d` 和现有日志系统 |

### 6.2 依赖管理

**核心依赖**：
- `commons` 基础库：错误处理、日志、配置、平台抽象
- `libcurl`：HTTP 客户端（服务注册和发现）
- `libcjson`：JSON 解析和生成
- `pthread`：多线程支持

**可选依赖**：
- `libevent`：高性能事件循环
1. **服务版本兼容性**：如何处理不同版本服务的共存？
2. **安全认证机制**：服务间通信的认证和授权方案？
3. **多数据中心支持**：跨数据中心的服务发现？
4. **服务网格集成**：未来是否集成 Istio/Linkerd 等服务网格？

### A.4 修订历史

| 版本 | 日期 | 修改内容 | 修改人 |
|------|------|----------|--------|
| 1.0 | 2026-04-11 | 初始版本，基于 Phase 3 设计 | AgentOS 开发组 |
| 1.1 | 2026-04-11 | 添加渐进实施计划和性能指标 | AgentOS 开发组 |

---

<div align="center">

**AgentOS 服务管理框架设计**  
*设计先行，渐进实施 - 构建统一、可靠、可扩展的服务管理体系*

© 2026 SPHARX Ltd. All Rights Reserved.

</div>