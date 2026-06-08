# Manager Sanitizer — 安全检测与内存工具配置

**模块路径**: `agentos/manager/sanitizer/`
**版本**: v0.0.5

## 概述

`manager/sanitizer/` 统一管理 AgentOS 的输入安全检测规则和内存检测工具抑制规则。安全检测规则基于 OWASP TOP 10，覆盖 XSS、SQL 注入、命令注入、路径遍历、PII 泄露、提示注入和恶意代码等 7 大攻击类别共 25 条规则。内存工具抑制规则用于消除第三方库在 LeakSanitizer 和 Valgrind 检测中的已知误报。

## 目录结构

```
sanitizer/
├── sanitizer_rules.json         # 输入安全检测规则集（25 条规则，7 大类别）
├── lsan-suppressions            # LeakSanitizer 内存泄漏抑制规则
└── valgrind-suppressions        # Valgrind 内存检测抑制规则
```

## 核心组件

### sanitizer_rules.json

输入安全检测规则集，遵循 `schema/sanitizer-rules.schema.json` 规范，按双重责任模型归属 cupolas 模块。

**规则类别**：

| 类别 | 规则数 | 严重程度 | 说明 |
|------|--------|---------|------|
| **XSS** | 6 | critical/high | 跨站脚本攻击防护（script 标签、事件处理器、iframe、javascript 协议、data URI） |
| **SQL 注入** | 3 | critical/high | SQL 注入检测（UNION SELECT、绕过模式、注释注入） |
| **路径遍历** | 2 | critical | 路径遍历攻击防护（../、编码形式） |
| **命令注入** | 3 | critical/high | 命令注入检测（管道、命令替换、危险命令） |
| **PII** | 4 | medium/high | 个人信息脱敏（信用卡、SSN、电话、邮箱） |
| **敏感数据** | 2 | critical | API 密钥和密码字段掩码 |
| **提示注入** | 4 | critical/high | AI 提示注入检测（忽略指令、角色扮演、系统消息模拟、越狱模式） |
| **恶意代码** | 2 | critical/high | 恶意代码特征检测（危险函数、混淆代码） |

**规则处理类型**：

| 类型 | 说明 |
|------|------|
| `block` | 阻止请求，替换为占位符 |
| `sanitize` | 脱敏处理，掩码敏感数据 |
| `warn` | 记录告警，不阻止请求 |

**全局设置**：

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `max_input_length` | 100000 | 最大输入长度 |
| `max_recursion_depth` | 10 | 最大递归深度 |
| `timeout_ms` | 1000 | 检测超时 |
| `fail_closed` | true | 检测失败时默认阻止 |

### lsan-suppressions

LeakSanitizer 内存泄漏抑制规则，用于消除第三方库（如 cjson、libmicrohttpd）的已知误报。

### valgrind-suppressions

Valgrind 内存检测抑制规则，用于消除第三方库在 Valgrind 检测中的已知误报。

## 使用说明

```bash
# 运行 Valgrind 内存检测（使用抑制规则）
valgrind --suppressions=sanitizer/valgrind-suppressions ./build/agentos

# 运行 LeakSanitizer（使用抑制规则）
LSAN_OPTIONS=suppressions=sanitizer/lsan-suppressions ./build/agentos
```

## 依赖关系

| 组件 | 用途 |
|------|------|
| `schema/sanitizer-rules.schema.json` | 规则格式校验 |
| cupolas 模块 | 内容定义责任方 |
| Valgrind | 内存检测工具 |
| LeakSanitizer | 内存泄漏检测 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
