# LLM Daemon — LLM 服务守护进程

> **模块路径**: `agentos/daemon/llm_d/` | **版本**: v0.1.0

## 概述

`daemon/llm_d/` 是 AgentOS 的大语言模型服务守护进程，提供统一的模型调用接口，屏蔽不同 LLM 提供商的 API 差异。它支持多提供商（OpenAI、Anthropic、DeepSeek、Google、本地模型），提供响应缓存、Token 计数、成本追踪等核心能力，是 AgentOS 智能体与 LLM 交互的关键桥梁。

### 核心职责

- **多提供商支持**：OpenAI、Anthropic、DeepSeek、Google、本地模型等，可扩展的 Provider 注册机制
- **统一接口**：屏蔽不同提供商 API 差异，对外暴露一致的 JSON-RPC 2.0 接口
- **响应缓存**：缓存相同请求的响应，降低延迟和成本
- **Token 计数**：精确计算输入/输出 Token 数量，支持多种模型
- **成本追踪**：记录每次调用的 Token 消耗和费用，支持预算控制
- **流式输出**：支持 Server-Sent Events 流式响应
- **安全集成**：通过 daemon_security 进行输入清洗和权限检查

## 目录结构

```
llm_d/
├── CMakeLists.txt                    # 构建配置
├── README.md                         # 本文件
├── include/                          # 公共头文件
│   ├── llm_service.h                 # LLM 服务对外接口
│   └── llm_svc_adapter.h             # LLM 服务适配器接口
├── src/                              # 实现文件
│   ├── main.c                        # 守护进程入口
│   ├── service.c                     # 服务核心实现
│   ├── service.h                     # 服务内部头文件
│   ├── llm_svc_adapter.c             # 请求解析与标准化适配器
│   ├── cache.h                       # 缓存模块头文件
│   ├── cache.c                       # 响应缓存实现
│   ├── token_counter.h               # Token 计数器头文件
│   ├── token_counter.c               # Token 计数实现
│   ├── cost_tracker.h                # 成本追踪器头文件
│   ├── cost_tracker.c                # 成本追踪实现
│   ├── response.h                    # 响应处理头文件
│   ├── response.c                    # 响应构建与解析
│   └── providers/                    # Provider 适配层
│       ├── provider.h                # Provider 接口定义
│       ├── provider.c                # Provider 基础实现
│       ├── registry.h                # Provider 注册表头文件
│       ├── registry.c                # Provider 注册与查找
│       ├── openai.c                  # OpenAI API 适配
│       ├── anthropic.c               # Anthropic API 适配
│       ├── deepseek.c                # DeepSeek API 适配
│       ├── google.c                  # Google AI API 适配
│       └── local.c                   # 本地模型推理适配
└── tests/                            # 单元测试
    ├── CMakeLists.txt
    ├── test_llm.c                    # 集成测试
    ├── test_service.c                # 服务测试
    ├── test_cache.c                  # 缓存测试
    ├── test_token_counter.c          # Token 计数测试
    ├── test_cost_tracker.c           # 成本追踪测试
    ├── test_response.c               # 响应处理测试
    ├── test_complexity_routing.c     # 复杂度路由测试
    ├── test_routing_e2e.c            # 路由端到端测试
    └── bench_routing_latency.c       # 路由延迟基准测试
```

## 核心组件说明

### 适配器架构

```
客户端请求 (JSON-RPC 2.0)
       ↓
  llm_svc_adapter  ← 请求解析与标准化
       ↓
  llm_service      ← 核心服务逻辑（缓存、限流、路由）
       ↓
  providers/        ← Provider 适配层
    ├─ openai.c      OpenAI API (GPT-4, GPT-3.5)
    ├─ anthropic.c   Anthropic API (Claude)
    ├─ deepseek.c    DeepSeek API
    ├─ google.c      Google AI API (Gemini)
    └─ local.c       本地模型推理
```

### Provider 注册机制

llm_d 采用可扩展的 Provider 注册机制，每个 Provider 实现统一的 `provider.h` 接口，通过 `registry` 动态注册和查找：

| Provider | 文件 | 支持模型 |
|----------|------|----------|
| OpenAI | `openai.c` | GPT-4, GPT-3.5-turbo, GPT-4o 等 |
| Anthropic | `anthropic.c` | Claude 3.5, Claude 3 等 |
| DeepSeek | `deepseek.c` | DeepSeek-V2, DeepSeek-Coder 等 |
| Google | `google.c` | Gemini Pro, Gemini Ultra 等 |
| Local | `local.c` | 本地部署的模型（llama.cpp 等） |

### 缓存模块（cache）

