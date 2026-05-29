# DocGen — 智能文档生成应用

> **Preview Status**: 本模块当前处于预览/开发阶段，作为 AgentOS v0.1.0 的一部分发布。API 和功能可能在未来版本中发生变化。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

`openlab/app/docgen/` 是一款基于 AgentOS 平台的智能文档生成应用，能够根据用户需求自动生成结构化文档。

## 核心能力

- **多格式支持**：支持 Markdown、HTML、PDF、Word 等输出格式
- **模板系统**：可自定义文档模板，支持变量注入
- **智能编排**：自动组织文档结构，生成目录和索引
- **协作编辑**：支持多人协作和版本管理

## 使用方式

```python
from docgen import DocGenApp

docgen = DocGenApp()

# 生成文档
doc = docgen.generate(
    title="API 设计文档",
    template="technical",
    sections=[
        {"title": "概述", "content": "..."},
        {"title": "架构设计", "content": "..."},
        {"title": "接口规范", "content": "..."}
    ],
    format="markdown"
)

# 导出文档
doc.export("output/api_design.md")
```

---

*AgentOS OpenLab — DocGen*
