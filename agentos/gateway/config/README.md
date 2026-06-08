# Gateway Config — 静态分析配置

**模块路径**: `agentos/gateway/config/`
**版本**: v0.0.5

## 概述

`gateway/config/` 包含 Gateway 模块的静态分析配置文件，用于 Cppcheck 代码质量检查。配置定义了代码复杂度限制、函数长度约束和已知模式的抑制规则，确保网关代码符合质量标准。

## 目录结构

```
config/
└── cppcheck.cfg          # Cppcheck 静态分析规则配置
```

## 核心组件

### cppcheck.cfg

Cppcheck 静态分析规则配置，包含以下约束：

| 配置项 | 值 | 说明 |
|--------|-----|------|
| C 标准 | C11 | 遵循 C11 标准进行代码检查 |
| 平台 | unix64 | 目标平台为 64 位 Unix |
| 最大圈复杂度 | 10 | 函数圈复杂度上限 |
| 最大函数长度 | 100 行 | 单函数代码行数上限 |
| 最大参数数量 | 8 | 函数参数数量上限 |
| 最大嵌套深度 | 5 | 代码嵌套层级上限 |

**抑制规则**：

| 抑制项 | 说明 |
|--------|------|
| `missingIncludeSystem` | 忽略系统头文件路径缺失 |
| `unusedFunction:gateway_http_create` | 允许公共 API 函数未引用 |
| `unusedFunction:gateway_ws_create` | 允许公共 API 函数未引用 |
| `unusedFunction:gateway_stdio_create` | 允许公共 API 函数未引用 |

**启用的检查级别**：warning / style / performance / information / portability

## 使用说明

```bash
# 通过 CMake 运行 Cppcheck
make cppcheck

# 手动运行（指定配置文件）
cppcheck --config-file=config/cppcheck.cfg src/
```

## 依赖关系

| 组件 | 用途 |
|------|------|
| Cppcheck | C/C++ 静态分析工具 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
