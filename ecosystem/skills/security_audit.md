# Security Audit Skill

> **版本**: 1.0.0 | **类别**: security | **标签**: security, audit, compliance, vulnerability

## 概述

对系统配置、代码、网络资产进行安全审计，识别潜在威胁和合规风险。

## 审计类型

| 类型 | 说明 |
|------|------|
| 配置审计 (config) | 检查安全配置是否符合最佳实践（Docker、Nginx、K8s 等） |
| 依赖审计 (dependencies) | 检查第三方依赖的已知 CVE 漏洞 |
| 权限审计 (permissions) | 检查文件/目录权限、容器运行用户等 |
| 网络审计 (network) | 检查开放端口、TLS 配置、不安全的协议 |
| 合规审计 (compliance) | 检查是否符合 OWASP/NIST/ISO27001/GDPR 等标准 |

## 输入参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|:---:|------|------|
| target | string | ✅ | — | 审计目标内容（代码、配置、日志等） |
| audit_type | string | ❌ | auto | 审计类型，auto 自动检测 |
| framework | string | ❌ | owasp | 合规框架 (owasp/nist/iso27001/dengbao/gdpr) |
| severity_threshold | string | ❌ | low | 最低报告严重级别 |

## 输出格式

```json
{
  "audit_type": "config",
  "framework": "owasp",
  "risk_score": 45.0,
  "risk_level": "medium",
  "findings": [
    {
      "id": "qs-1",
      "severity": "critical",
      "category": "config",
      "title": "Hardcoded password in configuration",
      "description": "Pattern detected: Hardcoded password in configuration",
      "remediation": "Review and remediate the identified issue"
    }
  ],
  "summary": "Security audit (config): MEDIUM risk (score 45.0/100), 1 critical, 2 high",
  "recommendations": [
    "Use environment variables for secrets",
    "Use SHA-256 or stronger hashing algorithm"
  ]
}
```

## 风险评分

- 基于发现问题的严重级别加权计算：
  - critical: 10 | high: 6 | medium: 3 | low: 1 | info: 0.5
- 归一化到 0-100 分
- 风险等级：critical(>=80) / high(>=50) / medium(>=25) / low(>=10) / info(<10)

## 使用示例

```python
from ecosystem.skills import SecurityAuditSkill
import asyncio

skill = SecurityAuditSkill()
result = asyncio.run(skill.execute({
    "target": 'password = "admin123"\nDEBUG = True\nALLOWED_HOSTS = ["*"]',
    "audit_type": "config",
    "framework": "owasp"
}))
print(result["risk_level"])  # "high"
print(len(result["findings"]))  # 3
```

## 实现文件

- [security_audit.py](security_audit.py) — SkillPlugin 实现