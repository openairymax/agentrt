# 工具服务 (tool_d)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`tool_d` 是 AgentOS 的**工具执行引擎**，提供技能的注册、验证、执行和缓存管理。作为系统的"双手"，本服务将 Agent 的意图转化为具体的操作执行。

### 核心价值

- **工具注册**: 统一的技能注册和发现机制
- **契约验证**: 严格的输入输出类型检查
- **高效执行**: 并发执行和结果缓存
- **错误隔离**: 沙箱环境执行，故障不影响主系统

---

## 📁 目录结构

```
tool_d/
├── README.md                 # 本文档
├── CMakeLists.txt            # 构建配置
├── include/
│   └── tool_service.h        # 服务接口定义
├── src/
│   ├── main.c                # 程序入口
│   ├── service.c             # 服务主逻辑
│   ├── registry.c            # 工具注册表
│   ├── executor.c            # 工具执行器
│   ├── validator.c           # 契约验证器
│   ├── cache.c               # 结果缓存
│   └── manager.c              # 配置管理
├── tests/                    # 单元测试
│   ├── test_registry.c
│   ├── test_executor.c
│   └── test_validator.c
└── agentos/manager/
    └── tool.yaml             # 服务配置模板
```

---

## 🔧 核心功能

### 1. 工具注册表

维护所有可用工具的元数据：

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | 工具唯一标识 |
| `name` | string | 工具名称 |
| `version` | string | 版本号 |
| `description` | string | 功能描述 |
| `input_schema` | JSON Schema | 输入参数格式 |
| `output_schema` | JSON Schema | 输出结果格式 |
| `execution_mode` | enum | 执行模式 (sync/async/sandbox) |
| `timeout_ms` | int | 超时时间（毫秒） |

**示例数据结构**:
```json
{
  "id": "tool-browser-navigate",
  "name": "browser_navigate",
  "version": "1.0.0",
  "description": "Navigate to a URL in browser",
  "input_schema": {
    "type": "object",
    "properties": {
      "url": {"type": "string", "format": "uri"},
      "timeout": {"type": "integer", "default": 30000}
    },
    "required": ["url"]
  },
  "output_schema": {
    "type": "object",
    "properties": {
      "success": {"type": "boolean"},
      "title": {"type": "string"},
      "error": {"type": "string"}
    }
  },
  "execution_mode": "async",
  "timeout_ms": 30000
}
```

### 2. 契约验证器

严格的输入输出类型检查：

```c
#include <validator.h>

// 验证输入参数
tool_validation_result_t result = tool_validate_input(
    tool_id,
    input_json,
    &error_message
);

if (result != TOOL_VALIDATION_SUCCESS) {
    LOG_ERROR("Validation failed: %s", error_message);
    return ERROR_INVALID_INPUT;
}

// 验证输出结果
result = tool_validate_output(
    tool_id,
    output_json,
    &error_message
);
```

### 3. 工具执行器

支持多种执行模式：

#### 同步执行
```c
tool_result_t* result = tool_execute_sync(
    "tool-browser-navigate",
    "{\"url\": \"https://example.com\"}",
    30000,  // 超时 30 秒
    &error
);
```

#### 异步执行
```c
tool_task_t* task = tool_execute_async(
    "tool-browser-navigate",
    input_json,
    &error
);

// 稍后查询结果
tool_result_t* result = tool_get_result(task->id);
```

#### 沙箱执行
```c
tool_config_t manager = {
    .execution_mode = TOOL_EXEC_SANDBOX,
    .sandbox_timeout = 60000,
    .memory_limit_mb = 256
};

tool_result_t* result = tool_execute_sandbox(
    "tool-code-runner",
    input_json,
    &manager
);
```

### 4. 结果缓存

LRU 缓存加速重复调用：

```yaml
# 缓存配置
cache:
  enabled: true
  max_size_mb: 512
  ttl_sec: 3600  # 1 小时过期
  eviction_policy: LRU
  
  # 缓存键生成规则
  key_pattern: "{tool_id}:{md5(input)}"
  
  # 缓存失效条件
  invalidate_on:
    - tool_version_change
    - config_change
```

---

## 🌐 API 接口

### RESTful API

#### 注册工具

```http
POST /api/v1/agentos/toolkit/register
Content-Type: application/json

{
  "id": "tool-custom-001",
  "name": "custom_tool",
  "description": "自定义工具",
  "input_schema": {...},
  "output_schema": {...},
  "entry_point": "/path/to/tool.py"
}

Response 201:
{
  "tool_id": "tool-custom-001",
  "status": "registered"
}
```

#### 执行工具

```http
POST /api/v1/agentos/toolkit/{tool_id}/execute
Content-Type: application/json

{
  "param1": "value1",
  "param2": 123
}

Response 200:
{
  "success": true,
  "result": {...},
  "execution_time_ms": 234,
  "cached": false
}
```

