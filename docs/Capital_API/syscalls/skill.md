Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Skill 管理 API

**版本**: Doc V1.8  
**最后更新**: 2026-04-12  
**状态**: 🟢 生产就绪

---

## 🎯 概述

Skill 管理 API 提供 AgentOS 技能（Skill）的完整生命周期管理。Skill 是 AgentOS 中可复用的能力单元，遵循**市场-安装-执行-卸载**的标准化流程。每个 Skill 通过 URL 标识来源，安装后获得唯一 Skill ID，可被 Agent 绑定调用，也可独立执行。

Skill 系统与 Agent 系统协同工作：Agent 通过 `agentos_agent_skill_bind()` 绑定 Skill 获得扩展能力，Skill 也可通过 `agentos_sys_skill_execute()` 独立执行。这种设计遵循**关注点分离**原则——Agent 负责任务编排，Skill 负责具体能力实现。

### 🧩 五维正交原则体现

Skill 管理 API 深度体现了 AgentOS 的五维正交设计原则：

| 维度 | 原则体现 | 具体实现 |
|------|----------|---------|
| **系统观** | Skill 生命周期的系统管理 | 安装→注册→执行→卸载的完整流程 |
| **内核观** | Skill 接口的标准化契约 | 统一的安装/执行/列表/卸载接口，明确的输入输出契约 |
| **认知观** | Skill 作为 Agent 认知能力的扩展 | Agent 绑定 Skill 后获得新认知能力，支持动态能力组合 |
| **工程观** | Skill 安全与可观测性 | URL 来源验证、执行隔离、安装/卸载审计追踪 |
| **设计美学** | 优雅的能力抽象 | 一致的 Skill 接口，灵活的 Agent-Skill 绑定机制 |

---

## 📋 API 索引

| 函数 | 描述 | 复杂度 |
|------|------|--------|
| `agentos_sys_skill_install()` | 安装 Skill | O(1) |
| `agentos_sys_skill_execute()` | 执行 Skill | O(n) |
| `agentos_sys_skill_list()` | 列出已安装 Skill | O(n) |
| `agentos_sys_skill_uninstall()` | 卸载 Skill | O(n) |

---

## 🔧 核心数据结构

### Skill 条目

```c
/**
 * @brief Skill 条目结构体
 *
 * 描述已安装 Skill 的完整信息。
 * @ownership 调用者拥有返回指针，需通过 agentos_sys_skill_list_free() 释放
 */
typedef struct agentos_skill_entry {
    /** Skill 唯一标识符（系统自动生成） */
    char skill_id[64];

    /** Skill 来源 URL */
    char url[512];

    /** Skill 名称（从 URL 或 manifest 解析） */
    char name[128];

    /** Skill 版本（语义化版本） */
    char version[32];

    /** Skill 描述 */
    char description[256];

    /** Skill 类型 */
    enum {
        SKILL_TYPE_TOOL = 0,        /* 工具类 Skill */
        SKILL_TYPE_EXECUTABLE = 1,  /* 可执行 Skill */
        SKILL_TYPE_WORKFLOW = 2,    /* 工作流 Skill */
        SKILL_TYPE_ADAPTER = 3,     /* 适配器 Skill */
        SKILL_TYPE_PLUGIN = 4,      /* 插件类 Skill */
        SKILL_TYPE_BRIDGE = 5,      /* 桥接类 Skill */
        SKILL_TYPE_RUNTIME = 6      /* 运行时 Skill */
    } type;

    /** 安装时间戳（Unix 毫秒） */
    uint64_t installed_at;

    /** 最后执行时间戳 */
    uint64_t last_executed_at;

    /** 累计执行次数 */
    uint64_t execution_count;

    /** 累计执行成功次数 */
    uint64_t success_count;

    /** 累计执行失败次数 */
    uint64_t failure_count;

    /** 平均执行时间（毫秒） */
    uint32_t avg_execution_time_ms;

    /** Skill 状态 */
    enum {
        SKILL_STATE_INSTALLED = 0,  /* 已安装 */
        SKILL_STATE_READY = 1,      /* 就绪 */
        SKILL_STATE_EXECUTING = 2,  /* 执行中 */
        SKILL_STATE_ERROR = 3,      /* 错误 */
        SKILL_STATE_DISABLED = 4    /* 已禁用 */
    } state;

    /** Skill 权限声明（JSON 格式） */
    char* permissions;

    /** Skill 元数据（JSON 格式） */
    char* metadata;
} agentos_skill_entry_t;
```

