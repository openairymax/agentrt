# Execution — 命令执行引擎

`commons/utils/execution/` 提供安全、可观测的命令执行环境，支持跨平台命令执行、安全校验和结果格式化。

## 设计目标

- **安全执行**：命令注入防护、参数白名单、路径安全检查
- **跨平台**：统一 API 屏蔽 Windows/Linux/MacOS 的命令执行差异
- **可观测**：执行日志、超时控制、资源限制
- **结果结构化**：标准化的执行结果格式，包含退出码、输出和错误信息

## 核心功能

| 功能 | 说明 |
|------|------|
| 命令执行 | 执行系统命令并捕获输出 |
| 安全校验 | 命令注入检测、参数白名单、路径合法性检查 |
| 超时控制 | 可配置的执行超时，超时自动终止 |
| 结果格式化 | 统一结果结构（exit_code, stdout, stderr, duration） |
| 工作目录 | 支持指定执行工作目录 |
| 环境变量 | 支持自定义环境变量传递 |

## 使用示例

```python
from execution import CommandExecutor

executor = CommandExecutor()

# 基本命令执行
result = executor.run("ls -la /tmp")
print(f"Exit: {result.exit_code}")
print(f"Output: {result.stdout}")

# 带超时的执行
try:
    result = executor.run("slow_command", timeout=30)
except TimeoutError:
    print("Command timed out")

# 安全执行（自动过滤危险命令）
result = executor.run_safe("git status", allowed_commands=["git", "python"])
```

## 安全校验规则

| 规则 | 说明 |
|------|------|
| 命令白名单 | 只允许执行白名单中的命令 |
| 参数校验 | 拒绝包含危险字符（`;`, `|`, `` ` ``, `$()`）的参数 |
| 路径安全检查 | 拒绝执行相对路径或包含 `..` 的命令 |
| 资源限制 | 可配置最大内存和 CPU 使用时间 |

## 平台差异

| 特性 | Linux | Windows | MacOS |
|------|-------|---------|-------|
| Shell | /bin/sh | cmd.exe | /bin/zsh |
| 路径分隔符 | / | \\ | / |
| 环境变量 | $VAR | %VAR% | $VAR |
| 默认编码 | UTF-8 | GBK/UTF-8 | UTF-8 |

---

*AgentOS Commons Utils — Execution*