#### 查询工具列表

```http
GET /api/v1/tools
Query Parameters:
  - category: 分类过滤
  - status: 状态过滤
  - page: 页码

Response 200:
{
  "total": 25,
  "tools": [...]
}
```

#### 获取工具详情

```http
GET /api/v1/agentos/toolkit/{tool_id}

Response 200:
{
  "id": "tool-browser-navigate",
  "name": "browser_navigate",
  "stats": {
    "total_executions": 1250,
    "avg_execution_time_ms": 456,
    "success_rate": 0.98,
    "cache_hit_rate": 0.35
  }
}
```

---

## ⚙️ 配置选项

### 服务配置 (agentos/manager/tool.yaml)

```yaml
server:
  port: 8084
  grpc_port: 9084
  max_connections: 500

registry:
  storage_backend: sqlite
  database_path: /var/agentos/data/tool_registry.db
  auto_discover_paths:
    - /opt/agentos/tools
    - ~/.agentos/tools

executor:
  default_timeout_ms: 30000
  max_concurrent_executions: 100
  sandbox_enabled: true
  sandbox_type: process  # process/docker

cache:
  enabled: true
  max_size_mb: 512
  ttl_sec: 3600
  backend: memory  # memory/redis

validation:
  strict_mode: true  # 严格验证输入输出
  coerce_types: false  # 不自动转换类型

logging:
  level: info
  format: json
  output: /var/agentos/logs/tool_d.log
```

---

## 🚀 使用指南

### 启动服务

```bash
# 方式 1: 直接启动
./build/agentos/daemon/tool_d/agentos-tool-d

# 方式 2: 指定配置文件
./build/agentos/daemon/tool_d/agentos-tool-d --manager ../agentos/manager/service/tool_d/tool.yaml
```

### 注册新工具

```bash
cat > my_tool.json << EOF
{
  "id": "tool-weather-query",
  "name": "weather_query",
  "description": "查询天气信息",
  "input_schema": {
    "type": "object",
    "properties": {
      "city": {"type": "string"},
      "date": {"type": "string", "format": "date"}
    },
    "required": ["city"]
  },
  "entry_point": "/opt/agentos/toolkit/weather.py"
}
EOF

curl -X POST http://localhost:8084/api/v1/agentos/toolkit/register \
  -H "Content-Type: application/json" \
  -d @my_tool.json
```

### 执行工具

```bash
# 同步执行
curl -X POST http://localhost:8084/api/v1/agentos/toolkit/weather_query/execute \
  -H "Content-Type: application/json" \
  -d '{"city": "北京", "date": "2026-03-25"}'

# 异步执行
curl -X POST http://localhost:8084/api/v1/agentos/toolkit/weather_query/execute?async=true \
  -H "Content-Type: application/json" \
  -d '{"city": "上海"}'

# 获取异步结果
task_id=$(curl -s -X POST ... | jq -r '.task_id')
sleep 2
curl "http://localhost:8084/api/v1/tasks/${task_id}/result"
```

### 查询统计信息

```bash
# 查看工具执行统计
curl http://localhost:8084/api/v1/agentos/toolkit/weather_query/stats

# 查看所有工具
curl http://localhost:8084/api/v1/tools
```

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM):

| 指标 | 数值 | 测试条件 |
|------|------|----------|
| 注册延迟 | < 5ms | 单个工具注册 |
| 执行启动延迟 | < 1ms | 同步执行 |
| 缓存命中率 | 30%~50% | 重复调用场景 |
| 并发执行数 | 100+ | 同时执行工具 |
| 验证吞吐 | 10000+/s | 契约验证 |

---

## 🧪 测试

### 运行单元测试

```bash
cd agentos/daemon/build
ctest -R tool_d --output-on-failure

# 运行特定测试
ctest -R test_registry --verbose
ctest -R test_executor --verbose
ctest -R test_validator --verbose
```

### Python SDK 测试

```python
import requests

# 测试工具注册
response = requests.post(
    'http://localhost:8084/api/v1/agentos/toolkit/register',
    json={
        'id': 'test-tool',
        'name': '测试工具',
        'input_schema': {'type': 'object'}
    }
)
assert response.status_code == 201

# 测试工具执行
response = requests.post(
    'http://localhost:8084/api/v1/agentos/toolkit/test-tool/execute',
    json={'param': 'value'}
)
assert response.status_code == 200
assert response.json()['success']
```

---

## 🔗 相关文档

- [服务层总览](../README.md) - daemon 架构说明
- [技能开发指南](../../paper/guides/folder/create_skill.md) - 如何开发工具技能
- [契约验证规范](../../paper/specifications/contract_validation.md) - 输入输出验证标准
- [部署指南](../../paper/guides/folder/deployment.md) - 生产环境部署

---

<div align="center">

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges 始于数据，终于智能。"*

</div>

