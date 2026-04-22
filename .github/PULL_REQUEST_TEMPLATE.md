# AgentOS Pull Request

## 基本信息

| 项目 | 内容 |
|------|------|
| **类型** | [ ] Bug修复 [ ] 新功能 [ ] 重构 [ ] 性能优化 [ ] 文档 [ ] 安全 |
| **优先级** | [ ] P0紧急 [ ] P1高 [ ] P2中 [ ] P3低 |
| **关联Issue** | Closes #___ |

## 变更摘要

> 一句话描述本PR的核心变更

**变更前**:

**变更后**:

## 变更文件

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| | 新增/修改/删除 | |

## 质量检查

- [ ] 编译通过 (`-Wall -Wextra` 无警告)
- [ ] 新增公共API有Doxygen注释
- [ ] 错误处理遵循 `[ERROR] CODE: message (context)` 格式
- [ ] 无敏感信息泄露
- [ ] 遵循命名规范: 函数`agentos_verb_noun()` / 类型`agentos_type_t` / 常量`AGENTOS_CONSTANT`

## 测试

- [ ] 单元测试通过
- [ ] 手动验证通过

```bash
# 验证命令
cmake -B build -DBUILD_TESTS=ON && cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## 破坏性变更

- [ ] 无破坏性变更
- [ ] 有破坏性变更 (请说明迁移方案)

---

**审查者**: @___  
**质量门禁**: ✅/❌
