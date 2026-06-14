# Manager Skill — 技能注册表配置

**模块路径**: `ecosystem/manager/skill/`
**版本**: v0.1.0

## 概述

`manager/skill/` 包含 AgentOS 的技能注册表配置，定义系统可用的技能列表及其参数。技能是 Agent 可调用的原子能力单元，注册表描述了每个技能的输入/输出规范、权限要求和资源消耗。配置遵循 `schema/skill-registry.schema.json` 规范。

## 目录结构

```
skill/
└── registry.yaml      # 技能注册表定义
```

## 核心组件

### registry.yaml

技能注册表，每个技能包含以下字段：

| 字段 | 说明 |
|------|------|
| `skill_id` | 技能唯一标识符 |
| `name` | 技能名称 |
| `description` | 技能描述 |
| `input_schema` | 输入参数 JSON Schema |
| `output_schema` | 输出格式 JSON Schema |
| `required_permissions` | 执行所需权限 |
| `estimated_tokens` | 预估 Token 消耗 |
| `timeout_ms` | 执行超时时间 |
| `category` | 技能分类 |

## 依赖关系

| 组件 | 用途 |
|------|------|
| `schema/skill-registry.schema.json` | 注册表格式校验 |
| PyYAML | YAML 配置解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