基于请求参数的哈希缓存，避免重复调用 LLM API：

- 支持可配置的缓存容量和 TTL
- 自动淘汰过期缓存项
- 支持手动清除缓存

### Token 计数器（token_counter）

精确计算输入和输出的 Token 数量：

- 支持多种模型的 Token 计数规则
- 用于成本估算和预算控制
- 与 cost_tracker 联动

### 成本追踪器（cost_tracker）

记录每次 LLM 调用的费用：

- 按模型和提供商分别统计
- 支持预算上限设置
- 提供费用查询接口

## 接口说明

### LLM 服务生命周期（llm_service.h）

```c
llm_service_t *llm_service_create(const char *config_path);
void llm_service_destroy(llm_service_t *svc);
```

### 请求接口

```c
int llm_service_complete(llm_service_t *svc, const llm_request_config_t *manager,
                         llm_response_t **out_response);
int llm_service_complete_stream(llm_service_t *svc, const llm_request_config_t *manager,
                                llm_stream_callback_t callback, void *callback_data,
                                llm_response_t **out_response);
void llm_response_free(llm_response_t *resp);
```

### 统计接口

```c
int llm_service_stats(llm_service_t *svc, char **out_json);
```

### 核心数据结构

```c
typedef struct {
    const char *role;       // "system" | "user" | "assistant"
    const char *content;
} llm_message_t;

typedef struct {
    const char *model;
    const llm_message_t *messages;
    size_t message_count;
    float temperature;
    float top_p;
    int max_tokens;
    int stream;
    const char **stop;
    size_t stop_count;
    double presence_penalty;
    double frequency_penalty;
    void *user_data;
} llm_request_config_t;

typedef struct {
    char *id;
    char *model;
    llm_message_t *choices;
    size_t choice_count;
    uint64_t created;
    uint32_t prompt_tokens;
    uint32_t completion_tokens;
    uint32_t total_tokens;
    char *finish_reason;
} llm_response_t;

typedef void (*llm_stream_callback_t)(const char *chunk, void *user_data);
```

### JSON-RPC 2.0 方法

| 方法 | 说明 |
|------|------|
| `llm.generate` | 文本生成（非流式） |
| `llm.chat` | 对话交互（支持流式） |
| `llm.embed` | 文本嵌入向量 |
| `llm.tokenize` | Token 计数 |
| `llm.models` | 可用模型列表 |
| `llm.cache.clear` | 清除响应缓存 |

## 通信方式

| 方向 | 协议 | 说明 |
|------|------|------|
| 入站 | JSON-RPC 2.0 | 通过 IPC Service Bus 接收 gateway_d 转发的请求 |
| 出站 | HTTPS | 调用外部 LLM 提供商 API（OpenAI/Anthropic/DeepSeek/Google） |
| 出站 | 本地调用 | 调用本地模型推理引擎 |

## 依赖关系

```
llm_d
├── common (svc_common, svc_logger, svc_config, svc_cache, ipc_service_bus,
│           method_dispatcher, jsonrpc_helpers, daemon_security, circuit_breaker)
├── cupolas (daemon_security)  # 输入清洗和权限检查
└── 外部 LLM API               # OpenAI/Anthropic/DeepSeek/Google
```

## 构建说明

```bash
# 构建 LLM 守护进程
cmake -B build -DBUILD_TESTS=ON
cmake --build build --target agentos-llm-d

# 运行 LLM 测试
ctest --test-dir build -R "test_llm|test_service|test_cache|test_token_counter|test_cost_tracker|test_response" -V
```

## 使用示例

### 启动 LLM 守护进程

```bash
# 启动 LLM 守护进程
./llm_d --config llm_config.json

# 指定默认模型
./llm_d --default-model gpt-4

# 启用详细日志
./llm_d --verbose
```

### 代码调用示例

```c
#include "daemon/llm_d/include/llm_service.h"

llm_service_t *svc = llm_service_create("llm_config.json");

llm_message_t messages[] = {
    {.role = "system", .content = "You are a helpful assistant."},
    {.role = "user",   .content = "Hello, how are you?"}
};

llm_request_config_t req = {
    .model = "gpt-4",
    .messages = messages,
    .message_count = 2,
    .temperature = 0.7,
    .max_tokens = 1024,
    .stream = 0
};

llm_response_t *resp = NULL;
llm_service_complete(svc, &req, &resp);

printf("Response: %s\n", resp->choices[0].content);
printf("Tokens: prompt=%u, completion=%u, total=%u\n",
       resp->prompt_tokens, resp->completion_tokens, resp->total_tokens);

llm_response_free(resp);
llm_service_destroy(svc);
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