### Skill 安装配置

```c
/**
 * @brief Skill 安装配置结构体
 *
 * 定义 Skill 安装时的参数和选项。
 */
typedef struct agentos_skill_install_config {
    /** Skill 来源 URL（必填） */
    const char* url;

    /** 安装验证选项 */
    struct {
        /** 是否验证签名 */
        bool verify_signature;

        /** 是否校验校验和 */
        bool verify_checksum;

        /** 期望的版本约束（语义化版本范围） */
        char version_constraint[64];

        /** 是否允许降级安装 */
        bool allow_downgrade;
    } verification;

    /** 安装选项 */
    struct {
        /** 是否自动启用 */
        bool auto_enable;

        /** 安装后是否运行自检 */
        bool run_self_test;

        /** 自定义安装路径（NULL 使用默认路径） */
        char* custom_install_path;

        /** 安装超时（毫秒，0 使用默认值） */
        uint32_t timeout_ms;
    } options;

    /** 权限授予 */
    struct {
        /** 授予的权限列表（JSON 格式） */
        char* granted_permissions;

        /** 最大资源限制 */
        struct {
            uint64_t max_memory_bytes;
            uint32_t max_cpu_percent;
            uint32_t max_execution_time_sec;
        } resource_limits;
    } permission_config;
} agentos_skill_install_config_t;
```

### Skill 执行结果

```c
/**
 * @brief Skill 执行结果结构体
 *
 * 描述 Skill 执行的完整结果信息。
 * @ownership 调用者拥有返回指针，需通过 agentos_sys_skill_result_free() 释放
 */
typedef struct agentos_skill_result {
    /** 执行状态码（0=成功，负值=错误） */
    int status;

    /** 执行输出（JSON 格式） */
    char* output;

    /** 执行耗时（微秒） */
    uint64_t execution_time_us;

    /** 使用的 token 数量 */
    uint32_t tokens_used;

    /** 执行追踪 ID */
    char trace_id[128];

    /** 错误信息（失败时非 NULL） */
    char* error_message;

    /** 执行警告列表（JSON 数组，可能为 NULL） */
    char* warnings;
} agentos_skill_result_t;
```

---

## 📖 API 详细说明

### agentos_sys_skill_install

```c
/**
 * @brief 安装 Skill
 *
 * 从指定 URL 安装 Skill 到本地系统。安装成功后 Skill 进入 INSTALLED 状态，
 * 可通过返回的 skill_id 进行后续操作。
 *
 * @param skill_url   Skill 来源 URL（必填，格式：scheme://host/path）
 * @param out_skill_id [out] 返回分配的 Skill ID（调用者负责 agentos_sys_free 释放）
 *
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL  参数无效（skill_url 或 out_skill_id 为 NULL）
 * @return AGENTOS_ENOMEM  内存不足
 * @return AGENTOS_EEXIST  Skill 已安装（URL 重复）
 * @return AGENTOS_ETIMEOUT 安装超时
 *
 * @ownership 调用者拥有 *out_skill_id，需通过 agentos_sys_free() 释放
 * @threadsafe 是。内部使用互斥锁保护 Skill 列表
 * @reentrant  否。使用全局 Skill 列表
 *
 * @note Skill ID 格式为 "skill_N"，N 为原子递增计数器
 * @note 当前实现为同步安装，未来版本将支持异步安装
 *
 * @see agentos_sys_skill_uninstall()
 * @see agentos_sys_skill_list()
 */
agentos_error_t agentos_sys_skill_install(const char* skill_url, char** out_skill_id);
```

