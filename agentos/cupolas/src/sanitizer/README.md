# Sanitizer — 输入清洗器

`cupolas/src/sanitizer/` 是输入校验与注入防护引擎，对所有进入系统的外部数据进行安全清洗。

## 设计目标

- **多层级校验**：从格式、类型、语义多个维度校验输入
- **注入防护**：全面防护 XSS、SQL 注入、命令注入、路径遍历等攻击
- **可扩展规则**：支持自定义校验规则和清洗策略
- **标准化输出**：清洗后的数据格式统一，便于下游处理

## 防护能力

| 防护类型 | 说明 |
|----------|------|
| **XSS 防护** | 检测和转义 HTML/JavaScript 注入代码 |
| **SQL 注入防护** | 检测 SQL 关键字和恶意 SQL 片段 |
| **命令注入防护** | 检测 Shell 命令注入字符 |
| **路径遍历防护** | 检测 `../` 等路径遍历攻击 |
| **类型校验** | 验证输入类型、长度、范围等基本属性 |

## 规则引擎

```
输入数据 → 规则链（按优先级排序）
           ├─ 白名单规则（允许的字符/模式）
           ├─ 正则规则（匹配危险模式）
           ├─ 长度规则（边界检查）
           └─ 类型规则（类型/格式校验）
               ↓
          通过 → 输出清洗后数据
          拒绝 → 返回错误并记录审计日志
```

## 使用示例

```c
#include "cupolas/src/sanitizer/sanitizer.h"

// 创建清洗器
sanitizer_t* sanitizer = sanitizer_create();

// 配置校验规则
sanitizer_add_rule(sanitizer, SANITIZER_RULE_XSS, SANITIZER_ACTION_BLOCK);
sanitizer_add_rule(sanitizer, SANITIZER_RULE_SQL_INJECTION, SANITIZER_ACTION_BLOCK);
sanitizer_add_rule(sanitizer, SANITIZER_RULE_PATH_TRAVERSAL, SANITIZER_ACTION_SANITIZE);

// 执行输入清洗
const char* user_input = "<script>alert('xss')</script>";
sanitizer_result_t result = sanitizer_clean(sanitizer, user_input, INPUT_TYPE_HTML);

if (result.action == SANITIZER_ACTION_PASS) {
    printf("清洗后: %s", result.cleaned_data);
} else if (result.action == SANITIZER_ACTION_BLOCK) {
    printf("输入被拒绝: %s", result.reason);
}
```

## 配置选项

```json
{
    "sanitizer": {
        "xss": {
            "enabled": true,
            "mode": "escape",
            "allowed_tags": ["p", "br", "b", "i"]
        },
        "sql_injection": {
            "enabled": true,
            "mode": "block"
        },
        "path_traversal": {
            "enabled": true,
            "max_depth": 3
        }
    }
}
```

---

*AgentOS Cupolas — Sanitizer*
