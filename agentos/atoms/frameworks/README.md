# Frameworks 外部 AI 框架适配器桥接层

`agentos/atoms/frameworks/`

**版本**: v3.0

---

## 概述

Frameworks 层是 Airymax 作为**行业底座**连接外部 AI 框架的桥梁。通过标准适配器接口，Airymax 可兼容驱动行业内所有其他 AI 框架（如 LangChain、AutoGPT、CrewAI、Semantic Kernel、LlamaIndex、Microsoft AutoGen 等），使它们接入 Airymax 的基础设施：

- **MicroCoreRT** 微核心 — IPC/内存/调度/时间
- **CoreLoopThree** 三层认知循环 — 认知/执行/记忆
- **MemoryRovol** 记忆卷载 — L1-L4 全功能记忆
- **Cupolas** 安全穹顶 — 统一安全服务
- **TaskFlow** 任务流 — Pregel 超步模型 DAG 编排

### 设计原则（微内核思想）

| 原则 | 说明 |
|------|------|
| **薄层内核** | atoms 层仅提供适配器注册表和生命周期管理，不实现业务逻辑 |
| **回调驱动** | 实际框架逻辑由外部适配器通过函数指针回调实现 |
| **线程安全** | 适配器注册/注销/查找/调用全部线程安全（pthread 互斥锁） |
| **不透明句柄** | 不持有外部框架内部状态，仅持有不透明 `void *handle` |
| **零拷贝路由** | 请求/响应通过栈上结构体传递，注册表操作在锁内、回调在锁外 |

---

## 目录结构

```
frameworks/
├── CMakeLists.txt              # 构建配置
├── README.md                   # 本文件
├── include/
│   └── agentos_frameworks.h    # 公共头文件（适配器接口 + 注册表 API）
└── src/
    └── agentos_frameworks.c    # 实现（线程安全注册表 + 生命周期管理）
```

---

## 核心接口

### 适配器接口

外部 AI 框架通过实现 `agentos_framework_adapter_t` 接入 Airymax：

```c
typedef struct agentos_framework_adapter_s {
    /* 元信息 */
    const char *name;          /* 框架名（如 "langchain"） */
    const char *version;       /* 适配器版本 */
    uint32_t capabilities;     /* 能力位掩码 */

    /* 生命周期回调（由外部框架实现） */
    int32_t (*init)(const agentos_fw_config_t *config, void **out_handle);
    int32_t (*start)(void *handle);
    int32_t (*stop)(void *handle);
    void    (*destroy)(void *handle);
    int32_t (*health_check)(void *handle);
    int32_t (*process_request)(void *handle,
                                const agentos_fw_request_t *request,
                                agentos_fw_response_t *response);
} agentos_framework_adapter_t;
```

### 能力位掩码

| 能力 | 含义 |
|------|------|
| `AGENTOS_FW_CAP_COGNITION` | 认知/推理能力 |
| `AGENTOS_FW_CAP_EXECUTION` | 代码/工具执行 |
| `AGENTOS_FW_CAP_MEMORY` | 记忆存储/检索 |
| `AGENTOS_FW_CAP_PLANNING` | 任务规划 |
| `AGENTOS_FW_CAP_PROTOCOL` | 协议支持（MCP/A2A/OpenAI） |
| `AGENTOS_FW_CAP_SAFETY` | 安全检查 |
| `AGENTOS_FW_CAP_STREAMING` | 流式响应 |
| `AGENTOS_FW_CAP_MULTI_AGENT` | 多智能体协作 |

### 注册表 API

```c
int32_t agentos_fw_registry_init(void);                                    /* 初始化注册表 */
int32_t agentos_fw_registry_shutdown(void);                                /* 关闭注册表 */
int32_t agentos_fw_register_adapter(const agentos_framework_adapter_t *);  /* 注册适配器 */
int32_t agentos_fw_unregister_adapter(const char *name);                   /* 注销适配器 */
const agentos_framework_adapter_t *agentos_fw_find_adapter(const char *);  /* 查找适配器 */
int32_t agentos_fw_list_adapters(char [][64], uint32_t, uint32_t *);       /* 列出适配器 */
```

### 实例生命周期 API

