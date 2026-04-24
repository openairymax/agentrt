# Market Daemon — 应用市场守护进程

`daemon/market_d/` 是 AgentOS 的应用市场守护进程，负责 Agent/Skill 市场的管理，包括发布、搜索、安装和版本管理。

## 核心职责

- **市场管理**：管理 Agent 和 Skill 的发布与分发
- **搜索发现**：按名称、能力、标签等维度搜索可用资源
- **版本管理**：跟踪资源版本、更新日志和依赖关系
- **安装管理**：处理资源的下载、安装和环境配置
- **评分评级**：用户评分、使用统计和质量评级

## 市场资源类型

| 类型 | 说明 |
|------|------|
| **Agent** | 可部署的智能体应用 |
| **Skill** | 可复用的能力模块 |
| **Tool** | 外部工具集成 |
| **Template** | 项目模板 |

## 核心能力

| 能力 | 说明 |
|------|------|
| `market.publish` | 发布资源到市场 |
| `market.search` | 搜索市场资源 |
| `market.info` | 查询资源详情 |
| `market.install` | 安装市场资源 |
| `market.uninstall` | 卸载已安装资源 |
| `market.update` | 更新已安装资源 |
| `market.versions` | 查询资源版本历史 |

## 使用方式

```bash
# 启动市场守护进程
./market_d --config market_config.json

# 指定市场数据目录
./market_d --data-dir /opt/agentos/market

# 启用自动同步
./market_d --sync-interval 3600
```

---

*AgentOS Daemon — Market Daemon*
