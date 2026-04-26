# AgentOS Pull Request / 拉取请求

感谢您的贡献！请填写以下信息以帮助我们审查您的变更。  
Thank you for contributing to AgentOS! Please fill in the following to help us review your changes.

## 描述 / Description

<!-- 简要描述此 PR 的作用和原因 -->
<!-- Briefly describe what this PR does and why -->

## 变更类型 / Type of Change

- [ ] 修复 Bug / Bug fix
- [ ] 新功能 / New feature
- [ ] 破坏性变更 / Breaking change
- [ ] 文档更新 / Documentation update
- [ ] 重构 / Refactoring
- [ ] 测试改进 / Test improvement

## 关联 Issue / Related Issues

<!-- 使用 `Closes #123` 或 `Fixes #123` 关联相关 Issue -->
<!-- Link any related issues using `Closes #123` or `Fixes #123` -->

## 测试 / Testing

<!-- 描述您如何测试这些变更 -->
<!-- Describe how you tested these changes -->

- [ ] 我已添加/更新测试 / I have added/updated tests
- [ ] 所有现有测试通过 / All existing tests pass

### 构建验证 / Build Verification

请确认构建通过 / Please confirm your build passes:

```bash
# C 核心 / C core
cmake -B AgentOS-build && cmake --build AgentOS-build && ctest --test-dir AgentOS-build

# SDK（如适用）/ SDK (as applicable)
cd toolkit/python && python -m pytest
cd toolkit/rust && cargo test
cd toolkit/go && go test ./...
cd toolkit/typescript && npx tsc --noEmit
```

## 检查清单 / Checklist

- [ ] 我的代码遵循项目的编码规范 / My code follows the project's coding style
- [ ] 我已在必要时添加代码注释 / I have commented my code where necessary
- [ ] 我已对文档作出相应更新（如适用）/ I have made corresponding changes to the documentation (if applicable)
- [ ] 我的变更不会产生新的警告 / My changes generate no new warnings

## 补充说明 / Additional Notes

<!-- 任何其他上下文、截图或审查者需要了解的信息 -->
<!-- Any other context, screenshots, or information reviewers should know -->
