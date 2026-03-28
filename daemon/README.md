# AgentOS 服务层 (daemon)

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.6-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://gitee.com/spharx/agentos)
[![Coverage](https://img.shields.io/badge/coverage-85%25-brightgreen.svg)](https://gitee.com/spharx/agentos)

**AgentOS 核心服务层 - 提供智能体运行时所需的全部后端服务**

*"Modular services, independent processes, seamless communication."*

</div>

---

## 📋 项目简介

`daemon/` 是 AgentOS 的**服务层（Service Layer）**，位于内核层 (`atoms/`) 之上、应用层 (`openlab/`) 之下，为智能体提供完整的后端服务支持。

服务层采用**微服务架构**，每个服务以独立进程运行，通过系统调用与内核通信，通过 IPC/RPC 与其他服务协作，实现高度模块化、可扩展和故障隔离。

### 核心价值

- **微服务架构**: 每个服务独立进程，故障隔离，易于维护和扩展
- **统一接口**: 标准化 RESTful API 和 gRPC 接口，支持跨语言调用
- **高性能通信**: 基于 libevent 的事件驱动架构，支持高并发请求
- **可观测性**: 集成 OpenTelemetry，支持全链路追踪和指标监控
- **热插拔**: 支持服务动态加载和卸载，无需重启系统
- **多提供者支持**: LLM 服务支持 OpenAI、Anthropic、DeepSeek 等多模型提供商

---

## 🏗️ 架构说明

### 服务层在 AgentOS 中的位置

```
┌────────────────────────────────────────────────────────────────────────────┐
│                   AgentOS 总体架构                                          │
└────────────────────────────────────────────────────────────────────────────┘
│                                                                            │
│ ┌──────────────────────────────────────────────────────────────────────┐ │
│ │             应用层 (openlab)                                          │ │
│ │ docgen | ecommerce | research | videoedit | ...                      │ │
│ └──────────────────────────────────────────────────────────────────────┘ │
│                          ↓↑                                              │
│ ┌──────────────────────────────────────────────────────────────────────┐ │
│ │            服务层 (daemon)  ← 本层                                      │ │
│ │ llm_d | market_d | monit_d | sched_d | tool_d                        │ │
│ └──────────────────────────────────────────────────────────────────────┘ │
│                          ↓↑                                              │
│ ┌──────────────────────────────────────────────────────────────────────┐ │
│ │             内核层 (atoms)                                            │ │
│ │ corekern | coreloopthree | memoryrovol | syscall | cupolas          │ │
│ └──────────────────────────────────────────────────────────────────────┘ │
│                                                                            │
│ ┌──────────────────────────────────────────────────────────────────────┐ │
│ │            SDK 层 (toolkit)                                             │ │
│ │ Go | Python | Rust | TypeScript                                      │ │
│ └──────────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────────┘
```

### 服务架构图

```
┌──────────────────────────────────────────────────────────┐
│              AgentOS daemon (服务层)                       │
├──────────────────────────────────────────────────────────┤
│                                                          │
│ ┌──────────┐ ┌──────────┐ ┌──────────┐                 │
│ │  llm_d   │ │  tool_d  │ │ market_d │                 │
│ │  LLM 服务  │ │ 工具服务  │ │ 市场服务  │                 │
│ └────┬─────┘ └────┬─────┘ └────┬─────┘                 │
│      │            │            │                        │
│      └─────────────┼────────────┘                        │
│                    │                                     │
│             ┌──────▼──────┐                             │
│             │  common/    │ │ 公共服务库                 │
│             │  (IPC/日志/缓存/配置)                      │
│             └──────┬──────┘                             │
│                    │                                     │
│ ┌──────────┐ ┌────▼──────┐ ┌──────────┐                │
│ │ sched_d  │ │  monit_d  │ │ perm_d*  │                │
│ │ 调度服务  │ │ 监控服务  │ │ 权限服务  │                │
│ └──────────┘ └───────────┘ └──────────┘                │
│                                                          │
└──────────────────────────────────────────────────────────┘
                          ↓↑ (通过 syscall 系统调用)
┌──────────────────────────────────────────────────────────┐
│          AgentOS Atoms (内核层)                           │
│     corekern | coreloopthree | memoryrovol              │
└──────────────────────────────────────────────────────────┘
```

### 目录结构

```
daemon/
├── README.md                 # 本文档
├── CMakeLists.txt           # 顶层构建文件
├── Dockerfile.ci            # CI/CD 容器镜像
├── cppcheck.xml             # 静态分析配置
├── scripts/                 # 脚本工具
│   ├── ci.sh               # CI/CD 脚本
│   ├── local-ci.sh         # 本地 CI 验证
│   ├── static-analysis.sh  # 静态分析
│   └── verify-coverage.sh  # 覆盖率验证
│
├── common/                  # 公共服务库 ⭐
│   ├── README.md
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── compat.h        # 平台兼容头文件
│   │   ├── error.h         # 错误处理接口
│   │   ├── platform.h      # 平台抽象接口
│   │   ├── svc_cache.h     # 通用缓存接口
│   │   ├── svc_common.h    # 公共服务接口
│   │   ├── svc_config.h    # 配置管理接口
│   │   └── svc_logger.h    # 日志记录接口
│   ├── src/
│   │   ├── config.c        # YAML 配置解析
│   │   ├── error.c         # 统一错误处理
│   │   ├── ipc_client.c    # IPC 客户端
│   │   ├── logger.c        # 结构化日志
│   │   ├── platform.c      # 平台抽象实现
│   │   └── svc_cache.c     # LRU 缓存实现
│   └── tests/              # 单元测试
│       ├── test_config.c
│       ├── test_error.c
│       ├── test_ipc_client.c
│       ├── test_logger.c
│       └── test_platform.c
│
├── llm_d/                   # LLM 服务守护进程 ⭐
│   ├── README.md
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── llm_service.h   # LLM 服务接口
│   ├── src/
│   │   ├── main.c          # 程序入口
│   │   ├── service.c       # 服务主逻辑
│   │   ├── service.h       # 服务接口
│   │   ├── cache.c         # 响应缓存 (LRU)
│   │   ├── cache.h
│   │   ├── response.c      # 响应处理
│   │   ├── response.h
│   │   ├── cost_tracker.c  # 成本追踪
│   │   ├── token_counter.c # Token 计数
│   │   └── providers/      # 模型提供商适配器
│   │       ├── provider.h  # 提供商接口
│   │       ├── provider.c  # 通用实现
│   │       ├── openai.c    # OpenAI API
│   │       ├── anthropic.c # Anthropic API
│   │       ├── deepseek.c  # DeepSeek API
│   │       ├── local.c     # 本地模型
│   │       └── registry.c  # 提供商注册表
│   └── tests/              # 单元测试
│       ├── test_llm.c
│       ├── test_service.c
│       ├── test_cache.c
│       ├── test_response.c
│       ├── test_cost_tracker.c
│       └── test_token_counter.c
│
├── tool_d/                  # 工具服务守护进程
│   ├── README.md
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── tool_service.h  # 工具服务接口
│   ├── src/
│   │   ├── main.c          # 程序入口
│   │   ├── service.c       # 服务主逻辑
│   │   ├── service.h
│   │   ├── registry.c      # 工具注册表
│   │   ├── registry.h
│   │   ├── executor.c      # 工具执行器
│   │   ├── executor.h
│   │   ├── validator.c     # 参数验证器
│   │   ├── validator.h
│   │   ├── cache.c         # 工具缓存
│   │   ├── cache.h
│   │   ├── config.c        # 配置管理
│   │   ├── config.h
│   │   └── utils/
│   │       └── tool_errors.h # 工具错误码
│   └── tests/              # 单元测试
│       ├── test_tool.c
│       ├── test_service.c
│       ├── test_registry.c
│       ├── test_executor.c
│       ├── test_validator.c
│       └── test_cache.c
│
├── market_d/                # 市场服务守护进程
│   ├── README.md
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── market_service.h # 市场服务接口
│   ├── src/
│   │   ├── main.c          # 程序入口
│   │   ├── agent_registry.c # Agent 注册表
│   │   ├── skill_registry.c # 技能注册表
│   │   ├── installer.c     # 安装管理器
│   │   └── publisher.c     # 发布管理器
│   └── tests/              # 单元测试
│       ├── test_market.c
│       ├── test_agent_registry.c
│       ├── test_skill_registry.c
│       └── test_installer.c
│
├── sched_d/                 # 调度服务守护进程
│   ├── README.md
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── scheduler_service.h # 调度服务接口
│   │   └── strategy_interface.h # 策略接口
│   ├── src/
│   │   ├── main.c          # 程序入口
│   │   ├── monitor.c       # 负载监控
│   │   └── strategies/     # 调度策略实现
│   │       ├── round_robin.c # 轮询策略
│   │       ├── weighted.c    # 加权策略
│   │       └── ml_based.c    # ML 预测策略
│   └── tests/              # 单元测试
│       ├── test_scheduler.c
│       └── test_strategies.c
│
└── monit_d/                 # 监控服务守护进程
    ├── README.md
    ├── CMakeLists.txt
    ├── include/
    │   └── monitor_service.h # 监控服务接口
    ├── src/
    │   ├── main.c          # 程序入口
    │   ├── metrics.c       # 指标采集
    │   ├── logging.c       # 日志聚合
    │   ├── tracing.c       # 链路追踪
    │   └── alert.c         # 告警管理
    └── tests/              # 单元测试
        ├── test_monitor.c
        ├── test_metrics.c
        ├── test_logging.c
        ├── test_tracing.c
        └── test_alert.c
```

**注**: `perm_d*` (权限服务) 为规划中服务，当前由 `cupolas/` 安全穹顶提供权限裁决功能。

---

## 🔧 核心服务详解

### 1. LLM 服务 (llm_d) ⭐

**功能**: 提供统一的大模型推理接口，支持多模型提供商和成本控制

**核心职责**:
- 接收认知层 (`coreloopthree/`) 的推理请求
- 调用合适的模型提供商 API
- 缓存响应以降低 API 调用成本
- 追踪 token 消耗和费用

**核心特性**:
- ✅ **多提供商支持**: OpenAI GPT、Anthropic Claude、DeepSeek、本地模型
- ✅ **智能缓存**: LRU 响应缓存，降低 API 调用成本
- ✅ **成本追踪**: 实时统计 token 消耗和费用
- ✅ **Token 计数**: 精确计算输入输出 token 数
- ✅ **流式响应**: 支持 SSE 流式输出
- ✅ **自动重试**: 网络错误自动重试机制
- ✅ **Provider 抽象**: 统一的提供商接口，易于扩展新模型

**架构设计**:
```
┌──────────────────────────────────────────────────────────┐
│                    llm_d                                  │
├──────────────────────────────────────────────────────────┤
│  ┌────────────────────────────────────────────────────┐ │
│  │              service.c (服务主逻辑)                 │ │
│  │  • 请求路由  • 缓存管理  • 成本追踪                │ │
│  └────────────────────────────────────────────────────┘ │
│                          ↓                               │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│  │ openai.c │ │anthropic.c│ │deepseek.c│ │ local.c  │  │
│  │          │ │          │ │          │ │          │  │
│  │ Provider 抽象层 (统一接口)                            │  │
│  └──────────┴──────────────┴──────────┴──────────────┘  │
└──────────────────────────────────────────────────────────┘
```

**API 示例**:
```bash
# 文本生成
curl -X POST http://localhost:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4",
    "messages": [{"role": "user", "content": "Hello!"}]
  }'

# 嵌入向量
curl -X POST http://localhost:8080/v1/embeddings \
  -H "Content-Type: application/json" \
  -d '{
    "model": "text-embedding-3-small",
    "input": "AgentOS is awesome"
  }'
```

---

### 2. 工具服务 (tool_d)

**功能**: 提供工具注册、验证和执行服务

**核心职责**:
- 维护可用工具注册表
- 验证工具调用参数
- 在沙箱环境中执行工具
- 缓存工具执行结果

**核心特性**:
- ✅ **工具注册**: 动态注册/注销工具
- ✅ **参数验证**: JSON Schema 验证
- ✅ **沙箱执行**: 安全隔离的工具执行环境
- ✅ **结果缓存**: 缓存常用工具执行结果
- ✅ **跨平台**: 支持 Windows/Linux/macOS

**工具示例**:
```json
{
  "name": "web_search",
  "description": "Search the web",
  "parameters": {
    "type": "object",
    "properties": {
      "query": {"type": "string"}
    },
    "required": ["query"]
  }
}
```

---

### 3. 市场服务 (market_d)

**功能**: Agent 和技能的注册、安装、发布管理

**核心职责**:
- 维护可用 Agent 注册表
- 维护可用技能注册表
- 管理 Agent/技能的安装和卸载
- 处理版本依赖解析

**核心特性**:
- ✅ **Agent 注册表**: 维护可用 Agent 列表
- ✅ **技能注册表**: 维护可用技能列表
- ✅ **版本管理**: 语义化版本控制
- ✅ **依赖解析**: 自动解析和安装依赖

---

### 4. 调度服务 (sched_d)

**功能**: 任务和资源的智能调度

**核心职责**:
- 接收行动层 (`coreloopthree/`) 的任务调度请求
- 根据负载情况选择合适的 Agent
- 实现多种调度策略
- 监控服务负载和健康状态

**核心特性**:
- ✅ **多种策略**: 轮询、加权、ML 预测
- ✅ **负载均衡**: 基于实时负载的动态分配
- ✅ **优先级调度**: 支持任务优先级队列
- ✅ **策略插件**: 可插拔的调度策略

---

### 5. 监控服务 (monit_d)

**功能**: 系统监控、日志聚合和告警

**核心职责**:
- 采集系统指标 (CPU/内存/磁盘/网络)
- 聚合各服务日志
- 实现链路追踪 (OpenTelemetry)
- 管理阈值告警和通知

**核心特性**:
- ✅ **指标采集**: CPU、内存、磁盘、网络
- ✅ **日志聚合**: 集中式日志管理
- ✅ **链路追踪**: OpenTelemetry 集成
- ✅ **告警管理**: 阈值告警和通知

---

## 🚀 构建指南

### 环境要求

- **操作系统**: Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2)
- **编译器**: GCC 11+ 或 Clang 14+
- **构建工具**: CMake 3.20+, Ninja 或 Make
- **依赖库**:
  - libcurl >= 7.68 (HTTP 客户端)
  - libcjson >= 1.7.15 (JSON 解析)
  - libyaml >= 0.2.5 (YAML 解析)
  - libevent >= 2.1.12 (事件循环)
  - pthread (线程库)
  - tiktoken (Token 分词，可选)

### 构建步骤

#### 1. 从项目根目录构建（推荐）

```bash
# 在项目根目录 (AgentOS/)
mkdir build && cd build

# 配置 CMake
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=OFF \
  -DENABLE_TRACING=ON

# 编译
cmake --build . --parallel $(nproc)

# 安装 (可选)
sudo cmake --install .
```

#### 2. 仅构建 daemon 服务层

```bash
cd daemon
mkdir build && cd build

# 配置
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=OFF

# 编译
cmake --build . --parallel $(nproc)

# 产物位于 build/ 目录
```

### CMake 编译选项

| CMake 变量 | 说明 | 默认值 |
| :--- | :--- | :--- |
| `CMAKE_BUILD_TYPE` | Debug/Release/RelWithDebInfo | `Release` |
| `BUILD_TESTS` | 构建单元测试 | `OFF` |
| `ENABLE_TRACING` | 启用 OpenTelemetry 追踪 | `OFF` |

---

## 💻 使用示例

### 启动服务

#### 单独启动 LLM 服务

```bash
# 设置环境变量
export OPENAI_API_KEY="your-api-key"
export ANTHROPIC_API_KEY="your-api-key"

# 启动服务
./build/llm_d/agentos-llm-d --manager ../manager/service/llm_d/llm.yaml
```

#### 使用 systemd 管理服务（Linux）

```ini
# /etc/systemd/system/agentos-llm-d.service
[Unit]
Description=AgentOS LLM Service
After=network.target

[Service]
Type=simple
User=agentos
ExecStart=/opt/agentos/bin/agentos-llm-d --manager /etc/agentos/llm_d.yaml
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
# 启用并启动服务
sudo systemctl enable agentos-llm-d
sudo systemctl start agentos-llm-d
sudo systemctl status agentos-llm-d
```

### Python SDK 调用示例

```python
import requests

# LLM 服务调用
response = requests.post(
    'http://localhost:8080/v1/chat/completions',
    json={
        'model': 'gpt-4',
        'messages': [{'role': 'user', 'content': 'Hello!'}]
    }
)
print(response.json())

# 工具服务调用
result = requests.post(
    'http://localhost:8081/v1/toolkit/execute',
    json={
        'tool_name': 'web_search',
        'parameters': {'query': 'AgentOS latest version'}
    }
)
print(result.json())
```

### Go SDK 调用示例

```go
package main

import (
    "fmt"
    "github.com/spharx/agentos-go-toolkit/llm"
)

func main() {
    client := llm.NewClient("http://localhost:8080")
    
    resp, err := client.ChatCompletion(&llm.ChatRequest{
        Model: "gpt-4",
        Messages: []llm.Message{
            {Role: "user", Content: "Hello!"},
        },
    })
    
    if err != nil {
        panic(err)
    }
    
    fmt.Println(resp.Choices[0].Message.Content)
}
```

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM):