**使用示例**：

```c
char* skill_id = NULL;
agentos_error_t err = agentos_sys_skill_install(
    "https://market.agentos.dev/skills/web-search/v2.1.0",
    &skill_id
);
if (err == AGENTOS_SUCCESS) {
    printf("Skill installed: %s\n", skill_id);
    agentos_sys_free(skill_id);
} else if (err == AGENTOS_EEXIST) {
    printf("Skill already installed\n");
} else {
    printf("Install failed: %d\n", err);
}
```

---

### agentos_sys_skill_execute

```c
/**
 * @brief 执行 Skill
 *
 * 执行指定 Skill，将输入数据传递给 Skill 处理并返回输出结果。
 * Skill 必须处于 READY 状态才能执行。
 *
 * @param skill_id   已安装 Skill 的标识符（必填）
 * @param input      输入数据（JSON 格式字符串，必填）
 * @param out_output [out] 返回执行输出（调用者负责 agentos_sys_free 释放）
 *
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL  参数无效（任何参数为 NULL）
 * @return AGENTOS_ENOMEM  内存不足
 * @return AGENTOS_ENOENT  Skill 未找到
 * @return AGENTOS_EACCES  Skill 无执行权限
 * @return AGENTOS_EBUSY   Skill 正在执行中
 *
 * @ownership 调用者拥有 *out_output，需通过 agentos_sys_free() 释放
 * @threadsafe 是。内部使用互斥锁保护 Skill 列表
 * @reentrant  否。Skill 执行可能修改全局状态
 *
 * @note 当前实现为回显模式（echo），生产环境将调用 Skill 的实际执行逻辑
 * @note 执行超时由 Skill 配置决定，默认为 30 秒
 *
 * @see agentos_sys_skill_install()
 * @see agentos_agent_skill_bind()
 */
agentos_error_t agentos_sys_skill_execute(const char* skill_id,
                                          const char* input,
                                          char** out_output);
```

**使用示例**：

```c
const char* skill_id = "skill_0";
const char* input = "{\"query\": \"AgentOS architecture\", \"limit\": 5}";
char* output = NULL;

agentos_error_t err = agentos_sys_skill_execute(skill_id, input, &output);
if (err == AGENTOS_SUCCESS) {
    printf("Skill output: %s\n", output);
    agentos_sys_free(output);
} else if (err == AGENTOS_ENOENT) {
    printf("Skill not found: %s\n", skill_id);
} else {
    printf("Execution failed: %d\n", err);
}
```

**与 Agent 绑定使用**：

```c
// 1. 安装 Skill
char* skill_id = NULL;
agentos_sys_skill_install("https://market.agentos.dev/skills/code-review/v1.0.0", &skill_id);

// 2. 创建 Agent 并绑定 Skill
agentos_agent_config_t config = {0};
strncpy(config.name, "code-reviewer", sizeof(config.name));
config.type = AGENT_TYPE_TASK;

char* agent_id = NULL;
agentos_agent_create(&config, &agent_id);
agentos_agent_skill_bind(agent_id, skill_id);

// 3. 通过 Agent 调用 Skill
char* output = NULL;
agentos_agent_invoke(agent_id, "Review this code: ...", strlen("Review this code: ..."), &output);
agentos_sys_free(output);

// 4. 清理
agentos_agent_skill_unbind(agent_id, skill_id);
agentos_agent_destroy(agent_id);
agentos_sys_skill_uninstall(skill_id);
agentos_sys_free(skill_id);
agentos_sys_free(agent_id);
```

---

### agentos_sys_skill_list

