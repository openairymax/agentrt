# Config Unified — 统一配置管理

**模块路径**: `agentrt/commons/utils/config_unified/`
**版本**: v0.1.0

## 概述

Config Unified 是 AgentRT 的统一配置管理模块，提供从配置定义、加载、校验到运行时访问的完整解决方案。采用三层分离架构（Core→Source→Service），将配置定义、来源管理和运行时访问完全解耦，支持热重载、Schema 校验、配置加密和版本管理。

## 设计目标

- **三层分离**：将配置定义（Core）、来源管理（Source）和运行时访问（Service）解耦
- **热重载**：配置变更无需重启进程，自动通知订阅者
- **Schema 校验**：配置结构校验，确保配置正确性
- **多来源聚合**：合并文件、环境变量、远程、内存等多种来源的配置
- **变更审计**：所有配置变更记录审计日志，支持回滚
- **配置加密**：支持 AES-256-GCM / ChaCha20-Poly1305 加密存储
- **版本管理**：配置快照、差异比较和版本回滚

## 目录结构

```
config_unified/
├── include/
│   ├── config_unified.h         # 统一头文件（包含所有子模块）
│   ├── core_config.h            # Core 层：配置数据模型与基础接口
│   ├── config_source.h          # Source 层：配置源适配接口
│   ├── config_service.h         # Service 层：高级功能接口
│   └── config_compat.h          # 兼容层：向后兼容旧 API
├── src/
│   ├── core_config.c            # Core 层实现
│   ├── config_source.c          # Source 层实现
│   ├── config_service.c         # Service 层实现
│   └── config_compat.c          # 兼容层实现
├── test/
│   ├── test_config_unified.c    # 统一配置测试
│   └── test_config_source_manager.c  # 源管理器测试
└── README.md                    # 本文档
```

## 架构

```
+-------------------------------------------------------------------+
|  Service 层（验证/热更新/加密/版本管理/模板）                        |
|  config_service_create / config_schema_validate / hot_reload       |
+-------------------------------------------------------------------+
|  Source 层（配置来源管理）                                          |
|  文件源  |  环境变量源  |  命令行源  |  内存源  |  远程源  |  默认值源 |
+-------------------------------------------------------------------+
|  Core 层（配置定义与数据模型）                                       |
|  config_context  |  config_value  |  类型系统  |  错误码            |
+-------------------------------------------------------------------+
```

### Core 层

配置核心层提供统一的配置数据模型和基础接口：

- **配置值类型**：`CONFIG_TYPE_NULL`、`BOOL`、`INT`、`INT64`、`DOUBLE`、`STRING`、`ARRAY`、`OBJECT`、`BINARY`
- **配置上下文**：`config_context_t`，支持点分键路径（如 `database.host`）
- **线程安全**：所有基础操作均为线程安全
- **内存所有权**：`config_context_set` 转移值所有权，`config_context_get` 返回内部引用

### Source 层

配置来源管理层提供统一的源适配接口：

| 来源类型 | 枚举值 | 优先级 | 说明 |
|----------|--------|--------|------|
| `CONFIG_SOURCE_DEFAULT` | 6 | 最低 | Schema 中声明的默认值 |
| `CONFIG_SOURCE_FILE` | 0 | 中 | JSON / YAML / TOML / INI 格式 |
| `CONFIG_SOURCE_ENV` | 1 | 高 | `AGENTRT_` 前缀的环境变量 |
| `CONFIG_SOURCE_ARGS` | 2 | 高 | 命令行参数 |
| `CONFIG_SOURCE_MEMORY` | 3 | 最高 | 运行时通过 API 设置 |
| `CONFIG_SOURCE_NETWORK` | 4 | 高 | 远程配置中心 |
| `CONFIG_SOURCE_DATABASE` | 5 | 高 | 数据库配置源 |

源管理器（`config_source_manager_t`）支持按优先级合并、变化监控和防抖通知。

### Service 层

服务层提供高级功能：

- **配置验证**：范围验证、正则验证、枚举验证、自定义验证
- **Schema 定义**：`config_schema_t`，支持必需项检查、默认值填充
- **热更新**：`config_hot_reload_manager_t`，支持回调注册和定时检查
- **配置加密**：AES-256-GCM / ChaCha20-Poly1305
- **版本管理**：快照创建、版本回滚、差异比较
- **模板展开**：变量替换（`${VAR}`）

