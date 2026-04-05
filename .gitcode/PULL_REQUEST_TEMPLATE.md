# 🚀 Pull Request Template

<!-- 
感谢您为 AgentOS 做出贡献！
Thank you for contributing to AgentOS!

📋 提交前请阅读：Please read before submitting:
- [贡献指南](../CONTRIBUTING.md) / Contributing Guide
- [架构原则](../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) / Architecture Principles (V1.8)
- [代码规范](../agentos/manuals/CODING_STANDARDS.md) / Coding Standards

💡 提示：使用 Ctrl+Enter 快速提交 PR！
Tip: Use Ctrl+Enter to submit PR quickly!
-->

---

## 📌 PR 概览 / PR Overview

### 基本信息 / Basic Information

| 项目 | 内容 | 说明 |
|------|------|------|
| **PR 类型** | - [ ] Bug 修复 🔧 | - [ ] 新功能 ✨ | - [ ] 性能优化 ⚡ | - [ ] 重构 ♻️ | - [ ] 文档 📝 | - [ ] 安全 🔒 | - [ ] 架构改进 🏗️ | | 选择最符合的类型 |
| **关联 Issue** | Closes #___, Fixes #___, Related #___ | 格式参考右侧示例 |
| **优先级** | - [ ] 紧急 (P0) | - [ ] 高 (P1) | - [ ] 中 (P2) | - [ ] 低 (P3) | | 根据影响范围选择 |
| **目标分支** | `main` / `develop` | 通常合并到 main |

### 一句话摘要 / One-line Summary

<!-- 用一句话概括本 PR 的核心价值 -->
<!-- Describe the core value of this PR in one sentence -->

> 示例：修复 daemon 层内存泄漏问题，提升系统稳定性 15%
> Example: Fix memory leak in daemon layer, improve system stability by 15%

---

## 🎯 变更详情 / Change Details

### 问题背景 / Problem Background

<!-- 这个变更解决了什么问题？为什么需要它？ -->
<!-- What problem does this change solve? Why is it needed? -->

**当前状态 / Current State:**
- 

**期望状态 / Expected State:**
- 

**影响范围 / Impact:**
- 用户数：-
- 性能影响：-
- 安全影响：-

### 解决方案 / Solution

<!-- 如何实现？采用什么技术方案？ -->
<!-- How is it implemented? What technical approach was used? -->

**技术方案 / Technical Approach:**

1. 
2. 
3. 

**关键设计决策 / Key Design Decisions:**

| 决策点 | 选择 | 原因 |
|--------|------|------|
| 方案A vs 方案B | | |
| 性能 vs 可读性 | | |
| 兼容性策略 | | |

---

## 📦 变更清单 / Change Checklist

### 模块选择 / Module Selection

<details>
<summary>📂 点击展开模块列表 / Click to expand module list</summary>

