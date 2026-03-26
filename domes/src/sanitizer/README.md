# Domes 输入净化模块 (Sanitizer)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`sanitizer/` 是 Domes 安全沙箱的**输入验证和过滤层**，防止恶意输入和注入攻击。

核心功能：
- **规则过滤**: 基于正则表达式的模式匹配
- **风险评级**: 根据严重程度分级处理
- **替换净化**: 支持删除、转义、替换等多种策略
- **缓存加速**: 常用规则缓存提升性能

**重要**: 所有用户输入、Agent 输出、网络数据都必须经过净化模块检查。

---

## 📁 目录结构

```
sanitizer/
├── sanitizer.h               # 统一头文件
├── sanitizer_core.c          # 核心净化引擎
├── sanitizer_rules.c         # 规则管理
├── sanitizer_cache.c         # 规则缓存
└── sanitizer_cache.h         # 缓存接口
```

---

## 🔧 核心功能详解

### 1. 净化引擎 (`sanitizer_core.c`)

执行输入检查和净化的核心引擎。

#### 处理流程

```
+-------------+      +-------------+      +-------------+
|  Raw Input  | ---> |   Matcher   | ---> |   Action    |
|  (原始输入)  |      | (规则匹配)  |      | (删除/替换) |
+-------------+      +-------------+      +-------------+
                              ↓
                    +-------------+
                    |   Report    |
                    | (风险评估)  |
                    +-------------+
```

#### 使用示例

```c
#include <sanitizer.h>

// 初始化净化器
sanitizer_t* sanitizer = sanitizer_create(
    "./config/sanitizer_rules.json"  // 规则文件
);

// === 示例 1: XSS 防护 ===
const char* user_input = "<script>alert('xss')</script>Hello";

sanitizer_result_t* result = sanitizer_sanitize(
    sanitizer,
    user_input,
    SANITIZER_CONTEXT_HTML  // HTML 上下文
);

if (result->risk_level >= RISK_HIGH) {
    // 高风险，直接拒绝
    LOG_WARNING("Blocked malicious input: %s", user_input);
    sanitizer_result_free(result);
    return ERROR_INPUT_UNSAFE;
}

// 获取净化后的文本
const char* cleaned = result->cleaned_text;
// cleaned = "alert('xss')Hello" (script 标签被移除)

// === 示例 2: SQL 注入防护 ===
const char* sql_input = "'; DROP TABLE users; --";

result = sanitizer_sanitize(
    sanitizer,
    sql_input,
    SANITIZER_CONTEXT_SQL
);

if (result->action == SANITIZER_ACTION_BLOCK) {
    // 检测到 SQL 注入，阻止查询
    LOG_ERROR("SQL injection attempt detected");
}

// === 示例 3: 命令注入防护 ===
const char* cmd_input = "ls -la; rm -rf /";

result = sanitizer_sanitize(
    sanitizer,
    cmd_input,
    SANITIZER_CONTEXT_SHELL
);

// 检测到危险命令组合，阻止执行
```

### 2. 规则管理 (`sanitizer_rules.c`)

定义和管理净化规则。

#### 规则文件格式

```json
{
  "version": "1.0",
  "rules": [
    {
      "rule_id": "XSS_SCRIPT_001",
      "name": "Script Tag Injection",
      "type": "block",
      "pattern": "<script[^>]*>.*?</script>",
      "flags": "i",
      "severity": "critical",
      "category": "xss",
      "contexts": ["html", "markdown"],
      "action": "remove",
      "description": "Remove all script tags"
    },
    {
      "rule_id": "SQL_DROP_001",
      "name": "SQL DROP Statement",
      "type": "block",
      "pattern": "\\b(DROP|DELETE|TRUNCATE)\\s+(TABLE|DATABASE)",
      "flags": "i",
      "severity": "critical",
      "category": "sql_injection",
      "contexts": ["sql"],
      "action": "block"
    },
    {
      "rule_id": "CMD_DANGEROUS_001",
      "name": "Dangerous Shell Commands",
      "type": "block",
      "pattern": "(rm\\s+-rf|chmod\\s+777|wget\\s+.*\\|.*sh)",
      "flags": "",
      "severity": "critical",
      "category": "command_injection",
      "contexts": ["shell"],
      "action": "block"
    },
    {
      "rule_id": "PATH_TRAVERSAL_001",
      "name": "Path Traversal",
      "type": "sanitize",
      "pattern": "\\.\\./",
      "flags": "g",
      "severity": "high",
      "category": "path_traversal",
      "contexts": ["file_path"],
      "action": "replace",
      "replacement": ""
    }
  ]
}
```

