# Manager Model

AgentOS LLM 模型配置，定义各 AI 提供商的模型参数和 API 配置。

## 文件

| 文件 | 说明 |
|------|------|
| `model.yaml` | 模型配置（YAML 格式，含详细注释） |
| `model.json` | 模型配置（JSON 格式，供 llm_d 程序化加载） |

两个文件包含相同数据，服务于不同消费者：
- `model.yaml` — 人类可读，含注释
- `model.json` — 程序化加载，供 C 守护进程使用

## 版本

v0.1.0