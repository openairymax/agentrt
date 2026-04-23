# OpenLab Core — 开放生态核心

`openlab/openlab/` 是 OpenLab 生态系统的核心管理模块，提供生态系统的注册、发现和管理能力。

## 核心职责

- **组件注册**：管理和维护生态系统中所有组件的注册信息
- **服务发现**：提供组件查找和能力匹配服务
- **生命周期管理**：管理组件从注册到注销的完整生命周期
- **依赖管理**：组件间的依赖关系管理和冲突检测

## 核心功能

| 功能 | 说明 |
|------|------|
| 应用注册 | 注册新的应用到生态系统 |
| 应用发现 | 按类别、能力、标签搜索应用 |
| 组件依赖 | 管理组件依赖关系和版本约束 |
| 运行时管理 | 管理组件的启用、禁用状态 |

## 使用方式

```python
from openlab import OpenLabRegistry

registry = OpenLabRegistry()

# 注册应用
registry.register_app(
    name="my-agent",
    version="1.0.0",
    capabilities=["text_generation", "code_review"],
    entry_point="agent.py"
)

# 发现应用
results = registry.discover(capability="text_generation")
for app in results:
    print(f"Found: {app.name} v{app.version}")
```

---

*AgentOS OpenLab — Core*
