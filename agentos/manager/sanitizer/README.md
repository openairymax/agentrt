# Manager Sanitizer

AgentOS Manager 模块的代码安全检测规则配置。

## 文件

| 文件 | 说明 |
|------|------|
| `sanitizer_rules.json` | 代码安全检测规则集（基于 OWASP TOP 10） |

## 说明

此模块管理代码级的安全检测规则，与根目录的 `.lsan-suppressions` / `.valgrind-suppressions`（C/C++ 内存工具抑制规则）功能不同。

## 版本

v0.1.0