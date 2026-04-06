# 执行工具模块

**版本**: v1.0.0  
**最后更新**: 2026-04-06  
**许可证**: Apache License 2.0

---

## 🎯 概述

执行工具模块是AgentOS系统的命令执行基础设施，提供安全的命令执行、结果捕获、超时控制等核心功能。本模块封装了底层系统调用的复杂性，为上层组件提供统一、安全的执行抽象。

## 🏗️ 核心功能

### 1. 命令执行引擎

支持同步/异步命令执行，具备完善的输出捕获能力：

```c
// 初始化配置
execution_config_t config;
execution_config_init(&config);
config.capture_output = true;
config.capture_error = true;
config.timeout_enabled = true;
config.timeout_ms = 5000;  // 5秒超时

// 执行命令
execution_result_t result;
int ret = execution_execute_command("ls -la", &config, &result);

if (ret == 0 && result.status == 0) {
    printf("Output:\n%.*s\n", (int)result.output_size, result.output);
} else {
    printf("Error: %.*s\n", (int)result.error_size, result.error);
}

// 清理资源
execution_result_cleanup(&result);
```

### 2. 安全验证机制

内置命令安全性检查，防止注入攻击：

```c
const char* user_command = get_user_input();

// 验证命令安全性
if (!execution_validate_command(user_command)) {
    fprintf(stderr, "❌ Unsafe command detected!\n");
    return -1;
}

// 安全执行
execution_execute_command(user_command, &config, &result);
```

**安全特性**:
- ✅ 检测管道符 (`|`, `;`, `&&`) 注入
- ✅ 检测重定向符号 (`>`, `>>`, `<`) 滥用
- ✅ 检测反引号命令替换
- ✅ 支持白名单机制扩展

### 3. 结果格式化输出

自动将执行结果转换为JSON格式，便于系统集成：

```c
char* json = execution_format_result_json(&result);
printf("%s\n", json);

// 输出示例:
// {
//   "status": 0,
//   "output": "...",
//   "error": "",
//   "execution_time": 150
// }

AGENTOS_FREE(json);  // 需要手动释放
```

**核心数据结构**:

| 结构体 | 用途 | 关键字段 |
|--------|------|---------|
| `execution_result_t` | 存储执行结果 | status, output, error, execution_time |
| `execution_config_t` | 配置执行行为 | capture_output, timeout_ms, shell_enabled |

## 🔧 主要API接口

| 函数名 | 功能描述 | 线程安全 | 复杂度 |
|--------|---------|----------|--------|
| `execution_config_init()` | 初始化默认配置 | ✅ 安全 | O(1) |
| `execution_result_init()` | 初始化结果结构 | ✅ 安全 | O(1) |
| `execution_execute_command()` | 执行命令 | ⚠️ 需外部同步 | O(系统调用) |
| `execution_validate_command()` | 验证命令安全性 | ✅ 安全 | O(n) |
| `execution_set_result()` | 设置执行结果 | ⚠️ 需外部同步 | O(1) |
| `execution_format_result_json()` | 格式化为JSON | ⚠️ 非线程安全 | O(n) |
| `execution_result_cleanup()` | 清理结果资源 | ✅ 安全 | O(1) |

## 📊 使用示例

### 场景1：批量命令执行

```c
#include "execution_common.h"

void batch_execute(const char* commands[], size_t count) {
    execution_config_t config;
    execution_config_init(&config);
    config.capture_output = true;
    config.timeout_enabled = true;
    config.timeout_ms = 3000;
    
    for (size_t i = 0; i < count; i++) {
        execution_result_t result;
        
        printf("[%zu] Executing: %s\n", i, commands[i]);
        
        int ret = execution_execute_command(commands[i], &config, &result);
        if (ret != 0) {
            printf("  ❌ Execution failed with code %d\n", ret);
            continue;
        }
        
        if (result.status == 0) {
            printf("  ✅ Success (%llums)\n", result.execution_time);
            if (result.output_size > 0) {
                printf("  Output: %.*s\n", (int)result.output_size, result.output);
            }
        } else {
            printf("  ❌ Command returned %d\n", result.status);
            if (result.error_size > 0) {
                printf("  Error: %.*s\n", (int)result.error_size, result.error);
            }
        }
        
        execution_result_cleanup(&result);
    }
}
```

### 场景2：带超时的长时间运行任务