```c
/**
 * @brief 列出已安装 Skill
 *
 * 返回当前系统中所有已安装 Skill 的 ID 列表。
 *
 * @param out_skills [out] 返回 Skill ID 数组（调用者负责逐个 agentos_sys_free 释放，然后释放数组本身）
 * @param out_count  [out] 返回 Skill 数量
 *
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL  参数无效（out_skills 或 out_count 为 NULL）
 * @return AGENTOS_ENOMEM  内存不足
 *
 * @ownership 调用者拥有 *out_skills 数组及其每个元素，需逐个 agentos_sys_free() 释放元素后 agentos_sys_free() 释放数组
 * @threadsafe 是。内部使用互斥锁保护 Skill 列表
 * @reentrant  否。使用全局 Skill 列表
 *
 * @note 当没有已安装 Skill 时，*out_skills 为 NULL，*out_count 为 0
 *
 * @see agentos_sys_skill_install()
 * @see agentos_sys_skill_uninstall()
 */
agentos_error_t agentos_sys_skill_list(char*** out_skills, size_t* out_count);
```

**使用示例**：

```c
char** skills = NULL;
size_t count = 0;

agentos_error_t err = agentos_sys_skill_list(&skills, &count);
if (err == AGENTOS_SUCCESS) {
    printf("Installed skills (%zu):\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  - %s\n", skills[i]);
        agentos_sys_free(skills[i]);
    }
    if (skills) agentos_sys_free(skills);
}
```

---

### agentos_sys_skill_uninstall

```c
/**
 * @brief 卸载 Skill
 *
 * 从系统中卸载指定 Skill。卸载前应确保没有 Agent 正在使用该 Skill。
 * 卸载后 Skill ID 失效，所有相关资源被释放。
 *
 * @param skill_id 要卸载的 Skill 标识符（必填）
 *
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL  参数无效（skill_id 为 NULL）
 * @return AGENTOS_ENOENT  Skill 未找到
 * @return AGENTOS_EBUSY   Skill 正在被 Agent 使用，无法卸载
 *
 * @ownership 无输出参数，不涉及所有权转移
 * @threadsafe 是。内部使用互斥锁保护 Skill 列表
 * @reentrant  否。修改全局 Skill 列表
 *
 * @warning 卸载正在被 Agent 使用的 Skill 可能导致运行时错误
 * @note 卸载操作不可逆，Skill 的所有运行时数据将被清除
 *
 * @see agentos_sys_skill_install()
 * @see agentos_agent_skill_unbind()
 */
agentos_error_t agentos_sys_skill_uninstall(const char* skill_id);
```

**使用示例**：

```c
const char* skill_id = "skill_0";

agentos_error_t err = agentos_sys_skill_uninstall(skill_id);
if (err == AGENTOS_SUCCESS) {
    printf("Skill uninstalled successfully\n");
} else if (err == AGENTOS_ENOENT) {
    printf("Skill not found: %s\n", skill_id);
} else if (err == AGENTOS_EBUSY) {
    printf("Skill is in use, cannot uninstall\n");
}
```

---

## 🔄 Skill 生命周期

### 状态机

```
                    ┌──────────────┐
                    │              │
    install()       │   INSTALLED  │
  ─────────────►    │              │
                    └──────┬───────┘
                           │ auto_enable / enable()
                           ▼
                    ┌──────────────┐
                    │              │
                    │    READY     │◄─────── enable()
                    │              │
                    └──────┬───────┘
                           │ execute()
                           ▼
                    ┌──────────────┐
                    │              │
                    │  EXECUTING   │───────► READY (完成)
                    │              │
                    └──────┬───────┘
                           │ 错误
                           ▼
                    ┌──────────────┐
                    │              │
                    │    ERROR     │───────► READY (恢复)
                    │              │
                    └──────────────┘
                           │
                           │ disable()
                           ▼
                    ┌──────────────┐
                    │              │
                    │   DISABLED   │───────► READY (enable())
                    │              │
                    └──────┬───────┘
                           │ uninstall()
                           ▼
                        [已卸载]
```

### 合法状态转换

