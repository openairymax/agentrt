# 安全测试

`tests/security/`

## 概述

`security/` 目录包含 AgentOS 的安全测试，共 **11 个文件**，涵盖 C 层安全审计、Cupolas 安全模糊测试和 Python 层模糊测试、SAST/DAST 扫描、输入净化、权限检查和沙箱隔离。

安全测试是 AgentOS 质量保障体系的关键组成部分，确保系统在面对恶意输入、权限越权、注入攻击等安全威胁时能够正确防护。测试覆盖以下安全领域：

- **输入验证**：XSS、SQL 注入、命令注入、路径遍历等攻击向量的防护
- **权限控制**：RBAC 角色权限 + ABAC 属性策略的双模型验证
- **审计追踪**：HMAC 签名链完整性、事件不可篡改性
- **沙箱隔离**：执行环境隔离、资源限制、逃逸防护
- **模糊测试**：通过随机/半随机输入发现边界条件漏洞
- **合规验证**：SEC-017 安全合规桩函数验证

> **版本**：v0.0.5

## 与 agentos/ 模块对应关系

| tests/security/ 目录 | 对应的 agentos/ 模块 | 测试内容 |
|---------------------|---------------------|----------|
| `c/test_security_audit.c` | `agentos/cupolas/audit/` | C 层安全审计套件（审计链完整性、事件签名验证） |
| `c/test_sec017_compliance.c` | `agentos/cupolas/` | SEC-017 桩函数合规验证（安全接口桩函数完整性） |
| `cupolas/fuzz_permission.c` | `agentos/cupolas/permission/` | 权限模糊测试（随机权限请求、越权检测） |
| `cupolas/fuzz_sanitizer.c` | `agentos/cupolas/sanitizer/` | 清洗器模糊测试（随机恶意输入、边界条件） |
| `python/fuzz_framework.py` | `agentos/cupolas/sanitizer/` | Python 模糊测试框架（输入清洗器自动化模糊测试） |
| `python/sast_dast_scanner.py` | `agentos/cupolas/security/` | SAST/DAST 静态/动态扫描器（代码安全扫描） |
| `python/test_input_sanitizer.py` | `agentos/cupolas/sanitizer/` | 输入净化测试（XSS/SQL 注入/命令注入/路径遍历） |
| `python/test_permissions.py` | `agentos/cupolas/permission/` | 权限检查测试（RBAC+ABAC 双模型） |
| `python/test_sandbox.py` | `agentos/daemon/common/` | 沙箱隔离测试（执行环境隔离/资源限制/逃逸防护） |

## 目录结构

```
security/                          # 共 11 个文件
├── README.md                      # 本文档
├── c/                             # C 安全审计测试（CMocka，3 个文件）
│   ├── CMakeLists.txt             #   C 安全测试构建配置
│   ├── test_security_audit.c      #   安全审计套件
│   │                              #     审计链完整性、HMAC 签名验证、事件不可篡改性
│   └── test_sec017_compliance.c   #   SEC-017 桩函数合规验证
│                                  #     安全接口桩函数完整性、返回值语义正确性
├── cupolas/                       # Cupolas 安全模糊测试（2 个文件）
│   ├── fuzz_permission.c          #   权限模糊测试
│   │                              #     随机权限请求生成、越权行为检测、角色边界验证
│   └── fuzz_sanitizer.c           #   清洗器模糊测试
│                                  #     随机恶意输入生成、边界条件探测、绕过尝试
└── python/                        # Python 安全测试（pytest，6 个文件）
    ├── __init__.py                #   Python 包初始化
    ├── fuzz_framework.py          #   模糊测试框架
    │                              #     自动化模糊测试引擎、输入变异策略、覆盖率引导
    ├── sast_dast_scanner.py       #   SAST/DAST 扫描器
    │                              #     静态代码分析、动态安全检测、漏洞报告生成
    ├── test_input_sanitizer.py    #   输入净化测试
    │                              #     XSS 防护、SQL 注入防护、命令注入防护、路径遍历防护
    ├── test_permissions.py        #   权限检查测试
    │                              #     RBAC 角色权限验证、ABAC 属性策略验证、越权检测
    └── test_sandbox.py            #   沙箱隔离测试
                                   #     执行环境隔离、资源限制、逃逸防护、文件系统隔离
```

## 测试框架说明

### C 安全审计测试（CMocka）

C 语言安全审计测试使用 CMocka 框架，通过 `CMakeLists.txt` 构建。测试重点验证 Cupolas 安全穹顶的 C 层实现，包括审计链完整性、HMAC 签名验证和 SEC-017 合规性。使用 `ctest` 运行，支持通过 `-R security` 过滤。

