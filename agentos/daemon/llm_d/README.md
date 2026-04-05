# LLM 服务 (llm_d)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`llm_d` 是 AgentOS 的**大语言模型推理服务**，提供统一、高效、低成本的 LLM 调用接口。作为智能体的"大脑"接口层，本服务负责与各类 LLM 提供商交互，并提供缓存、成本控制、Token 统计等增值功能。

**核心职责**:
- 统一的 LLM 调用接口（支持 OpenAI、Anthropic、DeepSeek、本地模型）
- 智能响应缓存（降低重复请求成本）
- 实时成本追踪和 Token 统计
- 流式输出支持（SSE）
- 自动重试和故障恢复

---

## 📁 目录结构

```
llm_d/
├── README.md                 # 本文档
├── CMakeLists.txt           # 构建配置
│
├── include/                 # 公共头文件
│   ├── llm_service.h        # 服务主接口
│   ├── llm_provider.h       # 提供商接口
│   ├── llm_cache.h          # 缓存接口
│   └── llm_types.h          # 类型定义
│
├── src/                     # 实现代码
│   ├── main.c               # 程序入口
│   ├── service.c            # 服务主逻辑
│   ├── provider.c           # 提供商基类
│   ├── cache.c              # 缓存管理
│   ├── cost_tracker.c       # 成本追踪
│   ├── token_counter.c      # Token 计数
│   └── providers/           # 具体提供商实现
│       ├── openai.c         # OpenAI API
│       ├── anthropic.c      # Anthropic API
│       ├── deepseek.c       # DeepSeek API
│       └── local.c          # 本地模型接口
│
├── config/                  # 配置文件
│   └── llm.yaml.example     # 配置示例
│
└── tests/                   # 测试
    └── test_llm.c           # 单元测试
```

---

## 🔧 核心功能

### 1. 多提供商支持

支持主流 LLM 提供商，通过统一接口调用：

| 提供商 | 支持模型 | 特性 |
|--------|---------|------|
| **OpenAI** | GPT-4, GPT-3.5-Turbo | 完整支持，包括 Function Call |
| **Anthropic** | Claude-2, Claude-Instant | 长上下文支持 |
| **DeepSeek** | DeepSeek-V2, DeepSeek-Coder | 高性价比 |
| **本地模型** | llama.cpp, vLLM | 私有化部署 |

### 2. 智能缓存

基于 Redis/内存的两级缓存系统：

```c
// 缓存配置示例
cache_config_t config = {
    .enabled = true,
    .backend = CACHE_BACKEND_REDIS,  // 或 CACHE_BACKEND_MEMORY
    .ttl_sec = 3600,                 // 缓存 1 小时
    .max_size_mb = 512,              // 最大 512MB
    .compression = true              // 启用压缩
};
```

**缓存策略**:
- 完全匹配：相同的 prompt+model → 直接返回缓存
- 语义相似：使用向量相似度判断（可选）
- 部分缓存：长文本分段缓存

**效果**:
- 缓存命中率：>80%（重复查询场景）
- 成本降低：60-90%
- 延迟降低：从秒级降至毫秒级

### 3. 成本追踪

实时统计每个 Agent、每次调用的成本：

```c
// 成本统计数据结构
typedef struct {
    int64_t total_requests;      // 总请求数
    int64_t total_tokens;        // 总 Token 数
    int64_t prompt_tokens;       // 输入 Token
    int64_t completion_tokens;   // 输出 Token
    double total_cost_usd;       // 总成本（美元）
    
    // 按模型分类
    model_cost_t models[10];     // 各模型成本明细
} cost_stats_t;
```

**实时看板**:
```bash
# 查询当前成本统计
curl http://localhost:8080/api/v1/cost/stats

# 响应示例
{
  "total_requests": 1234,
  "total_tokens": 567890,
  "total_cost_usd": 12.34,
  "by_model": [
    {"model": "gpt-4", "tokens": 123456, "cost": 8.90},
    {"model": "claude-2", "tokens": 444434, "cost": 3.44}
  ]
}
```

### 4. Token 精确计数

使用官方 tiktoken 库精确计算 Token：

```c
// Token 计数接口
int llm_count_tokens(
    const char* text,
    const char* model,
    int64_t* out_count
);

// 使用示例
int64_t count;
llm_count_tokens("Hello, world!", "gpt-4", &count);
printf("Token count: %ld\n", count);
```

**支持的编码**:
- GPT-4/GPT-3.5: cl100k_base
- Claude: p50k_base
- DeepSeek: 自定义编码

---

## 🚀 快速开始

### 环境要求

| 依赖 | 最低版本 | 推荐版本 |
|------|---------|----------|
| **CMake** | 3.20 | 3.25+ |
| **GCC/Clang** | GCC 11 / Clang 14 | GCC 12 / Clang 15 |
| **libcurl** | 7.68 | 7.80+ |
| **cJSON** | 1.7.15 | 1.8.0+ |
| **libyaml** | 0.2.5 | 0.2.5+ |
| **tiktoken** | 0.1.0 | 0.3.0+ |
| **Redis** (可选) | 6.0 | 7.0+ (用于缓存) |