#### 风险等级定义

```c
typedef enum {
    RISK_LOW = 0,      // 信息级，记录即可
    RISK_MEDIUM = 1,   // 警告级，需要关注
    RISK_HIGH = 2,     // 危险级，需要阻止
    RISK_CRITICAL = 3  // 严重级，立即阻止并告警
} risk_level_t;
```

#### 净化动作类型

```c
typedef enum {
    SANITIZER_ACTION_NONE = 0,     // 无操作（仅检测）
    SANITIZER_ACTION_REMOVE = 1,   // 删除匹配内容
    SANITIZER_ACTION_REPLACE = 2,  // 替换为指定文本
    SANITIZER_ACTION_ESCAPE = 3,   // HTML/XML 转义
    SANITIZER_ACTION_BLOCK = 4     // 阻止整个输入
} sanitizer_action_t;
```

### 3. 上下文感知

根据不同场景应用不同的规则集。

```c
typedef enum {
    SANITIZER_CONTEXT_GENERIC = 0,  // 通用场景
    SANITIZER_CONTEXT_HTML = 1,     // HTML 内容
    SANITIZER_CONTEXT_SQL = 2,      // SQL 查询
    SANITIZER_CONTEXT_SHELL = 3,    // Shell 命令
    SANITIZER_CONTEXT_FILE_PATH = 4,// 文件路径
    SANITIZER_CONTEXT_URL = 5,      // URL 参数
    SANITIZER_CONTEXT_JSON = 6      // JSON 数据
} sanitizer_context_t;
```

#### 使用示例

```c
// HTML 上下文：过滤 XSS
result = sanitizer_sanitize(sanitizer, 
    user_html, SANITIZER_CONTEXT_HTML);

// SQL 上下文：防止注入
result = sanitizer_sanitize(sanitizer, 
    sql_value, SANITIZER_CONTEXT_SQL);

// Shell 上下文：阻止命令注入
result = sanitizer_sanitize(sanitizer, 
    shell_arg, SANITIZER_CONTEXT_SHELL);
```

---

## 🚀 快速开始

### 编译

```bash
cd AgentOS/domes
mkdir build && cd build
cmake ..
make sanitizer  # 只编译 sanitizer 模块
```

### 测试

```bash
cd build
ctest -R sanitizer         # 运行 sanitizer 测试
./test_sanitizer_core      # 直接运行测试
```

### 完整集成示例

```c
#include <domes.h>

int main() {
    // 1. 初始化 domes（包含 sanitizer）
    domes_config_t config = {
        .sanitizer_enabled = true,
        .sanitizer_rules_path = "./config/sanitizer_rules.json",
        .sanitizer_default_action = SANITIZER_ACTION_BLOCK,
        .sanitizer_cache_enabled = true
    };
    
    domes_t* domes = domes_init(&config);
    
    // 2. Agent 输入净化示例
    const char* agent_input = get_agent_output();
    
    sanitizer_result_t* result = domes_sanitizer_check(
        domes,
        agent_input,
        SANITIZER_CONTEXT_GENERIC
    );
    
    if (result->risk_level >= RISK_HIGH) {
        // 高风险输入，阻止处理
        LOG_ERROR("Unsafe agent output blocked: %s", 
            result->matched_pattern);
        
        // 记录审计日志
        domes_audit(domes, AUDIT_LEVEL_WARNING,
            "sanitizer.blocked", "agent-123",
            "unsafe_output", agent_input);
            
        sanitizer_result_free(result);
        return ERROR_UNSAFE_OUTPUT;
    }
    
    // 3. 使用净化后的文本
    const char* safe_text = result->cleaned_text;
    process_safe_text(safe_text);
    
    // 4. 自定义规则检查
    sanitizer_rule_t custom_rule = {
        .rule_id = "CUSTOM_001",
        .pattern = "\\b(top_secret)\\b",
        .action = SANITIZER_ACTION_REPLACE,
        .replacement = "[REDACTED]",
        .severity = RISK_HIGH
    };
    
    domes_sanitizer_add_rule(domes, &custom_rule);
    
    // 5. 清理
    sanitizer_result_free(result);
    domes_destroy(domes);
    return 0;
}
```

---

## 📊 性能基准

### 净化性能