```c
int32_t agentos_fw_create_instance(const char *adapter, const agentos_fw_config_t *, char *out_name);
int32_t agentos_fw_start_instance(const char *instance_name);
int32_t agentos_fw_stop_instance(const char *instance_name);
int32_t agentos_fw_destroy_instance(const char *instance_name);
int32_t agentos_fw_health_check(const char *instance_name);
int32_t agentos_fw_get_info(const char *instance_name, agentos_fw_info_t *);
int32_t agentos_fw_process_request(const char *instance, const agentos_fw_request_t *, agentos_fw_response_t *);
```

---

## 使用示例

### 1. 实现适配器（外部框架侧）

```c
/* 例：LangChain 适配器（由 LangChain 桥接库实现） */
static int32_t langchain_init(const agentos_fw_config_t *cfg, void **out_handle) {
    /* 初始化 LangChain 运行时，返回句柄 */
    void *handle = langchain_runtime_create(cfg->config_json);
    if (!handle) return AGENTOS_FW_ERROR;
    *out_handle = handle;
    return AGENTOS_FW_OK;
}
/* ... start/stop/destroy/health_check/process_request ... */

static const agentos_framework_adapter_t langchain_adapter = {
    .name = "langchain",
    .version = "0.1.0",
    .capabilities = AGENTOS_FW_CAP_COGNITION | AGENTOS_FW_CAP_EXECUTION | AGENTOS_FW_CAP_MEMORY,
    .init = langchain_init,
    .start = langchain_start,
    .stop = langchain_stop,
    .destroy = langchain_destroy,
    .health_check = langchain_health_check,
    .process_request = langchain_process_request,
};
```

### 2. 注册并使用（Airymax 侧）

```c
agentos_fw_registry_init();
agentos_fw_register_adapter(&langchain_adapter);

char instance_name[64];
agentos_fw_config_t cfg = { .timeout_ms = 30000, .max_retries = 3 };
agentos_fw_create_instance("langchain", &cfg, instance_name);
agentos_fw_start_instance(instance_name);

agentos_fw_request_t req = { .method = "invoke_chain", .payload = "{...}" };
agentos_fw_response_t resp;
agentos_fw_process_request(instance_name, &req, &resp);

agentos_fw_stop_instance(instance_name);
agentos_fw_destroy_instance(instance_name);
agentos_fw_registry_shutdown();
```

---

## 架构定位

```
┌──────────────────────────────────────────────────────────┐
│           外部 AI 框架（LangChain/AutoGen/...）            │
│                    通过适配器接入                          │
├──────────────────────────────────────────────────────────┤
│                  Frameworks 适配器层                       │
│            （注册表 + 生命周期管理 + 请求路由）             │
├──────────────────────────────────────────────────────────┤
│     MicroCoreRT  │  CoreLoopThree  │  MemoryRovol  │ ... │
│                    Airymax 基础设施                        │
└──────────────────────────────────────────────────────────┘
```

Frameworks 层位于 atoms 内核层顶部，是外部框架进入 Airymax 的唯一入口。所有外部框架通过实现适配器接口，获得 Airymax 内核服务的全部能力。

---

## 线程安全

- 注册表操作（register/unregister/find/list）由 `pthread_mutex_t` 保护
- 实例生命周期操作（create/start/stop/destroy）在锁内完成状态转换
- `process_request` 的适配器回调在**锁外**执行（避免长耗时阻塞注册表）
- 回调执行后重新加锁更新统计计数（重新查找实例以处理回调期间可能的销毁）

---

## 错误码

| 码 | 含义 |
|----|------|
| `AGENTOS_FW_OK` (0) | 成功 |
| `AGENTOS_FW_ERROR` (-1) | 通用错误 |
| `AGENTOS_FW_NOT_INIT` (-2) | 注册表未初始化 |
| `AGENTOS_FW_INVALID_ARG` (-3) | 参数无效 |
| `AGENTOS_FW_TIMEOUT` (-4) | 超时 |
| `AGENTOS_FW_BUSY` (-5) | 资源忙碌（有活跃实例） |
| `AGENTOS_FW_NOT_FOUND` (-6) | 适配器/实例未找到 |
| `AGENTOS_FW_DENIED` (-7) | 权限拒绝 |
| `AGENTOS_FW_ALREADY_EXISTS` (-8) | 适配器已存在 |
| `AGENTOS_FW_CAPACITY_FULL` (-9) | 注册表已满（最大 32） |

---

## 构建说明

Frameworks 模块随 atoms 层自动构建：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物为静态库 `agentos_frameworks`，链接 `agentos_atoms` 和 `Threads::Threads`。

---

© 2026 SPHARX Ltd. All Rights Reserved.