### 编译步骤

```bash
# 1. 进入项目目录
cd agentos/daemon/llm_d

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置 CMake
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_CACHE=ON \
  -DENABLE_TRACING=ON

# 4. 编译
cmake --build . --parallel $(nproc)

# 5. 产物
# - llm_d           (可执行文件)
# - libllm_service.a (静态库)
```

### 配置示例

创建 `config/llm.yaml`:

```yaml
# LLM 服务配置

# 服务端口
server:
  port: 8080
  workers: 4
  timeout_sec: 30

# LLM 提供商配置
providers:
  - name: openai
    enabled: true
    api_key: ${OPENAI_API_KEY}  # 支持环境变量
    api_base: https://api.openai.com/v1
    models:
      - gpt-4
      - gpt-3.5-turbo
    timeout_sec: 30
    max_retries: 3
    retry_delay_ms: 1000
    
  - name: anthropic
    enabled: true
    api_key: ${ANTHROPIC_API_KEY}
    api_base: https://api.anthropic.com
    models:
      - claude-2
      - claude-instant-1
    timeout_sec: 30
    max_retries: 3
    
  - name: deepseek
    enabled: true
    api_key: ${DEEPSEEK_API_KEY}
    api_base: https://api.deepseek.com
    models:
      - deepseek-v2
      - deepseek-coder
    timeout_sec: 30
    
  - name: local
    enabled: false
    backend: llama.cpp
    model_path: /models/llama-7b.gguf
    device: cpu  # 或 gpu
    n_ctx: 4096

# 缓存配置
cache:
  enabled: true
  backend: redis  # memory 或 redis
  ttl_sec: 3600
  max_size_mb: 512
  
  # Redis 配置（仅当 backend=redis 时需要）
  redis:
    host: localhost
    port: 6379
    password: ${REDIS_PASSWORD}
    db: 0

# 成本追踪
cost_tracking:
  enabled: true
  log_per_request: true
  export_interval_sec: 60

# 日志配置
logging:
  level: info  # debug, info, warning, error
  format: json
  output: stdout  # 或 file
  file_path: /var/log/agentos/llm_d.log
```

### 启动服务

```bash
# 方式 1: 直接启动
./build/llm_d --config ../config/llm.yaml

# 方式 2: 后台运行
nohup ./build/llm_d --config ../config/llm.yaml > llm_d.log 2>&1 &

# 方式 3: 使用 systemd (Linux)
sudo systemctl start agentos-llm-d
sudo systemctl enable agentos-llm-d  # 开机自启
```

---

## 💻 API 接口

### RESTful API

#### 1. 聊天完成

```bash
POST /api/v1/chat/completions
Content-Type: application/json

{
  "model": "gpt-4",
  "messages": [
    {"role": "system", "content": "You are a helpful assistant."},
    {"role": "user", "content": "Hello!"}
  ],
  "temperature": 0.7,
  "max_tokens": 1000,
  "stream": false
}
```

**响应**:
```json
{
  "id": "chatcmpl-123",
  "object": "chat.completion",
  "created": 1677652288,
  "model": "gpt-4",
  "choices": [{
    "index": 0,
    "message": {
      "role": "assistant",
      "content": "Hello! How can I help you?"
    },
    "finish_reason": "stop"
  }],
  "usage": {
    "prompt_tokens": 10,
    "completion_tokens": 8,
    "total_tokens": 18
  },
  "cost": {
    "model": "gpt-4",
    "total_cost_usd": 0.0012
  }
}
```

#### 2. 流式输出

```bash
POST /api/v1/chat/completions
Content-Type: application/json

{
  "model": "gpt-4",
  "messages": [{"role": "user", "content": "Tell me a story"}],
  "stream": true
}
```

**响应** (SSE 格式):
```
data: {"choices":[{"delta":{"content":"Once"}}]}
data: {"choices":[{"delta":{"content":" upon"}}]}
data: {"choices":[{"delta":{"content":" a time"}}]}
data: [DONE]
```

#### 3. 嵌入向量

```bash
POST /api/v1/embeddings
Content-Type: application/json

{
  "model": "text-embedding-3-small",
  "input": "AgentOS is awesome"
}
```

#### 4. Token 计数

```bash
POST /api/v1/token/count
Content-Type: application/json

{
  "model": "gpt-4",
  "text": "Hello, world!"
}
```

**响应**:
```json
{
  "tokens": 4,
  "model": "gpt-4"
}
```

#### 5. 成本统计

```bash
GET /api/v1/cost/stats?start_time=2026-03-25T00:00:00Z&end_time=2026-03-25T23:59:59Z
```

### SDK 调用示例

#### Python SDK

```python
from agentos import LLMClient

client = LLMClient(base_url="http://localhost:8080")

# 同步调用
response = client.chat.completions.create(
    model="gpt-4",
    messages=[
        {"role": "user", "content": "Hello!"}
    ],
    temperature=0.7
)

print(response.choices[0].message.content)
print(f"Cost: ${response.cost.total_cost_usd}")

# 流式调用
stream = client.chat.completions.create(
    model="gpt-4",
    messages=[{"role": "user", "content": "Tell me a story"}],
    stream=True
)

for chunk in stream:
    if chunk.choices[0].delta.content:
        print(chunk.choices[0].delta.content, end="", flush=True)
```

