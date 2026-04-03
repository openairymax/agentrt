# Pull Request Template / Pull Request 模板

<!-- 
Thank you for contributing to AgentOS! Before submitting your PR, please ensure you have completed the following checks.
感谢您为 AgentOS 项目贡献代码！在提交 PR 之前，请确保您已经完成以下检查。

This template is designed based on ARCHITECTURAL_PRINCIPLES.md (V1.8) - the Five-Dimensional Orthogonal System.
本模板基于 ARCHITECTURAL_PRINCIPLES.md (V1.8) 五维正交系统设计。
-->

## 📋 Checklist / 检查清单

### 🔧 Code Quality / 代码质量 (E-5, E-6, A-1, A-4)

- [ ] Code follows project coding standards (C/C++11-17, Python 3.10+, Go 1.21+, Rust 1.70+) / 代码遵循项目的编码规范（C/C++11-17, Python 3.10+, Go 1.21+, Rust 1.70+）
- [ ] Code has been formatted with clang-format/clang-tidy / 代码已通过 clang-format/clang-tidy 格式化检查（`--dry-run --Werror`）
- [ ] New public APIs have complete Doxygen documentation including `@since`, `@deprecated`, `@threadsafe`, `@ownership` / 新增的公共 API 都有完整的 Doxygen 注释（包含 `@since`、`@deprecated`、`@threadsafe`、`@ownership`）
- [ ] Code does not introduce new compiler warnings (`-Wall -Wextra -Werror`) / 代码没有引入新的编译器警告（使用 `-Wall -Wextra -Werror`）
- [ ] Naming follows semantic conventions: `agentos_verb_noun()`, `agentos_type_t`, `AGENTOS_CONSTANT` / 命名遵循语义规范：`agentos_verb_noun()`、`agentos_type_t`、`AGENTOS_CONSTANT`
- [ ] No dead code or unused variables detected / 无死代码或未使用的变量被检测到

### 🧪 Testing / 测试 (E-8, C-2, E-3)

- [ ] New code has corresponding unit tests (targeting ≥90% coverage) / 新增代码有相应的单元测试（目标 ≥90% 覆盖率）
- [ ] All existing tests still pass (unit + integration) / 所有现有测试仍然通过（单元测试 + 集成测试）
- [ ] Test coverage has not decreased (verify with `gcov` or `lcov`) / 测试覆盖率没有下降（用 `gcov` 或 `lcov` 验证）
- [ ] For performance optimizations, provide benchmark results with before/after comparison / 如果是性能优化，请提供基准测试结果及前后对比
- [ ] Tests run in isolated environment (no external network/filesystem/LLM dependencies) / 测试在隔离环境中运行（无外部网络/文件系统/LLM 依赖）
- [ ] Contract tests pass for new interfaces (tests/contract/) / 新接口的契约测试通过（tests/contract/）

### 📚 Documentation / 文档 (E-7, A-3)

- [ ] Updated relevant documentation (README, API docs, architecture docs) / 更新了相关的文档（README、API 文档、架构文档）
- [ ] New features have usage examples or tutorials in `examples/` / 新增的功能有使用示例或教程在 `examples/` 目录中
- [ ] Updated CHANGELOG.md following Keep a Changelog format / 更新了 CHANGELOG.md（遵循 Keep a Changelog 格式）
- [ ] Updated ARCHITECTURAL_PRINCIPLES.md if design principles changed / 如果设计原则有变更，已更新 ARCHITECTURAL_PRINCIPLES.md
- [ ] Error messages follow standard format: `[ERROR] CODE: message (context). Suggestion:` / 错误消息遵循标准格式：`[ERROR] CODE: message (context). Suggestion:`

### 🛡️ Security & Engineering / 安全与工程 (E-1, E-2, E-3, E-4, K-3)

