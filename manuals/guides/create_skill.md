Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS Skill 创建指南

**版本**: Doc V1.5  
**最后更新**: 2026-03-23  
**难度**: ⭐⭐ 中级  
**预计时间**: 25 分钟

---

## 1. 概述

Skill（技能）是 AgentOS 中 Agent 的原子能力单元。每一个 Skill 封装一个独立的功能——从文件操作到网络请求，从数据分析到模型推理。Skill 是 AgentOS "执行单元"概念的具体实现，通过注册表动态加载，是 System 1 快速路径的核心组件。

### 1.1 Skill 在架构中的位置

```
┌─────────────────────────────────────────────┐
│              daemon/ 用户态服务                │
│          ┌─────────────────────┐            │
│          │   agent_d           │            │
│          │  ┌───────────────┐  │            │
│          │  │ Skill Registry │  │ ← 技能注册表
│          │  │  ┌─────┐┌────┐│  │            │
│          │  │  │ S_1 ││S_2 ││  │ ← 可插拔技能
│          │  │  └─────┘└────┘│  │            │
│          │  │  ┌─────┐┌────┐│  │            │
│          │  │  │ S_3 ││S_N ││  │            │
│          │  │  └─────┘└────┘│  │            │
│          │  └───────────────┘  │            │
│          └─────────┬───────────┘            │
├────────────────────┼────────────────────────┤
│            syscall/ 系统调用接口              │
│     task · memory · session · telemetry     │
├────────────────────┴────────────────────────┤
│         corekern/ 微内核基础                 │
│     IPC · Mem · Task · Time                 │
└─────────────────────────────────────────────┘
```

Skill 与 Agent 的关系是**组合而非继承**——一个 Agent 可以动态加载任意数量的 Skill，Skill 也可以被多个 Agent 共享。

### 1.2 设计原则

- **原子性**: 每个 Skill 只做一件事，做好一件事
- **可插拔**: Skill 通过注册表动态加载和卸载
- **契约驱动**: Skill 必须实现 Skill Contract 定义的标准接口
- **安全隔离**: Skill 在虚拟工位中执行，遵循最小权限原则

---

## 2. Skill 生命周期

```
     ┌──────────┐
     │  LOADED  │ skill_load()
     └────┬─────┘
          │ skill_register()
     ┌────▼─────┐
     │ REGISTERED│◄──────────────────────┐
     └────┬─────┘                        │
          │ skill_activate()              │
     ┌────▼─────┐                        │
     │  ACTIVE  │                        │
     └────┬─────┘                        │
          │                              │
     ┌────▼─────┐   skill_deactivate()   │
     │INACTIVE  ├────────────────────────┤
     └────┬─────┘                        │
          │ skill_activate()              │
     ┌────▼─────┐                        │
     │  ACTIVE  │                        │
     └────┬─────┘                        │
          │ skill_unregister()            │
     ┌────▼─────┐                        │
     │UNLOADED  │────────────────────────┘
     └──────────┘
```

---

## 3. Skill Contract

### 3.1 Skill Contract JSON

每个 Skill 必须提供 `skill_contract.json`：

```json
{
  "$schema": "https://agentos.io/schemas/skill_contract_v1.2.0.json",
  "name": "file_reader",
  "version": "1.0.0",
  "description": "安全地读取文件内容",
  "author": "AgentOS Core Team",
  "license": "MIT",
  "input": {
    "type": "object",
    "required": ["path"],
    "properties": {
      "path": {
        "type": "string",
        "description": "文件绝对路径"
      },
      "encoding": {
        "type": "string",
        "default": "utf-8",
        "description": "文件编码"
      },
      "max_bytes": {
        "type": "integer",
        "default": 1048576,
        "description": "最大读取字节数"
      }
    }
  },
  "output": {
    "type": "object",
    "properties": {
      "content": { "type": "string" },
      "size": { "type": "integer" },
      "encoding": { "type": "string" }
    }
  },
  "errors": [
    { "code": -1, "message": "文件不存在" },
    { "code": -2, "message": "权限不足" },
    { "code": -3, "message": "文件过大" },
    { "code": -4, "message": "编码不支持" }
  ],
  "security": {
    "sandbox": "workbench",
    "permissions": ["fs.read"],
    "resource_limits": {
      "max_memory": "64MB",
      "timeout": 10
    }
  },
  "performance": {
    "expected_latency_ms": 5,
    "max_concurrent": 100
  }
}
```

