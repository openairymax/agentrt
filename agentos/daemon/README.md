# AgentOS 用户态服务层 (daemon)

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://gitee.com/spharx/agentos)
[![Coverage](https://img.shields.io/badge/coverage-85%25-brightgreen.svg)](https://gitee.com/spharx/agentos)

**AgentOS 服务层 - 为智能体提供完整的后端服务支持**

*"Modular services, independent processes, seamless communication."*

</div>

---

## 📋 快速导航

- [模块定位](#-模块定位) - daemon 在 AgentOS 中的角色
- [架构设计](#-架构设计) - 服务层架构原则
- [核心服务](#-核心服务) - 六大服务详解
- [公共服务库](#-公共服务库-common) - 兼容层设计
- [快速开始](#-快速开始) - 构建和运行
- [API 概览](#-api-概览) - 主要接口说明
- [性能指标](#-性能指标) - 性能基准数据
- [常见问题](#-常见问题) - FAQ

---

## 🎯 模块定位

`agentos/daemon/` 是 AgentOS 的**用户态服务层（Service Layer）**，位于内核层之上、应用层之下，为智能体运行时提供完整的后端服务支持。

### 架构位置

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                           应用层 (openlab)                                    │
│               docgen | ecommerce | research | videoedit | ...                │
└──────────────────────────────────────────────────────────────────────────────┘
                                    ↓↑
┌──────────────────────────────────────────────────────────────────────────────┐
│                      用户态服务层 (daemon) ← 本层                             │
│         gateway_d | llm_d | tool_d | market_d | sched_d | monit_d            │
└──────────────────────────────────────────────────────────────────────────────┘
                                    ↓↑ syscall
┌──────────────────────────────────────────────────────────────────────────────┐
│                            内核层 (atoms)                                     │
│  ┌────────────────┐ ┌────────────────┐ ┌────────────────┐                    │
│  │    corekern    │ │ coreloopthree  │ │  memoryrovol   │                    │
│  │    微内核基础   │ │  三层认知运行时  │ │   四层记忆系统   │                    │
│  │  IPC · Mem · Task │ │ 认知 / 行动 / 记忆  │ │   L1/L2/L3/L4   │                    │
│  └────────────────┘ └────────────────┘ └────────────────┘                    │
└──────────────────────────────────────────────────────────────────────────────┘
                                    ↓↑
┌──────────────────────────────────────────────────────────────────────────────┐
│                        基础支撑层 (commons)                                   │
│         platform | utils (logging, config, error, memory, sync, cache)       │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 核心价值

| 特性 | 说明 | 对应原则 |
|------|------|----------|
| **微服务架构** | 每个服务独立进程运行，故障隔离，易于扩展 | K-3 服务隔离原则 |
| **统一接口** | 标准化 RESTful/JSON-RPC API，支持跨语言调用 | K-2 接口约束原则 |
| **内核隔离** | 所有服务运行在用户态，通过 syscall 与内核通信 | K-1 微内核化原则 |
| **可观测性** | OpenTelemetry 集成，全链路追踪和监控 | E-2 可观测性原则 |
| **热插拔** | 支持服务动态加载/卸载，无需重启系统 | K-4 可插拔原则 |
| **多提供者** | LLM 服务支持 OpenAI、Anthropic、DeepSeek 等 | K-4 可插拔原则 |

---

## 🏗️ 架构设计

### 设计原则

用户态服务层遵循 AgentOS **五维正交原则体系**：

```
维度一：系统观 (S-1~S-4)
  - S-3 三层控制原则：认知→规划→行动闭环
  
维度二：内核观 (K-1~K-4)
  - K-1 微内核化原则：服务运行在用户态
  - K-2 接口约束原则：所有 API 有 Doxygen 约束
  - K-3 服务隔离原则：独立进程，故障隔离
  - K-4 可插拔原则：算法可运行时替换
  
维度三：工程观 (E-1~E-8)
  - E-1 安全内生：沙箱执行、权限控制
  - E-2 可观测性：日志、指标、追踪
  - E-6 错误可追溯：统一错误码和错误链
```

### 服务通信模型

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           用户态服务层                                        │
│                                                                             │
│  ┌───────────┐   HTTP/REST   ┌───────────┐   HTTP/REST   ┌───────────┐     │
│  │ gateway_d │◄─────────────►│   llm_d   │◄─────────────►│  tool_d   │     │
│  │  网关服务  │               │  LLM服务   │               │  工具服务  │     │
│  └─────┬─────┘               └─────┬─────┘               └─────┬─────┘     │
│        │                           │                           │           │
│        │ syscall                   │ syscall                   │ syscall   │
│        ▼                           ▼                           ▼           │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        内核层 (atoms)                                │   │
│  │         agentos_sys_task_* | agentos_sys_memory_* | ...            │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌───────────┐               ┌───────────┐               ┌───────────┐     │
│  │ market_d  │               │  sched_d  │               │  monit_d  │     │
│  │  市场服务  │               │  调度服务  │               │  监控服务  │     │
│  └───────────┘               └───────────┘               └───────────┘     │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 🏛️ 核心服务

daemon 包含 **6 个核心服务** 和 **1 个公共服务库**：

```
agentos/daemon/
├── common/          # 公共服务库（兼容层 + IPC 客户端）
├── gateway_d/       # API 网关服务
├── llm_d/           # LLM 推理服务 ⭐
├── tool_d/          # 工具执行服务
├── market_d/        # Agent/技能市场服务
├── sched_d/         # 智能调度服务
└── monit_d/         # 监控和告警服务
```

### 1. 网关服务 (gateway_d)

**统一 API 网关和路由中心**

| 功能 | 说明 |
|------|------|
| **请求路由** | 统一入口，智能路由到后端服务 |
| **负载均衡** | 多实例负载均衡，健康检查 |
| **协议转换** | HTTP/WebSocket/gRPC 协议适配 |
| **认证鉴权** | JWT Token 验证，权限校验 |
| **限流熔断** | 请求限流，熔断保护 |

**详细文档**: [gateway_d/README.md](gateway_d/README.md)

---

### 2. LLM 服务 (llm_d) ⭐

**统一的大模型推理接口**

| 功能 | 说明 |
|------|------|
| **多提供商支持** | OpenAI GPT、Anthropic Claude、DeepSeek、本地模型 |
| **智能缓存** | LRU 响应缓存，降低 API 调用成本（命中率 > 80%） |
| **成本追踪** | 实时统计 token 消耗和费用 |
| **流式响应** | 支持 SSE 流式输出 |
| **自动重试** | 网络错误自动重试机制 |

**快速示例**:
```bash
# 启动服务
export OPENAI_API_KEY="your-key"
./build/agentos/daemon/llm_d/agentos-llm-d

# 调用 API
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4",
    "messages": [{"role": "user", "content": "Hello!"}]
  }'
```

**详细文档**: [llm_d/README.md](llm_d/README.md)

---

### 3. 工具服务 (tool_d)

**工具注册、验证和执行引擎**

| 功能 | 说明 |
|------|------|
| **工具注册** | 动态注册/注销工具，维护工具元数据 |
| **参数验证** | JSON Schema 验证输入输出 |
| **沙箱执行** | 安全隔离的工具执行环境 |
| **结果缓存** | LRU 缓存常用工具执行结果 |
| **多模式执行** | 同步/异步/沙箱三种执行模式 |

**工具示例**:
```json
{
  "skill_id": "skill-web-search",
  "name": "web_search",
  "description": "互联网搜索引擎接口",
  "input_schema": {
    "type": "object",
    "properties": {
      "query": {"type": "string"},
      "num_results": {"type": "integer", "default": 10}
    }
  }
}
```

**详细文档**: [tool_d/README.md](tool_d/README.md)

---

### 4. 市场服务 (market_d)

**Agent 和技能的"应用商店"**

| 功能 | 说明 |
|------|------|
| **Agent 注册表** | 维护可用 Agent 的元数据和版本 |
| **技能注册表** | 维护可用技能的详细信息 |
| **版本管理** | 语义化版本控制（Semantic Versioning） |
| **依赖解析** | 自动解析和安装依赖关系 |
| **权限管理** | 基于角色的访问控制 |

**安装 Agent 示例**:
```bash
# 搜索 Agent
curl "http://localhost:8082/api/v1/agents/search?keyword=客服"

# 安装 Agent
curl -X POST http://localhost:8082/api/v1/agents/agent-customer-service/install \
  -H "Content-Type: application/json" \
  -d '{"version": "^2.0.0"}'
```

**详细文档**: [market_d/README.md](market_d/README.md)

---

### 5. 调度服务 (sched_d)

**智能任务调度中心**

| 功能 | 说明 |
|------|------|
| **多策略调度** | 轮询、加权、ML 预测等多种算法 |
| **负载均衡** | 基于实时指标的动态资源分配 |
| **优先级管理** | 支持任务优先级队列和抢占式调度 |
| **评分函数** | 基于成本、成功率、信任度的综合评分 |

**评分函数**:
```
Score(agent) = w₁·(1/cost) + w₂·success_rate + w₃·trust_score 
             + w₄·availability + w₅·specialization
```

**详细文档**: [sched_d/README.md](sched_d/README.md)

---

### 6. 监控服务 (monit_d)

**全栈可观测性中心**

| 功能 | 说明 |
|------|------|
| **指标采集** | CPU、内存、磁盘、网络等系统指标 |
| **日志聚合** | 集中式日志管理，结构化查询 |
| **链路追踪** | OpenTelemetry 集成的全链路追踪 |
| **智能告警** | 基于阈值的告警，多渠道通知 |

**AgentOS 特有指标**:
- `agentos_tasks_total` - 任务总数
- `agentos_tasks_active` - 活跃任务数
- `agentos_memory_records` - 记忆记录数
- `agentos_agents_registered` - 注册 Agent 数

**详细文档**: [monit_d/README.md](monit_d/README.md)

---

## 📦 公共服务库 (common/)

**所有服务共享的基础设施，作为 commons 统一基础库的兼容层**

### 架构设计

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        agentos/daemon/common (兼容层)                                │
│                                                                             │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐          │
│  │   error.h   │ │  platform.h │ │ svc_logger.h│ │ svc_config.h│          │
│  │  错误兼容层  │ │  平台兼容层  │ │  日志兼容层  │ │  配置兼容层  │          │
│  └──────┬──────┘ └──────┬──────┘ └──────┬──────┘ └──────┬──────┘          │
│         │               │               │               │                 │
│         ▼               ▼               ▼               ▼                 │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                     commons 统一基础库                               │   │
│  │  utils/error | platform | utils/logging | utils/config_unified     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐                          │
│  │ svc_cache.h │ │ svc_common.h│ │  compat.h   │                          │
│  │  缓存兼容层  │ │  服务框架   │ │  兼容性定义  │                          │
│  └──────┬──────┘ └─────────────┘ └──────┬──────┘                          │
│         │                                     │                            │
│         ▼                                     ▼                            │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                     commons 统一基础库                               │   │
│  │           utils/cache                        utils/compat            │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                     daemon 特有模块                                   │   │
│  │                     ipc_client.c (IPC 客户端)                        │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 兼容层模块

| 头文件 | 功能 | 引用的 commons 模块 |
|--------|------|---------------------|
| `error.h` | 统一错误码和错误处理 | `utils/error/error.h` |
| `platform.h` | 跨平台抽象（线程、互斥锁、时间等） | `platform/platform.h` |
| `svc_logger.h` | 结构化日志，TraceID 追踪 | `utils/logging/logging.h` |
| `svc_config.h` | YAML/JSON 配置解析 | `utils/config_unified/config_unified.h` |
| `svc_cache.h` | LRU 缓存实现 | `utils/cache/cache_common.h` |
| `compat.h` | 编译器兼容性、位操作工具 | `utils/compat/compat.h` |
| `svc_common.h` | 服务框架接口（daemon 特有） | - |

### IPC 客户端

`ipc_client.c` 是 daemon 特有的模块，提供与内核层的通信能力：

```c
/**
 * @brief 初始化 IPC 客户端
 * @param baseruntime_url 内核运行时 URL
 * @return 0 成功，非 0 失败
 */
int svc_ipc_init(const char* baseruntime_url);

/**
 * @brief 执行 RPC 调用
 * @param method 方法名
 * @param params JSON 参数
 * @param out_result 输出结果
 * @param timeout_ms 超时时间
 * @return 0 成功，非 0 失败
 */
int svc_rpc_call(const char* method, const char* params, 
                 char** out_result, uint32_t timeout_ms);

/**
 * @brief 清理 IPC 客户端
 */
void svc_ipc_cleanup(void);
```

### 日志使用示例

```c
#include "svc_logger.h"

// 初始化日志
agentos_logger_config_t config = {
    .name = "llm_d",
    .level = AGENTOS_LOG_INFO,
    .json_format = true
};
agentos_log_init(&config);

// 记录日志
LOG_INFO("Service started on port %d", port);
LOG_ERROR("Failed to connect: %s", error_msg);

// 带追踪上下文
agentos_trace_context_t trace;
agentos_trace_new(&trace);
LOG_INFO_T(&trace, "Processing request %s", request_id);

// 清理
agentos_log_shutdown();
```

**详细文档**: [common/README.md](common/README.md)

---

## 🚀 快速开始

### 环境要求

| 类别 | 要求 |
|------|------|
| **操作系统** | Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2) |
| **编译器** | GCC 11+ / Clang 14+ / MSVC 2022+ |
| **构建工具** | CMake 3.20+ |
| **系统依赖** | libcurl, libcjson, libyaml, pthread |
| **可选依赖** | libevent, tiktoken |

### 构建步骤

```bash
# 1. 克隆项目
git clone https://gitee.com/spharx/agentos.git
cd agentos

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置 CMake（包含 commons 依赖）
cmake ../daemon -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

# 4. 编译
cmake --build . --parallel $(nproc)

# 5. 产物位于 build/ 目录
```

### 启动服务

```bash
# 启动 LLM 服务
export OPENAI_API_KEY="your-key"
./build/agentos/daemon/llm_d/agentos-llm-d

# 启动工具服务
./build/agentos/daemon/tool_d/agentos-tool-d

# 启动网关服务
./build/agentos/daemon/gateway_d/gateway_d

# 使用 systemd 管理（Linux）
sudo systemctl enable agentos-llm-d
sudo systemctl start agentos-llm-d
```

---

## 🌐 API 概览

### RESTful API 端口

| 服务 | 端口 | 主要端点 |
|------|------|---------|
| **gateway_d** | 8000 | `/api/v1/*` (统一入口) |
| **llm_d** | 8080 | `/v1/chat/completions`, `/v1/embeddings` |
| **tool_d** | 8081 | `/v1/tools`, `/v1/tools/execute` |
| **market_d** | 8082 | `/api/v1/agents`, `/api/v1/skills` |
| **sched_d** | 8083 | `/api/v1/schedule`, `/api/v1/agents/available` |
| **monit_d** | 9090 | `/api/v1/metrics`, `/api/v1/logs/search` |

### 通用 API 示例

**LLM 服务**:
```bash
POST /v1/chat/completions
{
  "model": "gpt-4",
  "messages": [{"role": "user", "content": "Hello!"}]
}
```

**工具服务**:
```bash
POST /v1/tools/execute
{
  "tool_id": "web_search",
  "params_json": "{\"query\": \"AgentOS\"}"
}
```

**调度服务**:
```bash
POST /api/v1/schedule
{
  "task_id": "task-001",
  "task_type": "data_analysis",
  "required_skills": ["python", "pandas"],
  "priority": "high"
}
```

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM):

| 服务 | 指标 | 数值 | 测试条件 |
|------|------|------|---------|
| **gateway_d** | 请求延迟 | < 5ms | 路由转发 |
| | 并发连接 | 10000+ | 每服务实例 |
| **llm_d** | 请求延迟 | < 50ms | 不含模型推理时间 |
| | 缓存命中率 | > 80% | 重复查询场景 |
| **tool_d** | 执行延迟 | < 10ms | 简单工具 |
| | 并发执行 | 100+ | 每服务实例 |
| **sched_d** | 调度延迟 | < 1ms | 单次调度决策 |
| | 吞吐量 | 10000+ 任务/秒 | 轮询策略 |
| **monit_d** | 指标采集延迟 | < 10ms | 单次采集 |
| | 日志写入吞吐 | 50MB/s | 压缩后 |

---

## 🔧 常见问题

### Q1: 如何添加新的 LLM 提供商？

在 `llm_d/src/providers/` 目录下创建新的提供商实现：

```c
// providers/my_provider.c
#include "provider.h"

static llm_provider_t my_provider = {
    .name = "my_provider",
    .init = my_provider_init,
    .complete = my_provider_complete,
    .cleanup = my_provider_cleanup
};
```

然后在 `registry.c` 中注册该提供商。

---

### Q2: 服务间如何通信？

服务间通过 **syscall** 和 **RESTful API** 两种方式通信：

- **syscall**: 通过 `svc_ipc_init()` 和 `svc_rpc_call()` 与内核层通信
- **RESTful API**: 通过 HTTP 调用其他服务

---

### Q3: 如何配置服务？

每个服务都有独立的 YAML 配置文件，位于 `agentos/manager/service/<service_name>/`:

```yaml
# llm.yaml
server:
  port: 8080
  max_connections: 1000

providers:
  openai:
    enabled: true
    api_key_env: OPENAI_API_KEY
    default_model: gpt-4

cache:
  enabled: true
  max_size_mb: 512
  ttl_sec: 3600
```

---

### Q4: agentos/daemon/common 与 commons 的关系？

`agentos/daemon/common` 是 `commons` 统一基础库的**兼容层**：

| agentos/daemon/common | commons | 说明 |
|---------------|---------|------|
| `error.h` | `utils/error/error.h` | 错误码兼容映射 |
| `platform.h` | `platform/platform.h` | 平台抽象封装 |
| `svc_logger.h` | `utils/logging/logging.h` | 日志接口封装 |
| `svc_config.h` | `utils/config_unified/` | 配置管理封装 |
| `svc_cache.h` | `utils/cache/cache_common.h` | 缓存接口封装 |
| `ipc_client.c` | - | daemon 特有，IPC 客户端 |
| `svc_common.h` | - | daemon 特有，服务框架 |

新代码建议直接使用 commons 的头文件，agentos/daemon/common 提供向后兼容。

---

## 🔗 相关文档

### 服务详细文档
- [网关服务](gateway_d/README.md) - API 网关和路由
- [LLM 服务](llm_d/README.md) - 多模型推理服务
- [工具服务](tool_d/README.md) - 工具执行引擎
- [市场服务](market_d/README.md) - Agent/技能市场
- [调度服务](sched_d/README.md) - 智能调度中心
- [监控服务](monit_d/README.md) - 全栈监控
- [公共服务](common/README.md) - 兼容层和 IPC 客户端

### 架构文档
- [架构设计原则 Doc V1.7](../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md)
- [内核层架构](../agentos/atoms/README.md)
- [统一基础库](../agentos/commons/README.md)
- [三层认知循环](../agentos/manuals/architecture/coreloopthree.md)
- [四层记忆系统](../agentos/manuals/architecture/memoryrovol.md)
- [系统调用规范](../agentos/manuals/architecture/syscall.md)

### 开发指南
- [快速开始](../agentos/manuals/guides/getting_started.md)
- [创建 Agent](../agentos/manuals/guides/create_agent.md)
- [创建技能](../agentos/manuals/guides/create_skill.md)
- [部署指南](../agentos/manuals/guides/deployment.md)

---

<div align="center">

**AgentOS 用户态服务层 - 为智能体提供强大的后端服务能力**

[返回顶部](#agentos-用户态服务层-daemon)

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
