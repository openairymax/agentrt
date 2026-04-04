# OpenLab - AgentOS 开放生态与应用中心

<div align="center">

**版本**: v1.0.0.9  
**最后更新**: 2026-03-29  
**许可证**: Apache License 2.0

[![Version](https://img.shields.io/badge/version-v1.0.0.9-blue.svg)]()
[![License](https://img.shields.io/badge/license-Apache%202.0-green.svg)]()
[![Python](https://img.shields.io/badge/python-3.10%2B-blue.svg)]()
[![Status](https://img.shields.io/badge/status-production%20ready-green.svg)]()

</div>

---

## 📖 快速导航

- [模块定位](#-模块定位)
- [核心功能](#-核心功能)
- [快速开始](#-快速开始)
- [目录结构](#-目录结构)
- [使用指南](#-使用指南)
- [架构设计](#-架构设计)
- [贡献指南](#-贡献指南)

---

## 🎯 模块定位

**OpenLab** 是 AgentOS 的**开放生态平台**和**应用创新中心**，承担三大核心职责：

### 核心职责

```
┌─────────────────────────────────────────────────────────┐
│                  OpenLab 三大核心职责                     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  1️⃣  应用示例中心    - 展示 AgentOS 能力的完整应用       │
│  2️⃣  社区贡献平台    - 智能体、技能、策略的孵化器        │
│  3️⃣  市场机制实现    - 智能体和技能的交易、安装、管理     │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 在 AgentOS 中的位置

```
AgentOS 整体架构:
┌─────────────────────────────────────┐
│     应用层 (Applications)           │ ← OpenLab 提供示例应用
├─────────────────────────────────────┤
│     生态层 (Ecosystem)              │ ← OpenLab 核心职责
├─────────────────────────────────────┤
│     服务层 (Daemon Services)        │
├─────────────────────────────────────┤
│     内核层 (Kernel - Atoms)         │
└─────────────────────────────────────┘
```

---

## 🌟 核心功能

### 1. 应用示例中心 (app/)

提供 **4 个完整的应用示例**，展示如何使用 AgentOS 构建实际应用：

| 应用 | 功能描述 | 核心技术 | 完成度 |
|------|---------|---------|--------|
| **docgen** | 文档自动生成 | 代码解析、Doxygen/JSDoc、Mermaid 图 | 80% 🔄 |
| **ecommerce** | 电商智能体 | 多轮对话、个性化推荐、订单处理 | 85% 🔄 |
| **research** | 研究助手 | 信息收集、分析、报告生成 | 规划中 |
| **videoedit** | 视频编辑 | 智能体辅助视频剪辑 | 规划中 |

**每个应用包含**：
- ✅ 完整的源代码
- ✅ 配置文件 (`config.yaml`)
- ✅ 清单文件 (`manifest.json`)
- ✅ 运行脚本 (`run.sh`)
- ✅ 使用说明 (`README.md`)

### 2. 社区贡献平台 (contrib/)

#### 2.1 智能体 (agents/) - 7 个预定义角色

提供覆盖软件开发全流程的 **7 个预定义角色智能体**：

| 智能体 | 核心职责 | 双系统支持 | 文件结构 |
|--------|---------|-----------|---------|
| **architect** | 架构设计与评审 | ✅ System1/System2 | `agent.py`, `contract.json`, `prompts/` |
| **backend** | 后端开发 | ✅ System1/System2 | 同上 |
| **frontend** | 前端开发 | ✅ System1/System2 | 同上 |
| **devops** | 运维部署 | ✅ System1/System2 | 同上 |
| **product_manager** | 产品规划 | ✅ System1/System2 | 同上 |
| **security** | 安全审计 | ✅ System1/System2 | 同上 |
| **tester** | 质量测试 | ✅ System1/System2 | 同上 |

**每个智能体包含**：
- `agent.py` - 智能体核心实现
- `contract.json` - 服务合约定义（符合 JSON Schema）
- `prompts/system1.md` - 快速思考模式提示词
- `prompts/system2.md` - 深度思考模式提示词

#### 2.2 技能 (skills/) - 3 个核心技能模块

| 技能 | 功能 | 技术栈 | 核心能力 |
|------|------|--------|---------|
| **browser_skill** | 浏览器自动化 | Playwright + Selenium | 导航、交互、截图、爬虫 |
| **database_skill** | 数据库操作 | SQLAlchemy | SQL 查询、ORM、事务管理 |
| **github_skill** | GitHub API 集成 | PyGithub | 仓库管理、CI/CD、Issue |

**每个技能包含**：
- `README.md` - 技能说明文档
- `*.py` - 技能实现代码
- `skill.json` - 技能元数据
- `lib*.so` - 编译后的动态链接库（可选）

#### 2.3 策略 (strategies/) - 2 个核心算法

| 策略 | 功能 | 算法 | 应用场景 |
|------|------|------|---------|
| **dispatching** | 任务分发和负载均衡 | 多维度评分、能力匹配 | 智能体路由 |
| **planning** | 任务规划和路径搜索 | 任务分解、依赖分析 | DAG 规划 |

### 3. 市场机制 (markets/)

#### 3.1 智能体市场

```
markets/agents/
├── contracts/              # 合约管理
│   ├── schema.json         # JSON Schema 验证
│   ├── validator.py        # 合约验证器
│   └── example_contract.json
├── installer/              # 安装器
│   └── cli.py              # 命令行工具
└── registry/               # 注册表
    └── index.json          # 智能体索引
```

**核心功能**：
- ✅ 智能体合约定义和验证
- ✅ 一键安装/卸载智能体
- ✅ 智能体元数据管理和检索

#### 3.2 技能市场

```
markets/skills/
├── contracts/              # 合约管理
│   ├── schema.json         # JSON Schema 验证
│   └── validator.py        # 合约验证器
├── installer/              # 安装器
│   └── cli.py              # 命令行工具
└── registry/               # 注册表
    └── index.json          # 技能索引
```

#### 3.3 开发模板

| 模板 | 用途 | 包含内容 |
|------|------|---------|
| **python-agent** | Python 智能体 | 项目结构、示例代码、配置 |
| **rust-skill** | Rust 技能 | Cargo 配置、FFI 接口、示例 |

---

## 🚀 快速开始

### 环境要求

- **Python**: 3.10+
- **Node.js**: 18+ (可选，用于前端应用)
- **Docker**: 20+ (可选，用于容器化部署)

### 安装

```bash
# 1. 克隆项目
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS/openlab

# 2. 安装依赖
pip install -e .

# 3. 开发模式安装（推荐）
pip install -e ".[dev]"
```

### 运行示例应用

```bash
# 运行文档生成应用
cd openlab/app/docgen
./run.sh

# 或手动运行
python src/main.py --config config.yaml
```

### 使用智能体

```python
from openlab.contrib.agents.architect import ArchitectAgent

# 创建智能体
agent = ArchitectAgent()

# 执行任务
result = agent.execute({
    "task": "设计一个电商系统架构",
    "requirements": ["微服务", "高可用", "可扩展"]
})

print(result.output)
```

### 使用技能

```python
from openlab.contrib.skills.browser_skill import BrowserSkill

# 创建技能实例
skill = BrowserSkill()

# 使用技能
result = skill.execute({
    "action": "navigate",
    "url": "https://example.com"
})

print(result.data)
```

### 从市场安装智能体

```bash
# 安装架构师智能体
cd openlab/markets/agents/installer
python cli.py install architect

# 安装浏览器技能
cd openlab/markets/skills/installer
python cli.py install browser_skill
```

---

## 📁 目录结构

```
openlab/
│
├── app/                           # 应用示例
│   ├── docgen/                   # 文档生成应用
│   │   ├── src/
│   │   │   ├── generator.py      # 文档生成器
│   │   │   └── main.py           # 主程序
│   │   ├── templates/
│   │   │   └── default.html.j2   # HTML 模板
│   │   ├── config.yaml           # 配置文件
│   │   ├── manifest.json         # 清单文件
│   │   ├── run.sh                # 运行脚本
│   │   └── README.md
│   │
│   ├── ecommerce/                # 电商应用
│   │   ├── src/
│   │   │   ├── main.py
│   │   │   └── utils.py
│   │   ├── config.yaml
│   │   ├── manifest.json
│   │   ├── run.sh
│   │   └── README.md
│   │
│   ├── research/                 # 研究应用
│   │   └── README.md
│   │
│   └── videoedit/                # 视频编辑应用
│       ├── src/
│       │   ├── edit_pipeline.py
│       │   └── main.py
│       ├── config.yaml
│       ├── manifest.json
│       ├── run.sh
│       └── README.md
│
├── contrib/                       # 社区贡献
│   ├── agents/                   # 智能体贡献
│   │   ├── architect/           # 架构师
│   │   ├── backend/             # 后端开发
│   │   ├── devops/              # 运维
│   │   ├── frontend/            # 前端开发
│   │   ├── product_manager/     # 产品经理
│   │   ├── security/            # 安全专家
│   │   └── tester/              # 测试工程师
│   │
│   ├── skills/                   # 技能贡献
│   │   ├── browser_skill/       # 浏览器技能
│   │   ├── database_skill/      # 数据库技能
│   │   └── github_skill/        # GitHub 技能
│   │
│   └── strategies/               # 策略贡献
│       ├── dispatching/         # 调度策略
│       └── planning/            # 规划策略
│
├── markets/                       # 市场机制
│   ├── agents/                   # 智能体市场
│   │   ├── contracts/           # 合约
│   │   ├── installer/           # 安装器
│   │   └── registry/            # 注册表
│   │
│   ├── skills/                   # 技能市场
│   │   ├── contracts/           # 合约
│   │   ├── installer/           # 安装器
│   │   └── registry/            # 注册表
│   │
│   └── templates/                # 开发模板
│       ├── python-agent/        # Python 智能体模板
│       └── rust-skill/          # Rust 技能模板
│
├── openlab/                       # OpenLab 核心
│   ├── core/                     # 核心模块
│   │   ├── agent.py             # Agent 管理
│   │   ├── task.py              # Task 调度
│   │   ├── tool.py              # Tool 抽象
│   │   └── storage.py           # 存储后端
│   ├── agents/                   # 内置智能体
│   │   └── architect/
│   ├── __init__.py
│   ├── config.yaml
│   ├── requirements.txt
│   └── run.sh
│
├── tests/                         # 测试套件
│   ├── unit/
│   │   ├── test_agent.py
│   │   ├── test_dispatching.py
│   │   ├── test_planning.py
│   │   └── test_videoedit.py
│   ├── conftest.py
│   └── __init__.py
│
├── .github/
│   └── workflows/
│       └── ci.yml                # CI/CD 配置
│
├── Dockerfile                     # Docker 镜像
├── pyproject.toml                 # Python 项目配置
└── README.md                      # 本文档
```

---

## 📚 使用指南

### 1. 开发新应用

```bash
# 1. 创建应用目录
cd openlab/app
mkdir myapp
cd myapp

# 2. 创建基本结构
mkdir -p src templates
touch src/main.py src/utils.py config.yaml manifest.json run.sh

# 3. 编辑配置文件
cat > config.yaml << EOF
app:
  name: myapp
  version: 1.0.0
  description: 我的应用

agents:
  - name: architect
    role: system_architect

skills:
  - name: browser_skill
    enabled: true

execution:
  parallel: true
  max_workers: 4
  timeout: 300
EOF
```

### 2. 开发新智能体

```bash
# 1. 创建智能体目录
cd openlab/contrib/agents
mkdir my_agent
cd my_agent

# 2. 创建必要文件
touch agent.py contract.json
mkdir prompts
touch prompts/system1.md prompts/system2.md

# 3. 编写智能体代码
cat > agent.py << 'EOF'
from openlab.core.agent import Agent, AgentContext
from typing import Dict, Any

class MyAgent(Agent):
    """我的智能体"""
    
    CAPABILITIES = {
        AgentCapability.CODE_GENERATION,
        AgentCapability.CODE_REVIEW,
    }
    
    async def _do_initialize(self, config: Dict[str, Any]):
        """初始化智能体"""
        pass
    
    async def _do_execute(self, context: AgentContext, input_data: Dict[str, Any]):
        """执行任务"""
        pass
    
    async def _do_shutdown(self):
        """关闭智能体"""
        pass
EOF
```

### 3. 开发新技能

```bash
# 1. 创建技能目录
cd openlab/contrib/skills
mkdir my_skill
cd my_skill

# 2. 创建必要文件
touch my_skill.py skill.json README.md

# 3. 编写技能代码
cat > my_skill.py << 'EOF'
from dataclasses import dataclass
from typing import Any, Dict, Optional

@dataclass
class MySkillResult:
    """技能执行结果"""
    success: bool
    data: Any = None
    error: Optional[str] = None
    duration: float = 0.0

class MySkill:
    """我的技能"""
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        self.config = config or {}
    
    def execute(self, action: str, **kwargs) -> MySkillResult:
        """执行技能"""
        pass
EOF
```

### 4. 运行测试

```bash
# 运行单元测试
cd openlab
pytest tests/ -v

# 运行特定测试
pytest tests/unit/test_agent.py -v

# 生成覆盖率报告
pytest --cov=openlab --cov-report=html
```

---

## 🏗️ 架构设计

### 设计原则

OpenLab 遵循 AgentOS 的核心架构设计原则：

| 原则 | 说明 | 实现位置 |
|------|------|---------|
| **模块化** | 高内聚、低耦合 | `openlab/core/` 四大模块 |
| **可扩展** | 支持无缝添加新组件 | `contrib/` 扩展机制 |
| **双系统协同** | System1 + System2 | `prompts/system1.md`, `system2.md` |
| **安全内生** | 安全机制内置 | `workbench_id` 隔离 |
| **反馈闭环** | 三层嵌套反馈 | `TaskScheduler._feedback_loop()` |

### 核心架构

```
┌─────────────────────────────────────────┐
│           应用层 (Applications)          │
│  docgen  │  ecommerce  │  videoedit     │
├─────────────────────────────────────────┤
│           生态层 (Ecosystem)             │
│  ┌────────────┬────────────┬─────────┐  │
│  │  智能体    │    技能    │  策略   │  │
│  │  (Agents)  │  (Skills)  │(Strategy)│ │
│  └────────────┴────────────┴─────────┘  │
├─────────────────────────────────────────┤
│           市场层 (Markets)               │
│  ┌────────────┬────────────┬─────────┐  │
│  │  合约      │   安装器   │ 注册表  │  │
│  │(Contracts) │ (Installer)│(Registry)│ │
│  └────────────┴────────────┴─────────┘  │
├─────────────────────────────────────────┤
│           核心层 (CoreKern)              │
│  ┌────────────┬────────────┬─────────┐  │
│  │  Agent     │   Task     │  Tool   │  │
│  │  Manager   │ Scheduler  │ Registry│  │
│  └────────────┴────────────┴─────────┘  │
└─────────────────────────────────────────┘
```

### 核心模块职责

| 模块 | 文件 | 核心类 | 功能 |
|------|------|--------|------|
| **Agent** | `openlab/core/agent.py` | `Agent`, `AgentManager`, `AgentRegistry` | 智能体生命周期管理 |
| **Task** | `openlab/core/task.py` | `Task`, `TaskScheduler`, `TaskDefinition` | 任务调度和执行 |
| **Tool** | `openlab/core/tool.py` | `Tool`, `ToolRegistry`, `ToolExecutor` | 工具抽象和执行 |
| **Storage** | `openlab/core/storage.py` | `Storage`, `SQLiteStorage`, `Query` | 持久化存储 |

---

## 🤝 贡献指南

我们热烈欢迎社区贡献！

### 贡献类型

1. **提交智能体** - 在 `contrib/agents/` 下创建新的智能体
2. **开发技能** - 在 `contrib/skills/` 下添加新的技能模块
3. **分享策略** - 在 `contrib/strategies/` 下贡献算法策略
4. **完善文档** - 改进现有文档和示例
5. **报告问题** - 提交 Bug 报告和功能建议

### 贡献流程

```bash
# 1. Fork 项目
# 2. 创建分支
git checkout -b feature/my-contribution

# 3. 提交代码
git commit -m "feat: 添加我的贡献"

# 4. 推送分支
git push origin feature/my-contribution

# 5. 创建 Pull Request
```

### 代码规范

- ✅ 遵循 [PEP 8](https://pep8.org/) Python 编码规范
- ✅ 使用类型注解
- ✅ 编写单元测试
- ✅ 添加文档字符串
- ✅ 运行 `black`, `flake8`, `mypy` 检查

---

## 📖 相关文档

- **[AgentOS 主文档](../README.md)** - AgentOS 总体介绍
- **[核心服务层文档](../daemon/README.md)** - Daemon 服务层详解
- **[内核架构文档](../atoms/README.md)** - Atoms 内核架构
- **[SDK 使用指南](../toolkit/README.md)** - 开发 SDK 使用
- **[架构设计原则](../manuals/architecture/ARCHITECTURAL_PRINCIPLES.md)** - 五维正交体系

---

## 📄 许可证

Apache License 2.0

详见 [LICENSE](../LICENSE)

---

## 📞 联系方式

- **项目主页**: https://github.com/SpharxTeam/AgentOS
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues
- **技术支持**: lidecheng@spharx.cn

---

<div align="center">

**"From data intelligence emerges."**

*始于数据，终于智能*

© 2026 SPHARX Ltd. 保留所有权利

</div>
