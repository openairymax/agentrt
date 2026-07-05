# Security — 安全模块

**模块路径**: `agentrt/commons/utils/security/`
**版本**: v0.1.0

## 概述

Security 模块提供统一的输入验证和净化功能，防止注入攻击、路径遍历、缓冲区溢出、SSRF 等常见安全漏洞。该模块遵循白名单验证原则，只允许已知安全的输入模式，是 AgentRT 安全内生体系（E-1 原则）的核心实现。

## 设计目标

- **白名单验证**：只允许已知安全的输入模式，拒绝一切未明确允许的内容
- **多层防护**：字符串验证、路径验证、命令验证、SQL 验证、URL 验证、数值验证、缓冲区验证
- **安全失败**：验证失败时明确返回错误信息，不静默通过
- **净化输出**：提供输入净化函数，将危险输入转换为安全形式
- **线程安全**：所有公共接口均为线程安全

## 安全原则

1. **永不信任外部输入** — 所有来自用户、网络、文件的输入必须经过验证
2. **白名单优于黑名单** — 定义允许的模式，拒绝其他一切
3. **边界检查必须严格** — 长度、范围、缓冲区大小检查不可省略
4. **错误时安全失败** — 验证失败必须拒绝操作，不能静默通过

## 目录结构

```
security/
├── src/
│   ├── input_validator.h        # 输入验证接口定义
│   └── input_validator.c        # 输入验证实现
└── README.md                    # 本文档
```

## 核心数据结构

### agentrt_validation_result_t — 验证结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `is_valid` | `int` | 是否通过验证（1=有效，0=无效） |
| `error_message` | `const char *` | 错误消息 |
| `error_code` | `int` | 错误码 |
| `error_field` | `const char *` | 错误字段名称 |

## 接口说明

### 字符串验证

| 函数 | 说明 |
|------|------|
| `agentrt_validate_string_length(str, min_len, max_len, result)` | 验证字符串长度是否在范围内 |
| `agentrt_validate_string_charset(str, allowed_chars, result)` | 验证字符串是否只包含白名单字符 |
| `agentrt_validate_identifier(str, max_len, result)` | 验证标识符（字母/数字/下划线，首字符为字母或下划线） |
| `agentrt_validate_json_string(str, max_len, result)` | 验证 JSON 字符串（检查括号平衡、引号闭合） |

### 路径验证

| 函数 | 说明 |
|------|------|
| `agentrt_validate_file_path(path, allowed_root, result)` | 验证文件路径安全性（检测 `../` 遍历、空字节注入、根目录限制） |
| `agentrt_normalize_path(path, out_normalized, out_len)` | 规范化路径（解析符号链接、相对路径） |

### 命令验证

| 函数 | 说明 |
|------|------|
| `agentrt_validate_shell_command(cmd, allowed_commands, result)` | 验证 Shell 命令安全性（检测 `;` `\|` `&` `$` `` ` `` 等注入字符） |
| `agentrt_sanitize_shell_param(param, out_sanitized)` | 净化 Shell 参数（转义单引号，移除危险字符） |

### SQL 验证

| 函数 | 说明 |
|------|------|
| `agentrt_validate_sql_query(sql, result)` | 验证 SQL 查询安全性（检测 DROP、UNION SELECT、OR 1=1、注释注入等） |
| `agentrt_sanitize_sql_identifier(identifier, out_sanitized)` | 净化 SQL 标识符（验证后加双引号包裹） |

### URL 验证

| 函数 | 说明 |
|------|------|
| `agentrt_validate_url(url, allowed_schemes, result)` | 验证 URL 安全性（检测危险协议、内网 IP、localhost） |
| `agentrt_parse_url(url, out_scheme, out_host, out_port, out_path)` | 解析 URL 组件（协议、主机、端口、路径） |

### 数值验证

| 函数 | 说明 |
|------|------|
| `agentrt_validate_int_range(value, min_val, max_val, result)` | 验证整数范围 |
| `agentrt_validate_float_range(value, min_val, max_val, result)` | 验证浮点数范围 |

### 缓冲区验证

| 函数 | 说明 |
|------|------|
| `agentrt_safe_memcpy(dest, dest_size, src, src_size)` | 安全内存复制（带边界检查） |
| `agentrt_safe_strcpy(dest, dest_size, src)` | 安全字符串复制（带终止符空间检查） |
| `agentrt_safe_strcat(dest, dest_size, src)` | 安全字符串拼接（带长度检查） |

### 便捷宏

| 宏 | 说明 |
|------|------|
| `AGENTRT_VALIDATE_OR_RETURN(result, error_code)` | 验证失败则返回错误码 |
| `AGENTRT_VALIDATE_OR_GOTO(result, label, error_code)` | 验证失败则跳转到清理标签 |
| `AGENTRT_SAFE_STRCPY(dest, src)` | 安全字符串复制（自动计算 sizeof） |
| `AGENTRT_SAFE_STRCAT(dest, src)` | 安全字符串拼接（自动计算 sizeof） |

## 使用示例

```c
#include "input_validator.h"

