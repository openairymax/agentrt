# Applications — 智能应用

`openlab/app/` 包含 AgentOS 生态系统中的官方智能应用，涵盖文档生成、电商、研究和视频编辑等领域。

## 应用列表

| 应用 | 路径 | 说明 |
|------|------|------|
| **DocGen** | `docgen/` | 智能文档生成，支持多种文档格式和模板 |
| **E-Commerce** | `ecommerce/` | 智能电商助手，支持商品管理、订单处理 |
| **Research** | `research/` | 智能研究助手，支持文献检索、数据分析 |
| **VideoEdit** | `videoedit/` | 智能视频编辑，支持剪辑、字幕、特效 |

## 应用架构

```
app/
├── docgen/           # 文档生成应用
├── ecommerce/        # 电商助手应用
├── research/         # 研究助手应用
├── videoedit/        # 视频编辑应用
└── README.md         # 本文件
```

所有应用基于 AgentOS 平台开发，使用统一的 JSON-RPC 2.0 协议与后端服务通信，可独立部署和扩展。

---

*AgentOS OpenLab — Applications*
