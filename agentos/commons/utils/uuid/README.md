# UUID — UUID 生成模块

**模块路径**: `agentos/commons/utils/uuid/`
**版本**: v0.1.0

## 概述

UUID 模块提供符合 RFC 4122 标准的 UUID 生成功能，为 AgentRT 系统中各类实体（任务、会话、记忆、Agent 等）提供全局唯一标识符。模块支持标准 UUID v4（随机）生成、带前缀的 UUID 生成、UUID 格式验证以及二进制与字符串格式之间的双向转换。

## 设计目标

- **全局唯一**：基于随机数生成 UUID v4，确保分布式环境下的唯一性
- **带前缀标识**：支持为不同实体类型生成带前缀的 UUID（如 `mem_`、`task_`），便于可读性和调试
- **格式验证**：提供 UUID 格式有效性检查，确保标识符符合规范
- **双向转换**：支持 UUID 字符串与原始 16 字节二进制格式之间的互转

## 目录结构

```
uuid/
├── CMakeLists.txt               # 构建配置
├── include/
│   └── uuid_generator.h       # UUID 生成器 API 声明
├── src/
│   └── uuid_generator.c       # UUID 生成器实现
└── README.md                  # 本文档
```

## 核心数据结构

### agentos_uuid_error_t — UUID 生成错误码

| 枚举值 | 说明 |
|------|------|
| `AGENTOS_UUID_SUCCESS` | 操作成功 |
| `AGENTOS_UUID_EINVALID` | 无效参数 |
| `AGENTOS_UUID_ENOMEM` | 内存不足 |
| `AGENTOS_UUID_EUNAVAIL` | UUID 生成器不可用 |

### 常量定义

| 宏 | 值 | 说明 |
|------|------|------|
| `AGENTOS_UUID_STR_LEN` | 37 | 标准 UUID 字符串长度（含空字符） |
| `AGENTOS_UUID_PREFIXED_STR_LEN` | 64 | 带前缀 UUID 字符串最大长度（含空字符） |

## 接口说明

| 函数 | 说明 |
|------|------|
| `agentos_uuid_init()` | 初始化 UUID 生成器（随机数种子等） |
| `agentos_uuid_cleanup()` | 清理 UUID 生成器资源 |
| `agentos_uuid_v4(out_buf, buf_len)` | 生成标准 UUID v4 字符串（格式：`xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`） |
| `agentos_uuid_with_prefix(prefix, out_buf, buf_len)` | 生成带前缀的 UUID（格式：`prefix_xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`） |
| `agentos_uuid_is_valid(uuid)` | 验证 UUID 格式是否有效，有效返回 1，无效返回 0 |
| `agentos_uuid_bin_to_str(uuid_bin, out_buf, buf_len)` | 将 16 字节原始 UUID 二进制转换为字符串 |
| `agentos_uuid_str_to_bin(uuid_str, out_bin)` | 将 UUID 字符串转换为 16 字节原始二进制 |

## 使用示例

```c
#include "uuid_generator.h"
#include <stdio.h>

/* === 初始化 UUID 生成器 === */
if (agentos_uuid_init() != AGENTOS_UUID_SUCCESS) {
    fprintf(stderr, "Failed to initialize UUID generator\n");
    return -1;
}

/* === 生成标准 UUID v4 === */
char uuid_str[AGENTOS_UUID_STR_LEN];
if (agentos_uuid_v4(uuid_str, sizeof(uuid_str)) == AGENTOS_UUID_SUCCESS) {
    printf("UUID v4: %s\n", uuid_str);
    // 输出示例: "550e8400-e29b-41d4-a716-446655440000"
}

/* === 生成带前缀的 UUID === */
char mem_uuid[AGENTOS_UUID_PREFIXED_STR_LEN];
if (agentos_uuid_with_prefix("mem", mem_uuid, sizeof(mem_uuid)) == AGENTOS_UUID_SUCCESS) {
    printf("Memory UUID: %s\n", mem_uuid);
    // 输出示例: "mem_550e8400-e29b-41d4-a716-446655440000"
}

char task_uuid[AGENTOS_UUID_PREFIXED_STR_LEN];
if (agentos_uuid_with_prefix("task", task_uuid, sizeof(task_uuid)) == AGENTOS_UUID_SUCCESS) {
    printf("Task UUID: %s\n", task_uuid);
    // 输出示例: "task_660e8400-e29b-41d4-a716-446655440001"
}

char session_uuid[AGENTOS_UUID_PREFIXED_STR_LEN];
if (agentos_uuid_with_prefix("session", session_uuid, sizeof(session_uuid)) == AGENTOS_UUID_SUCCESS) {
    printf("Session UUID: %s\n", session_uuid);
    // 输出示例: "session_770e8400-e29b-41d4-a716-446655440002"
}

/* === 验证 UUID 格式 === */
if (agentos_uuid_is_valid(uuid_str)) {
    printf("Valid UUID format\n");
} else {
    printf("Invalid UUID format\n");
}

// 非法 UUID 示例
if (!agentos_uuid_is_valid("not-a-valid-uuid")) {
    printf("Correctly rejected invalid UUID\n");
}

/* === 二进制与字符串互转 === */
// 字符串 -> 二进制
uint8_t uuid_bin[16];
if (agentos_uuid_str_to_bin(uuid_str, uuid_bin) == AGENTOS_UUID_SUCCESS) {
    printf("Converted to binary: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", uuid_bin[i]);
    }
    printf("\n");
}

// 二进制 -> 字符串
char reconverted[AGENTOS_UUID_STR_LEN];
if (agentos_uuid_bin_to_str(uuid_bin, reconverted, sizeof(reconverted)) == AGENTOS_UUID_SUCCESS) {
    printf("Reconverted string: %s\n", reconverted);
}

/* === 清理 UUID 生成器 === */
agentos_uuid_cleanup();
```

## UUID 格式说明

UUID v4 的标准格式为 `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`，其中：

| 位置 | 内容 | 说明 |
|------|------|------|
| 第 13 位 | `4` | 版本号（固定为 4，表示 UUID v4） |
| 第 17 位 | `8`、`9`、`a` 或 `b` | 变体位（RFC 4122 变体） |
| 其余位 | 随机十六进制字符 | 随机生成 |

带前缀的 UUID 格式为 `{prefix}_xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`，前缀由调用者指定，用于区分不同实体类型的标识符。

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `stddef.h` | `size_t` 等类型 |
| `stdint.h` | 固定宽度整数类型（`uint8_t`） |
| 系统随机数源 | `/dev/urandom`（Linux）或等效随机数 API |

---

© 2026 SPHARX Ltd. All Rights Reserved.