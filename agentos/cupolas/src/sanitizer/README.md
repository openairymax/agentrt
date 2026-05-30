# Sanitizer — 输入清洗器

**模块路径**: `agentos/cupolas/src/sanitizer/`
**版本**: v0.0.5

## 概述

Sanitizer 模块提供全面的输入清洗和注入防护能力，是 Cupolas 安全穹顶的第一道防线。覆盖 XSS（跨站脚本）、SQL 注入、命令注入和路径遍历四种主要攻击向量，支持规则引擎和缓存机制，确保所有外部输入在进入系统前经过严格清洗。

## 设计目标

- **全面防护**：覆盖 XSS、SQL 注入、命令注入、路径遍历四大攻击向量
- **规则引擎**：可配置的清洗规则，支持自定义规则扩展
- **高性能缓存**：清洗结果缓存，避免重复计算
- **零误判**：严格区分恶意输入和合法输入，最小化误报
- **可扩展**：支持自定义清洗规则和回调函数

## 目录结构

```
sanitizer/
├── sanitizer.h                  # 清洗器公共接口
├── sanitizer_core.c             # 清洗核心引擎实现
├── sanitizer_rules.h            # 规则引擎接口
├── sanitizer_rules.c            # 规则管理实现
├── sanitizer_cache.h            # 清洗缓存接口
├── sanitizer_cache.c            # 缓存实现
└── README.md                    # 本文档
```

## 攻击向量与防护策略

### XSS 防护

| 检测项 | 说明 |
|--------|------|
| `<script>` 标签 | 检测并移除 `<script>...</script>` |
| 事件处理器 | 检测 `on*=` 属性（onclick, onerror 等） |
| `javascript:` 协议 | 检测 `href="javascript:..."` |
| `<iframe>` 标签 | 检测并移除 iframe 嵌入 |
| `<object>/<embed>` | 检测并移除插件嵌入 |
| CSS 表达式 | 检测 `expression()` 和 `url()` |

### SQL 注入防护

| 检测项 | 说明 |
|--------|------|
| 关键字检测 | `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `DROP`, `UNION`, `EXEC` |
| 注释绕过 | `--`, `/* */`, `#` |
| 布尔注入 | `OR 1=1`, `AND 1=1`, `' OR '` |
| 堆叠查询 | 分号分隔的多语句 |
| 编码绕过 | URL 编码、Unicode 编码、Hex 编码 |

### 命令注入防护

| 检测项 | 说明 |
|--------|------|
| 命令链接 | `;`, `&&`, `\|\|` |
| 命令替换 | `` `command` ``, `$(command)` |
| 管道操作 | `\|`, `|&` |
| 重定向 | `>`, `>>`, `<` |
| 危险命令 | `rm`, `chmod`, `chown`, `sudo`, `su`, `curl`, `wget` |

### 路径遍历防护

| 检测项 | 说明 |
|--------|------|
| 目录穿越 | `../`, `..\\` |
| 绝对路径 | `/etc/passwd`, `C:\\Windows\\` |
| 编码绕过 | `%2e%2e%2f`, `..%252f`, `..%c0%af` |
| 符号链接 | 检测 symlink 跟随 |
| NULL 字节 | `%00`, `\\0` 截断 |

## 接口说明

| 函数 | 说明 |
|------|------|
| `sanitizer_create()` | 创建清洗器实例 |
| `sanitizer_destroy(sanitizer)` | 销毁清洗器实例 |
| `sanitizer_sanitize(sanitizer, input, output, output_size, context)` | 执行清洗（返回 `sanitizer_result_t`） |
| `sanitizer_sanitize_xss(sanitizer, input, output, output_size)` | XSS 专用清洗 |
| `sanitizer_sanitize_sql(sanitizer, input, output, output_size)` | SQL 注入专用清洗 |
| `sanitizer_sanitize_command(sanitizer, input, output, output_size)` | 命令注入专用清洗 |
| `sanitizer_sanitize_path(sanitizer, input, output, output_size)` | 路径遍历专用清洗 |
| `sanitizer_add_rule(sanitizer, rule)` | 添加自定义规则 |
| `sanitizer_remove_rule(sanitizer, rule_id)` | 移除规则 |
| `sanitizer_clear_cache(sanitizer)` | 清除清洗缓存 |
| `sanitizer_get_stats(sanitizer, stats)` | 获取清洗统计 |

### sanitizer_result_t — 清洗结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `passed` | `bool` | 是否通过清洗 |
| `threats_detected` | `int` | 检测到的威胁数量 |
| `threat_types` | `sanitizer_threat_type_t *` | 威胁类型数组 |
| `sanitized_output` | `char *` | 清洗后的输出 |
| `sanitized_size` | `size_t` | 清洗输出大小 |

## 使用示例

```c
#include "sanitizer.h"

sanitizer_t *san = sanitizer_create();

char output[4096];
sanitizer_result_t result = sanitizer_sanitize(
    san, "<script>alert('xss')</script>", output, sizeof(output), NULL);

if (result.passed) {
    printf("Clean output: %s\n", output);
} else {
    printf("Threats detected: %d\n", result.threats_detected);
    for (int i = 0; i < result.threats_detected; i++) {
        printf("  Threat: %d\n", result.threat_types[i]);
    }
}

sanitizer_result_t sql_result = sanitizer_sanitize_sql(
    san, "1' OR '1'='1", output, sizeof(output));

sanitizer_result_t cmd_result = sanitizer_sanitize_command(
    san, "ls; rm -rf /", output, sizeof(output));

sanitizer_result_t path_result = sanitizer_sanitize_path(
    san, "../../../etc/passwd", output, sizeof(output));

sanitizer_destroy(san);
```

## 清洗缓存

| 参数 | 默认值 | 说明 |
|------|--------|------|
| max_entries | 1024 | 最大缓存条目数 |
| ttl_seconds | 300 | 缓存 TTL（秒） |
| eviction_policy | LRU | 淘汰策略 |

缓存命中时清洗时间 < 100ns，缓存未命中时取决于输入长度和规则数量。

## 相关子系统

| 子系统 | 关系 |
|--------|------|
| [Permission](../permission/README.md) | 清洗通过后进入权限检查 |
| [Audit](../audit/README.md) | 清洗拒绝事件记录审计日志 |
| [Workbench](../workbench/README.md) | 工作台执行前对命令进行清洗 |
| [Security](../security/README.md) | 安全引擎调用清洗器进行输入校验 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
