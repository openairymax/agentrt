# 市场服务 (market_d)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`market_d` 是 AgentOS 的**资源注册与分发中心**，提供 Agent 和技能的完整生命周期管理。作为智能体生态的核心枢纽，本服务维护所有可用资源的元数据、版本信息和依赖关系。

### 核心价值

- **统一注册**: 集中管理所有 Agent 和技能的注册信息
- **版本控制**: 语义化版本管理，支持多版本共存
- **依赖解析**: 自动解析和安装依赖包
- **状态追踪**: 实时监控资源运行状态和健康度

---

## 📁 目录结构

```
market_d/
├── README.md                 # 本文档
├── CMakeLists.txt            # 构建配置
├── include/
│   └── market_service.h      # 服务接口定义
├── src/
│   ├── main.c                # 程序入口
│   ├── service.c             # 服务主逻辑
│   ├── agent_registry.c      # Agent 注册表实现
│   ├── skill_registry.c      # 技能注册表实现
│   ├── installer.c           # 安装管理器
│   ├── publisher.c           # 发布管理器
│   ├── version_manager.c     # 版本控制器
│   └── dependency_resolver.c # 依赖解析器
├── tests/                    # 单元测试
│   ├── test_registry.c
│   ├── test_installer.c
│   └── test_versioning.c
└── config/
    └── market.yaml           # 服务配置模板
```

---

## 🔧 核心功能

### 1. Agent 注册表

维护所有可用 Agent 的完整信息：

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | Agent 唯一标识 |
| `name` | string | 显示名称 |
| `type` | string | 类型 (planning/execution/hybrid) |
| `version` | string | 语义化版本号 |
| `status` | enum | 状态 (active/inactive/maintenance) |
| `capabilities` | array | 能力列表 |
| `config_path` | string | 配置文件路径 |
| `health_score` | float | 健康评分 (0-1) |

**示例数据结构**:
```json
{
  "id": "agent-architect-001",
  "name": "架构师智能体",
  "type": "planning",
  "version": "1.2.0",
  "status": "active",
  "capabilities": ["task_decomposition", "dependency_analysis"],
  "config_path": "/var/agentos/config/architect.yaml",
  "health_score": 0.98
}
```

### 2. 技能注册表

管理所有可插拔技能模块：

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | 技能唯一标识 |
| `name` | string | 技能名称 |
| `version` | string | 版本号 |
| `library_path` | string | .so/.dll 文件路径 |
| `manifest_path` | string | manifest.json 路径 |
| `dependencies` | array | 依赖技能列表 |
| `installed_at` | timestamp | 安装时间 |

### 3. 安装管理器

提供资源的安装、升级、卸载功能：

```bash
# 安装 Agent
market_d install agent --name architect --version 1.2.0

# 安装技能
market_d install skill --name browser_skill --from local:/path/to/skill

# 升级
market_d upgrade agent architect --to-version 1.3.0

# 卸载
market_d uninstall agent architect
```

### 4. 依赖解析器

自动解析复杂的依赖关系：

```python
# 依赖图示例
architect_agent:
  depends_on:
    - planning_strategy: ">=1.0.0"
    - memory_access: "^2.0.0"
  conflicts_with:
    - legacy_scheduler
```

---

## 🌐 API 接口

### RESTful API

#### 查询 Agent 列表

```http
GET /api/v1/agents
Query Parameters:
  - type: 过滤类型
  - status: 过滤状态
  - page: 页码
  - per_page: 每页数量

Response 200:
{
  "total": 15,
  "page": 1,
  "per_page": 10,
  "agents": [...]
}
```

#### 获取 Agent 详情

```http
GET /api/v1/agents/{agent_id}

Response 200:
{
  "id": "agent-architect-001",
  "name": "架构师智能体",
  ...
}
```

#### 注册新 Agent

```http
POST /api/v1/agents
Content-Type: application/json

{
  "name": "新智能体",
  "type": "execution",
  "version": "1.0.0",
  "config": {...}
}

Response 201:
{
  "id": "agent-new-001",
  "status": "registered"
}
```

#### 技能安装

```http
POST /api/v1/skills/install
Content-Type: application/json

{
  "skill_name": "browser_skill",
  "source": "marketplace",
  "version": "2.1.0"
}

Response 202:
{
  "task_id": "install-task-123",
  "status": "processing"
}
```

### gRPC 接口

```protobuf
service MarketService {
  // Agent 管理
  rpc RegisterAgent(RegisterAgentRequest) returns (RegisterAgentResponse);
  rpc GetAgent(GetAgentRequest) returns (AgentInfo);
  rpc ListAgents(ListAgentsRequest) returns (ListAgentsResponse);
  rpc UpdateAgent(UpdateAgentRequest) returns (UpdateAgentResponse);
  rpc UnregisterAgent(UnregisterAgentRequest) returns (UnregisterAgentResponse);
  
  // 技能管理
  rpc RegisterSkill(RegisterSkillRequest) returns (RegisterSkillResponse);
  rpc InstallSkill(InstallSkillRequest) returns (InstallSkillResponse);
  rpc UninstallSkill(UninstallSkillRequest) returns (UninstallSkillResponse);
  
  // 依赖解析
  rpc ResolveDependencies(ResolveDependenciesRequest) returns (DependencyGraph);
}
```