| 服务 | 指标 | 数值 | 测试条件 |
| :--- | :--- | :--- | :--- |
| **llm_d** | 请求延迟 | < 50ms | 不含模型推理时间 |
| | 并发连接 | 1000+ | 每服务实例 |
| | 缓存命中率 | > 80% | 重复查询场景 |
| **tool_d** | 执行延迟 | < 10ms | 简单工具 |
| | 并发执行 | 100+ | 每服务实例 |
| **sched_d** | 调度延迟 | < 1ms | 单次调度决策 |
| | 吞吐量 | 10000+ | 任务/秒 |
| **monit_d** | 指标采集频率 | 1s | 默认间隔 |
| | 日志写入吞吐 | 50MB/s | 压缩后 |

---

## 🔒 安全性

### 服务间认证

- **mTLS**: 服务间通信使用双向 TLS
- **JWT**: API 请求使用 JWT 令牌认证
- **API Key**: 外部访问使用 API Key 鉴权

### 沙箱隔离

- **工具沙箱**: 工具在隔离环境中执行
- **资源限制**: cgroups 限制 CPU/内存使用
- **网络隔离**: 网络命名空间隔离

---

## 🧪 测试

```bash
# 运行服务测试
cd daemon/build
ctest --output-on-failure

# 运行特定服务测试
ctest -R llm_d --verbose
ctest -R tool_d --verbose

# 集成测试
../tests/integration/test_services.sh
```

