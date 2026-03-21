# AgentOS 服务层 (Backs)

<div align="center">

[![Version](https://img.shields.io/badge/version-1.0.0.5-blue.svg)](https://gitee.com/spharx/agentos)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)

**AgentOS 核心服务层 - 提供智能体运行时所需的全部后端服务**

*"Modular services, independent processes, seamless communication."*

</div>

---

## 📋 项目简介

Backs 是 AgentOS 的服务层，提供 LLM 推理、工具执行、市场管理、任务调度、系统监控等核心服务。每个服务以独立进程运行，通过 HTTP/gRPC 与内核及其他服务通信，实现高度模块化和可扩展性。

### 核心价值

- **微服务架构**: 每个服务独立进程，故障隔离，易于维护和扩展
- **统一接口**: 标准化 RESTful API 和 gRPC 接口，支持跨语言调用
- **高性能通信**: 基于 libevent 的事件驱动架构，支持高并发请求
- **可观测性**: 集成 OpenTelemetry，支持全链路追踪和指标监控
<!-- From data intelligence emerges. by spharx -->
- **热插拔**: 支持服务动态加载和卸载，无需重启系统
- **多提供者支持**: LLM 服务支持 OpenAI、Anthropic、DeepSeek 等多模型提供商

---

## 🏗️ 架构说明

### 服务架构图

```
┌─────────────────────────────────────────────────────────┐
│              AgentOS Backs (服务层)                      │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐             │
│  │  llm_d   │  │  tool_d  │  │ market_d │             │
│  │ LLM 服务  │  │ 工具服务  │  │ 市场服务  │             │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘             │
│       │             │             │                    │
│       └─────────────┼─────────────┘                    │
│                     │                                  │
│              ┌──────▼──────┐                          │
│              │ common libs │ ← 公共库 (IPC/日志/配置)   │
│              └──────┬──────┘                          │
│                     │                                  │
│  ┌──────────┐  ┌────▼──────┐  ┌──────────┐            │
│  │ sched_d  │  │ monit_d   │  │ perm_d*  │            │
│  │ 调度服务  │  │ 监控服务  │  │ 权限服务  │            │
│  └──────────┘  └───────────┘  └──────────┘            │
│                                                         │
└─────────────────────────────────────────────────────────┘
                          ↕
┌─────────────────────────────────────────────────────────┐
│           AgentOS Atoms (内核层) via Syscall           │
└─────────────────────────────────────────────────────────┘
```

### 目录结构

```
backs/
├── README.md                 # 本文档
├── CMakeLists.txt           # 顶层构建文件
│
├── common/                  # 公共服务库
│   ├── include/
│   │   └── svc_common.h     # 公共头文件
│   ├── src/
│   │   ├── ipc_client.c     # IPC 客户端
│   │   ├── config.c         # 配置管理
│   │   └── logger.c         # 日志记录
│   └── CMakeLists.txt
│
├── llm_d/                   # LLM 服务守护进程 ⭐
│   ├── include/
│   │   └── llm_service.h    # LLM 服务接口
│   ├── src/
│   │   ├── main.c           # 程序入口
│   │   ├── service.c        # 服务主逻辑
│   │   ├── providers/       # 模型提供商适配层
│   │   │   ├── openai.c     # OpenAI API
│   │   │   ├── anthropic.c  # Anthropic API
│   │   │   ├── deepseek.c   # DeepSeek API
│   │   │   ├── local.c      # 本地模型
│   │   │   └── provider.c   # 提供商基类
│   │   ├── cache.c          # 响应缓存
│   │   ├── cost_tracker.c   # 成本追踪
│   │   ├── token_counter.c  # Token 计数
│   │   └── response.c       # 响应处理
│   └── CMakeLists.txt
│
├── tool_d/                  # 工具服务守护进程
│   ├── include/
│   │   └── tool_service.h   # 工具服务接口
│   ├── src/
│   │   ├── registry.c       # 工具注册表
│   │   ├── executor.c       # 工具执行器
│   │   ├── validator.c      # 参数验证器
│   │   ├── cache.c          # 工具缓存
│   │   └── config.c         # 配置管理
│   └── CMakeLists.txt
│
├── market_d/                # 市场服务守护进程
│   ├── include/
│   │   └── market_service.h # 市场服务接口
│   ├── src/
│   │   ├── agent_registry.c # Agent 注册表
│   │   ├── skill_registry.c # 技能注册表
│   │   ├── installer.c      # 安装管理器
│   │   └── publisher.c      # 发布管理器
│   └── CMakeLists.txt
│
├── sched_d/                 # 调度服务守护进程
│   ├── include/
│   │   ├── scheduler_service.h  # 调度服务接口
│   │   └── strategy_interface.h # 策略接口
│   ├── src/
│   │   ├── strategies/      # 调度策略实现
│   │   │   ├── round_robin.c # 轮询策略
│   │   │   ├── weighted.c   # 加权策略
│   │   │   └── ml_based.c   # ML 策略
│   │   └── monitor.c        # 负载监控
│   └── CMakeLists.txt
│
└── monit_d/                 # 监控服务守护进程
    ├── include/
    │   └── monitor_service.h # 监控服务接口
    ├── src/
    │   ├── metrics.c        # 指标采集
    │   ├── logging.c        # 日志聚合
    │   ├── tracing.c        # 链路追踪
    │   └── alert.c          # 告警管理
    └── CMakeLists.txt
```

---

## 🔧 核心服务详解

### 1. LLM 服务 (llm_d)

**功能**: 提供统一的大模型推理接口，支持多模型提供商和成本控制

**核心特性**:
- ✅ 多提供商支持：OpenAI GPT、Anthropic Claude、DeepSeek、本地模型
- ✅ 智能缓存：响应缓存降低 API 调用成本
- ✅ 成本追踪：实时统计 token 消耗和费用
- ✅ Token 计数：精确计算输入/输出 token 数
- ✅ 流式响应：支持 SSE 流式输出
- ✅ 自动重试：网络错误自动重试机制

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

### 2. 工具服务 (tool_d)

**功能**: 提供工具注册、验证和执行服务

**核心特性**:
- ✅ 工具注册：动态注册/注销工具
- ✅ 参数验证：JSON Schema 验证
- ✅ 沙箱执行：安全隔离的工具执行环境
- ✅ 结果缓存：缓存常用工具执行结果

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

### 3. 市场服务 (market_d)

**功能**: Agent 和技能的注册、安装、发布管理

**核心特性**:
- ✅ Agent 注册表：维护可用 Agent 列表
- ✅ 技能注册表：维护可用技能列表
- ✅ 版本管理：语义化版本控制
- ✅ 依赖解析：自动解析和安装依赖

### 4. 调度服务 (sched_d)

**功能**: 任务和资源的智能调度

**核心特性**:
- ✅ 多种策略：轮询、加权、ML 预测
- ✅ 负载均衡：基于实时负载的动态分配
- ✅ 优先级调度：支持任务优先级队列
- ✅ 策略插件：可插拔的调度策略

### 5. 监控服务 (monit_d)

**功能**: 系统监控、日志聚合和告警

**核心特性**:
- ✅ 指标采集：CPU、内存、磁盘、网络
- ✅ 日志聚合：集中式日志管理
- ✅ 链路追踪：OpenTelemetry 集成
- ✅ 告警管理：阈值告警和通知

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

#### 2. 仅构建 backs 服务层

```bash
cd backs
mkdir build && cd build

# 配置
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=OFF

# 编译
cmake --build . --parallel $(nproc)

# 产物位于 build/ 目录
```

### 编译选项

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
./build/llm_d/agentos-llm-d --config ../config/services/llm_d.yaml
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
ExecStart=/opt/agentos/bin/agentos-llm-d --config /etc/agentos/llm_d.yaml
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
    'http://localhost:8081/v1/tools/execute',
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
    "github.com/spharx/agentos-go-sdk/llm"
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
cd backs/build
ctest --output-on-failure

# 运行特定服务测试
ctest -R llm_d --verbose
ctest -R tool_d --verbose

# 集成测试
../tests/integration/test_services.sh
```

---

## 📄 许可证

Backs 服务层采用 **Apache License 2.0** 开源协议。

详见项目根目录 [LICENSE](../../LICENSE) 文件。

---

## 🔗 相关文档

- [内核层架构](../atoms/README.md)
- [LLM 服务详细设计](llm_d/README.md)
- [工具服务详细设计](tool_d/README.md)
- [市场服务详细设计](market_d/README.md)
- [调度服务详细设计](sched_d/README.md)
- [监控服务详细设计](monit_d/README.md)
- [系统调用文档](../partdocs/architecture/syscall.md)

---

<div align="center">

**AgentOS Backs - 为智能体提供强大的后端服务能力**

[返回顶部](#agentos-服务层 -backs)

© 2026 SPHARX Ltd. All Rights Reserved.

</div>