## 接口说明

### Core 层 API

| 函数 | 说明 |
|------|------|
| `config_context_create(name)` | 创建配置上下文 |
| `config_context_destroy(ctx)` | 销毁配置上下文 |
| `config_context_set(ctx, key, value)` | 设置配置值（所有权转移） |
| `config_context_get(ctx, key)` | 获取配置值（内部引用） |
| `config_context_delete(ctx, key)` | 删除配置项 |
| `config_context_has(ctx, key)` | 检查配置项是否存在 |
| `config_context_lock / unlock` | 锁定/解锁配置（防止修改） |
| `config_value_create_string / int / bool / double` | 创建配置值 |
| `config_value_destroy(value)` | 销毁配置值 |
| `config_value_get_type(value)` | 获取配置值类型 |

### 便捷宏

| 宏 | 说明 |
|------|------|
| `CONFIG_STRING(val)` | 创建字符串配置值 |
| `CONFIG_INT(val)` | 创建整数配置值 |
| `CONFIG_SET_SAFE(ctx, key, val)` | 安全设置（自动销毁旧值） |
| `CONFIG_GET_STRING_SAFE(ctx, key, def)` | 安全获取字符串 |
| `CONFIG_PATH(base, key)` | 构建配置路径 |
| `CONFIG_SUCCESS(err)` | 检查操作是否成功 |
| `CONFIG_RETURN_IF_FAILED(err)` | 失败则提前返回 |

### Source 层 API

| 函数 | 说明 |
|------|------|
| `config_source_create_file(opts)` | 创建文件配置源 |
| `config_source_create_env(opts)` | 创建环境变量配置源 |
| `config_source_create_args(opts)` | 创建命令行配置源 |
| `config_source_create_memory(opts)` | 创建内存配置源 |
| `config_source_create_remote(url, token, ns, interval)` | 创建远程配置源 |
| `config_source_manager_create()` | 创建源管理器 |
| `config_source_manager_load_all(mgr, ctx, strategy)` | 从所有源加载配置 |
| `config_source_manager_poll_changes(mgr)` | 轮询检查变化（带防抖） |

### Service 层 API

| 函数 | 说明 |
|------|------|
| `config_service_create(name, schema, hot_reload, encryption)` | 创建完整配置服务 |
| `config_schema_validate(schema, ctx, strict)` | Schema 校验 |
| `config_hot_reload_manager_create(ctx, mgr)` | 创建热更新管理器 |
| `config_hot_reload_start(mgr, interval_ms)` | 开始监控配置变化 |
| `config_version_create_snapshot(mgr, author, desc)` | 创建配置快照 |
| `config_version_rollback(mgr, version)` | 回滚到指定版本 |
| `config_encrypt_value / decrypt_value` | 加密/解密配置值 |

## 使用示例

```c
#include "config_unified.h"

config_context_t *ctx = config_service_create("agentos", NULL, true, false);
config_file_source_options_t file_opts = {
    .file_path = "config.yaml", .format = "yaml"
};
config_source_t *source = config_source_create_file(&file_opts);
config_service_load(ctx, &source, 1);

const char *host = CONFIG_GET_STRING_SAFE(ctx, "server.host", "0.0.0.0");
int port = CONFIG_GET_INT_SAFE(ctx, "server.port", 8080);

config_hot_reload_manager_t *hrm = config_hot_reload_manager_create(ctx, NULL);
config_hot_reload_register_callback(hrm, "logging.level",
    [](config_context_t *c, const char *k,
       const config_value_t *ov, const config_value_t *nv, void *ud) {
        printf("Config changed: %s\n", k);
    }, NULL);
config_hot_reload_start(hrm, 30000);

config_service_save(ctx, NULL);
config_hot_reload_manager_destroy(hrm);
config_context_destroy(ctx);
```

## 性能特性

| 操作 | 性能 |
|------|------|
| 读取缓存命中 | < 100ns |
| 读取缓存未命中 | < 1μs |
| 配置加载（1000 键） | < 5ms |
| 热重载检测 | < 10ms |
| Schema 校验 | < 500μs |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `atomic_compat.h` | 原子操作（热更新状态管理） |
| `memory_compat.h` | 统一内存管理 |
| libyaml | YAML 格式支持（可选） |
| cJSON | JSON 格式支持（可选） |

---

© 2026 SPHARX Ltd. All Rights Reserved.
