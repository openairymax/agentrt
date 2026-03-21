# LLM 服务 (llm_d)

提供统一的大模型调用接口，支持多个供应商（OpenAI、Anthropic、DeepSeek、本地模型）以及缓存、成本跟踪和 Token 计数。

## 配置

服务启动时需要传入 YAML 配置文件路径，示例：

```yaml
providers:
  - name: openai
    api_key: sk-xxx
    api_base: https://api.openai.com/v1
    timeout_sec: 30
    max_retries: 3

  - name: anthropic
    api_key: sk-ant-xxx
    timeout_sec: 30
    max_retries: 3
```
## 接口

- `llm_service_complete`：同步完成请求
- `llm_service_complete_stream`：流式请求（待实现）
- `llm_service_stats`：获取统计信息

## 编译

```
mkdir build && cd build
cmake ../llm_d
make
```

## 运行

./llm_d /path/to/config.yaml

## 依赖

- libcurl
- cJSON
- libyaml
- tiktoken

---

© 2026 SPHARX Ltd. All Rights Reserved.
