# Research — 智能研究助手应用

**模块路径**: `agentos/openlab/app/research/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS v0.1.0 的正式组成部分，API 已稳定。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Research 是基于 AgentOS 平台的智能研究助手应用，辅助研究人员进行文献检索、数据分析和报告生成。通过 AI 驱动的知识管理能力，帮助研究者高效完成从文献调研到成果输出的全流程工作。

## 目录结构

```
research/
└── README.md                   # 本文件
```

## 核心能力

- **文献检索**：自动检索和筛选相关学术文献，支持多数据库源
- **数据分析**：数据清洗、统计分析和可视化，支持多种数据格式
- **报告生成**：自动生成研究报告和论文草稿，支持模板化输出
- **知识管理**：文献管理和知识图谱构建，支持引用关系追踪
- **趋势分析**：研究热点识别和趋势预测
- **协作支持**：多人协作研究和版本管理

## 接口说明

### 文献检索 API

| 接口 | 方法 | 说明 |
|------|------|------|
| `research.search_papers` | POST | 检索学术文献 |
| `research.get_paper` | GET | 获取论文详情 |
| `research.get_citations` | GET | 获取引用关系 |
| `research.generate_summary` | POST | 生成文献摘要 |

### 数据分析 API

| 接口 | 方法 | 说明 |
|------|------|------|
| `research.analyze_data` | POST | 执行数据分析 |
| `research.visualize` | POST | 生成可视化图表 |
| `research.export_results` | POST | 导出分析结果 |

### 报告生成 API

| 接口 | 方法 | 说明 |
|------|------|------|
| `research.create_report` | POST | 创建研究报告 |
| `research.export_report` | POST | 导出报告文件 |
| `research.generate_draft` | POST | 生成论文草稿 |

## 依赖关系

- **核心依赖**: AgentOS OpenLab Core, FastAPI, Pydantic
- **可选依赖**: 各种学术数据库 API 客户端

## 使用示例

```python
from research import ResearchApp

research = ResearchApp()

papers = research.search_papers(
    query="reinforcement learning",
    max_results=20,
    sort_by="citations"
)

summary = research.generate_summary(papers)

research.export_report("output/research_report.md")
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
