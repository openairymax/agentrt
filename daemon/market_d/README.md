# 市场服务 (market_d)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`market_d` 是 AgentOS 的**Agent 和技能市场管理系统**，提供 Agent 注册、技能发布、版本管理、依赖解析等功能。作为生态系统的"应用商店"，本服务连接 Agent 开发者和使用者，促进能力共享和复用。

### 核心价值

- **统一市场**: Agent 和技能的集中管理和发现
- **版本控制**: 语义化版本管理和兼容性检查
- **依赖解析**: 自动解析和安装依赖关系
- **权限管理**: 基于角色的访问控制和授权

---

## 📁 目录结构

```
market_d/
├── README.md                 # 本文档
├── CMakeLists.txt            # 构建配置
├── include/
│   ├── market_service.h      # 服务接口
│   ├── agent_registry.h      # Agent 注册表
│   ├── skill_registry.h      # 技能注册表
│   └── version_manager.h     # 版本管理
├── src/
│   ├── main.c                # 程序入口
│   ├── service.c             # 服务主逻辑
│   ├── agent_registry.c      # Agent 注册管理
│   ├── skill_registry.c      # 技能注册管理
│   ├── installer.c           # 安装管理器
│   ├── publisher.c           # 发布管理器
│   └── dependency.c          # 依赖解析
├── tests/                    # 测试
│   ├── test_registry.c
│   ├── test_installer.c
│   └── test_dependency.c
└── config/
    └── market.yaml           # 服务配置
```

---

## 🔧 核心功能

### 1. Agent 注册表

维护所有可用 Agent 的元数据：

| 字段 | 类型 | 说明 |
|------|------|------|
| `agent_id` | string | Agent 唯一标识 |
| `name` | string | Agent 名称 |
| `version` | string | 版本号（语义化） |
| `description` | string | 功能描述 |
| `author` | string | 作者信息 |
| `category` | string | 分类标签 |
| `skills` | array | 包含的技能列表 |
| `dependencies` | array | 依赖的其他 Agent |
| `download_url` | string | 下载地址 |
| `checksum` | string | SHA256 校验和 |

**示例数据结构**:
```json
{
  "agent_id": "agent-customer-service",
  "name": "智能客服 Agent",
  "version": "2.1.0",
  "description": "专业的客户服务助手",
  "author": "Spharx Team",
  "category": "customer_service",
  "skills": [
    "skill-faq-answer",
    "skill-complaint-handle",
    "skill-order-query"
  ],
  "dependencies": [
    {"agent_id": "agent-nlp-base", "version": "^1.0.0"}
  ],
  "download_url": "https://market.agentos.com/agents/customer-service-v2.1.0.tar.gz",
  "checksum": "sha256:a1b2c3d4e5f6..."
}
```

### 2. 技能注册表

维护所有可用技能的详细信息：

```json
{
  "skill_id": "skill-web-search",
  "name": "web_search",
  "version": "1.2.3",
  "description": "互联网搜索引擎接口",
  "category": "information_retrieval",
  "provider": "search-inc",
  "input_schema": {
    "type": "object",
    "properties": {
      "query": {"type": "string"},
      "num_results": {"type": "integer", "default": 10}
    }
  },
  "output_schema": {
    "type": "array",
    "items": {
      "title": "string",
      "url": "string",
      "snippet": "string"
    }
  },
  "pricing": {
    "model": "per_call",
    "price_usd": 0.001
  },
  "rating": {
    "average": 4.8,
    "count": 1250
  }
}
```

### 3. 版本管理

语义化版本控制和兼容性检查：

**版本格式**: `MAJOR.MINOR.PATCH` (如：2.1.0)

**版本规则**:
- `MAJOR`: 不兼容的 API 变更
- `MINOR`: 向后兼容的功能增加
- `PATCH`: 向后兼容的问题修复

**版本范围表示**:
```yaml
精确匹配："1.2.3"        # 必须是指定版本
>= 匹配： ">=1.2.0"      # 大于等于指定版本
^ 兼容： "^1.2.0"        # 1.2.0 <= v < 2.0.0
~ 近似： "~1.2.0"        # 1.2.0 <= v < 1.3.0
* 通配： "1.*"           # 1.0.0 <= v < 2.0.0
```

### 4. 依赖解析

自动解析和安装依赖关系：

