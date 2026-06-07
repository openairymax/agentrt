# Manager Sanitizer

AgentOS Manager 模块的代码安全检测规则配置。

## 文件

| 文件 | 说明 |
|------|------|
| `sanitizer_rules.json` | 代码安全检测规则集（基于 OWASP TOP 10） |
| `lsan-suppressions` | LeakSanitizer 内存泄漏抑制规则（第三方库已知误报） |
| `valgrind-suppressions` | Valgrind 内存检测抑制规则（第三方库已知误报） |

## 说明

此模块统一管理 AgentOS 的代码安全检测规则与内存工具抑制规则。

## 版本

v0.1.0