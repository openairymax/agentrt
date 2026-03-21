# OpenHub - 开放协作中心

OpenHub（开放协作中心）是 AgentOS 的开放生态平台，提供应用示例、社区贡献和市场机制的完整支持。

## 📁 目录结构

```
openhub/
├── app/                       # 应用示例 (Applications)
│   ├── docgen/               # 文档生成应用
│   ├── ecommerce/            # 电子商务应用
│   ├── research/             # 研究应用
│   └── videoedit/            # 视频编辑应用
│
├── contrib/                  # 社区贡献 (Community Contributions)
│   ├── agents/              # 智能体贡献
│   │   ├── architect/       # 架构师智能体
│   │   ├── backend/         # 后端开发智能体
│   │   ├── devops/          # 运维智能体
│   │   ├── frontend/        # 前端开发智能体
│   │   ├── product_manager/ # 产品经理智能体
│   │   ├── security/        # 安全智能体
│   │   └── tester/          # 测试智能体
│   ├── skills/              # 技能贡献
│   │   ├── browser_skill/   # 浏览器技能
<!-- From data intelligence emerges. by spharx -->
│   │   ├── database_skill/  # 数据库技能
│   │   └── github_skill/    # GitHub 技能
│   └── strategies/          # 策略贡献
│       ├── dispatching/     # 调度策略
│       └── planning/        # 规划策略
│
└── markets/                  # 市场机制 (Markets)
    ├── agents/              # 智能体市场
    │   ├── contracts/       # 智能体合约
    │   ├── installer/       # 智能体安装器
    │   └── registry/        # 智能体注册表
    ├── skills/              # 技能市场
    │   ├── contracts/       # 技能合约
    │   ├── installer/       # 技能安装器
    │   └── registry/        # 技能注册表
    └── templates/           # 模板市场
        ├── python-agent/    # Python 智能体模板
        └── rust-skill/      # Rust 技能模板
```

## 🎯 主要功能

### 应用示例 (app/)

提供基于 AgentOS 构建的示例应用，展示如何使用系统提供的各种能力：

- **docgen**: 自动生成 API 文档和 Markdown 文档站点
- **ecommerce**: 电子商务应用示例
- **research**: 研究应用示例
- **videoedit**: 视频编辑应用示例

每个应用都包含完整的配置文件、源代码和运行脚本。

### 社区贡献 (contrib/)

#### 智能体 (agents/)

预定义的角色智能体，每个智能体都有：
- `agent.py` - 智能体实现代码
- `contract.json` - 智能体服务合约
- `prompts/` - 提示词模板
  - `system1.md` - 快速思考模式
  - `system2.md` - 深度思考模式

支持的智能体角色：
- 架构师 (architect)
- 后端开发 (backend)
- 运维 (devops)
- 前端开发 (frontend)
- 产品经理 (product_manager)
- 安全专家 (security)
- 测试工程师 (tester)

#### 技能 (skills/)

可插拔的技能模块，每个技能包含：
- `README.md` - 技能说明文档
- `lib*.so` - 编译后的动态链接库
- `skill.json` - 技能描述文件

可用技能：
- 浏览器操作技能 (browser_skill)
- 数据库操作技能 (database_skill)
- GitHub 集成技能 (github_skill)

#### 策略 (strategies/)

核心算法策略实现：
- **调度策略 (dispatching)**: 任务分发和负载均衡
- **规划策略 (planning)**: 任务规划和路径搜索

### 市场机制 (markets/)

#### 智能体市场 (agents/)

- **合约 (contracts/)**: 智能体服务合约定义和验证
- **安装器 (installer/)**: 智能体安装和管理工具
- **注册表 (registry/)**: 智能体元数据和索引

#### 技能市场 (skills/)

- **合约 (contracts/)**: 技能服务合约定义和验证
- **安装器 (installer/)**: 技能安装和管理工具
- **注册表 (registry/)**: 技能元数据和索引

#### 模板市场 (templates/)

提供智能体和技能开发的模板：
- **Python 智能体模板**: 快速创建 Python 智能体
- **Rust 技能模板**: 快速创建 Rust 技能

## 🚀 使用指南

### 运行示例应用

```bash
cd openhub/app/docgen
./run.sh
```

### 安装智能体

```bash
cd openhub/markets/agents/installer
python cli.py install <agent-name>
```

### 使用技能

```python
from agentos import Skill

# 加载技能
skill = Skill.load("browser_skill")

# 使用技能
result = skill.execute(action="navigate", url="https://example.com")
```

## 🤝 贡献指南

我们欢迎社区贡献：

1. **提交智能体**: 在 `contrib/agents/` 下创建新的智能体
2. **开发技能**: 在 `contrib/skills/` 下添加新的技能模块
3. **分享策略**: 在 `contrib/strategies/` 下贡献算法策略
4. **完善文档**: 改进现有文档和示例

## 📖 相关文档

- [AgentOS 主文档](../README.md)
- [核心服务层文档](../backs/README.md)
- [内核架构文档](../atoms/README.md)
- [SDK 使用指南](../tools/README.md)

## 📄 许可证

Apache License 2.0 - 详见 [LICENSE](../LICENSE)

---

© 2026 SPHARX Ltd. 保留所有权利。