---

## ⚙️ 配置选项

### 服务配置 (config/market.yaml)

```yaml
server:
  port: 8082
  grpc_port: 9082
  max_connections: 1000

registry:
  storage_backend: sqlite  # sqlite/postgresql/mysql
  database_path: /var/agentos/data/market.db
  cache_enabled: true
  cache_ttl_sec: 300

installer:
  download_timeout_sec: 300
  verify_signatures: true
  auto_backup: true
  rollback_on_failure: true

dependency_resolver:
  algorithm: topological_sort  # 拓扑排序
  max_depth: 10
  detect_cycles: true

logging:
  level: info
  format: json
  output: /var/agentos/logs/market_d.log
```

---

## 🚀 使用指南

### 启动服务

```bash
# 方式 1: 直接启动
./build/backs/market_d/agentos-market-d

# 方式 2: 指定配置文件
./build/backs/market_d/agentos-market-d --config ../config/service/market_d/market.yaml

# 方式 3: 环境变量覆盖
MARKET_D_PORT=9000 ./build/backs/market_d/agentos-market-d
```

### 注册 Agent

```bash
# 准备 Agent 描述文件
cat > architect_agent.json << EOF
{
  "name": "架构师",
  "type": "planning",
  "version": "1.2.0",
  "capabilities": ["task_planning", "architecture_design"],
  "entry_point": "/opt/agentos/agents/architect/main.py",
  "config_file": "/opt/agentos/config/architect.yaml"
}
EOF

# 注册
curl -X POST http://localhost:8082/api/v1/agents \
  -H "Content-Type: application/json" \
  -d @architect_agent.json
```

### 查询已安装的 Agent

```bash
# 列出所有 Agent
curl http://localhost:8082/api/v1/agents

# 按类型过滤
curl "http://localhost:8082/api/v1/agents?type=planning"

# 查看特定 Agent
curl http://localhost:8082/api/v1/agents/agent-architect-001
```

### 安装技能

```bash
# 从市场安装
curl -X POST http://localhost:8082/api/v1/skills/install \
  -H "Content-Type: application/json" \
  -d '{"skill_name": "browser_skill", "version": "2.1.0"}'

# 从本地安装
curl -X POST http://localhost:8082/api/v1/skills/install \
  -H "Content-Type: application/json" \
  -d '{"skill_name": "custom_skill", "source": "file:///path/to/skill"}'
```

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM):

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| 注册延迟 | < 10ms | 单个 Agent 注册 |
| 查询延迟 | < 5ms | 缓存命中场景 |
| 安装吞吐 | 50+ 个/分钟 | 技能包安装 |
| 并发连接 | 1000+ | 每服务实例 |
| 数据库大小 | ~50MB | 1000 个 Agent 记录 |

---

## 🧪 测试

### 运行单元测试

```bash
cd backs/build
ctest -R market_d --output-on-failure

# 运行特定测试
ctest -R test_registry --verbose
ctest -R test_installer --verbose
```

### Python SDK 测试

```python
import requests

# 测试 Agent 注册
response = requests.post(
    'http://localhost:8082/api/v1/agents',
    json={
        'name': '测试智能体',
        'type': 'execution',
        'version': '1.0.0'
    }
)
assert response.status_code == 201

# 测试 Agent 列表
response = requests.get('http://localhost:8082/api/v1/agents')
assert response.status_code == 200
assert 'agents' in response.json()
```

---

## 🔧 故障排查

### 问题 1: Agent 注册失败

**症状**: 返回 400 错误

**解决方案**:
```bash
# 检查配置文件格式
python -c "import yaml; yaml.safe_load(open('agent_config.yaml'))"

# 验证必填字段
jq 'has("name") and has("type") and has("version")' agent_config.json
```

### 问题 2: 依赖冲突

**症状**: 安装时提示依赖冲突

**解决方案**:
```bash
# 查看依赖树
market_d deps tree architect_agent

# 强制安装 (不推荐)
market_d install skill --name xyz --force
```

### 问题 3: 数据库锁定

**症状**: SQLite 数据库锁定错误

**解决方案**:
```bash
# 检查进程占用
lsof /var/agentos/data/market.db

# 重启服务
systemctl restart agentos-market-d
```

---

## 🔗 相关文档

- [服务层总览](../README.md) - Backs 架构说明
- [Agent 开发指南](../../partdocs/guides/folder/create_agent.md) - 如何创建 Agent
- [技能开发指南](../../partdocs/guides/folder/create_skill.md) - 如何开发技能
- [部署指南](../../partdocs/guides/folder/deployment.md) - 生产环境部署

---

<div align="center">

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges 始于数据，终于智能。"*

</div>