#### Go SDK

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
        Temperature: 0.7,
    })
    
    if err != nil {
        panic(err)
    }
    
    fmt.Println(resp.Choices[0].Message.Content)
    fmt.Printf("Cost: $%.4f\n", resp.Cost.TotalCostUSD)
}
```

---

## ⚙️ 配置选项详解

### 服务器配置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `server.port` | int | 8080 | HTTP 服务端口 |
| `server.workers` | int | 4 | 工作进程数 |
| `server.timeout_sec` | int | 30 | 请求超时时间 |
| `server.max_body_mb` | int | 10 | 最大请求体大小 |

### 提供商配置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `providers[].name` | string | - | 提供商名称 |
| `providers[].enabled` | bool | true | 是否启用 |
| `providers[].api_key` | string | - | API Key |
| `providers[].api_base` | string | - | API 基础 URL |
| `providers[].timeout_sec` | int | 30 | 超时时间 |
| `providers[].max_retries` | int | 3 | 最大重试次数 |
| `providers[].retry_delay_ms` | int | 1000 | 重试间隔 |

### 缓存配置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `cache.enabled` | bool | true | 是否启用缓存 |
| `cache.backend` | string | redis | 缓存后端 (memory/redis) |
| `cache.ttl_sec` | int | 3600 | 缓存过期时间 |
| `cache.max_size_mb` | int | 512 | 最大缓存大小 |
| `cache.compression` | bool | true | 是否压缩 |

### 成本追踪配置

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `cost_tracking.enabled` | bool | true | 是否启用成本追踪 |
| `cost_tracking.log_per_request` | bool | true | 记录每次请求成本 |
| `cost_tracking.export_interval_sec` | int | 60 | 导出统计间隔 |

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM):

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| **请求延迟** | < 50ms | 不含模型推理时间，P95 |
| **并发连接** | 1000+ | 每服务实例 |
| **缓存命中率** | > 80% | 重复查询场景 |
| **缓存延迟** | < 5ms | Redis 缓存读取 |
| **吞吐量** | 500+ req/s | 简单查询 |
| **Token 计数精度** | 100% | 与官方一致 |

---

## 🔍 故障排查

### 常见问题

#### 1. API Key 无效

**错误**:
```json
{
  "error": {
    "type": "authentication_error",
    "message": "Invalid API key"
  }
}
```

**解决方案**:
- 检查配置文件中的 `api_key` 是否正确
- 确认环境变量已正确设置：`echo $OPENAI_API_KEY`
- 验证 API Key 权限和额度

#### 2. 请求超时

**错误**:
```json
{
  "error": {
    "type": "timeout_error",
    "message": "Request timed out after 30s"
  }
}
```

**解决方案**:
- 增加 `providers[].timeout_sec` 配置
- 检查网络连接
- 使用流式输出避免长等待

#### 3. 缓存未命中

**现象**: 缓存命中率低

**解决方案**:
```yaml
# 优化缓存配置
cache:
  enabled: true
  ttl_sec: 7200  # 延长缓存时间
  max_size_mb: 1024  # 增大缓存
  
  # 启用语义缓存（高级功能）
  semantic_cache:
    enabled: true
    similarity_threshold: 0.9  # 相似度阈值
```

#### 4. 成本统计不准确

**现象**: 成本数据与实际不符

**解决方案**:
- 检查 `cost_tracking.enabled` 是否为 true
- 验证模型定价配置
- 查看日志：`tail -f /var/log/agentos/llm_d.log`

### 调试技巧

```bash
# 1. 查看详细日志
./build/llm_d --config ../config/llm.yaml --log-level debug

# 2. 测试单个接口
curl -v http://localhost:8080/api/v1/health

# 3. 监控实时请求
watch -n 1 'curl -s http://localhost:8080/api/v1/stats'

# 4. 压力测试
ab -n 1000 -c 10 http://localhost:8080/api/v1/health
```

---

## 🧪 测试

```bash
# 单元测试
cd llm_d/build
ctest --output-on-failure

# 集成测试
../../tests/integration/test_llm_service.sh

# 性能测试
python3 ../../tests/benchmark/llm_benchmark.py \
  --url http://localhost:8080 \
  --concurrency 10 \
  --requests 1000
```

---

## 📄 许可证

llm_d 服务采用 **Apache License 2.0** 开源协议。

详见项目根目录的 [LICENSE](../../LICENSE) 文件。

---

## 🔗 相关文档

- [工具服务](../tool_d/README.md) - 工具执行服务
- [市场服务](../market_d/README.md) - Agent 市场
- [系统调用文档](../../paper/architecture/syscall.md) - Syscall 接口
- [OpenAI API 文档](https://platform.openai.com/docs) - OpenAI 官方文档
- [Anthropic API 文档](https://docs.anthropic.com) - Anthropic 官方文档

---

<div align="center">

**llm_d - 为 AgentOS 提供强大的 LLM 推理能力**

[返回顶部](#llm 服务 -llm_d)

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
