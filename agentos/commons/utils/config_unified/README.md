# Config Unified — 统一配置管理

`commons/utils/config_unified/` 是 AgentOS 的统一配置管理模块，提供从配置定义、加载、校验到运行时访问的完整解决方案。

## 设计目标

- **三层分离**：将配置定义、来源管理和运行时访问解耦
- **热重载**：配置变更无需重启进程，自动通知订阅者
- **Schema 校验**：配置结构校验，确保配置正确性
- **多来源聚合**：合并文件、环境变量、远程、内存等多种来源的配置
- **变更审计**：所有配置变更记录审计日志，支持回滚

## 架构

```
+-------------------------------------------------------------------+
|  Service 层（运行时访问接口）                                        |
|  config_unified_get/set/watch/reload                               |
+-------------------------------------------------------------------+
|  Source 层（配置来源管理）                                           |
|  文件源  |  环境变量源  |  远程源  |  内存源  |  默认值源            |
+-------------------------------------------------------------------+
|  Core 层（配置定义与校验）                                           |
|  Schema 定义  |  类型系统  |  校验引擎  |  默认值管理               |
+-------------------------------------------------------------------+
```

### Core 层

配置核心层负责 Schema 定义、类型系统和校验：

- **Schema 定义**：使用 JSON Schema / 结构体注解定义配置结构
- **类型系统**：支持基本类型（int/float/string/bool）和复合类型（array/object）
- **校验引擎**：递归校验配置值，报告详细的校验错误
- **默认值管理**：Schema 中声明默认值，未配置时自动填充

### Source 层

配置来源管理层负责从不同来源加载配置并按优先级合并：

| 来源 | 优先级 | 说明 |
|------|--------|------|
| 默认值 | 最低 | Schema 中声明的默认值 |
| 配置文件 | 中 | JSON / YAML / TOML 格式 |
| 环境变量 | 高 | `AGENTOS_` 前缀的环境变量 |
| 远程源 | 高 | 远程配置中心（etcd / Consul） |
| 内存源 | 最高 | 运行时通过 API 设置的配置 |

来源优先级从低到高，高优先级覆盖低优先级。

### Service 层

运行时访问接口层提供：

- **配置读取**：类型安全的配置读取 API
- **配置写入**：运行时配置更新，触发审计日志
- **变更监听**：Watch 机制，配置变更时通知订阅者
- **热重载**：自动检测配置文件变化并重新加载

## 使用示例

```python
from config_unified import ConfigUnified, ConfigSource

# 1. 定义配置结构
schema = {
    "type": "object",
    "properties": {
        "server": {
            "type": "object",
            "properties": {
                "host": {"type": "string", "default": "0.0.0.0"},
                "port": {"type": "integer", "default": 8080},
                "workers": {"type": "integer", "default": 4, "minimum": 1}
            }
        },
        "logging": {
            "type": "object",
            "properties": {
                "level": {"type": "string", "default": "info", "enum": ["debug", "info", "warn", "error"]},
                "file": {"type": "string", "default": "/var/log/agentos.log"}
            }
        }
    }
}

# 2. 创建配置管理器
config = ConfigUnified(schema)

# 3. 添加配置来源
config.add_source(ConfigSource.FILE, "config.yaml")
config.add_source(ConfigSource.ENV, prefix="AGENTOS_")

# 4. 加载并校验配置
config.load()

# 5. 运行时访问
host = config.get("server.host", str)      # "0.0.0.0"
port = config.get("server.port", int)      # 8080
level = config.get("logging.level", str)   # "info"

# 6. 监听配置变更
def on_config_change(key, old_value, new_value):
    print(f"Config changed: {key}: {old_value} -> {new_value}")

config.watch("logging.level", on_config_change)

# 7. 热重载
config.hot_reload(interval=30)  # 每30秒检查一次
```

## 热重载机制

```python
# 自动检测配置变化
config.enable_hot_reload(interval=30, callback=reload_handler)

# 手动触发重载
config.reload()

# 暂停/恢复自动重载
config.pause_hot_reload()
config.resume_hot_reload()
```

## 配置变更审计

所有配置变更操作自动记录审计日志：

```python
# 查看配置变更历史
history = config.get_audit_log()
for entry in history:
    print(f"[{entry.timestamp}] {entry.user}: {entry.key} = {entry.new_value}")

# 回滚到历史版本
config.rollback("2024-01-15T10:30:00Z")
```

## 迁移兼容性

提供配置迁移工具，支持将旧版本配置格式自动转换为当前版本：

```python
# 版本化配置迁移
config.register_migration("1.0", "2.0", migration_v1_to_v2)
config.register_migration("2.0", "3.0", migration_v2_to_v3)
config.auto_migrate()
```

## 性能特性

| 操作 | 性能 |
|------|------|
| 读取缓存命中 | < 100ns |
| 读取缓存未命中 | < 1μs |
| 配置加载（1000 键） | < 5ms |
| 热重载检测 | < 10ms |
| Schema 校验 | < 500μs |

---

*AgentOS Commons Utils — Config Unified*
