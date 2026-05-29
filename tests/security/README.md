# 安全测试

`tests/security/`

## 概述

`security/` 目录包含 AgentOS 的安全测试，共 **9 个文件**，涵盖 C 层安全审计和 Python 层模糊测试、SAST/DAST 扫描、输入净化、权限检查和沙箱隔离。

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| tests/security/ 目录 | 对应的 agentos/ 模块 | 测试内容 |
|---------------------|---------------------|----------|
| `c/` | `agentos/cupolas/` | C 层安全审计测试（CMocka），SEC-017 合规验证 |
| `python/fuzz_framework.py` | `agentos/cupolas/sanitizer/` | 模糊测试框架（输入清洗器测试） |
| `python/sast_dast_scanner.py` | `agentos/cupolas/security/` | SAST/DAST 静态/动态扫描 |
| `python/test_input_sanitizer.py` | `agentos/cupolas/sanitizer/` | 输入净化测试（XSS/SQL 注入/命令注入/路径遍历） |
| `python/test_permissions.py` | `agentos/cupolas/permission/` | 权限检查测试（RBAC+ABAC 双模型） |
| `python/test_sandbox.py` | `agentos/daemon/common/` | 沙箱隔离测试 |

## 目录结构

```
security/                          # 共 9 个文件
├── README.md                      # 本文档
├── c/                             # C 安全审计测试（CMocka，3 个文件）
│   ├── CMakeLists.txt
│   ├── test_security_audit.c      # 安全审计套件
│   └── test_sec017_compliance.c   # SEC-017 桩函数合规验证
└── python/                        # Python 安全测试（pytest，6 个文件）
    ├── __init__.py
    ├── fuzz_framework.py          # 模糊测试框架
    ├── sast_dast_scanner.py       # SAST/DAST 扫描器
    ├── test_input_sanitizer.py    # 输入净化测试
    ├── test_permissions.py        # 权限检查测试
    ├── test_sandbox.py            # 沙箱隔离测试
    └── ...
```

## 运行方式

```bash
# C 安全测试
cd build && ctest -R security

# Python 安全测试
pytest tests/security/python/ -v -m security
```

## 安全测试覆盖范围

| 安全领域 | 对应的 agentos/ 模块 | 测试项目 |
|---------|---------------------|----------|
| **输入验证** | `cupolas/sanitizer/` | XSS、SQL 注入、命令注入、路径遍历 |
| **权限控制** | `cupolas/permission/` | RBAC 角色权限、ABAC 属性策略 |
| **审计追踪** | `cupolas/audit/` | HMAC 签名链、事件完整性 |
| **沙箱隔离** | `daemon/common/` | 执行环境隔离、资源限制 |
| **代码扫描** | 全部模块 | SAST 静态分析、DAST 动态检测 |

---

© 2026 SPHARX Ltd. All Rights Reserved.