#### 核心模块 / Core Modules
- [ ] **内核层 / Kernel Layer**: agentos/atoms/corekern, agentos/atoms/coreloopthree, agentos/atoms/syscall, agentos/atomslite/*
- [ ] **安全层 / Security Layer**: agentos/cupolas/*, agentos/daemon/sanitizer, agentos/daemon/permission, agentos/daemon/auditor, agentos/daemon/workbench
- [ ] **服务层 / Service Layer**: agentos/daemon/gateway_d, agentos/daemon/llm_d, agentos/daemon/market_d, agentos/daemon/monit_d, agentos/daemon/sched_d, agentos/daemon/tool_d

#### 基础设施 / Infrastructure
- [ ] **基础库 / Common Libraries**: agentos/commons/*
- [ ] **网关 / Gateway**: agentos/gateway/*
- [ ] **存储 / Storage**: agentos/heapstore/*
- [ ] **配置 / Configuration**: agentos/manager/*

#### SDK 与工具 / SDK & Tools
- [ ] **Python SDK**: agentos/toolkit/python/*
- [ ] **Go SDK**: agentos/toolkit/go/*
- [ ] **Rust SDK**: agentos/toolkit/rust/*
- [ ] **TypeScript SDK**: agentos/toolkit/typescript/*
- [ ] **构建脚本 / Build Scripts**: scripts/*

#### 测试与文档 / Test & Documentation
- [ ] **测试套件 / Test Suite**: tests/*
- [ ] **文档 / Documentation**: agentos/manuals/*
- [ ] **CI/CD 配置**: .github/*, .gitcode/*, .gitee/*
- [ ] **构建配置 / Build Config**: CMakeLists.txt, vcpkg.json

</details>

### 质量检查 / Quality Checks

<details>
<summary>✅ 点击展开质量检查项 / Click to expand quality checks</summary>

#### 代码质量 / Code Quality
- [ ] **编码规范**: 遵循 C/C++11-17, Python 3.10+, Go 1.21+, Rust 1.70+ 规范
- [ ] **格式化**: 已运行 `clang-format` / `black` / `gofmt` / `rustfmt`
- [ ] **注释**: 新增公共 API 有完整 Doxygen 注释（含 @brief/@param/@return）
- [ ] **编译**: 无编译器警告 (`-Wall -Wextra -Werror`)
- [ ] **命名**: 遵循规范：
  - 函数: `agentos_verb_noun()`
  - 类型: `agentos_type_t`
  - 常量: `AGENTOS_CONSTANT`

#### 测试覆盖 / Test Coverage
- [ ] **单元测试**: 新增代码有单元测试（目标 ≥90% 覆盖率）
- [ ] **回归测试**: 所有现有测试通过
- [ ] **覆盖率**: 测试覆盖率未下降
- [ ] **性能测试**: 性能优化提供基准测试对比（before/after）

#### 文档更新 / Documentation Updates
- [ ] **API 文档**: 更新相关 API 文档
- [ ] **用户文档**: 更新 README、用户指南
- [ ] **架构文档**: 更新架构决策记录 (ADR)
- [ ] **CHANGELOG**: 更新 CHANGELOG.md
- [ ] **示例代码**: 新增功能有使用示例

#### 错误处理 / Error Handling
- [ ] **消息格式**: `[ERROR] CODE: message (context). Suggestion: <action>`
- [ ] **错误链**: 使用错误链模式，包含完整上下文
- [ ] **日志**: 结构化日志，便于排查

#### 安全与合规 / Security & Compliance
- [ ] **输入净化**: 所有外部输入已净化
- [ ] **权限检查**: 遵循最小权限原则
- [ ] **沙箱隔离**: 危险操作在沙箱中执行
- [ ] **敏感数据**: 无敏感数据泄露（密钥、密码等）

#### 工程实践 / Engineering Practices
- [ ] **资源管理**: 内存/文件句柄正确释放（RAII模式）
- [ ] **跨平台**: Linux/macOS/Windows 兼容性验证
- [ ] **分支命名**: `feature/xxx`, `bugfix/xxx`, `refactor/xxx`, `hotfix/xxx`
- [ ] **提交消息**: `type(scope): description` (遵循 Conventional Commits)

</details>

---

## 🏗️ 架构原则检查 / Architecture Principles Check

<details>
<summary>🔍 五维正交系统检查 / Five-Dimensional Orthogonal System Check</summary>

基于 **[ARCHITECTURAL_PRINCIPLES.md](../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) V1.8**

### 维度一：系统观 (System View)
- [ ] **S-1 反馈闭环**: 是否实现完整的感知-决策-执行-反馈循环？
- [ ] **S-2 层次分解**: 是否保持清晰的层次结构，无跨层依赖？
- [ ] **S-3 总体设计部**: 是否存在全局协调层，避免局部最优？
- [ ] **S-4 涌现性管理**: 是否抑制负面涌现，促进正面涌现？

### 维度二：内核观 (Kernel View)
- [ ] **K-1 内核极简**: 内核是否只保留原子机制，无业务逻辑？
- [ ] **K-2 接口契约化**: 公共接口是否有完整的契约定义和版本标记？
- [ ] **K-3 服务隔离**: 守护进程是否独立运行，通过 syscall 通信？
- [ ] **K-4 可插拔策略**: 策略是否可运行时替换？

### 维度三：认知观 (Cognition View)
- [ ] **C-1 双系统协同**: 是否实现快慢路径分离？
- [ ] **C-2 增量演化**: 是否支持增量规划，限制规划深度？
- [ ] **C-3 记忆卷载**: 记忆系统是否逐层提炼知识？
- [ ] **C-4 遗忘机制**: 是否有合理的遗忘策略？

### 维度四：工程观 (Engineering View)
- [ ] **E-1 安全内生**: 安全是否内嵌于每个环节？
- [ ] **E-2 可观测性**: 是否提供完整的指标、追踪、日志？
- [ ] **E-3 资源确定性**: 资源生命周期是否确定？
- [ ] **E-4 跨平台一致性**: 多平台行为是否一致？
- [ ] **E-5 命名语义化**: 名称是否精确表达语义？
- [ ] **E-6 错误可追溯**: 错误是否可追溯到根源？
- [ ] **E-7 文档即代码**: 文档是否与代码同步？
- [ ] **E-8 可测试性**: 测试是否是设计的首要考量？

### 维度五：设计美学 (Aesthetic View)
- [ ] **A-1 简约至上**: 是否用最少接口提供最大价值？
- [ ] **A-2 极致细节**: 边界情况是否处理完善？
- [ ] **A-3 人文关怀**: 开发者体验是否友好？
- [ ] **A-4 完美主义**: 是否追求极致品质？

**统计 / Statistics:**
- 已检查原则: __/24
- 符合原则: __/24
- 不符合原因:

</details>

---

## 🧪 测试说明 / Testing Instructions

### 手动测试步骤 / Manual Test Steps

```markdown
# 环境准备 / Environment Setup
- 操作系统: Ubuntu 22.04 LTS
- 编译器: GCC 11.4
- 构建类型: Release

# 测试步骤 / Test Steps
1. 克隆仓库并切换到本分支
   ```bash
   git checkout <branch-name>
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
   make -j$(nproc)
   ```

2. 运行特定测试
   ```bash
   ctest --test-dir build -R <test_name> --output-on-failure
   ```

3. 验证结果
   # 预期输出:
```

### 自动化测试 / Automated Tests

- [ ] **单元测试**: `ctest --test-dir build -R <test_name>`
- [ ] **集成测试**: `ctest --test-dir build -I <integration_test>`
- [ ] **契约测试**: `./tests/contract/test_<module>_contract`
- [ ] **性能测试**: `./tests/perf/benchmark_<module>`

### 测试环境信息 / Test Environment

| 项目 | 版本 |
|------|------|
| OS | Ubuntu 22.04 / macOS 14 / Windows 11 |
| Compiler | GCC 11.4 / Clang 18 / MSVC 2022 |
| CMake | 3.28+ |
| Python | 3.11+ |
| Go | 1.22+ |
| Rust | 1.77+ |

---

## ⚠️ 破坏性变更 / Breaking Changes

- [x] **无破坏性变更** / No breaking changes
- [ ] **有破坏性变更** / Has breaking changes

<details>
<summary>📋 迁移指南 / Migration Guide</summary>

### 受影响的 API / Affected APIs

```c
// Before / 修改前
int agentos_old_api(int param);

// After / 修改后
int agentos_new_api(int param, int new_option);
```

### 迁移步骤 / Migration Steps

1. 
2. 
3. 

### 废弃时间线 / Deprecation Timeline

- **当前版本**: 标记为 deprecated
- **v1.1.0**: 移除旧 API

</details>

---

## 📊 变更统计 / Change Statistics

### 代码统计 / Code Stats

<!-- 由 CI 自动填充，或手动填写 -->
- **新增行数**: +___ 行
- **删除行数**: -___ 行
- **净变化**: ±___ 行
- **文件变更**: ___ 个文件

### 复杂度指标 / Complexity Metrics

| 指标 | 修改前 | 修改后 | 变化 |
|------|--------|--------|------|
| 圈复杂度 (平均) | - | - | - |
| 代码重复率 | - | - | - |
| 测试覆盖率 | - | - | - |

---

## 🎬 审查流程 / Review Process

### 审查前自查 / Self-review Checklist

- [ ] 我已仔细审查自己的代码
- [ ] 代码符合项目编码规范
- [ ] 所有测试通过
- [ ] 文档已更新
- [ ] 无敏感信息泄露
- [ ] 分支命名规范

### 审查者注意事项 / Reviewer Notes

**重点关注 / Focus Areas:**
- 
- 
- 

**潜在风险 / Potential Risks:**
- 
- 
- 

---

## 📞 其他信息 / Additional Information

### 相关资源 / Related Resources

- 设计文档: 
- Issue 讨论: 
- 参考资料: 

### 截图或演示 / Screenshots or Demos

<!-- 如果适用，请添加截图或录屏 -->
<!-- Add screenshots or recordings if applicable -->


---

## ✨ 致谢 / Acknowledgments

**特别感谢 / Special Thanks To:**
- @___ for code review suggestions
- @___ for testing assistance
- @___ for documentation improvements

---

**再次感谢您的贡献！🎉**

> *"From data intelligence emerges."*  
> *"始于数据，终于智能。"*

---

### PR 状态跟踪 / PR Status Tracking

| 阶段 | 状态 | 时间 | 备注 |
|------|------|------|------|
| Draft | ⬜ | | 初始创建 |
| Ready for Review | ⬜ | | 自查完成 |
| In Review | ⬜ | | 审查中 |
| Approved | ⬜ | | 已批准 |
| Merged | ⬜ | | 已合并 |

**审查者 / Reviewers:** @___ @___  
**预计合并日期 / Estimated Merge Date:**  
**PR 编号 / PR Number:** #___

---

🎉 **Thank you again for your contribution!**  
**Intelligence emergence, and nothing less, is the ultimate sublimation of AI.**  
**让我们一起见证AI智能涌现的极尽升华时刻！**