---

## 📄 许可证

daemon 服务层采用 **Apache License 2.0** 开源协议。

详见项目根目录的 [LICENSE](../../LICENSE) 文件。

---

## 🔗 相关文档

### 项目文档

- [内核层架构](../atoms/README.md)
- [系统调用规范](../manuals/architecture/folder/syscall.md)
- [CoreLoopThree 架构](../manuals/architecture/folder/coreloopthree.md)
- [MemoryRovol 架构](../manuals/architecture/folder/memoryrovol.md)
- [cupolas 安全穹顶](../cupolas/README.md)

### 服务详细文档

- [LLM 服务详细设计](llm_d/README.md)
- [工具服务详细设计](tool_d/README.md)
- [市场服务详细设计](market_d/README.md)
- [调度服务详细设计](sched_d/README.md)
- [监控服务详细设计](monit_d/README.md)

### 开发指南

- [快速开始](../manuals/guides/folder/getting_started.md)
- [创建 Agent](../manuals/guides/folder/create_agent.md)
- [创建技能](../manuals/guides/folder/create_skill.md)
- [部署指南](../manuals/guides/folder/deployment.md)

---

<div align="center">

**AgentOS daemon - 为智能体提供强大的后端服务能力**

[返回顶部](#agentos-服务层-daemon)

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