### 3.2 Skill 接口定义

```c
#ifndef AGENTOS_SKILL_H
#define AGENTOS_SKILL_H

#include <agentos/bases.h>

typedef struct agentos_skill agentos_skill_t;
typedef struct agentos_skill_input agentos_skill_input_t;
typedef struct agentos_skill_output agentos_skill_output_t;

typedef int (*skill_execute_fn)(const agentos_skill_input_t* input, agentos_skill_output_t* output);
typedef int (*skill_validate_fn)(const agentos_skill_input_t* input);
typedef int (*skill_cleanup_fn)(void);

struct agentos_skill {
    const char*            name;
    const char*            version;
    const char*            description;
    skill_execute_fn       execute;
    skill_validate_fn      validate;
    skill_cleanup_fn       cleanup;
    void*                  user_data;
};

struct agentos_skill_input {
    const char*  json_payload;
    size_t       payload_size;
    uint64_t     trace_id;
    uint64_t     task_id;
    void*        user_context;
};

struct agentos_skill_output {
    char*        json_result;
    size_t       result_size;
    int          error_code;
    char         error_message[256];
    uint64_t     elapsed_ms;
};

#endif
```

---

## 4. 实现 Skill

### 4.1 项目结构

```
skill_file_reader/
├── CMakeLists.txt
├── skill_contract.json
├── include/
│   └── file_reader_skill.h
└── src/
    └── file_reader_skill.c
```

### 4.2 头文件

```c
#ifndef FILE_READER_SKILL_H
#define FILE_READER_SKILL_H

#include <agentos/skill.h>

#define FILE_READER_VERSION "1.0.0"
#define FILE_READER_MAX_SIZE (10 * 1024 * 1024)

/**
 * @brief 创建文件读取技能实例
 * 
 * 初始化技能元数据和执行函数指针。
 * 
 * @return 技能实例指针，需由调用者释放
 */
agentos_skill_t* file_reader_skill_create(void);

/**
 * @brief 销毁文件读取技能实例
 * 
 * @param skill 技能实例指针
 */
void file_reader_skill_destroy(agentos_skill_t* skill);

#endif
```

### 4.3 核心实现

```c
#include "file_reader_skill.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 验证输入参数
 */
static int file_reader_validate(const agentos_skill_input_t* input)
{
    if (!input || !input->json_payload) {
        return AGENTOS_EINVAL;
    }

    // 解析 JSON 检查必需字段
    // 简化示例：实际使用 JSON 解析库
    if (strstr(input->json_payload, "\"path\"") == NULL) {
        return AGENTOS_EINVAL;
    }

    return 0;
}

/**
 * @brief 执行文件读取
 */
static int file_reader_execute(const agentos_skill_input_t* input, agentos_skill_output_t* output)
{
    uint64_t start = agentos_time_now_ns();

    // 1. 解析输入
    char path[4096] = { 0 };
    const char* encoding = "utf-8";
    size_t max_bytes = FILE_READER_MAX_SIZE;

    // 简化 JSON 解析 — 实际使用 agentos_json_parse()
    // parse_path(input->json_payload, path, sizeof(path));
    // parse_encoding(input->json_payload, &encoding);
    // parse_max_bytes(input->json_payload, &max_bytes);

    (void)input; // 实际实现中解析 JSON

    // 2. 安全检查 — 输入净化（安全穹顶自动执行）
    // domes_sanitize_path(path);
    // domes_check_permission(path, "fs.read");

    // 3. 读取文件
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        output->error_code = AGENTOS_ENOENT;
        snprintf(output->error_message, sizeof(output->error_message),
                 "文件不存在: %s", path);
        return AGENTOS_ENOENT;
    }

    // 检查文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size > (long)max_bytes) {
        fclose(fp);
        output->error_code = -3;
        snprintf(output->error_message, sizeof(output->error_message),
                 "文件过大: %ld > %zu", file_size, max_bytes);
        return -3;
    }

    // 读取内容
    char* content = malloc((size_t)file_size + 1);
    if (!content) {
        fclose(fp);
        return AGENTOS_ENOMEM;
    }

    size_t bytes_read = fread(content, 1, (size_t)file_size, fp);
    content[bytes_read] = '\0';
    fclose(fp);

    // 4. 构建输出
    // 简化 JSON 构建 — 实际使用 agentos_json_build()
    char json_buf[bytes_read + 256];
    snprintf(json_buf, sizeof(json_buf),
             "{\"content\":\"%s\",\"size\":%zu,\"encoding\":\"%s\"}",
             content, bytes_read, encoding);

    output->json_result = strdup(json_buf);
    output->result_size = strlen(json_buf);
    output->error_code = 0;
    output->elapsed_ms = (agentos_time_now_ns() - start) / 1000000;

    free(content);
    return 0;
}

/**
 * @brief 清理资源
 */
static int file_reader_cleanup(void)
{
    return 0;
}

/**
 * @brief 创建技能实例
 */
agentos_skill_t* file_reader_skill_create(void)
{
    agentos_skill_t* skill = calloc(1, sizeof(agentos_skill_t));
    if (!skill) return NULL;

    skill->name = "file_reader";
    skill->version = FILE_READER_VERSION;
    skill->description = "安全地读取文件内容";
    skill->execute = file_reader_execute;
    skill->validate = file_reader_validate;
    skill->cleanup = file_reader_cleanup;

    return skill;
}

/**
 * @brief 销毁技能实例
 */
void file_reader_skill_destroy(agentos_skill_t* skill)
{
    if (skill) {
        if (skill->cleanup) skill->cleanup();
        free(skill);
    }
}
```

