# Market Daemon — 应用市场守护进程

> **模块路径**: `agentos/daemon/market_d/` | **版本**: v0.0.5

## 概述

`daemon/market_d/` 是 AgentOS 的应用市场守护进程，负责 Agent、Skill、Tool、Template 等资源的全生命周期管理，包括注册、发现、搜索、安装、卸载、版本控制和更新检查。它支持本地和远程注册中心，提供按名称、能力、标签等多维度搜索，以及评分评级和使用统计功能。

### 核心职责

- **资源管理**：管理 Agent、Skill、Tool、Template 四类资源的全生命周期
- **搜索发现**：按名称、能力、标签、类型等维度搜索可用资源
- **版本管理**：跟踪资源版本、更新日志和依赖关系
- **安装管理**：处理资源的下载、安装和环境配置
- **评分评级**：用户评分、使用统计和质量评级
- **远程同步**：支持与远程注册中心同步资源信息
- **安全验证**：通过 daemon_security 验证包签名

## 目录结构

```
market_d/
├── CMakeLists.txt                    # 构建配置
├── README.md                         # 本文件
├── include/                          # 公共头文件
│   ├── market_service.h              # 市场服务接口定义
│   ├── market_svc_adapter.h          # 市场服务适配器接口
│   └── agent_registry_core.h         # Agent 注册核心接口
├── src/                              # 实现文件
│   ├── main.c                        # 守护进程入口
│   ├── market_service_impl.c         # 市场服务核心实现
│   ├── market_svc_adapter.c          # 请求解析与标准化适配器
│   ├── agent_registry.c              # Agent 注册管理
│   ├── agent_registry_core.c         # Agent 注册核心逻辑
│   ├── skill_registry.c              # Skill 注册管理
│   ├── publisher.c                   # 资源发布管理
│   └── installer.c                   # 资源安装管理
└── tests/                            # 单元测试
    ├── CMakeLists.txt
    ├── test_market.c                 # 集成测试
    ├── test_agent_registry.c         # Agent 注册测试
    ├── test_agent_registry_core.c    # Agent 注册核心测试
    ├── test_skill_registry.c         # Skill 注册测试
    ├── test_installer.c              # 安装器测试
    └── agents/                       # 测试用 Agent 数据
        └── install_test_agent/
            └── agent.json
```

## 核心组件说明

### 市场资源类型

| 类型 | 枚举值 | 说明 |
|------|--------|------|
| Agent | `AGENT_TYPE_ASSISTANT/EXPERT/SPECIALIZED/CUSTOM` | 可部署的智能体应用 |
| Skill | `SKILL_TYPE_TOOL/KNOWLEDGE/INTEGRATION/CUSTOM` | 可复用的能力模块 |
| Tool | — | 外部工具集成 |
| Template | — | 项目模板 |

### Agent 注册核心（agent_registry_core）

Agent 注册的核心逻辑，管理 Agent 的元数据、状态和实例信息：

- Agent 类型分类：助手型、专家型、专业型、自定义
- Agent 状态管理：可用、安装中、错误、禁用
- 支持依赖关系声明和检查

### Skill 注册（skill_registry）

Skill 注册管理，管理可复用的能力模块：

- Skill 类型分类：工具型、知识型、集成型、自定义
- 支持版本管理和依赖追踪

### 发布管理（publisher）

资源发布管理，处理资源从本地到注册中心的发布流程：

- 资源元数据验证
- 依赖关系检查
- 版本号管理

### 安装管理（installer）

资源安装管理，处理资源的下载、安装和环境配置：

- 支持指定版本安装
- 支持强制更新
- 支持自定义安装路径
- 包签名验证（通过 daemon_security）

## 接口说明

### 市场服务生命周期（market_service.h）

```c
int market_service_create(const market_config_t *manager, market_service_t **service);
int market_service_destroy(market_service_t *service);
int market_service_reload_config(market_service_t *service, const market_config_t *manager);
int market_service_sync_registry(market_service_t *service);
```

### 资源注册接口

```c
int market_service_register_agent(market_service_t *service, const agent_info_t *agent_info);
int market_service_register_skill(market_service_t *service, const skill_info_t *skill_info);
```

