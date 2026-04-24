# 开发辅助脚本

`scripts/development/`

## 概述

`development/` 目录提供面向 AgentOS 开发者的辅助工具和脚本，涵盖代码格式化、静态分析、依赖管理、开发环境配置等环节，旨在提升开发效率和代码质量。

## 脚本列表

| 脚本 | 说明 |
|------|------|
| `setup_dev_env.sh` | 一键配置开发环境 |
| `format_code.sh` | 代码格式化（C/C++/Python/Go/Rust） |
| `lint_check.sh` | 静态代码检查 |
| `update_deps.sh` | 依赖更新与管理 |

## 使用示例

```bash
# 配置开发环境
./development/setup_dev_env.sh

# 格式化所有代码
./development/format_code.sh --all

# 代码检查
./development/lint_check.sh --strict

# 更新依赖
./development/update_deps.sh --check-only
```

## 开发环境要求

| 工具 | 版本 | 用途 |
|------|------|------|
| GCC/G++ | ≥ 11.0 | C 内核编译 |
| CMake | ≥ 3.20 | 构建系统 |
| Python | ≥ 3.10 | Python SDK |
| Go | ≥ 1.21 | Go SDK |
| Rust | ≥ 1.75 | Rust SDK |
| Node.js | ≥ 18.0 | TypeScript SDK |

---

© 2026 SPHARX Ltd. All Rights Reserved.