| 当前状态 | 目标状态 | 触发条件 |
|---------|---------|---------|
| INSTALLED | READY | 自动启用或手动 enable() |
| READY | EXECUTING | execute() 调用 |
| EXECUTING | READY | 执行完成 |
| EXECUTING | ERROR | 执行失败 |
| ERROR | READY | 恢复成功 |
| READY | DISABLED | disable() 调用 |
| DISABLED | READY | enable() 调用 |
| 任意 | [已卸载] | uninstall() 调用 |

---

## 🧪 使用示例

### 完整 Skill 管理流程

```c
#include "syscalls.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    agentos_error_t err;

    // 1. 初始化系统调用层
    err = agentos_syscalls_init();
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Failed to init syscalls: %d\n", err);
        return 1;
    }

    // 2. 安装多个 Skill
    const char* skill_urls[] = {
        "https://market.agentos.dev/skills/web-search/v2.1.0",
        "https://market.agentos.dev/skills/code-review/v1.0.0",
        "https://market.agentos.dev/skills/data-analysis/v3.0.0"
    };

    char* skill_ids[3] = {NULL};
    for (int i = 0; i < 3; i++) {
        err = agentos_sys_skill_install(skill_urls[i], &skill_ids[i]);
        if (err != AGENTOS_SUCCESS) {
            fprintf(stderr, "Failed to install skill from %s: %d\n", skill_urls[i], err);
            goto cleanup;
        }
        printf("Installed: %s -> %s\n", skill_urls[i], skill_ids[i]);
    }

    // 3. 列出所有已安装 Skill
    char** skills = NULL;
    size_t count = 0;
    err = agentos_sys_skill_list(&skills, &count);
    if (err == AGENTOS_SUCCESS) {
        printf("\nInstalled skills (%zu):\n", count);
        for (size_t i = 0; i < count; i++) {
            printf("  [%zu] %s\n", i, skills[i]);
            agentos_sys_free(skills[i]);
        }
        if (skills) agentos_sys_free(skills);
    }

    // 4. 执行 Skill
    char* output = NULL;
    err = agentos_sys_skill_execute(skill_ids[0],
        "{\"query\": \"microkernel architecture patterns\", \"limit\": 5}",
        &output);
    if (err == AGENTOS_SUCCESS) {
        printf("\nSkill output: %s\n", output);
        agentos_sys_free(output);
    }

    // 5. 卸载 Skill
cleanup:
    for (int i = 0; i < 3; i++) {
        if (skill_ids[i]) {
            agentos_sys_skill_uninstall(skill_ids[i]);
            agentos_sys_free(skill_ids[i]);
        }
    }

    // 6. 清理系统调用层
    agentos_syscalls_cleanup();
    return 0;
}
```

### Skill 与 Agent 协同使用

```c
// 场景：创建一个代码审查 Agent，绑定代码审查 Skill

// 1. 安装所需 Skill
char* review_skill_id = NULL;
agentos_sys_skill_install(
    "https://market.agentos.dev/skills/code-review/v1.0.0",
    &review_skill_id);

char* lint_skill_id = NULL;
agentos_sys_skill_install(
    "https://market.agentos.dev/skills/lint-checker/v2.3.0",
    &lint_skill_id);

// 2. 创建 Agent 并绑定 Skill
agentos_agent_config_t config = {0};
strncpy(config.name, "code-reviewer", sizeof(config.name));
config.type = AGENT_TYPE_TASK;
config.max_concurrent_tasks = 5;

char* agent_id = NULL;
agentos_agent_create(&config, &agent_id);
agentos_agent_skill_bind(agent_id, review_skill_id);
agentos_agent_skill_bind(agent_id, lint_skill_id);

// 3. 通过 Agent 执行任务（Agent 自动选择合适的 Skill）
char* result = NULL;
agentos_agent_invoke(agent_id,
    "Review the following C code for security vulnerabilities:\n"
    "char buf[64]; strcpy(buf, user_input);",
    strlen("Review the following C code..."),
    &result);

printf("Review result: %s\n", result);
agentos_sys_free(result);

// 4. 清理
agentos_agent_skill_unbind(agent_id, review_skill_id);
agentos_agent_skill_unbind(agent_id, lint_skill_id);
agentos_agent_destroy(agent_id);
agentos_sys_skill_uninstall(review_skill_id);
agentos_sys_skill_uninstall(lint_skill_id);
agentos_sys_free(review_skill_id);
agentos_sys_free(lint_skill_id);
agentos_sys_free(agent_id);
```

