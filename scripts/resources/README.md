# 项目资源与教程

`scripts/resources/`

## 概述

`resources/` 目录存放 AgentOS 项目的静态图片资源和交互式教程素材，共 **5 个文件**。

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| resources/ 组件 | 支持的 agentos/ 模块 | 用途 |
|-----------------|---------------------|------|
| `images/` | 全部模块 | 项目宣传与社区推广资源 |
| `tutorial/tutorial_engine.py` | `openlab/` | 交互式教程引擎，帮助新贡献者了解 OpenLab 生态系统 |
| `tutorial/new-contributor.json` | `openlab/contrib/` | 新贡献者入门教程配置（Skills/Strategies/Agents） |

## 目录结构

```
resources/                       # 共 5 个文件
├── images/                      # 静态图片资源（2 个文件）
│   ├── AgentOS-desktop-preview.gif   # 桌面端预览动图
│   └── feishu-community-qr.png       # 飞书社区二维码
└── tutorial/                    # 交互式教程引擎（2 个文件）
    ├── tutorial_engine.py       # 交互式教程引擎（命令行/Web 双模式，渐进式学习路径）
    └── new-contributor.json     # 新贡献者入门教程配置（环境配置→代码结构→首个 PR，约 4 小时）
```

## 使用方式

```bash
# 启动交互式教程
python scripts/resources/tutorial/tutorial_engine.py

# 使用新贡献者教程
python scripts/resources/tutorial/tutorial_engine.py --tutorial new-contributor
```

---

© 2026 SPHARX Ltd. All Rights Reserved.