| 指标 | 数值 | 测试条件 |
|------|------|----------|
| 单次检查延迟 | ~10μs | 100 条规则，1KB 输入 |
| 规则加载时间 | < 5ms | 1000 条规则 |
| 缓存命中率 | 85%~95% | 典型场景 |
| 内存占用 | ~1MB | 1000 条规则 |

### 规则匹配性能

| 规则数量 | 无缓存延迟 | 有缓存延迟 | 提升倍数 |
|----------|-----------|-----------|----------|
| 100 | ~5μs | < 1μs | 5x |
| 1000 | ~50μs | < 5μs | 10x |
| 10000 | ~500μs | < 50μs | 10x |

---

## 🛠️ 最佳实践

### 1. 规则设计原则

```json
// ✅ 推荐：精确的模式和明确的动作
{
  "rule_id": "XSS_EVENT_HANDLER",
  "pattern": "\\bon\\w+\\s*=\\s*['\"][^'\"]*['\"]",
  "severity": "high",
  "action": "remove",
  "description": "Remove inline event handlers"
}

// ❌ 不推荐：过于宽泛的模式
{
  "pattern": "<.*>",  // 会匹配所有 HTML 标签（误报率高）
  "action": "remove"
}
```

### 2. 多层防护策略

```c
// 第一层：基础净化（低风险容忍）
result1 = sanitizer_sanitize(sanitizer, 
    input, SANITIZER_CONTEXT_GENERIC);

if (result1->risk_level >= RISK_MEDIUM) {
    // 第二层：严格检查（中等风险容忍）
    result2 = sanitizer_sanitize(sanitizer,
        result1->cleaned_text, 
        SANITIZER_CONTEXT_HTML);
    
    if (result2->risk_level >= RISK_HIGH) {
        // 第三层：阻止输入
        return ERROR_UNSAFE;
    }
    
    use_text(result2->cleaned_text);
}
```

### 3. 自定义规则扩展示例

```c
// 添加业务特定的敏感词过滤
sanitizer_rule_t sensitive_word_rule = {
    .rule_id = "BUSINESS_SENSITIVE_001",
    .pattern = "\\b(confidential|proprietary)\\b",
    .action = SANITIZER_ACTION_REPLACE,
    .replacement = "[CONFIDENTIAL]",
    .severity = RISK_MEDIUM,
    .context = SANITIZER_CONTEXT_GENERIC
};

domes_sanitizer_add_rule(domes, &sensitive_word_rule);

// 添加特定格式验证（如邮箱）
sanitizer_rule_t email_validation = {
    .rule_id = "FORMAT_EMAIL_001",
    .pattern = "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$",
    .action = SANITIZER_ACTION_NONE,  // 仅验证，不修改
    .severity = RISK_LOW,
    .context = SANITIZER_CONTEXT_GENERIC
};
```

### 4. 性能优化技巧

```c
// 启用缓存加速常用规则
config.cache_enabled = true;
config.cache_size = 512;  // 512 条缓存

// 预编译常用规则
sanitizer_precompile_rules(sanitizer);

// 根据场景选择合适的上下文
// 避免在简单场景使用复杂上下文（减少不必要的检查）
if (is_simple_text(input)) {
    context = SANITIZER_CONTEXT_GENERIC;  // 快速路径
} else {
    context = SANITIZER_CONTEXT_HTML;     // 完整检查
}
```

---

## 🔍 常见问题

### Q1: 如何处理误报？

```c
// 方法 1: 调整规则优先级
rule.priority = 50;  // 降低优先级

// 方法 2: 添加白名单
if (is_trusted_source(input)) {
    skip_sanitization = true;
}

// 方法 3: 细化规则模式
// 从宽泛模式改为精确模式
```

### Q2: 如何平衡安全性和性能？

```c
// 分层检查策略
if (risk_level == RISK_LOW) {
    // 低风险：快速通过
    return cleaned_text;
} else if (risk_level == RISK_MEDIUM) {
    // 中风险：详细检查 + 缓存
    enable_cache = true;
} else {
    // 高风险：阻止并告警
    block_and_alert();
}
```

---

## 🔗 相关文档

- [Domes 总览](../README.md)
- [审计日志模块](../audit/README.md)
- [权限裁决模块](../permission/README.md)
- [虚拟工位模块](../workbench/README.md)
- [输入验证规范](../../../partdocs/specifications/input_validation.md)

---

## 🤝 贡献指南

欢迎提交 Issue 或 Pull Request！

## 📞 联系方式

- **维护者**: AgentOS 安全委员会
- **安全问题**: wangliren@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"Sanitize first, trust later. 净化优先，信任后置。"*
