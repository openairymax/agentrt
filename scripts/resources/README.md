# 项目资源与教程

`scripts/resources/`

## 概述

`resources/` 目录存放 AgentRT 项目的技术演示脚本、静态图片资源和交互式教程素材。该模块是项目宣传、社区推广和新贡献者引导的核心资源库，提供从技术演示到入门教程的完整学习路径。

资源模块的设计原则：

- **渐进式学习**：交互式教程引擎支持命令行和 Web 双模式，提供从环境配置到首个 PR 的渐进式学习路径
- **可复用演示**：技术演示脚本设计为可独立运行和可定制的展示模块，方便在不同场合复用
- **社区友好**：包含飞书社区二维码等社区推广资源，降低社区参与门槛

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| resources/ 组件 | 支持的 agentos/ 模块 | 用途 |
|-----------------|---------------------|------|
| `demos/phase3_technology_demo.py` | 全部模块 | 第三阶段技术演示（服务框架/基准测试/工具链/开源治理） |
| `images/AgentOS-desktop-preview.gif` | 全部模块 | 桌面端预览动图，用于 README 和宣传材料 |
| `images/feishu-community-qr.png` | 全部模块 | 飞书社区二维码，用于社区推广 |
| `tutorial/tutorial_engine.py` | `openlab/` | 交互式教程引擎，帮助新贡献者了解 OpenLab 生态系统 |
| `tutorial/new-contributor.json` | `openlab/contrib/` | 新贡献者入门教程配置（Skills/Strategies/Agents） |

## 目录结构

```
resources/
├── README.md                              # 本文档
├── demos/                                 # 技术演示脚本（1 个文件）
│   └── phase3_technology_demo.py          #   第三阶段技术演示（服务框架/基准测试/工具链/开源治理）
├── images/                                # 静态图片资源（2 个文件）
│   ├── AgentOS-desktop-preview.gif        #   桌面端预览动图
│   └── feishu-community-qr.png            #   飞书社区二维码
└── tutorial/                              # 交互式教程引擎（2 个文件）
    ├── tutorial_engine.py                 #   交互式教程引擎（命令行/Web 双模式，渐进式学习路径）
    └── new-contributor.json               #   新贡献者入门教程配置（环境配置→代码结构→首个 PR，约 4 小时）
```

## 核心组件说明

### demos/ — 技术演示脚本

技术演示脚本目录，包含可独立运行的演示模块：

- **phase3_technology_demo.py**：第三阶段技术演示脚本，展示 AgentRT 的核心技术栈，包括：
  - 服务框架演示：展示 `daemon/` 和 `gateway/` 的服务架构和通信机制
  - 基准测试演示：展示 `atoms/` 层核心组件的性能特征
  - 工具链演示：展示 `toolkit/` 多语言 SDK 的使用方式
  - 开源治理演示：展示 `openlab/` 的贡献流程和治理模型

### images/ — 静态图片资源

项目宣传和社区推广使用的静态图片资源：

- **AgentOS-desktop-preview.gif**：桌面端预览动图，用于项目 README、官网和宣传材料，展示 AgentRT 的桌面端交互界面
- **feishu-community-qr.png**：飞书社区二维码，用于社区推广和用户引导，扫码即可加入 AgentRT 飞书社区

### tutorial/ — 交互式教程引擎

交互式教程系统，帮助新贡献者快速了解 AgentRT 项目：

- **tutorial_engine.py**：交互式教程引擎，支持两种运行模式：
  - **命令行模式**：在终端中运行，提供文本交互式学习体验
  - **Web 模式**：在浏览器中运行，提供图形化交互式学习体验
  - 支持渐进式学习路径，从基础概念到高级功能的逐步引导
  - 支持进度保存和恢复，可中断后继续学习

- **new-contributor.json**：新贡献者入门教程配置文件，定义完整的学习路径：
  - **环境配置**（约 30 分钟）：安装依赖、配置工具链、验证环境
  - **代码结构**（约 60 分钟）：了解项目架构、模块划分、核心概念
  - **开发流程**（约 60 分钟）：分支管理、代码规范、测试要求
  - **首个 PR**（约 30 分钟）：选择任务、编写代码、提交审核
  - 总计约 4 小时，覆盖从零到贡献者的完整路径

## 使用方式

### 技术演示

```bash
# 运行第三阶段技术演示
python scripts/resources/demos/phase3_technology_demo.py

# 指定演示模块
python scripts/resources/demos/phase3_technology_demo.py --module service-framework
python scripts/resources/demos/phase3_technology_demo.py --module benchmark
python scripts/resources/demos/phase3_technology_demo.py --module toolkit
python scripts/resources/demos/phase3_technology_demo.py --module governance
```

### 交互式教程

```bash
# 启动交互式教程（默认命令行模式）
python scripts/resources/tutorial/tutorial_engine.py

# 使用新贡献者教程
python scripts/resources/tutorial/tutorial_engine.py --tutorial new-contributor

# 启动 Web 模式
python scripts/resources/tutorial/tutorial_engine.py --mode web

# 恢复上次进度
python scripts/resources/tutorial/tutorial_engine.py --resume

# 查看可用教程列表
python scripts/resources/tutorial/tutorial_engine.py --list
```

### 图片资源引用

在 Markdown 文档中引用图片资源：

```markdown
![AgentOS Desktop Preview](scripts/resources/images/AgentOS-desktop-preview.gif)
![Feishu Community](scripts/resources/images/feishu-community-qr.png)
```

## 依赖说明

| 组件 | 核心依赖 | 说明 |
|------|---------|------|
| `demos/phase3_technology_demo.py` | Python 3.8+ | 技术演示脚本为纯 Python 实现 |
| `tutorial/tutorial_engine.py` | Python 3.8+ | 教程引擎为纯 Python 实现，Web 模式需要 Flask |
| `tutorial/new-contributor.json` | 无 | JSON 配置文件，无运行时依赖 |
| `images/` | 无 | 静态图片资源，无运行时依赖 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
