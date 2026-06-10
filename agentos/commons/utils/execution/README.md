# Execution — 命令执行引擎

**模块路径**: `agentos/commons/utils/execution/`
**版本**: v0.1.0

## 概述

Execution 模块提供安全、可观测的命令执行环境，支持跨平台命令执行、安全校验和结果格式化。该模块是 AgentOS 中 Agent 执行系统命令的基础设施，所有命令执行均经过注入防护和参数校验，确保系统安全。

## 设计目标

- **安全执行**：命令注入防护、参数白名单、路径安全检查
- **跨平台**：统一 API 屏蔽 Windows/Linux/macOS 的命令执行差异
- **可观测**：执行日志、超时控制、资源限制
- **结果结构化**：标准化的执行结果格式，包含退出码、输出和错误信息

## 目录结构

```
execution/
├── include/
│   └── execution_common.h       # 执行引擎公共接口定义
├── src/
│   └── execution_common.c       # 执行引擎实现
└── README.md                    # 本文档
```

## 核心数据结构

### execution_result_t — 执行结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `status` | `int` | 执行状态码（退出码） |
| `output` | `char *` | 标准输出内容 |
| `output_size` | `size_t` | 输出大小 |
| `error` | `char *` | 错误信息（stderr） |
| `error_size` | `size_t` | 错误信息大小 |
| `execution_time` | `uint64_t` | 执行时间（毫秒） |

### execution_config_t — 执行配置

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `capture_output` | `bool` | false | 是否捕获标准输出 |
| `capture_error` | `bool` | false | 是否捕获标准错误 |
| `timeout_enabled` | `bool` | false | 是否启用超时 |
| `timeout_ms` | `uint32_t` | 0 | 超时时间（毫秒） |
| `shell_enabled` | `bool` | false | 是否在 shell 中执行 |

## 接口说明

| 函数 | 说明 |
|------|------|
| `execution_result_init(result)` | 初始化执行结果，返回 0 成功 |
| `execution_result_cleanup(result)` | 清理执行结果，释放内存 |
| `execution_set_result(result, status, output, ...)` | 设置执行结果各字段 |
| `execution_execute_command(cmd, config, result)` | 执行命令，返回 0 成功 |
| `execution_validate_command(cmd)` | 验证命令安全性，返回 true 安全 |
| `execution_format_result_json(result)` | 格式化结果为 JSON 字符串（需手动释放） |
| `execution_config_init(config)` | 初始化默认执行配置 |

## 使用示例

```c
#include "execution_common.h"

execution_config_t config;
execution_config_init(&config);
config.capture_output = true;
config.timeout_enabled = true;
config.timeout_ms = 30000;

if (!execution_validate_command("ls -la /tmp")) {
    fprintf(stderr, "Command rejected by security check\n");
    return;
}

execution_result_t result;
execution_result_init(&result);

int rc = execution_execute_command("ls -la /tmp", &config, &result);
if (rc == 0 && result.status == 0) {
    printf("Output: %.*s\n", (int)result.output_size, result.output);
} else {
    fprintf(stderr, "Error: %.*s\n", (int)result.error_size, result.error);
}

char *json = execution_format_result_json(&result);
printf("Result JSON: %s\n", json);
AGENTOS_FREE(json);

execution_result_cleanup(&result);
```

## 安全校验规则

| 规则 | 说明 |
|------|------|
| 命令白名单 | 只允许执行白名单中的命令 |
| 参数校验 | 拒绝包含危险字符（`;`, `|`, `` ` ``, `$()`）的参数 |
| 路径安全检查 | 拒绝执行相对路径或包含 `..` 的命令 |
| 资源限制 | 可配置最大内存和 CPU 使用时间 |

## 平台差异

| 特性 | Linux | Windows | macOS |
|------|-------|---------|-------|
| Shell | /bin/sh | cmd.exe | /bin/zsh |
| 路径分隔符 | / | \\ | / |
| 环境变量 | $VAR | %VAR% | $VAR |
| 默认编码 | UTF-8 | GBK/UTF-8 | UTF-8 |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `memory_compat.h` | 统一内存管理宏（`AGENTOS_FREE` 等） |
| `security/input_validator.h` | 输入校验（命令注入检测） |
| `logging.h` | 执行日志记录 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