```python
# 依赖解析算法（简化版）
def resolve_dependencies(agent_requirements):
    """
    输入: [{agent_id: "agent-a", version: "^1.0.0"}, ...]
    输出: 安装顺序图或冲突报告
    """
    dependency_graph = build_graph(agent_requirements)
    
    # 检测循环依赖
    if has_cycle(dependency_graph):
        return Error("Circular dependency detected")
    
    # 版本冲突检测
    conflicts = detect_version_conflicts(dependency_graph)
    if conflicts:
        return Error(f"Version conflicts: {conflicts}")
    
    # 拓扑排序确定安装顺序
    install_order = topological_sort(dependency_graph)
    
    return install_order
```

---

## 🌐 API 接口

### RESTful API

#### 1. 注册 Agent

```http
POST /api/v1/agents/register
Content-Type: application/json

{
  "agent_id": "agent-new-001",
  "name": "新 Agent",
  "version": "1.0.0",
  "description": "功能描述",
  "package_url": "https://...",
  "checksum": "sha256:..."
}

Response 201:
{
  "agent_id": "agent-new-001",
  "status": "registered",
  "registry_url": "/api/v1/agents/agent-new-001"
}
```

#### 2. 搜索 Agent

```http
GET /api/v1/agents/search?keyword=客服&category=customer_service&page=1
```

**响应**:
```json
{
  "total": 25,
  "page": 1,
  "page_size": 20,
  "agents": [
    {
      "agent_id": "agent-customer-service",
      "name": "智能客服 Agent",
      "version": "2.1.0",
      "rating": 4.8,
      "downloads": 12500
    }
  ]
}
```

#### 3. 获取 Agent 详情

```http
GET /api/v1/agents/{agent_id}
```

**响应**:
```json
{
  "agent_id": "agent-customer-service",
  "name": "智能客服 Agent",
  "versions": ["2.0.0", "2.0.1", "2.1.0"],
  "latest_version": "2.1.0",
  "stats": {
    "total_downloads": 12500,
    "monthly_downloads": 850,
    "rating": 4.8,
    "review_count": 125
  }
}
```

#### 4. 安装 Agent

```http
POST /api/v1/agents/{agent_id}/install
Content-Type: application/json

{
  "version": "^2.0.0",
  "target_path": "/opt/agentos/agents"
}

Response 200:
{
  "status": "installing",
  "task_id": "task-install-001",
  "estimated_time_sec": 30
}
```

#### 5. 解析依赖

```http
POST /api/v1/dependency/resolve
Content-Type: application/json

{
  "requirements": [
    {"agent_id": "agent-a", "version": "^1.0.0"},
    {"agent_id": "agent-b", "version": ">=2.0.0"}
  ]
}

Response 200:
{
  "resolved": true,
  "install_order": [
    {"agent_id": "agent-base", "version": "1.5.0"},
    {"agent_id": "agent-a", "version": "1.2.0"},
    {"agent_id": "agent-b", "version": "2.3.0"}
  ]
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
  storage_backend: postgresql
  database_url: postgresql://user:pass@localhost/market_db
  pool_size: 20
  
  # 存储路径
  storage_path: /var/agentos/market/storage
  cdn_enabled: true
  cdn_url: https://cdn.agentos.com

search:
  engine: elasticsearch
  url: http://localhost:9200
  index_prefix: agentos_market

cache:
  enabled: true
  backend: redis
  ttl_sec: 3600
  redis_url: redis://localhost:6379

security:
  require_auth_for_publish: true
  allowed_publishers:
    - admin
    - verified_developer
  scan_packages: true  # 安全扫描
  virus_scan_engine: clamav

rate_limit:
  enabled: true
  requests_per_minute: 100
  burst: 20

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
./build/daemon/market_d/agentos-market-d

# 方式 2: 指定配置文件
./build/daemon/market_d/agentos-market-d \
  --config ../config/market.yaml
```

### 发布 Agent

```bash
# 1. 打包 Agent
tar -czf my-agent-1.0.0.tar.gz agent/

# 2. 计算校验和
sha256sum my-agent-1.0.0.tar.gz > my-agent-1.0.0.sha256

# 3. 上传到存储
curl -X PUT https://storage.agentos.com/agents/my-agent-1.0.0.tar.gz \
  --upload-file my-agent-1.0.0.tar.gz

# 4. 注册到市场
curl -X POST http://localhost:8082/api/v1/agents/register \
  -H "Content-Type: application/json" \
  -d '{
    "agent_id": "agent-my-agent",
    "name": "我的 Agent",
    "version": "1.0.0",
    "package_url": "https://storage.agentos.com/agents/my-agent-1.0.0.tar.gz",
    "checksum": "sha256:abc123..."
  }'
```

