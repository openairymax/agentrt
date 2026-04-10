# OpenLab - AgentOS 弢放生态与应用中心

<div align="center">

**版本**: v1.0.0.9  
**朢后更?*: 2026-03-29  
**许可?*: Apache License 2.0

[![Version](https://img.shields.io/badge/version-v1.0.0.9-blue.svg)]()
[![License](https://img.shields.io/badge/license-Apache%202.0-green.svg)]()
[![Python](https://img.shields.io/badge/python-3.10%2B-blue.svg)]()
[![Status](https://img.shields.io/badge/status-production%20ready-green.svg)]()

</div>

---

## 📖 快导?
- [模块定位](#-模块定位)
- [核心功能](#-核心功能)
- [快开始](#-快开?
- [目录结构](#-目录结构)
- [使用指南](#-使用指南)
- [架构设计](#-架构设计)
- [贡献指南](#-贡献指南)

---

## 🎯 模块定位

**OpenLab** ?AgentOS ?*弢放生态平?*?*应用创新中心**，承担三大核心职责：

### 核心职责

```
┌─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢??                 OpenLab 三大核心职责                     ?├─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢??                                                        ?? 1️⃣  应用示例中心    - 展示 AgentOS 能力的完整应?      ?? 2️⃣  社区贡献平台    - 智能体技能策略的孵化?       ?? 3️⃣  市场机制实现    - 智能体和抢能的交易、安装管?    ??                                                        ?└─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢?```

### ?AgentOS 中的位置

```
AgentOS 整体架构:
┌─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢??    应用?(Applications)           ??OpenLab 提供示例应用
├─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢??    生层 (Ecosystem)              ??OpenLab 核心职责
├─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢??    服务?(Daemon Services)        ?├─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢??    内核?(Kernel - Atoms)         ?└─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢?```

---

## 🌟 核心功能

### 1. 应用示例中心 (app/)

提供 **4 个完整的应用示例**，展示如何使?AgentOS 构建实际应用?
| 应用 | 功能描述 | 核心抢?| 完成?|
|------|---------|---------|--------|
| **docgen** | 文档自动生成 | 代码解析、Doxygen/JSDoc、Mermaid ?| 80% 🔄 |
| **ecommerce** | 电商智能?| 多轮对话、个性化推荐、订单处?| 85% 🔄 |
| **research** | 研究助手 | 信息收集、分析报告生?| 规划?|
| **videoedit** | 视频编辑 | 智能体辅助视频剪?| 规划?|

**每个应用包含**?- ?完整的源代码
- ?配置文件 (`config.yaml`)
- ?清单文件 (`manifest.json`)
- ?运行脚本 (`run.sh`)
- ?使用说明 (`README.md`)

### 2. 社区贡献平台 (contrib/)

#### 2.1 智能?(agents/) - 7 个预定义角色

提供覆盖软件弢发全流程?**7 个预定义角色智能?*?
| 智能?| 核心职责 | 双系统支?| 文件结构 |
|--------|---------|-----------|---------|
| **architect** | 架构设计与评?| ?System1/System2 | `agent.py`, `contract.json`, `prompts/` |
| **backend** | 后端弢?| ?System1/System2 | 同上 |
| **frontend** | 前端弢?| ?System1/System2 | 同上 |
| **devops** | 运维部署 | ?System1/System2 | 同上 |
| **product_manager** | 产品规划 | ?System1/System2 | 同上 |
| **security** | 安全审计 | ?System1/System2 | 同上 |
| **tester** | 质量测试 | ?System1/System2 | 同上 |

**每个智能体包?*?- `agent.py` - 智能体核心实?- `contract.json` - 服务合约定义（符?JSON Schema?- `prompts/system1.md` - 快模式提示词
- `prompts/system2.md` - 深度思模式提示词

#### 2.2 抢?(skills/) - 3 个核心技能模?
| 抢?| 功能 | 抢术栈 | 核心能力 |
|------|------|--------|---------|
| **browser_skill** | 浏览器自动化 | Playwright + Selenium | 导航、交互截图爬?|
| **database_skill** | 数据库操?| SQLAlchemy | SQL 查询、ORM、事务管?|
| **github_skill** | GitHub API 集成 | PyGithub | 仓库管理、CI/CD、Issue |

**每个抢能包?*?- `README.md` - 抢能说明文?- `*.py` - 抢能实现代?- `skill.json` - 抢能元数据
- `lib*.so` - 编译后的动链接库（可选）

#### 2.3 策略 (strategies/) - 2 个核心算?
| 策略 | 功能 | 算法 | 应用场景 |
|------|------|------|---------|
| **dispatching** | 任务分发和负载均?| 多维度评分能力匹?| 智能体路?|
| **planning** | 任务规划和路径搜?| 任务分解、依赖分?| DAG 规划 |

### 3. 市场机制 (markets/)

#### 3.1 智能体市?
```
markets/agents/
├─┢ contracts/              # 合约管理
?  ├─┢ schema.json         # JSON Schema 验证
?  ├─┢ validator.py        # 合约验证??  └─┢ example_contract.json
├─┢ installer/              # 安装??  └─┢ cli.py              # 命令行工?└─┢ registry/               # 注册?    └─┢ index.json          # 智能体索?```

**核心功能**?- ?智能体合约定义和验证
- ?丢键安?卸载智能?- ?智能体元数据管理和检?
#### 3.2 抢能市?
```
markets/skills/
├─┢ contracts/              # 合约管理
?  ├─┢ schema.json         # JSON Schema 验证
?  └─┢ validator.py        # 合约验证?├─┢ installer/              # 安装??  └─┢ cli.py              # 命令行工?└─┢ registry/               # 注册?    └─┢ index.json          # 抢能索?```

#### 3.3 弢发模?
| 模板 | 用?| 包含内容 |
|------|------|---------|
| **python-agent** | Python 智能?| 项目结构、示例代码配?|
| **rust-skill** | Rust 抢?| Cargo 配置、FFI 接口、示?|

---

## 🚀 快开?
### 环境要求

- **Python**: 3.10+
- **Node.js**: 18+ (可，用于前端应用)
- **Docker**: 20+ (可，用于容器化部?

### 安装

```bash
# 1. 克隆项目
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS/openlab

# 2. 安装依赖
pip install -e .

# 3. 弢发模式安装（推荐?pip install -e ".[dev]"
```

### 运行示例应用

```bash
# 运行文档生成应用
cd openlab/app/docgen
./run.sh

# 或手动运?python src/main.py --config config.yaml
```

### 使用智能?
```python
from openlab.contrib.agents.architect import ArchitectAgent

# 创建智能?agent = ArchitectAgent()

# 执行任务
result = agent.execute({
    "task": "设计丢个电商系统架?,
    "requirements": ["微服?, "高可?, "可扩?]
})

print(result.output)
```

### 使用抢?
```python
from openlab.contrib.skills.browser_skill import BrowserSkill

# 创建抢能实?skill = BrowserSkill()

# 使用抢?result = skill.execute({
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

# 安装浏览器技?cd openlab/markets/skills/installer
python cli.py install browser_skill
```

---

## 📁 目录结构

```
openlab/
?├─┢ app/                           # 应用示例
?  ├─┢ docgen/                   # 文档生成应用
?  ?  ├─┢ src/
?  ?  ?  ├─┢ generator.py      # 文档生成??  ?  ?  └─┢ main.py           # 主程??  ?  ├─┢ templates/
?  ?  ?  └─┢ default.html.j2   # HTML 模板
?  ?  ├─┢ config.yaml           # 配置文件
?  ?  ├─┢ manifest.json         # 清单文件
?  ?  ├─┢ run.sh                # 运行脚本
?  ?  └─┢ README.md
?  ??  ├─┢ ecommerce/                # 电商应用
?  ?  ├─┢ src/
?  ?  ?  ├─┢ main.py
?  ?  ?  └─┢ utils.py
?  ?  ├─┢ config.yaml
?  ?  ├─┢ manifest.json
?  ?  ├─┢ run.sh
?  ?  └─┢ README.md
?  ??  ├─┢ research/                 # 研究应用
?  ?  └─┢ README.md
?  ??  └─┢ videoedit/                # 视频编辑应用
?      ├─┢ src/
?      ?  ├─┢ edit_pipeline.py
?      ?  └─┢ main.py
?      ├─┢ config.yaml
?      ├─┢ manifest.json
?      ├─┢ run.sh
?      └─┢ README.md
?├─┢ contrib/                       # 社区贡献
?  ├─┢ agents/                   # 智能体贡??  ?  ├─┢ architect/           # 架构??  ?  ├─┢ backend/             # 后端弢??  ?  ├─┢ devops/              # 运维
?  ?  ├─┢ frontend/            # 前端弢??  ?  ├─┢ product_manager/     # 产品经理
?  ?  ├─┢ security/            # 安全专家
?  ?  └─┢ tester/              # 测试工程??  ??  ├─┢ skills/                   # 抢能贡??  ?  ├─┢ browser_skill/       # 浏览器技??  ?  ├─┢ database_skill/      # 数据库技??  ?  └─┢ github_skill/        # GitHub 抢??  ??  └─┢ strategies/               # 策略贡献
?      ├─┢ dispatching/         # 调度策略
?      └─┢ planning/            # 规划策略
?├─┢ markets/                       # 市场机制
?  ├─┢ agents/                   # 智能体市??  ?  ├─┢ contracts/           # 合约
?  ?  ├─┢ installer/           # 安装??  ?  └─┢ registry/            # 注册??  ??  ├─┢ skills/                   # 抢能市??  ?  ├─┢ contracts/           # 合约
?  ?  ├─┢ installer/           # 安装??  ?  └─┢ registry/            # 注册??  ??  └─┢ templates/                # 弢发模??      ├─┢ python-agent/        # Python 智能体模??      └─┢ rust-skill/          # Rust 抢能模??├─┢ openlab/                       # OpenLab 核心
?  ├─┢ core/                     # 核心模块
?  ?  ├─┢ agent.py             # Agent 管理
?  ?  ├─┢ task.py              # Task 调度
?  ?  ├─┢ tool.py              # Tool 抽象
?  ?  └─┢ storage.py           # 存储后端
?  ├─┢ agents/                   # 内置智能??  ?  └─┢ architect/
?  ├─┢ __init__.py
?  ├─┢ config.yaml
?  ├─┢ requirements.txt
?  └─┢ run.sh
?├─┢ tests/                         # 测试套件
?  ├─┢ unit/
?  ?  ├─┢ test_agent.py
?  ?  ├─┢ test_dispatching.py
?  ?  ├─┢ test_planning.py
?  ?  └─┢ test_videoedit.py
?  ├─┢ conftest.py
?  └─┢ __init__.py
?├─┢ .github/
?  └─┢ workflows/
?      └─┢ ci.yml                # CI/CD 配置
?├─┢ Dockerfile                     # Docker 镜像
├─┢ pyproject.toml                 # Python 项目配置
└─┢ README.md                      # 本文?```

---

## 📚 使用指南

### 1. 弢发新应用

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

### 2. 弢发新智能?
```bash
# 1. 创建智能体目?cd openlab/contrib/agents
mkdir my_agent
cd my_agent

# 2. 创建必要文件
touch agent.py contract.json
mkdir prompts
touch prompts/system1.md prompts/system2.md

# 3. 编写智能体代?cat > agent.py << 'EOF'
from openlab.core.agent import Agent, AgentContext
from typing import Dict, Any

class MyAgent(Agent):
    """我的智能?""
    
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
        """关闭智能?""
        pass
EOF
```

### 3. 弢发新抢?
```bash
# 1. 创建抢能目?cd openlab/contrib/skills
mkdir my_skill
cd my_skill

# 2. 创建必要文件
touch my_skill.py skill.json README.md

# 3. 编写抢能代?cat > my_skill.py << 'EOF'
from dataclasses import dataclass
from typing import Any, Dict, Optional

@dataclass
class MySkillResult:
    """抢能执行结?""
    success: bool
    data: Any = None
    error: Optional[str] = None
    duration: float = 0.0

class MySkill:
    """我的抢?""
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        self.config = config or {}
    
    def execute(self, action: str, **kwargs) -> MySkillResult:
        """执行抢?""
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

# 生成覆盖率报?pytest --cov=openlab --cov-report=html
```

---

## 🏗?架构设计

### 设计原则

OpenLab 遵循 AgentOS 的核心架构设计原则：

| 原则 | 说明 | 实现位置 |
|------|------|---------|
| **模块?* | 高内聚低耦合 | `openlab/core/` 四大模块 |
| **可扩?* | 支持无缝添加新组?| `contrib/` 扩展机制 |
| **双系统协?* | System1 + System2 | `prompts/system1.md`, `system2.md` |
| **安全内生** | 安全机制内置 | `workbench_id` 隔离 |
| **反馈闭环** | 三层嵌套反馈 | `TaskScheduler._feedback_loop()` |

### 核心架构

```
┌─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢??          应用?(Applications)          ?? docgen  ? ecommerce  ? videoedit     ?├─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢??          生层 (Ecosystem)             ?? ┌─┢┢┢┢┢┢┢┢┢┢┢┬─┢┢┢┢┢┢┢┢┢┢┢┬─┢┢┢┢┢┢┢┢? ?? ? 智能?   ?   抢?   ? 策略   ? ?? ? (Agents)  ? (Skills)  ?Strategy)??? └─┢┢┢┢┢┢┢┢┢┢┢┴─┢┢┢┢┢┢┢┢┢┢┢┴─┢┢┢┢┢┢┢┢? ?├─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢??          市场?(Markets)               ?? ┌─┢┢┢┢┢┢┢┢┢┢┢┬─┢┢┢┢┢┢┢┢┢┢┢┬─┢┢┢┢┢┢┢┢? ?? ? 合约      ?  安装?  ?注册? ? ?? ?Contracts) ?(Installer)?Registry)??? └─┢┢┢┢┢┢┢┢┢┢┢┴─┢┢┢┢┢┢┢┢┢┢┢┴─┢┢┢┢┢┢┢┢? ?├─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢??          核心?(CoreKern)              ?? ┌─┢┢┢┢┢┢┢┢┢┢┢┬─┢┢┢┢┢┢┢┢┢┢┢┬─┢┢┢┢┢┢┢┢? ?? ? Agent     ?  Task     ? Tool   ? ?? ? Manager   ?Scheduler  ?Registry? ?? └─┢┢┢┢┢┢┢┢┢┢┢┴─┢┢┢┢┢┢┢┢┢┢┢┴─┢┢┢┢┢┢┢┢? ?└─┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢┢?```

### 核心模块职责

| 模块 | 文件 | 核心?| 功能 |
|------|------|--------|------|
| **Agent** | `openlab/core/agent.py` | `Agent`, `AgentManager`, `AgentRegistry` | 智能体生命周期管?|
| **Task** | `openlab/core/task.py` | `Task`, `TaskScheduler`, `TaskDefinition` | 任务调度和执?|
| **Tool** | `openlab/core/tool.py` | `Tool`, `ToolRegistry`, `ToolExecutor` | 工具抽象和执?|
| **Storage** | `openlab/core/storage.py` | `Storage`, `SQLiteStorage`, `Query` | 持久化存?|

---

## 🤝 贡献指南

我们热烈欢迎社区贡献?
### 贡献类型

1. **提交智能?* - ?`contrib/agents/` 下创建新的智能体
2. **弢发技?* - ?`contrib/skills/` 下添加新的技能模?3. **分享策略** - ?`contrib/strategies/` 下贡献算法策?4. **完善文档** - 改进现有文档和示?5. **报告问题** - 提交 Bug 报告和功能建?
### 贡献流程

```bash
# 1. Fork 项目
# 2. 创建分支
git checkout -b feature/my-contribution

# 3. 提交代码
git commit -m "feat: 添加我的贡献"

# 4. 推分?git push origin feature/my-contribution

# 5. 创建 Pull Request
```

### 代码规范

- ?遵循 [PEP 8](https://pep8.org/) Python 编码规范
- ?使用类型注解
- ?编写单元测试
- ?添加文档字符?- ?运行 `black`, `flake8`, `mypy` 棢?
---

## 📖 相关文档

- **[AgentOS 主文档](../README.md)** - AgentOS 总体介绍
- **[核心服务层文档](../agentos/daemon/README.md)** - Daemon 服务层详?- **[内核架构文档](../agentos/atoms/README.md)** - Atoms 内核架构
- **[SDK 使用指南](../agentos/toolkit/README.md)** - 弢?SDK 使用
- **[架构设计原则](../agentos/docs/architecture/ARCHITECTURAL_PRINCIPLES.md)** - 五维正交体系

---

## 📄 许可?
Apache License 2.0

详见 [LICENSE](../LICENSE)

---

## 📞 联系方式

- **项目主页**: https://github.com/SpharxTeam/AgentOS
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues
- **抢术支?*: lidecheng@spharx.cn

---

<div align="center">

**"From data intelligence emerges."**

*始于数据，终于智?

© 2026 SPHARX Ltd. 保留扢有权?
</div>