- [ ] **Security Built-in**: Input validation via sanitizer/, permission checks via permission/, sandbox isolation via workbench/ / **安全内生**：输入净化（sanitizer/）、权限检查（permission/）、沙箱隔离（workbench/）
- [ ] **Resource Determinism**: Memory allocation/free pairs verified, file handles closed, threads joined / **资源确定性**：内存分配/释放配对验证、文件句柄关闭、线程加入
- [ ] **Error Traceability**: Errors use error chain pattern with module/function/line context / **错误可追溯**：错误使用错误链模式并包含模块/函数/行号上下文
- [ ] **Observability**: Added metrics/traces/logs for key paths using commons/logger.h and agentos_sys_telemetry_* APIs / **可观测性**：关键路径添加指标/追踪/日志（使用 commons/logger.h 和 agentos_sys_telemetry_* API）
- [ ] **Cross-Platform Compatibility**: Verified on Linux (GCC), macOS (Clang), Windows (MSVC) / **跨平台兼容性**：已在 Linux (GCC)、macOS (Clang)、Windows (MSVC) 上验证
- [ ] **Service Isolation**: Changes respect daemon boundaries, no direct cross-daemon communication / **服务隔离**：变更尊重守护进程边界，无直接跨守护进程通信

### 📝 Other / 其他