### 搜索发现接口

```c
int market_service_search_agents(market_service_t *service, const search_params_t *params,
                                 agent_info_t ***agents, size_t *count);
int market_service_search_skills(market_service_t *service, const search_params_t *params,
                                 skill_info_t ***skills, size_t *count);
```

### 安装管理接口

```c
int market_service_install_agent(market_service_t *service, const install_request_t *request,
                                 install_result_t **result);
int market_service_install_skill(market_service_t *service, const install_request_t *request,
                                 install_result_t **result);
int market_service_uninstall_agent(market_service_t *service, const char *agent_id);
int market_service_uninstall_skill(market_service_t *service, const char *skill_id);
```

### 查询接口

```c
int market_service_get_installed_agents(market_service_t *service, agent_info_t ***agents,
                                        size_t *count);
int market_service_get_installed_skills(market_service_t *service, skill_info_t ***skills,
                                        size_t *count);
int market_service_check_update(market_service_t *service, const char *id, bool *has_update,
                                char **latest_version);
```

### 核心数据结构

```c
typedef struct {
    char *registry_url;
    char *storage_path;
    uint32_t sync_interval_ms;
    uint32_t cache_ttl_ms;
    bool enable_remote_registry;
    bool enable_auto_update;
} market_config_t;

typedef struct {
    char *query;
    agent_type_t agent_type;
    skill_type_t skill_type;
    bool only_installed;
    bool sort_by_rating;
    bool sort_by_download;
    size_t limit;
    size_t offset;
} search_params_t;

typedef struct {
    char *id;
    char *version;
    bool force_update;
    char *install_path;
} install_request_t;

typedef struct {
    bool success;
    char *message;
    char *installed_version;
    char *install_path;
    int error_code;
} install_result_t;
```

### JSON-RPC 2.0 方法

| 方法 | 说明 |
|------|------|
| `market.publish` | 发布资源到市场 |
| `market.search` | 搜索市场资源 |
| `market.info` | 查询资源详情 |
| `market.install` | 安装市场资源 |
| `market.uninstall` | 卸载已安装资源 |
| `market.update` | 更新已安装资源 |
| `market.versions` | 查询资源版本历史 |

## 通信方式

| 方向 | 协议 | 说明 |
|------|------|------|
| 入站 | JSON-RPC 2.0 | 通过 IPC Service Bus 接收请求 |
| 出站 | HTTPS | 与远程注册中心同步（可选） |

## 依赖关系

```
market_d
├── common (svc_common, svc_logger, svc_config, svc_cache, ipc_service_bus,
│           method_dispatcher, jsonrpc_helpers, daemon_security)
├── cupolas (daemon_security)  # 包签名验证
└── 远程注册中心（可选）        # 资源同步
```

## 构建说明

```bash
# 构建市场守护进程
cmake -B build -DBUILD_TESTS=ON
cmake --build build --target agentos-market-d

# 运行市场测试
ctest --test-dir build -R "test_market|test_agent_registry|test_skill_registry|test_installer" -V
```

## 使用示例

### 启动市场守护进程

```bash
# 启动市场守护进程
./market_d --config market_config.json

# 指定市场数据目录
./market_d --data-dir /opt/agentos/market

# 启用自动同步
./market_d --sync-interval 3600
```

### 代码调用示例

```c
#include "daemon/market_d/include/market_service.h"

market_config_t config = {
    .storage_path = "/opt/agentos/market",
    .sync_interval_ms = 3600000,
    .cache_ttl_ms = 300000,
    .enable_remote_registry = true,
    .enable_auto_update = false
};

market_service_t *svc = NULL;
market_service_create(&config, &svc);

search_params_t params = {
    .query = "code review",
    .agent_type = AGENT_TYPE_EXPERT,
    .only_installed = false,
    .sort_by_rating = true,
    .limit = 10
};

agent_info_t **agents = NULL;
size_t count = 0;
market_service_search_agents(svc, &params, &agents, &count);

install_request_t req = {
    .id = "code-review-agent",
    .version = "",
    .force_update = false
};

install_result_t *result = NULL;
market_service_install_agent(svc, &req, &result);

market_service_destroy(svc);
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