### C 模糊测试

C 语言模糊测试（`cupolas/fuzz_*.c`）使用自定义模糊测试引擎，通过生成大量随机/半随机输入来探测边界条件漏洞。模糊测试策略包括：

- **随机输入生成**：基于种子生成随机权限请求和恶意输入
- **边界条件探测**：针对整数溢出、缓冲区边界、空指针等场景
- **变异策略**：对已知合法输入进行位翻转、字节插入、字段交换等变异

### Python 安全测试（pytest）

Python 安全测试使用 pytest 框架，标记为 `@pytest.mark.security`。测试覆盖以下方面：

- **输入净化**：验证 `test_input_sanitizer.py` 对各类攻击向量的防护效果
- **权限控制**：验证 `test_permissions.py` 中 RBAC 和 ABAC 模型的正确性
- **沙箱隔离**：验证 `test_sandbox.py` 中执行环境隔离和资源限制
- **模糊测试框架**：`fuzz_framework.py` 提供可复用的模糊测试基础设施
- **SAST/DAST**：`sast_dast_scanner.py` 执行静态和动态安全扫描

## 运行方式

```bash
# C 安全测试（全部）
cd build && ctest -R security -V

# C 安全审计测试
cd build && ctest -R security_audit -V

# SEC-017 合规验证
cd build && ctest -R sec017 -V

# Python 安全测试（全部）
pytest tests/security/python/ -v -m security

# 输入净化测试
pytest tests/security/python/test_input_sanitizer.py -v

# 权限检查测试
pytest tests/security/python/test_permissions.py -v

# 沙箱隔离测试
pytest tests/security/python/test_sandbox.py -v

# 模糊测试框架
python tests/security/python/fuzz_framework.py

# SAST/DAST 扫描
python tests/security/python/sast_dast_scanner.py

# Cupolas 模糊测试（C）
cd build && ctest -R fuzz -V

# 使用统一入口
python tests/utils/python/run_tests.py --type security

# 并行运行
pytest tests/security/ -v -n auto -m security
```

## 安全测试覆盖范围

| 安全领域 | 对应的 agentos/ 模块 | 测试项目 | 测试文件 |
|---------|---------------------|----------|---------|
| **输入验证** | `cupolas/sanitizer/` | XSS、SQL 注入、命令注入、路径遍历 | `python/test_input_sanitizer.py` |
| **权限控制** | `cupolas/permission/` | RBAC 角色权限、ABAC 属性策略、越权检测 | `python/test_permissions.py` |
| **审计追踪** | `cupolas/audit/` | HMAC 签名链、事件完整性、不可篡改性 | `c/test_security_audit.c` |
| **沙箱隔离** | `daemon/common/` | 执行环境隔离、资源限制、逃逸防护 | `python/test_sandbox.py` |
| **代码扫描** | 全部模块 | SAST 静态分析、DAST 动态检测 | `python/sast_dast_scanner.py` |
| **模糊测试** | `cupolas/` | 权限模糊、清洗器模糊 | `cupolas/fuzz_permission.c`, `cupolas/fuzz_sanitizer.c`, `python/fuzz_framework.py` |
| **合规验证** | `cupolas/` | SEC-017 桩函数合规性 | `c/test_sec017_compliance.c` |

## 测试覆盖说明

| agentos/ 模块 | 安全测试文件 | 测试框架 | 覆盖范围 |
|--------------|------------|---------|---------|
| `cupolas/audit/` | `c/test_security_audit.c` | CMocka | 审计链完整性、HMAC 签名验证、事件不可篡改性 |
| `cupolas/permission/` | `python/test_permissions.py`, `cupolas/fuzz_permission.c` | pytest + C | RBAC/ABAC 权限模型、越权检测、模糊测试 |
| `cupolas/sanitizer/` | `python/test_input_sanitizer.py`, `cupolas/fuzz_sanitizer.c`, `python/fuzz_framework.py` | pytest + C | 输入净化、XSS/SQL/命令注入防护、模糊测试 |
| `cupolas/security/` | `python/sast_dast_scanner.py` | pytest | 静态代码分析、动态安全检测 |
| `cupolas/` (合规) | `c/test_sec017_compliance.c` | CMocka | SEC-017 安全接口桩函数合规性 |
| `daemon/common/` | `python/test_sandbox.py` | pytest | 沙箱隔离、资源限制、逃逸防护 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