---

## 5. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(skill_file_reader VERSION 1.0.0 LANGUAGES C)

set(C_STANDARD 11)
set(C_STANDARD_REQUIRED ON)

find_package(AgentOS REQUIRED COMPONENTS corekern)

add_library(skill_file_reader SHARED src/file_reader_skill.c)
target_include_directories(skill_file_reader PRIVATE include)
target_link_libraries(skill_file_reader PRIVATE AgentOS::corekern)

install(TARGETS skill_file_reader DESTINATION lib/agentos/skills)
install(FILES skill_contract.json DESTINATION share/agentos/skills/file_reader)
```

---

## 6. 注册与使用

### 6.1 编译安装

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
sudo cmake --install .
```

### 6.2 在 Agent 中注册

```c
#include <agentos/skill_registry.h>
#include "file_reader_skill.h"

int setup_skills(void)
{
    // 创建技能
    agentos_skill_t* reader = file_reader_skill_create();
    if (!reader) return AGENTOS_ENOMEM;

    // 验证契约
    int ret = agentos_skill_validate_contract(reader, "skill_contract.json");
    if (ret != 0) {
        file_reader_skill_destroy(reader);
        return ret;
    }

    // 注册到注册表
    ret = agentos_skill_registry_register(reader);
    if (ret != 0) {
        file_reader_skill_destroy(reader);
        return ret;
    }

    return 0;
}
```

### 6.3 调用 Skill

```c
/**
 * @brief 通过注册表调用技能
 */
int call_file_reader(const char* file_path, agentos_skill_output_t* output)
{
    // 查找技能
    agentos_skill_t* skill = agentos_skill_registry_find("file_reader");
    if (!skill) return AGENTOS_ENOENT;

    // 准备输入
    agentos_skill_input_t input = {
        .json_payload = "{\"path\":\"/tmp/data.csv\"}",
        .payload_size = 23,
        .trace_id = agentos_trace_current_id(),
        .task_id = 0
    };

    // 执行技能
    return skill->execute(&input, output);
}
```

---

## 7. 高级 Skill 模式

### 7.1 组合 Skill（Pipeline）

多个 Skill 可以组合成流水线，实现复杂功能：

```c
/**
 * @brief 创建技能流水线 — 读取→解析→分析
 */
int create_analysis_pipeline(void)
{
    agentos_pipeline_t* pipeline = agentos_pipeline_create("data_analysis");

    agentos_pipeline_add_stage(pipeline, "file_reader", NULL);
    agentos_pipeline_add_stage(pipeline, "csv_parser", NULL);
    agentos_pipeline_add_stage(pipeline, "stat_analyzer", NULL);

    return agentos_pipeline_register(pipeline);
}
```