- [ ] PR description is clear, explaining the problem, solution, and impact / PR 描述清晰，说明问题、解决方案和影响
- [ ] Linked relevant Issues (format: Closes #123, Fixes #456, Related #789) / 关联相关的 Issue（格式：Closes #123, Fixes #456, Related #789）
- [ ] Branch name follows conventions: `feature/xxx`, `bugfix/xxx`, `refactor/xxx`, `docs/xxx`, `security/xxx` / 分支名称符合规范：`feature/xxx`、`bugfix/xxx`、`refactor/xxx`、`docs/xxx`、`security/xxx`
- [ ] No sensitive data exposed (API keys, passwords, tokens, private keys in code) / 未泄露敏感数据（API 密钥、密码、令牌、私钥）
- [ ] Commit messages follow conventional commits format: `type(scope): description` / 提交消息遵循约定式提交格式：`type(scope): description`

---

## 📖 PR Description / PR 描述

### Change Type / 变更类型
<!-- Please select one / 请选择一项 -->
- [ ] **Bug Fix** / Bug 修复 - 修复已知问题或回归缺陷
- [ ] **New Feature** / 新功能 - 添加新的功能或能力
- [ ] **Performance Optimization** / 性能优化 - 改进性能或资源利用率
- [ ] **Code Refactoring** / 代码重构 - 改善代码结构而不改变行为
- [ ] **Documentation Update** / 文档更新 - 改进或补充文档
- [ ] **Security Enhancement** / 安全增强 - 加强安全防护措施
- [ ] **Architecture Improvement** / 架构改进 - 优化系统架构设计
- [ ] **Other** / 其他（请说明）

### Change Summary / 变更摘要
<!-- One sentence summary of what this PR does -->
<!-- 用一句话概括本 PR 的内容 -->


### Problem Statement / 问题陈述
<!-- What problem does this PR solve? Why is this change needed? -->
<!-- 本 PR 解决什么问题？为什么需要这个变更？ -->


### Solution Approach / 解决方案
<!-- How did you implement the solution? What is the technical approach? -->
<!-- 您如何实现解决方案？采用的技术方案是什么？ -->

**Key Design Decisions / 关键设计决策:**
1. 
2. 
3. 

### Architecture Decision Record / 架构决策记录
<!-- If this PR involves significant architectural changes, reference or create an ADR -->
<!-- 如果涉及重大架构变更，请引用或创建 ADR -->

- [ ] ADR created/referenced / ADR 已创建或引用: ADR-XXX
- [ ] Impact on kernel interfaces / 对内核接口的影响:
- [ ] Impact on syscalls / 对系统调用的影响:
- [ ] Impact on daemon services / 对守护进程服务的影响:

### Related Issues / 相关 Issue
<!-- Link related issues using format: Closes #123, Fixes #456, Related #789 -->
<!-- 关联相关的 Issue，例如：Closes #123, Fixes #456, Related #789 -->


### Breaking Changes / 破坏性变更
<!-- Does this PR introduce any breaking changes? If yes, describe migration path -->
<!-- 本 PR 是否引入破坏性变更？如果是，请描述迁移路径 -->

- [ ] No breaking changes / 无破坏性变更
- [ ] **Breaking changes** (describe below) / **破坏性变更**（请在下方描述）:


### Migration Guide / 迁移指南
<!-- If breaking changes, provide step-by-step migration instructions -->
<!-- 如有破坏性变更，请提供分步迁移指南 -->


---

## 🏗️ Architecture Principles Compliance / 架构原则符合性

Based on **ARCHITECTURAL_PRINCIPLES.md V1.8** Five-Dimensional Orthogonal System.
基于 **ARCHITECTURAL_PRINCIPLES.md V1.8** 五维正交系统。

### Dimension 1: System View / 维度一：系统观 (S-1 ~ S-4)
<!-- Control Theory & Systems Engineering / 控制论与系统工程 -->

| Principle | Applied? | Evidence / 证据 |
|-----------|----------|----------------|
| **S-1: Feedback Loop** / 反馈闭环 | [ ] | Feedback mechanism for this change: |
| **S-2: Hierarchical Decomposition** / 层次分解 | [ ] | Layer(s) affected: |
| **S-3: Overall Design Department** / 总体设计部 | [ ] | Decision layer vs execution layer separation: |
| **S-4: Emergence Management** / 涌现性管理 | [ ] | Positive/negative emergence addressed: |

### Dimension 2: Kernel View / 维度二：内核观 (K-1 ~ K-4)
<!-- Microkernel Philosophy / 微内核哲学 -->

| Principle | Applied? | Evidence / 证据 |
|-----------|----------|----------------|
| **K-1: Kernel Minimalism** / 内核极简 | [ ] | Corekern interface changes (if any): |
| **K-2: Interface Contract** / 接口契约化 | [ ] | New contracts defined with @since/@deprecated: |
| **K-3: Service Isolation** / 服务隔离 | [ ] | Daemon boundary respected: |
| **K-4: Pluggable Strategy** / 可插拔策略 | [ ] | Strategy pattern used (if applicable): |

### Dimension 3: Cognitive View / 维度三：认知观 (C-1 ~ C-4)
<!-- Dual-System Cognitive Theory / 双系统认知理论 -->

| Principle | Applied? | Evidence / 证据 |
|-----------|----------|----------------|
| **C-1: Dual-System Synergy** / 双系统协同 | [ ] | Fast/slow path design: |
| **C-2: Incremental Evolution** / 增量演化 | [ ] | Incremental planning approach: |
| **C-3: Memory Rovol** / 记忆卷载 | [ ] | Memory layer interaction (if any): |
| **C-4: Forgetting Mechanism** / 遗忘机制 | [ ] | Data lifecycle management: |

### Dimension 4: Engineering View / 维度四：工程观 (E-1 ~ E-8)
<!-- Engineering Two Theory / 工程两论 -->

| Principle | Applied? | Evidence / 证据 |
|-----------|----------|----------------|
| **E-1: Security Built-in** / 安全内生 | [ ] | Sanitizer/Permission/Audit applied: |
| **E-2: Observability** / 可观测性 | [ ] | Metrics/Traces/Logs added: |
| **E-3: Resource Determinism** / 资源确定性 | [ ] | Resource ownership clear: |
| **E-4: Cross-Platform Consistency** / 跨平台一致性 | [ ] | Platform abstraction used: |
| **E-5: Naming Semantics** / 命名语义化 | [ ] | Naming follows convention: |
| **E-6: Error Traceability** / 错误可追溯 | [ ] | Error chain pattern used: |
| **E-7: Documentation as Code** / 文档即代码 | [ ] | Docs updated and versioned: |
| **E-8: Testability** / 可测试性 | [ ] | Test strategy documented: |

### Dimension 5: Aesthetic View / 维度五：设计美学 (A-1 ~ A-4)
<!-- Jobs' Design Philosophy / 乔布斯设计哲学 -->

| Principle | Applied? | Evidence / 证据 |
|-----------|----------|----------------|
| **A-1: Simplicity** / 简约至上 | [ ] | Interface complexity minimized: |
| **A-2: Extreme Details** / 极致细节 | [ ] | Edge cases handled: |
| **A-3: Human Care** / 人文关怀 | [ ] | Developer experience improved: |
| **A-4: Perfectionism** / 完美主义 | [ ] | Zero warnings, full coverage: |

### Principles Summary / 原则总结
**Total principles checked: / 总原则数:** __/24  
**Critical principles (must pass): / 关键原则（必须通过）:**  
- [ ] At least one principle from each dimension / 至少每个维度一个原则

---

## 🧪 Testing Instructions / 测试说明

### Manual Test Steps / 手动测试步骤
<!-- Step-by-step instructions for manual testing -->
<!-- 手动测试的分步指南 -->

1. **Setup / 环境**: 
2. **Steps / 步骤**: 
3. **Expected Result / 预期结果**: 

### Automated Tests / 自动化测试
<!-- List of automated test cases that should pass -->
<!-- 应该通过的自动化测试用例列表 -->

- [ ] Unit tests: `ctest --test-dir build -R <test_name>`
- [ ] Integration tests: `ctest --test-dir build -I <integration_test>`
- [ ] Contract tests: `./tests/contract/test_<module>_contract`

### Test Environment / 测试环境
<!-- Specify the test environment configuration -->
<!-- 指定测试环境配置 -->

- **OS / 操作系统**: Linux / macOS / Windows
- **Compiler / 编译器**: GCC X.X / Clang X.X / MSVC 202X
- **Build Type / 构建类型**: Debug / Release
- **Dependencies / 依赖版本**: 

### Performance Baseline / 性能基线
<!-- For performance-related PRs, provide benchmarks -->
<!-- 与性能相关的 PR，请提供基准测试 -->

```
Before / 优化前: 
After / 优化后: 
Improvement / 提升: 
```

---

## 📊 Screenshots/Demo / 截图或演示

<!-- If applicable, provide screenshots, screen recordings, or performance graphs -->
<!-- 如果适用，请提供截图、录屏或性能图表 -->



---

## ⚠️ Important Notes / 重要提示

### Code Review Process / 代码审查流程
1. Maintainers will review within 48 hours / 维护者将在 48 小时内审查
2. Address all review comments promptly / 及时处理所有审查意见
3. Re-request review after addressing comments / 处理意见后重新请求审查
4. All CI/CD pipelines must be green / 所有 CI/CD 流水线必须通过

### Merge Strategy / 合并策略
- **Feature branches** (`feature/*`): Squash and merge / 使用 squash and merge
- **Bug fixes** (`bugfix/*`): Create merge commit / 创建 merge commit
- **Refactoring** (`refactor/*`): Squash and merge / 使用 squash and merge
- **Documentation** (`docs/*`): Squash and merge / 使用 squash and merge
- **Security** (`security/*`): Requires 2 approvals / 需要 2 个审批

### License Compliance / 许可证合规
- ✅ Contribution complies with Apache-2.0 License / 贡献符合 Apache-2.0 许可证
- ✅ No proprietary code without proper license / 无无适当许可的专有代码
- ✅ DCO sign-off required (Developer Certificate of Origin) / 需要 DCO 签署（开发者原始证书）

### Post-Merge Actions / 合并后操作
- [ ] Update issue status / 更新 Issue 状态
- [ ] Notify stakeholders / 通知相关方
- [ ] Monitor production metrics (if applicable) / 监控生产指标（如适用）

---

## 🙏 Acknowledgments / 致谢

**Thank you again for your contribution!** Your effort helps make AgentOS better.  
**再次感谢您的贡献！** 您的努力让 AgentOS 更加优秀。

> *"From data intelligence emerges."*  
> *始于数据，终于智能。*

---

**PR Status / PR 状态:** Draft / Ready for Review / In Review / Approved / Merged  
**Reviewer / 审查者:**  
**Merge Date / 合并日期:**  
