# DocGen — 智能文档生成应用

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
