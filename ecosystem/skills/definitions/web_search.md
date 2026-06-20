# Web Search Skill

> **版本**: 1.0.0 | **类别**: information | **标签**: search, web, information-retrieval, research

## 概述

执行网络搜索，整合多源结果，返回结构化搜索报告。支持多搜索引擎、结果去重、相关性排序和摘要提取。

## 功能特性

1. **多引擎搜索** — 支持 DuckDuckGo、SearXNG 等引擎
2. **结果去重** — 基于 URL 规范化去重（去除 utm 参数、www 前缀等）
3. **相关性排序** — 基于关键词匹配度和来源权威度加权排序
4. **摘要提取** — 从搜索结果中提取关键信息生成摘要

## 输入参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|:---:|------|------|
| query | string | ✅ | — | 搜索查询关键词（≤500字符） |
| max_results | integer | ❌ | 10 | 最大返回结果数 |
| engine | string | ❌ | auto | 搜索引擎 (duckduckgo/searxng/auto) |
| time_range | string | ❌ | all | 时间范围 (day/week/month/year/all) |
| region | string | ❌ | us | 搜索区域 (us/cn/jp/de等) |

## 输出格式

```json
{
  "query": "Rust async programming",
  "results": [
    {
      "title": "Asynchronous Programming in Rust",
      "url": "https://doc.rust-lang.org/async-book/",
      "snippet": "An overview of async programming in Rust...",
      "source": "rust-lang.org",
      "relevance": 0.95
    }
  ],
  "total_found": 10,
  "summary": "Found 10 results for 'Rust async programming'. Top results:\n  1. [rust-lang.org] Asynchronous Programming in Rust",
  "engine": "auto"
}
```

## 权威来源加分

以下域名获得相关性加分：

| 域名 | 加分系数 |
|------|:---:|
| docs.rs, python.org, rust-lang.org | ×1.3 |
| github.com, arxiv.org, mozilla.org | ×1.2 |
| stackoverflow.com, wikipedia.org | ×1.1 |

## 使用示例

```python
from ecosystem.skills import WebSearchSkill
import asyncio

skill = WebSearchSkill()
result = asyncio.run(skill.execute({
    "query": "AgentRT 智能体运行时",
    "max_results": 5
}))
print(result["total_found"])  # 5
print(result["summary"])
```

## 降级策略

当 Gateway 不可用时，自动降级为生成搜索引擎链接（DuckDuckGo + Google），确保功能可用性。

## 实现文件

- [web_search.py](web_search.py) — SkillPlugin 实现
