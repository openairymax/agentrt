# AgentOS 数据分区 (Partdata)

**版本**: v1.0.0.6
**最后更新**: 2026-03-23

---

## 📋 概述

`partdata/` 是 AgentOS 的数据分区，存储所有运行时产生的数据，包括日志、注册表、追踪数据等。该目录设计为可持久化存储，支持数据备份和恢复。

本模块提供**生产级 C 语言库**实现，支持以下功能：
- 目录结构自动初始化
- 分层日志系统（内核/服务/应用）
- SQLite 注册表（Agent/技能/会话）
- OpenTelemetry 追踪数据存储
- IPC 和内存管理数据记录
- 自动清理与数据生命周期管理

---

## 🔧 快速开始

### 构建

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
```

### 运行测试

```bash
./tests/partdata_tests
```

### 基本使用

```c
#include <agentos/partdata.h>
#include <agentos/partdata_log.h>

int main(void) {
    // 初始化数据分区
    partdata_config_t config = {
        .root_path = "partdata",
        .max_log_size_mb = 100,
        .log_retention_days = 7,
        .enable_auto_cleanup = true
    };

    partdata_error_t err = partdata_init(&config);
    if (err != PARTDATA_SUCCESS) {
        fprintf(stderr, "初始化失败: %s\n", partdata_strerror(err));
        return 1;
    }

    // 写入日志
    PARTDATA_LOG_INFO("my_service", "trace_123", "服务启动成功");

    // 关闭
    partdata_shutdown();
    return 0;
}
```

---

## 📁 目录结构

```
partdata/
├── README.md                    # 本文件
│
├── kernel/                      # 内核数据
│   ├── ipc/                     # IPC 通信数据
│   │   ├── channels/           # Binder 通道数据
│   │   └── buffers/            # 共享内存缓冲区
│   └── memory/                  # 内存管理数据
│       ├── pools/              # 内存池状态
From data intelligence emerges. by spharx
│       ├── allocations/        # 分配记录
│       └── stats/              # 统计信息
│
├── logs/                        # 日志文件 ⭐ 重要
│   ├── apps/                   # 应用层日志
│   │   └── *.log               # 各应用独立日志
│   ├── kernel/                 # 内核层日志
│   │   └── agentos.log         # 主内核日志
│   └── services/               # 服务层日志
│       ├── llm_d.log           # LLM 服务日志
│       ├── tool_d.log          # 工具服务日志
│       ├── market_d.log        # 市场服务日志
│       ├── sched_d.log         # 调度服务日志
│       └── monit_d.log         # 监控服务日志
│
├── registry/                    # 全局注册表 ⭐ 核心
│   ├── agents.db               # Agent 注册表（SQLite）
│   ├── skills.db               # 技能注册表（SQLite）
│   └── sessions.db             # 会话注册表（SQLite）
│
├── services/                    # 服务数据
│   ├── llm_d/                  # LLM 服务数据
│   │   ├── cache/              # 响应缓存
│   │   └── stats/              # 统计数据
│   ├── market_d/               # 市场服务数据
│   │   ├── installed/          # 已安装项目
│   │   └── metadata/           # 元数据
│   └── tool_d/                 # 工具服务数据
│       └── executions/         # 执行记录
│
└── traces/                      # OpenTelemetry 追踪数据
    └── spans/                  # Span 数据存储
        └── *.json              # Span 记录文件