### 7.2 条件路由 Skill

根据输入动态选择 Skill：

```c
/**
 * @brief 条件路由 — 根据文件类型选择解析器
 */
static int route_by_extension(const agentos_skill_input_t* input, char* skill_name, size_t len)
{
    const char* ext = strrchr(input->json_payload, '.');
    if (!ext) return AGENTOS_EINVAL;

    if (strcmp(ext, ".csv") == 0) {
        snprintf(skill_name, len, "csv_parser");
    } else if (strcmp(ext, ".json") == 0) {
        snprintf(skill_name, len, "json_parser");
    } else if (strcmp(ext, ".xml") == 0) {
        snprintf(skill_name, len, "xml_parser");
    } else {
        snprintf(skill_name, len, "text_reader");
    }

    return 0;
}
```

### 7.3 异步 Skill

长时间运行的 Skill 支持异步执行：

```c
/**
 * @brief 异步执行技能
 */
int execute_skill_async(const char* skill_name, const char* input_json,
                        agentos_async_callback_t callback, void* user_data)
{
    agentos_skill_t* skill = agentos_skill_registry_find(skill_name);
    if (!skill) return AGENTOS_ENOENT;

    agentos_skill_input_t input = {
        .json_payload = input_json,
        .payload_size = strlen(input_json),
        .trace_id = agentos_trace_current_id()
    };

    return agentos_task_spawn((agentos_task_fn)skill->execute, &input, callback, user_data);
}
```

---

## 8. 测试

### 8.1 单元测试

```c
#include "file_reader_skill.h"
#include <ctest/ctest.h>

CTEST(file_reader, validate_success)
{
    agentos_skill_input_t input = {
        .json_payload = "{\"path\":\"/tmp/test.txt\"}",
        .payload_size = 26
    };

    agentos_skill_t* skill = file_reader_skill_create();
    int ret = skill->validate(&input);
    ASSERT_EQUAL(0, ret);

    file_reader_skill_destroy(skill);
}

CTEST(file_reader, validate_missing_path)
{
    agentos_skill_input_t input = {
        .json_payload = "{\"encoding\":\"utf-8\"}",
        .payload_size = 21
    };

    agentos_skill_t* skill = file_reader_skill_create();
    int ret = skill->validate(&input);
    ASSERT_NOT_EQUAL(0, ret);

    file_reader_skill_destroy(skill);
}
```

### 8.2 契约验证

```bash
agentos-cli skill validate --contract skill_contract.json
```

---

## 9. 最佳实践

### 9.1 设计准则

| 原则 | 说明 | 示例 |
|------|------|------|
| **单一职责** | 一个 Skill 只做一件事 | `file_reader` vs `file_processor` |
| **无状态** | Skill 不保存跨调用状态 | 所有状态通过输入输出传递 |
| **幂等性** | 相同输入产生相同输出 | `json_parser` 对同一 JSON 总返回相同结果 |
| **快速失败** | 遇到错误立即返回 | 参数验证在执行前完成 |

### 9.2 性能优化

- 使用内存池减少 `malloc` 开销
- 大文件读取使用分块传输
- 热路径 Skill 使用 `__attribute__((hot))` 标记
- 避免在 `execute` 中进行不必要的内存分配

### 9.3 安全注意事项

- 所有文件路径必须经过 `domes_sanitize_path()` 净化
- 环境变量访问必须声明权限
- 网络请求必须通过安全穹顶代理
- Skill 不得直接访问内核接口

---

## 10. 相关文档

- [Skill 契约规范](../specifications/agentos_contract/skill/skill_contract.md)
- [Agent 创建指南](create_agent.md)
- [编码规范](../specifications/coding_standard/C_coding_style_guide.md)
- [安全穹顶](../architecture/folder/microkernel.md)

---

**最后更新**: 2026-03-23  
**维护者**: AgentOS 核心团队

---

© 2026 SPHARX Ltd. All Rights Reserved.  
*"From data intelligence emerges."*