---

## ⚠️ 注意事项

1. **线程安全**：所有 Skill API 均为线程安全，使用原子操作和互斥锁保护全局 Skill 列表。惰性锁初始化采用 Double-Checked Locking 模式（`__atomic_compare_exchange_n`），避免静态初始化顺序问题。

2. **内存管理**：`agentos_sys_skill_install()` 和 `agentos_sys_skill_list()` 返回的字符串需要调用者通过 `agentos_sys_free()` 释放。`agentos_sys_skill_uninstall()` 会自动释放 Skill 内部数据。

3. **Skill ID 唯一性**：Skill ID 使用原子递增计数器生成（格式 `skill_N`），在单次运行期间保证唯一。系统重启后计数器重置。

4. **执行模式**：当前 Skill 执行为回显模式（echo input），生产环境将集成实际的 Skill 运行时。Skill 执行超时由配置决定，默认 30 秒。

5. **卸载安全**：卸载正在被 Agent 使用的 Skill 会导致未定义行为。建议在卸载前先解绑所有 Agent 的引用。

---

## 🔢 错误码参考

| 错误码 | 值 | 描述 |
|-------|-----|------|
| `AGENTOS_SUCCESS` | 0 | 成功 |
| `AGENTOS_EINVAL` | -2 | 参数无效 |
| `AGENTOS_ENOMEM` | -4 | 内存不足 |
| `AGENTOS_ENOENT` | -6 | Skill 未找到 |
| `AGENTOS_EEXIST` | -7 | Skill 已安装 |
| `AGENTOS_EBUSY` | -11 | Skill 正在使用 |
| `AGENTOS_EACCES` | -10 | 权限不足 |
| `AGENTOS_ETIMEOUT` | -8 | 操作超时 |

### 高层错误码（Capital_API 层）

| 错误码 | 值 | 描述 |
|-------|-----|------|
| `AGENTOS_ERROR_SKILL_NOT_FOUND` | 0x6001 | Skill 未找到 |
| `AGENTOS_ERROR_SKILL_ALREADY_INSTALLED` | 0x6002 | Skill 已安装 |
| `AGENTOS_ERROR_SKILL_INSTALL_FAILED` | 0x6003 | Skill 安装失败 |
| `AGENTOS_ERROR_SKILL_EXECUTION_FAILED` | 0x6004 | Skill 执行失败 |
| `AGENTOS_ERROR_SKILL_PERMISSION_DENIED` | 0x6005 | Skill 权限不足 |
| `AGENTOS_ERROR_SKILL_BUSY` | 0x6006 | Skill 正在执行 |
| `AGENTOS_ERROR_SKILL_INVALID_URL` | 0x6007 | Skill URL 无效 |
| `AGENTOS_ERROR_SKILL_VERIFICATION_FAILED` | 0x6008 | Skill 验证失败 |

---

## 📚 相关文档

| 文档 | 描述 |
|------|------|
| [Agent 管理 API](./agent.md) | Agent 生命周期管理，包含 Skill 绑定/解绑 |
| [任务管理 API](./task.md) | 任务提交与查询 |
| [Skill 契约规范](../../Capital_Specifications/agentos_contract/skill_contract.md) | Skill 契约定义与验证 |
| [架构设计原则](../../ARCHITECTURAL_PRINCIPLES.md) | 五维正交设计体系 |
| [系统调用接口](../../../agentos/atoms/syscall/include/syscalls.h) | 低层系统调用 C 头文件 |

---

**最后更新**: 2026-04-12  
**维护者**: AgentOS API 委员会 (作者: LirenWang)

---

© 2026 SPHARX Ltd. All Rights Reserved.  
*"From data intelligence emerges."*