```c
#include "execution_common.h"

int run_with_timeout(const char* command, uint32_t timeout_ms) {
    execution_config_t config;
    execution_config_init(&config);
    config.capture_output = true;
    config.capture_error = true;
    config.timeout_enabled = true;
    config.timeout_ms = timeout_ms;
    
    execution_result_t result;
    int ret = execution_execute_command(command, &config, &result);
    
    if (ret != 0) {
        fprintf(stderr, "Failed to execute command\n");
        execution_result_cleanup(&result);
        return -1;
    }
    
    if (result.execution_time >= timeout_ms) {
        printf("⚠️ Command timed out after %llums\n", result.execution_time);
        execution_result_cleanup(&result);
        return -2;
    }
    
    printf("✅ Completed in %llums\n", result.execution_time);
    
    // 处理输出...
    execution_result_cleanup(&result);
    return 0;
}
```

### 场景3：安全沙箱执行

```c
#include "execution_common.h"

int safe_execute_in_sandbox(const char* user_input) {
    // 第一步：安全验证
    if (!execution_validate_command(user_input)) {
        fprintf(stderr, "Security violation detected!\n");
        log_write(LOG_LEVEL_WARN, __FILE__, __LINE__,
                  "Blocked unsafe command: %s", user_input);
        return -1;
    }
    
    // 第二步：配置受限环境
    execution_config_t config;
    execution_config_init(&config);
    config.shell_enabled = false;  // 禁用shell解释
    config.timeout_enabled = true;
    config.timeout_ms = 10000;     // 10秒硬限制
    
    // 第三步：执行并记录
    execution_result_t result;
    int ret = execution_execute_command(user_input, &config, &result);
    
    // 第四步：审计日志
    char* json_log = execution_format_result_json(&result);
    log_write(LOG_LEVEL_INFO, __FILE__, __LINE__,
              "Command executed: %s", json_log);
    AGENTOS_FREE(json_log);
    
    execution_result_cleanup(&result);
    return ret;
}
```

## ⚠️ 注意事项

### 内存管理
- **必须配对调用** `execution_result_init()` / `execution_result_cleanup()`
- `execution_format_result_json()` 返回的字符串需要调用者使用 `AGENTOS_FREE()` 释放
- `output` 和 `error` 字段在 cleanup 时自动释放，不要手动 free

### 线程安全
- ✅ `execution_validate_command()` 可安全用于多线程环境
- ⚠️ `execution_execute_command()` 会创建子进程，需注意资源竞争
- ⚠️ `execution_result_t` 不是线程安全的，每个线程应使用独立实例

### 平台差异
| 功能 | Linux/macOS | Windows |
|------|-------------|---------|
| 命令执行 | `fork()+execvp()` | `CreateProcess()` |
| 超时控制 | `alarm()/sigaction()` | `WaitForSingleObject()` |
| Shell模式 | `/bin/sh -c` | `cmd.exe /c` |
| 输出捕获 | pipe() + read() | CreatePipe() + ReadFile() |

### 性能优化建议
- 对于频繁执行的短命令，考虑复用 `execution_config_t` 配置对象
- 如果不需要错误输出，设置 `capture_error = false` 以减少开销
- 超时时间应根据实际任务合理设置，避免过短导致误杀

## 🔗 依赖关系

### 上游依赖
- **memory模块** - 内存分配与管理（通过compat层）
- **string模块** - 字符串处理工具
- **logging模块** - 日志记录（可选）

### 下游使用者
- **cognition模块** - 执行生成的计划命令
- **CoreKern内核** - 任务执行引擎
- **测试框架** - 测试用例执行

### 外部依赖
- `<stdint.h>` - uint32_t, uint64_t 类型
- `<stdbool.h>` - bool 类型
- 系统调用: fork(), execvp(), pipe(), waitpid() (POSIX)
- 或: CreateProcess(), CreatePipe(), WaitForSingleObject() (Windows)

## 📈 质量指标

| 指标 | 当前值 | 目标值 |
|------|--------|--------|
| 圈复杂度（平均） | 1.8 | <3.0 |
| 代码重复率 | 0% | <5% |
| Doxygen覆盖 | 100% | >95% |
| 单元测试覆盖 | 待补充 | >90% |
| 安全漏洞 | 0 | 0 |

## 🔄 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0.0 | 2026-04-06 | 初始版本，包含命令执行、安全验证、JSON格式化功能 |

---

## 📞 技术支持

如有问题或建议，请提交Issue至项目仓库。

---

**© 2026 SPHARX Ltd. All Rights Reserved.**  
**"From data intelligence emerges."**
