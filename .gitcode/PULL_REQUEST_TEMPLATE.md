# Pull Request Template

<!-- 
感谢贡献 AgentOS！提交前请完成检查清单。
Based on ARCHITECTURAL_PRINCIPLES.md V1.8
-->

---

## 1. 模块选择

<details>
<summary>选择相关模块（点击展开）</summary>

- [ ] **内核模块** - atoms/corekern, atoms/coreloopthree, atoms/syscall, atomslite/*
- [ ] **安全模块** - cupolas, daemon/* (sanitizer/permission/auditor/workbench)
- [ ] **基础库** - commons/*, gateway, heapstore
- [ ] **SDK 工具** - toolkit/python, toolkit/go, toolkit/rust, toolkit/typescript
- [ ] **测试脚本** - tests/*, scripts/*
- [ ] **文档配置** - manuals/*, manager/*, .github/*, CI/CD workflows, CMakeLists.txt

</details>

---

## 2. 检查清单

<details>
<summary>完成检查项（点击展开）</summary>

### 代码质量
- [ ] 代码遵循编码规范（C/C++11-17, Python 3.10+, Go 1.21+, Rust 1.70+）
- [ ] 代码已通过 clang-format/clang-tidy 格式化
- [ ] 新增公共 API 有完整 Doxygen 注释
- [ ] 无编译器警告
- [ ] 命名遵循规范：`agentos_verb_noun()`, `agentos_type_t`, `AGENTOS_CONSTANT`

### 测试
- [ ] 新增代码有单元测试（目标 ≥90% 覆盖率）
- [ ] 所有现有测试通过
- [ ] 测试覆盖率未下降
- [ ] 性能优化提供基准测试对比

### 文档
- [ ] 更新相关文档（README、API 文档、架构文档）
- [ ] 新增功能有使用示例
- [ ] 更新 CHANGELOG.md
- [ ] 错误消息格式：`[ERROR] CODE: message (context). Suggestion:`

### 安全与工程
- [ ] 安全内生：输入净化、权限检查、沙箱隔离
- [ ] 资源确定性：内存分配/释放配对、文件句柄关闭
- [ ] 错误可追溯：错误链模式含上下文
- [ ] 跨平台兼容：Linux、macOS、Windows 验证

### 其他
- [ ] PR 描述清晰
- [ ] 关联 Issue（格式：Closes #123, Fixes #456）
- [ ] 分支命名规范：`feature/xxx`, `bugfix/xxx`, `refactor/xxx`
- [ ] 无敏感数据泄露
- [ ] 提交消息规范：`type(scope): description`

</details>

---

## 3. PR 描述

### 变更类型
- [ ] Bug 修复
- [ ] 新功能
- [ ] 性能优化
- [ ] 代码重构
- [ ] 文档更新
- [ ] 安全增强
- [ ] 架构改进

### 一句话摘要
<!-- 用一句话概括本 PR 的核心内容 -->



### 问题陈述
<!-- 本 PR 解决什么问题？为什么需要这个变更？ -->



### 解决方案
<!-- 如何实现解决方案？采用的技术方案是什么？ -->



### 关键设计决策
1. 
2. 
3. 

### 相关 Issue
<!-- Closes #123, Fixes #456, Related #789 -->



### 破坏性变更
- [ ] 无破坏性变更
- [ ] 有破坏性变更（请在下方描述迁移路径）:



---

## 4. 架构原则检查

<details>
<summary>五维正交系统检查（点击展开）</summary>

基于 **ARCHITECTURAL_PRINCIPLES.md V1.8**

### 系统观 (S-1 ~ S-4)
- [ ] S-1 反馈闭环
- [ ] S-2 层次分解
- [ ] S-3 总体设计部
- [ ] S-4 涌现性管理

### 内核观 (K-1 ~ K-4)
- [ ] K-1 内核极简
- [ ] K-2 接口契约化
- [ ] K-3 服务隔离
- [ ] K-4 可插拔策略

### 认知观 (C-1 ~ C-4)
- [ ] C-1 双系统协同
- [ ] C-2 增量演化
- [ ] C-3 记忆卷载
- [ ] C-4 遗忘机制

### 工程观 (E-1 ~ E-8)
- [ ] E-1 安全内生
- [ ] E-2 可观测性
- [ ] E-3 资源确定性
- [ ] E-4 跨平台一致性
- [ ] E-5 命名语义化
- [ ] E-6 错误可追溯
- [ ] E-7 文档即代码
- [ ] E-8 可测试性

### 设计美学 (A-1 ~ A-4)
- [ ] A-1 简约至上
- [ ] A-2 极致细节
- [ ] A-3 人文关怀
- [ ] A-4 完美主义

**总检查原则数**: __/24  
**关键原则**: 至少每个维度一个原则

</details>

---

## 5. 测试说明

### 手动测试步骤
1. 环境准备: 
2. 测试步骤: 
3. 预期结果: 

### 自动化测试
- [ ] 单元测试：`ctest --test-dir build -R <test_name>`
- [ ] 集成测试：`ctest --test-dir build -I <integration_test>`
- [ ] 契约测试：`./tests/contract/test_<module>_contract`

### 测试环境
- 操作系统: 
- 编译器: 
- 构建类型: 
- 依赖版本: 

---

## 6. 重要提示

- 维护者将在 48 小时内审查
- 所有 CI/CD 流水线必须通过
- 符合 Apache-2.0 许可证
- 需要 DCO 签署

---

**再次感谢您的贡献！**

> *"From data intelligence emerges."*  
> *始于数据，终于智能。*

---

**PR 状态:** Draft / Ready for Review / In Review / Approved / Merged  
**审查者:**   
**合并日期:**   
**PR 编号:** #___

---

🎉
Thank you again for your contribution!
Intelligence emergence, and nothing less, is the ultimate sublimation of AI.
🎉
再次感谢您的贡献！
让我们一起见证AI智能涌现的极尽升华时刻。