### 安装 Agent

```bash
# 搜索 Agent
curl "http://localhost:8082/api/v1/agents/search?keyword=客服"

# 安装 Agent
curl -X POST http://localhost:8082/api/v1/agents/agent-customer-service/install \
  -H "Content-Type: application/json" \
  -d '{
    "version": "^2.0.0",
    "target_path": "/opt/agentos/agents"
  }'

# 查询安装进度
curl http://localhost:8082/api/v1/tasks/task-install-001/status
```

### 依赖解析示例

```bash
# 准备依赖文件 cat requirements.yaml
requirements:
  - agent_id: agent-nlp-base
    version: "^1.0.0"
  - agent_id: agent-knowledge-base
    version: ">=2.0.0"

# 解析依赖
curl -X POST http://localhost:8082/api/v1/dependency/resolve \
  -H "Content-Type: application/json" \
  -d @requirements.yaml

# 批量安装
resolved=$(curl -X POST ...)
install_order=$(echo $resolved | jq -r '.install_order[]')

for agent in $install_order; do
  agent_id=$(echo $agent | jq -r '.agent_id')
  version=$(echo $agent | jq -r '.version')
  
  echo "Installing $agent_id ($version)..."
  curl -X POST http://localhost:8082/api/v1/agents/$agent_id/install \
    -H "Content-Type: application/json" \
    -d "{\"version\": \"$version\"}"
done
```

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM, PostgreSQL):

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| 注册延迟 | < 10ms | 单个 Agent 注册 |
| 搜索延迟 | < 50ms | P95，带缓存 |
| 详情查询延迟 | < 5ms | 单次查询 |
| 并发安装数 | 50+ | 同时安装 Agent |
| 依赖解析速度 | < 100ms | 10 个依赖以内 |
| 数据库 QPS | 5000+ | 只读查询 |

---

## 🔍 故障排查

### 常见问题

#### 1. 版本冲突

**错误**:
```json
{
  "error": "Version conflict detected",
  "details": "agent-a requires lib-x ^1.0.0, but agent-b requires lib-x ^2.0.0"
}
```

**解决方案**:
- 检查各 Agent 的依赖要求
- 寻找兼容的版本组合
- 联系开发者更新依赖声明

#### 2. 循环依赖

**错误**:
```json
{
  "error": "Circular dependency detected",
  "path": "agent-a -> agent-b -> agent-c -> agent-a"
}
```

**解决方案**:
- 重构依赖关系，打破循环
- 提取公共依赖为独立模块

#### 3. 安装包下载失败

**错误**:
```json
{
  "error": "Download failed",
  "status_code": 404,
  "url": "https://..."
}
```

**解决方案**:
- 检查 package_url 是否正确
- 验证网络连接
- 确认文件存在且可访问

### 调试技巧

```bash
# 查看详细日志
tail -f /var/agentos/logs/market_d.log

# 测试 API 连通性
curl http://localhost:8082/api/v1/health

# 查询数据库状态
psql -U agentos -d market_db -c "SELECT COUNT(*) FROM agents;"

# 清理缓存
redis-cli FLUSHDB
```

---

## 🧪 测试

```bash
# 单元测试
cd daemon/build
ctest -R market_d --output-on-failure

# 集成测试
../../tests/integration/test_market_service.sh

# Python SDK 测试
python3 ../../tests/sdk/market_test.py
```

---

## 📄 许可证

market_d 服务采用 **Apache License 2.0** 开源协议。

详见项目根目录的 [LICENSE](../../LICENSE) 文件。

---

## 🔗 相关文档

- [服务层总览](../README.md) - daemon 架构说明
- [Agent 开发指南](../../paper/guides/folder/create_agent.md) - 如何开发 Agent
- [技能开发指南](../../paper/guides/folder/create_skill.md) - 如何开发技能
- [版本管理规范](../../paper/specifications/versioning.md) - 语义化版本标准
- [部署指南](../../paper/guides/folder/deployment.md) - 生产环境部署

---

<div align="center">

**market_d - 构建繁荣的 Agent 生态系统**

[返回顶部](#市场服务 -market_d)

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
