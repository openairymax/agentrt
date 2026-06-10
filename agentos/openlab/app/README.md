# Applications — 智能应用集合

**模块路径**: `agentos/openlab/app/`
**版本**: v0.1.0

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。各应用通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Applications 是 OpenLab 生态系统中的官方智能应用集合，涵盖文档生成、电商运营、学术研究和视频编辑四大领域。所有应用基于 AgentOS 平台开发，使用统一的 JSON-RPC 2.0 协议与后端服务通信，可独立部署和扩展。

## 目录结构

```
app/
├── docgen/                     # 文档生成应用
│   ├── src/
│   │   ├── __init__.py
│   │   ├── main.py             # 应用入口，CLI 接口
│   │   └── generator.py        # DocumentationGenerator 核心引擎
│   ├── templates/
│   │   └── default.html.j2     # Jinja2 默认模板
│   ├── config.yaml             # 应用配置
│   ├── manifest.json           # 应用清单
│   ├── run.sh                  # 启动脚本
│   └── README.md
├── ecommerce/                  # 电商助手应用
│   ├── src/
│   │   ├── __init__.py
│   │   ├── main.py             # 应用入口
│   │   ├── utils.py            # 工具函数（format_price/validate_email）
│   │   └── requirements.txt    # Python 依赖
│   ├── config.yaml             # 应用配置（含 Stripe/JWT/Redis）
│   ├── manifest.json           # 应用清单
│   ├── run.sh                  # 启动脚本
│   └── README.md
├── research/                   # 研究助手应用（规划阶段）
│   └── README.md
├── videoedit/                  # 视频编辑应用
│   ├── src/
│   │   ├── __init__.py
│   │   ├── main.py             # FastAPI 应用入口（VideoEditApp）
│   │   ├── edit_pipeline.py    # 编辑管线核心（EditPipeline/FFmpegWrapper）
│   │   └── requirements.txt    # Python 依赖
│   ├── config.yaml             # 应用配置（含 FFmpeg 参数）
│   ├── manifest.json           # 应用清单
│   ├── run.sh                  # 启动脚本
│   └── README.md
└── README.md                   # 本文件
```

## 应用列表

| 应用 | 路径 | 技术栈 | 说明 | 状态 |
|------|------|--------|------|------|
| **DocGen** | `docgen/` | Jinja2, Markdown, PyYAML | 智能文档生成，支持多格式输出和文件监听 | 已实现 |
| **E-Commerce** | `ecommerce/` | FastAPI, Stripe, JWT, Redis | 智能电商助手，支持支付/订单/库存管理 | 骨架实现 |
| **Research** | `research/` | — | 智能研究助手，支持文献检索和数据分析 | 规划阶段 |
| **VideoEdit** | `videoedit/` | FastAPI, FFmpeg, OpenCV | 智能视频编辑，支持剪辑/合并/特效/字幕 | 已实现 |

## 应用架构

```
+-----------------------------------------------------------+
|                    Applications Layer                       |
+-----------------------------------------------------------+
|  DocGen        | E-Commerce    | Research  | VideoEdit     |
|  ┌───────────┐ | ┌───────────┐ | ┌──────┐ | ┌───────────┐ |
|  │Generator  │ | │FastAPI    │ | │      │ | │FastAPI    │ |
|  │  Engine   │ | │  Server   │ | │      │ | │  Server   │ |
|  ├───────────┤ | ├───────────┤ | │      │ | ├───────────┤ |
|  │Jinja2     │ | │Stripe     │ | │      │ | │FFmpeg     │ |
|  │Templates  │ | │Payment    │ | │      │ | │Pipeline   │ |
|  ├───────────┤ | ├───────────┤ | │      │ | ├───────────┤ |
|  │Markdown   │ | │JWT Auth   │ | │      │ | │OpenCV     │ |
|  │Rendering  │ | │Security   │ | │      │ | │Processing │ |
|  └───────────┘ | └───────────┘ | └──────┘ | └───────────┘ |
+-----------------------------------------------------------+
|              AgentOS Core Runtime (JSON-RPC 2.0)           |
+-----------------------------------------------------------+
```

## 通用特性

所有应用共享以下特性：

- **配置驱动**：通过 `config.yaml` 统一管理应用参数
- **应用清单**：`manifest.json` 描述应用元信息
- **独立部署**：每个应用可独立运行和扩展
- **启动脚本**：`run.sh` 提供便捷的启动方式

## 依赖关系

| 应用 | 核心依赖 | 可选依赖 |
|------|----------|----------|
| DocGen | Jinja2, Markdown, PyYAML | WeasyPrint (PDF), Watchdog (文件监听) |
| E-Commerce | FastAPI, Stripe, SQLAlchemy, Redis | psycopg2 (PostgreSQL), PyMySQL |
| Research | — | — |
| VideoEdit | FastAPI, FFmpeg, PyYAML | OpenCV, MoviePy, Pillow |

---

© 2026 SPHARX Ltd. All Rights Reserved.
