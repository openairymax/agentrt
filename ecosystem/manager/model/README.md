# Manager Model — LLM 模型配置

**模块路径**: `ecosystem/manager/model/`
**版本**: v0.1.0

## 概述

`manager/model/` 包含 AgentOS 的 LLM 模型配置，定义各 AI 提供商的模型参数和 API 配置。提供 YAML 和 JSON 两种格式，分别服务于人类可读编辑和 C 守护进程程序化加载。配置遵循 `schema/model.schema.json` 规范。

## 目录结构

```
model/
├── model.yaml       # 模型配置（YAML 格式，人类可读，含详细注释）
└── model.json       # 模型配置（JSON 格式，供 llm_d 程序化加载）
```

## 核心组件

### model.yaml

YAML 格式的模型配置，包含详细注释，适合人工编辑。定义以下内容：

| 配置域 | 说明 |
|--------|------|
| **提供商** | AI 服务提供商配置（OpenAI / Anthropic / DeepSeek 等） |
| **模型参数** | temperature / max_tokens / top_p / frequency_penalty 等 |
| **上下文窗口** | 各模型的最大上下文长度 |
| **超时与重试** | 请求超时、重试策略、退避参数 |
| **熔断器** | 失败阈值、重置超时 |
| **成本配置** | Token 单价、预算限制、成本中心 |

### model.json

JSON 格式的模型配置，与 `model.yaml` 包含相同数据，供 C 守护进程 `llm_d` 程序化加载使用。

> 两个文件包含相同数据，服务于不同消费者：
> - `model.yaml` — 人类可读，含注释
> - `model.json` — 程序化加载，供 C 守护进程使用

## 依赖关系

| 组件 | 用途 |
|------|------|
| `schema/model.schema.json` | 配置格式校验 |
| llm_d 守护进程 | JSON 格式配置消费者 |
| PyYAML | YAML 配置解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