```

---

## 💾 数据存储策略

### 1. 日志文件 (logs/)

#### 分层存储

| 层级 | 路径 | 说明 |
|------|------|------|
| **应用层** | `logs/apps/*.log` | 各应用独立日志文件 |
| **服务层** | `logs/services/*.log` | 各服务守护进程日志 |
| **内核层** | `logs/kernel/agentos.log` | 内核统一日志 |

#### 日志轮转

```yaml
# config/logging/config.yaml
logging:
  rotation:
    max_size_mb: 100
    backup_count: 7
    compress: true
    format: "%Y-%m-%d_%H-%M-%S"
```

#### 示例

```bash
# 查看最新日志
tail -f partdata/logs/services/llm_d.log

# 搜索错误
grep "ERROR" partdata/logs/services/*.log

# 清理旧日志
find partdata/logs -name "*.log.*" -mtime +7 -delete
```

### 2. 注册表数据库 (registry/)

#### SQLite 数据库结构

**agents.db**:
```sql
-- Agent 注册表
CREATE TABLE agents (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    type TEXT,
    version TEXT,
    status TEXT,
    config_path TEXT,
    created_at TIMESTAMP,
    updated_at TIMESTAMP
);

-- Agent 能力
CREATE TABLE agent_capabilities (
    agent_id TEXT,
    capability TEXT,
    FOREIGN KEY (agent_id) REFERENCES agents(id)
);
```

**skills.db**:
```sql
-- 技能注册表
CREATE TABLE skills (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    version TEXT,
    library_path TEXT,
    manifest_path TEXT,
    installed_at TIMESTAMP
);
```

#### 查询示例

```bash
# 使用 SQLite 命令行
sqlite3 partdata/registry/agents.db

# 列出所有 Agent
SELECT id, name, type, version FROM agents WHERE status='active';

# 列出所有技能
SELECT * FROM skills ORDER BY installed_at DESC;
```

### 3. 追踪数据 (traces/spans/)

#### OpenTelemetry Span 格式

```json
{
  "trace_id": "abc123def456",
  "span_id": "span789",
  "parent_span_id": "span456",
  "name": "task.execute",
  "start_time": "2026-03-18T10:23:45.123Z",
  "end_time": "2026-03-18T10:23:46.789Z",
  "attributes": {
    "service": "tool_d",
    "task_id": "12345",
    "skill": "browser_skill"
  },
  "status": "OK"
}
```

#### 导出配置

```yaml
# config/logging/config.yaml
telemetry:
  tracing:
    enabled: true
    export_format: json
    export_path: "partdata/traces/spans/"
    batch_size: 100
    export_interval_sec: 10
```

---

## 🛠️ 数据管理工具

### 1. 日志查看器

```bash
# 实时查看所有服务日志
./scripts/view_logs.sh --follow

# 查看特定服务的日志
./scripts/view_logs.sh --service llm_d --level ERROR

# 导出日志为 JSON
./scripts/view_logs.sh --export json --output logs_export.json
```

### 2. 注册表管理

```python
# Python 脚本管理注册表
from agentos import Registry

registry = Registry("partdata/registry")

# 查询 Agent
agents = registry.query_agents(type="planning")
for agent in agents:
    print(f"{agent.id}: {agent.name}")

# 查询技能
skills = registry.query_skills(status="active")
```

### 3. 数据备份

```bash
# 备份所有数据
./scripts/backup_data.sh --output backup_$(date +%Y%m%d).tar.gz

# 恢复数据
./scripts/restore_data.sh --input backup_20260318.tar.gz
```

---

## 📊 数据清理策略

### 自动清理配置

```yaml
# config/kernel/settings.yaml
data_management:
  logs:
    retention_days: 7
    max_total_size_gb: 10
    
  traces:
    retention_days: 3
    
  registry:
    auto_vacuum: true
    vacuum_interval_days: 7
```

### 手动清理

```bash
# 清理 7 天前的日志
find partdata/logs -type f -mtime +7 -delete

# 清理追踪数据
rm -rf partdata/traces/spans/*.json

# 优化数据库
sqlite3 partdata/registry/agents.db "VACUUM;"
```

---

## 🔒 数据安全

### 1. 访问控制

```yaml
# config/security/policy.yaml
data_access:
  logs:
    read: ["admin", "developer"]
    write: ["system"]
    
  registry:
    read: ["admin", "developer", "viewer"]
    write: ["admin"]
```

### 2. 敏感数据加密

```bash
# 加密敏感配置文件
openssl enc -aes-256-cbc -salt -in secrets.yaml -out secrets.yaml.enc

# 解密
openssl enc -aes-256-cbc -d -in secrets.yaml.enc -out secrets.yaml
```

---

## 📈 监控指标

### 1. 磁盘使用

```python
import os

def get_disk_usage(path):
    total, used, free = shutil.disk_usage(path)
    return {
        'total_gb': total / (1024**3),
        'used_gb': used / (1024**3),
        'free_gb': free / (1024**3),
        'usage_percent': (used / total) * 100
    }

usage = get_disk_usage('partdata/')
print(f"Disk Usage: {usage['usage_percent']:.1f}%")
```

### 2. 日志增长速率

```bash
# 查看每日日志增长
du -sh partdata/logs/*/ | sort -h

# 预测磁盘空间耗尽时间
# (当前大小 / 日均增长) = 剩余天数
```

---

## 🤝 最佳实践

### 1. 日志规范

✅ **推荐**:
```python
logger.info(f"Task {task_id} completed successfully")
logger.error(f"Failed to connect: {error}", exc_info=True)
```

❌ **避免**:
```python
logger.debug("x=1, y=2, z=3, a=4, b=5, ...")  # 过多调试信息
logger.info(password)  # 记录敏感信息
```

### 2. 数据库优化

```sql
-- 添加索引加速查询
CREATE INDEX idx_agent_type ON agents(type);
CREATE INDEX idx_skill_status ON skills(status);

-- 定期维护
PRAGMA optimize;
ANALYZE;
```

### 3. 数据归档

```bash
# 按月归档日志
tar -czf logs_2026_03.tar.gz partdata/logs/*-03-*
mv logs_2026_03.tar.gz archive/
```

---

## 📚 相关文档

- [日志系统架构](../partdocs/architecture/logging_system.md) - 详细日志设计
- [部署指南](../partdocs/guides/deployment.md) - 生产环境配置
- [运维手册](../partdocs/guides/troubleshooting.md) - 日常维护

---

**Apache License 2.0 © 2026 SPHARX**

---

© 2026 SPHARX Ltd. 保留所有权利。
