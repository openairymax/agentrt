# Pull Request Template / Pull Request 模板

<!-- 
Thank you for contributing to AgentOS! Before submitting your PR, please ensure you have completed the following checks.
感谢您为 AgentOS 项目贡献代码！在提交 PR 之前，请确保您已经完成以下检查。
-->

## Checklist / 检查清单

### Code Quality / 代码质量
- [ ] Code follows project coding standards (C/C++/Python, etc.) / 代码遵循项目的编码规范（C/C++/Python 等）
- [ ] Code has been formatted with clang-format/clang-tidy / 代码已通过 clang-format/clang-tidy 格式化检查
- [ ] New public APIs have complete Doxygen documentation / 新增的公共 API 都有完整的 Doxygen 注释
- [ ] Code does not introduce new compiler warnings / 代码没有引入新的编译器警告
- [ ] Follows ARCHITECTURAL_PRINCIPLES.md guidelines / 遵循 ARCHITECTURAL_PRINCIPLES.md 中的架构原则

### Testing / 测试
- [ ] New code has corresponding unit tests / 新增代码有相应的单元测试
- [ ] All existing tests still pass / 所有现有测试仍然通过
- [ ] Test coverage has not decreased / 测试覆盖率没有下降
- [ ] For performance optimizations, provide benchmark results / 如果是性能优化，请提供基准测试结果
- [ ] Tests run in isolated environment (no external dependencies) / 测试在隔离环境中运行（无外部依赖）

### Documentation / 文档
- [ ] Updated relevant documentation (README, API docs, etc.) / 更新了相关的文档（README、API 文档等）
- [ ] New features have usage examples or tutorials / 新增的功能有使用示例或教程
- [ ] Updated CHANGELOG.md (if applicable) / 更新了 CHANGELOG.md（如果适用）
- [ ] Updated ARCHITECTURAL_PRINCIPLES.md if design principles changed / 如果设计原则有变更，已更新 ARCHITECTURAL_PRINCIPLES.md

### Security & Engineering / 安全与工程
- [ ] Security is built-in (input validation, permission checks, etc.) / 安全内生（输入验证、权限检查等）
- [ ] Resource lifecycle is deterministic (memory, file handles, threads) / 资源生命周期确定（内存、文件句柄、线程）
- [ ] Error handling is complete and traceable / 错误处理完整且可追溯
- [ ] Observability is complete (metrics, traces, logs) / 可观测性完整（指标、追踪、日志）
- [ ] Cross-platform compatibility verified (Linux/macOS/Windows) / 已验证跨平台兼容性（Linux/macOS/Windows）

### Other / 其他
- [ ] PR description is clear, explaining the reason and content of changes / PR 描述清晰，说明了变更的原因和内容
- [ ] Linked relevant Issues (format: Closes #123) / 关联了相关的 Issue（格式：Closes #123）
- [ ] Branch name follows conventions (feature/xxx, bugfix/xxx, etc.) / 分支名称符合规范（feature/xxx, bugfix/xxx 等）
- [ ] No sensitive data exposed (keys, passwords, etc.) / 未泄露敏感数据（密钥、密码等）

## PR Description / PR 描述

### Change Type / 变更类型
<!-- Please select one / 请选择一项 -->
- [ ] Bug Fix / Bug 修复
- [ ] New Feature / 新功能
- [ ] Performance Optimization / 性能优化
- [ ] Code Refactoring / 代码重构
- [ ] Documentation Update / 文档更新
- [ ] Security Enhancement / 安全增强
- [ ] Other (please specify) / 其他（请说明）

### Change Description / 变更说明
<!-- Clearly and concisely describe the changes in this PR. Explain the problem being solved and the solution approach. -->
<!-- 清晰简洁地描述本次 PR 的变更内容。说明要解决的问题和解决方案。 -->


### Related Issues / 相关 Issue
<!-- Link related issues using format: Closes #123, Fixes #456 -->
<!-- 关联相关的 Issue，例如：Closes #123, Fixes #456 -->


### Testing Instructions / 测试说明
<!-- Describe how to test these changes. Include test cases, expected output, and any special requirements. -->
<!-- 描述如何测试这个变更。包括测试用例、预期输出和任何特殊要求。 -->


### Architecture Principles Compliance / 架构原则符合性
<!-- Explain how this PR aligns with the ARCHITECTURAL_PRINCIPLES.md. Reference specific principles if applicable. -->
<!-- 说明本次变更如何符合 ARCHITECTURAL_PRINCIPLES.md。如适用，请引用具体原则。 -->

**Applied Principles / 应用的原则:**
- [ ] S-1: Feedback Loop / 反馈闭环
- [ ] S-2: Hierarchical Decomposition / 层次分解
- [ ] S-3: Overall Design Department / 总体设计部
- [ ] S-4: Emergence Management / 涌现性管理
- [ ] K-1: Kernel Minimalism / 内核极简
- [ ] K-2: Interface Contract / 接口契约化
- [ ] K-3: Service Isolation / 服务隔离
- [ ] K-4: Pluggable Strategy / 可插拔策略
- [ ] C-1: Dual-System Synergy / 双系统协同
- [ ] C-2: Incremental Evolution / 增量演化
- [ ] C-3: Memory Rovol / 记忆卷载
- [ ] C-4: Forgetting Mechanism / 遗忘机制
- [ ] E-1: Security Built-in / 安全内生
- [ ] E-2: Observability / 可观测性
- [ ] E-3: Resource Determinism / 资源确定性
- [ ] E-4: Cross-Platform Consistency / 跨平台一致性
- [ ] E-5: Naming Semantics / 命名语义化
- [ ] E-6: Error Traceability / 错误可追溯
- [ ] E-7: Documentation as Code / 文档即代码
- [ ] E-8: Testability / 可测试性
- [ ] A-1: Simplicity / 简约至上
- [ ] A-2: Extreme Details / 极致细节
- [ ] A-3: Human Care / 人文关怀
- [ ] A-4: Perfectionism / 完美主义

### Screenshots/Logs / 截图或日志
<!-- If applicable, provide screenshots, logs, or performance benchmarks -->
<!-- 如果适用，请提供截图、日志或性能基准测试数据 -->


## Important Notes / 重要提示

### Code Review / 代码审查
- Maintainers may suggest modifications. Please respond promptly. / 维护者可能会提出修改建议，请及时响应。
- All code must pass CI/CD pipelines before merging. / 所有代码必须在合并前通过 CI/CD 流水线。

### Merge Strategy / 合并策略
- Feature branches: squash and merge / 功能分支：使用 squash and merge
- Bug fixes: merge commit / Bug 修复：使用 merge commit
- Documentation updates: squash and merge / 文档更新：使用 squash and merge

### License Compliance / 许可证合规
- Confirm your contribution complies with Apache-2.0 License / 确认您的贡献符合 Apache-2.0 许可证
- No proprietary code or third-party code without proper license / 无专有代码或无适当许可的第三方代码

---

**Thank you again for your contribution!** 🎉  
**再次感谢您的贡献！** 🎉

<!-- 
Remember: "From data intelligence emerges." 
记住："From data intelligence emerges."（始于数据，终于智能）
-->
