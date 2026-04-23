# Security — 安全防护模块

`cupolas/src/security/` 提供核心安全防护能力，包括文件扫描、API 防护、行为分析和环境检测。

## 核心能力

| 能力 | 说明 |
|------|------|
| **文件扫描** | 对上传文件和系统文件进行安全扫描 |
| **API 防护** | 检测和阻止 API 层面的攻击行为 |
| **行为分析** | 基于规则的运行时行为异常检测 |
| **环境检测** | 检测运行环境的安全状态 |
| **报告生成** | 生成安全事件报告 |

## 引擎架构

```
输入 → 预处理 → 规则匹配 → 风险评估 → 处置决策 → 输出
       ↓          ↓          ↓           ↓
   规范化    规则引擎    评分模型     放行/阻断/告警
```

## 扫描规则配置

```json
{
    "file_scan": {
        "max_size": "100MB",
        "allowed_types": ["pdf", "docx", "txt"],
        "scan_engines": ["clamav", "yara"],
        "action_on_threat": "quarantine"
    },
    "api_protection": {
        "rate_limit": 1000,
        "detect_sql_injection": true,
        "detect_xss": true,
        "detect_path_traversal": true
    }
}
```

## 使用示例

```c
#include "cupolas/src/security/security.h"

// 初始化安全引擎
security_engine_t* engine = security_engine_create();

// 文件扫描
security_scan_result_t result = security_scan_file(engine, "/path/to/file.pdf");
if (result.threat_detected) {
    printf("威胁检测: %s (严重度: %d)", result.threat_name, result.severity);
    security_quarantine_file(engine, "/path/to/file.pdf");
}

// API 请求检查
bool is_safe = security_check_request(engine, request);
if (!is_safe) {
    security_block_request(engine, request);
}
```

---

*AgentOS Cupolas — 安全防护*