// ===== 字符串验证 =====
agentrt_validation_result_t result;

// 验证标识符
agentrt_validate_identifier("my_var_123", 64, &result);
if (result.is_valid) {
    // 标识符安全，继续处理
}

// 验证 JSON
agentrt_validate_json_string("{\"key\": \"value\"}", 4096, &result);
if (!result.is_valid) {
    fprintf(stderr, "Invalid JSON: %s\n", result.error_message);
}

// ===== 路径验证 =====
agentrt_validate_file_path("/tmp/data/config.json", "/tmp", &result);
AGENTRT_VALIDATE_OR_RETURN(result, AGENTRT_ESECURITY);

// 规范化路径
char *normalized = NULL;
size_t normalized_len = 0;
if (agentrt_normalize_path("../../etc/passwd", &normalized, &normalized_len) == AGENTRT_SUCCESS) {
    printf("Normalized: %s\n", normalized);
    AGENTRT_FREE(normalized);
}

// ===== 命令验证 =====
const char *allowed[] = {"ls", "cat", "echo", "grep", NULL};
agentrt_validate_shell_command("ls -la /tmp", allowed, &result);
if (result.is_valid) {
    // 命令安全，可以执行
}

// 净化 Shell 参数
char *safe_param = NULL;
agentrt_sanitize_shell_param("user's input; rm -rf /", &safe_param);
printf("Sanitized: %s\n", safe_param);  // 输出: 'user\'s input; rm -rf /'
AGENTRT_FREE(safe_param);

// ===== SQL 验证 =====
agentrt_validate_sql_query("SELECT * FROM users WHERE id = 1", &result);
if (result.is_valid) {
    // SQL 安全
}

// 净化 SQL 标识符
char *safe_identifier = NULL;
agentrt_sanitize_sql_identifier("user_name", &safe_identifier);
printf("Safe identifier: %s\n", safe_identifier);  // 输出: "user_name"
AGENTRT_FREE(safe_identifier);

// ===== URL 验证 =====
const char *schemes[] = {"http", "https", NULL};
agentrt_validate_url("https://api.example.com/data", schemes, &result);
if (result.is_valid) {
    // URL 安全，可以发起请求
}

// 解析 URL
char *scheme = NULL, *host = NULL, *path = NULL;
uint16_t port = 0;
agentrt_parse_url("https://api.example.com:443/v1/data", &scheme, &host, &port, &path);
printf("Scheme: %s, Host: %s, Port: %d, Path: %s\n", scheme, host, port, path);
AGENTRT_FREE(scheme);
AGENTRT_FREE(host);
AGENTRT_FREE(path);

// ===== 数值验证 =====
agentrt_validate_int_range(100, 0, 255, &result);
if (result.is_valid) {
    // 数值在有效范围内
}

// ===== 缓冲区安全操作 =====
char dest[64];
if (agentrt_safe_strcpy(dest, sizeof(dest), user_input) == AGENTRT_SUCCESS) {
    printf("Safe copy: %s\n", dest);
}
```

## 安全检测规则

### Shell 命令注入检测

| 检测项 | 说明 |
|------|------|
| 危险字符 | `;` `\|` `&` `$` `` ` `` `\n` |
| 危险命令 | `rm -rf`、`dd`、`mkfs`、`fdisk`、`shutdown`、`reboot`、`chmod 777`、`chown` 等 |
| 命令白名单 | 可选，启用后只允许执行白名单中的命令 |

### SQL 注入检测

| 检测项 | 说明 |
|------|------|
| 危险关键字 | `DROP`、`TRUNCATE`、`ALTER`、`DELETE FROM`、`UNION SELECT`、`EXEC(`、`EXECUTE(` |
| 注释注入 | `--`、`/*`、`*/`、`; --` |
| 布尔注入 | `OR 1=1`、`OR '1'='1'` |
| 引号平衡 | 检测单引号数量是否成对 |

### URL 安全检测

| 检测项 | 说明 |
|------|------|
| 危险协议 | `javascript:`、`data:`、`vbscript:`、`file:`、`about:`、`blob:`、`filesystem:` |
| SSRF 防护 | 检测内网 IP（`10.x`、`172.16-31.x`、`192.168.x`、`127.x`、`169.254.x`） |
| 本地主机 | 检测 `localhost`、`127.0.0.1`、`::1`、`fc`/`fd`/`fe`/`ff` 前缀 |

### 路径遍历检测

| 检测项 | 说明 |
|------|------|
| 目录遍历 | 检测 `..`、`../`、`..\\` |
| 空字节注入 | 检测路径中间的空字节 |
| 根目录限制 | 可选，限制路径必须在指定的根目录下 |
| 路径长度 | 最大 4096 字符 |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `error.h` | 统一错误码定义（`AGENTRT_SUCCESS`、`AGENTRT_EINVAL`、`AGENTRT_ESECURITY` 等） |
| `logger.h` | 日志记录（安全事件告警） |
| `memory_compat.h` | 统一内存管理宏 |
| `string_compat.h` | 字符串操作兼容层 |

---

© 2026 SPHARX Ltd. All Rights Reserved.