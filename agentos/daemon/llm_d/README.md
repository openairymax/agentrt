# LLM Daemon — LLM 服务守护进程

`daemon/llm_d/` 是 AgentOS 的大语言模型服务守护进程，提供统一的模型调用接口，支持多种 LLM 提供商。

## 核心职责

- **多提供商支持**：OpenAI、Anthropic、DeepSeek、本地模型等
- **统一接口**：屏蔽不同提供商 API 差异
- **响应缓存**：缓存相同请求的响应，降低延迟和成本
- **成本追踪**：记录每次调用的 Token 消耗和费用
- **Provider 注册**：可扩展的提供商注册机制

## 适配器架构

```
客户端请求 (JSON-RPC 2.0)
       ↓
  llm_svc_adapter  ← 请求解析与标准化
       ↓
  llm_service      ← 核心服务逻辑（缓存、限流、路由）
       ↓
  provider/        ← Provider 适配层
    ├─ openai.c      OpenAI API
    ├─ anthropic.c   Anthropic API
    ├─ deepseek.c    DeepSeek API
    └─ local.c       本地模型推理
```

## 核心能力

| 能力 | 说明 |
|------|------|
| `llm.generate` | 文本生成 |
| `llm.chat` | 对话交互 |
| `llm.embed` | 文本嵌入向量 |
| `llm.tokenize` | Token 计数 |
| `llm.models` | 可用模型列表 |
| `llm.cache.clear` | 清除响应缓存 |

## 使用方式

```bash
# 启动 LLM 守护进程
./llm_d --config llm_config.json

# 指定默认模型
./llm_d --default-model gpt-4

# 启用详细日志
./llm_d --verbose
```

---

*AgentOS Daemon — LLM Daemon*
