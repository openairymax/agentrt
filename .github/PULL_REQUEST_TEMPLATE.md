# 🚀 AgentOS Pull Request Template (V4.0)

<!-- 
感谢您为 AgentOS 做出贡献！
Thank you for contributing to AgentOS!

📋 提交前请阅读：Please read before submitting:
- [贡献指南](../CONTRIBUTING.md) / Contributing Guide
- [架构原则 V1.8](../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) / Architecture Principles (V1.8)
- [代码规范](../agentos/manuals/CODING_STANDARDS.md) / Coding Standards
- [CI/CD 文档](../scripts/ci/README.md) / CI/CD Documentation

💡 提示：使用 Ctrl+Enter 快速提交 PR！
Tip: Use Ctrl+Enter to submit PR quickly!

🔄 版本：V4.0 (2026-04-07) - 全面重构，深度集成五维正交原则 V1.8
-->

## 🎯 快速导航 / Quick Navigation

| 部分 | 描述 | 预计用时 |
|------|------|----------|
| [📌 核心信息](#-核心信息--core-information) | PR 基本信息、类型、优先级 | 2分钟 |
| [📋 变更详情](#-变更详情--change-details) | 问题背景、解决方案、技术方案 | 5分钟 |
| [✅ 质量检查](#-质量检查--quality-checks) | 代码质量、测试覆盖、安全合规 | 3分钟 |
| [🏗️ 架构原则](#️-架构原则检查--architecture-principles-check) | 五维正交原则 V1.8 检查 | 5分钟 |
| [🧪 测试说明](#-测试说明--testing-instructions) | 手动测试、自动化测试 | 3分钟 |
| [📊 变更统计](#-变更统计--change-statistics) | 代码统计、复杂度指标 | 2分钟 |
| [🤖 AI辅助审查](#-ai辅助审查--ai-assisted-review) | AI分析、建议 | 自动 |
| [⚡ 性能基准](#-性能基准--performance-benchmarks) | 性能测试结果 | 可选 |
| [🔒 安全审查](#-安全审查--security-review) | 安全扫描结果 | 自动 |

---

## 📌 核心信息 / Core Information

### 基本信息 / Basic Information

| 项目 | 内容 | 说明 |
|------|------|------|
| **PR 类型** | - [ ] Bug 修复 🔧 | - [ ] 新功能 ✨ | - [ ] 性能优化 ⚡ | - [ ] 重构 ♻️ | - [ ] 文档 📝 | - [ ] 安全 🔒 | - [ ] 架构改进 🏗️ | - [ ] 依赖更新 📦 | 选择最符合的类型 |
| **关联 Issue** | Closes #___, Fixes #___, Related #___ | 格式参考右侧示例 |
| **优先级** | - [ ] 紧急 (P0) | - [ ] 高 (P1) | - [ ] 中 (P2) | - [ ] 低 (P3) | | 根据影响范围选择 |
| **目标分支** | `main` / `develop` | 通常合并到 main |

### 一句话摘要 / One-line Summary

<!-- 用一句话概括本 PR 的核心价值 -->
<!-- Describe the core value of this PR in one sentence -->

> 示例：修复 daemon 层内存泄漏问题，提升系统稳定性 15%
> Example: Fix memory leak in daemon layer, improve system stability by 15%

---

## 📋 变更详情 / Change Details

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
- 复杂度变化：-

### 解决方案 / Solution

<!-- 如何实现？采用什么技术方案？ -->
<!-- How is it implemented? What technical approach was used? -->

**技术方案 / Technical Approach:**

1. 
2. 
3. 

**关键设计决策 / Key Design Decisions:**

| 决策点 | 选择 | 原因 | 架构原则对齐 |
|--------|------|------|--------------|
| 方案A vs 方案B | | | |
| 性能 vs 可读性 | | | |
| 兼容性策略 | | | |
| 扩展性考虑 | | | |

---

## ✅ 质量检查 / Quality Checks

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
- [ ] **CI/CD 配置**: .gitcode/*, .github/*, .gitee/*
- [ ] **构建配置 / Build Config**: CMakeLists.txt, vcpkg.json

</details>

### 自动化检查结果 / Automated Check Results

<!-- 以下内容将由 CI/CD 自动填充 -->
<!-- The following will be auto-populated by CI/CD -->

| 检查项 | 状态 | 详情 | 时间戳 |
|--------|------|------|--------|
| **基础 CI** | ✅/❌ | [查看结果]() | - |
| **质量门禁** | ✅/❌ | [查看结果]() | - |
| **安全扫描** | ✅/❌ | [查看结果]() | - |
| **代码覆盖率** | ✅/❌ | [查看结果]() | - |

### 代码质量 / Code Quality

<details>
<summary>✅ 点击展开详细检查项 / Click to expand detailed checks</summary>

#### 编码规范 / Coding Standards
- [ ] **语言规范**: 遵循 C/C++11-17, Python 3.10+, Go 1.21+, Rust 1.70+ 规范
- [ ] **格式化**: 已运行 `clang-format` / `black` / `gofmt` / `rustfmt`
- [ ] **注释**: 新增公共 API 有完整 Doxygen 注释（含 @brief/@param/@return）
- [ ] **编译**: 无编译器警告 (`-Wall -Wextra -Werror`)
- [ ] **命名**: 遵循规范：
  - 函数: `agentos_verb_noun()`
  - 类型: `agentos_type_t`
  - 常量: `AGENTOS_CONSTANT`

#### 复杂度控制 / Complexity Control
- [ ] **圈复杂度**: 新增函数 CC ≤ 10，关键函数 CC ≤ 5
- [ ] **函数长度**: 新增函数 ≤ 100 行，关键函数 ≤ 50 行
- [ ] **嵌套深度**: 条件嵌套 ≤ 4 层
- [ ] **认知复杂度**: 新增代码易于理解，无过度复杂逻辑

#### 测试覆盖 / Test Coverage
- [ ] **单元测试**: 新增代码有单元测试（目标 ≥90% 覆盖率）
- [ ] **回归测试**: 所有现有测试通过
- [ ] **覆盖率**: 测试覆盖率未下降
- [ ] **性能测试**: 性能优化提供基准测试对比（before/after）
- [ ] **契约测试**: API 契约测试已更新

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
- [ ] **资源清理**: RAII 模式或明确清理路径

#### 安全与合规 / Security & Compliance
- [ ] **输入净化**: 所有外部输入已净化
- [ ] **权限检查**: 遵循最小权限原则
- [ ] **沙箱隔离**: 危险操作在沙箱中执行
- [ ] **敏感数据**: 无敏感数据泄露（密钥、密码等）
- [ ] **合规检查**: 符合 OWASP Top 10, CWE Top 25

#### 工程实践 / Engineering Practices
- [ ] **资源管理**: 内存/文件句柄正确释放（RAII模式）
- [ ] **跨平台**: Linux/macOS/Windows 兼容性验证
- [ ] **分支命名**: `feature/xxx`, `bugfix/xxx`, `refactor/xxx`, `hotfix/xxx`
- [ ] **提交消息**: `type(scope): description` (遵循 Conventional Commits)
- [ ] **原子提交**: 每个提交是独立可测试的变更

</details>

---

## 🏗️ 架构原则检查 / Architecture Principles Check

<details>
<summary>🔍 五维正交系统检查 V1.8 / Five-Dimensional Orthogonal System Check V1.8</summary>

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
- 不符合原则: __/24
- 不适用原则: __/24

**不符合原因 / Non-compliance Reasons:**

</details>

---

## 🧪 测试说明 / Testing Instructions

### 手动测试步骤 / Manual Test Steps

```markdown
# 环境准备 / Environment Setup
- 操作系统: Ubuntu 22.04 LTS / Windows 11 / macOS 14
- 编译器: GCC 11.4 / MSVC 2022 / Apple Clang 18
- 构建类型: Release
- 依赖版本: 参考 `vcpkg.json` 和 `requirements.txt`

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
- [ ] **端到端测试**: `./tests/e2e/test_<scenario>`

### 测试环境信息 / Test Environment

| 项目 | 版本 | 状态 |
|------|------|------|
| OS | Ubuntu 22.04 / macOS 14 / Windows 11 | |
| Compiler | GCC 11.4 / Clang 18 / MSVC 2022 | |
| CMake | 3.28+ | |
| Python | 3.11+ | |
| Go | 1.22+ | |
| Rust | 1.77+ | |

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
- **模块分布**: ___

### 复杂度指标 / Complexity Metrics

| 指标 | 修改前 | 修改后 | 变化 | 目标 |
|------|--------|--------|------|------|
| 平均圈复杂度 | - | - | - | < 5.0 |
| 最大圈复杂度 | - | - | - | < 15 |
| 代码重复率 | - | - | - | < 3% |
| 测试覆盖率 | - | - | - | ≥ 90% |
| 认知复杂度 | - | - | - | < 10 |

---

## 🤖 AI辅助审查 / AI-Assisted Review

<!-- 此部分可由 AI 工具自动生成 -->
<!-- This section can be auto-generated by AI tools -->

### 代码分析 / Code Analysis

**架构一致性检查 / Architecture Consistency Check:**
- [ ] 符合五维正交原则
- [ ] 无跨层依赖
- [ ] 接口契约完整

**复杂度评估 / Complexity Assessment:**
- 圈复杂度趋势: ⬆️/⬇️/➡️
- 认知复杂度: 高/中/低
- 可维护性评分: __/10

**潜在问题 / Potential Issues:**
1. 
2. 
3. 

### 改进建议 / Improvement Suggestions

**立即改进项 / Immediate Improvements:**
1. 
2. 

**长期优化项 / Long-term Optimizations:**
1. 
2. 

---

## ⚡ 性能基准 / Performance Benchmarks

<!-- 性能优化相关的 PR 需要填写此部分 -->
<!-- This section is required for performance-related PRs -->

### 测试环境 / Test Environment

| 项目 | 配置 |
|------|------|
| CPU | |
| 内存 | |
| 存储 | |
| 网络 | |

### 基准测试结果 / Benchmark Results

| 测试场景 | 修改前 | 修改后 | 提升 | 单位 |
|----------|--------|--------|------|------|
| 内存占用 | | | | MB |
| 响应时间 | | | | ms |
| 吞吐量 | | | | ops/s |
| CPU 使用率 | | | | % |

### 性能分析 / Performance Analysis

**瓶颈识别 / Bottleneck Identification:**
1. 
2. 

**优化策略 / Optimization Strategy:**
1. 
2. 

---

## 🔒 安全审查 / Security Review

<!-- 安全相关的 PR 需要填写此部分 -->
<!-- This section is required for security-related PRs -->

### 安全扫描结果 / Security Scan Results

| 扫描工具 | 结果 | 关键问题 | 状态 |
|----------|------|----------|------|
| **Trivy** | ✅/❌ | | |
| **CodeQL** | ✅/❌ | | |
| **Gitleaks** | ✅/❌ | | |
| **SAST (cppcheck)** | ✅/❌ | | |

### 安全影响分析 / Security Impact Analysis

**攻击面变化 / Attack Surface Changes:**
- [ ] 新增攻击面
- [ ] 减少攻击面
- [ ] 无变化

**安全控制 / Security Controls:**
- [ ] 输入验证
- [ ] 输出编码
- [ ] 访问控制
- [ ] 审计日志
- [ ] 加密存储

### 合规性检查 / Compliance Check

- [ ] OWASP Top 10
- [ ] CWE Top 25
- [ ] GDPR 相关
- [ ] 其他合规要求

---

## 🎬 审查流程 / Review Process

### 审查前自查 / Self-review Checklist

- [ ] 我已仔细审查自己的代码
- [ ] 代码符合项目编码规范
- [ ] 所有测试通过
- [ ] 文档已更新
- [ ] 无敏感信息泄露
- [ ] 分支命名规范
- [ ] 提交消息规范
- [ ] 架构原则检查完成

### 审查者注意事项 / Reviewer Notes

**重点关注 / Focus Areas:**
- 
- 
- 

**潜在风险 / Potential Risks:**
- 
- 
- 

**测试建议 / Testing Suggestions:**
- 
- 
- 

### 审查状态跟踪 / Review Status Tracking

| 审查者 | 状态 | 意见 | 时间 |
|--------|------|------|------|
| @___ | ⬜ 未开始 | | |
| @___ | ⬜ 未开始 | | |
| @___ | ⬜ 未开始 | | |

**预计合并日期 / Estimated Merge Date:**  
**PR 编号 / PR Number:** #___

---

## 📞 其他信息 / Additional Information

### 相关资源 / Related Resources

- 设计文档: 
- Issue 讨论: 
- 参考资料: 
- 相关 PR: 

### 截图或演示 / Screenshots or Demos

<!-- 如果适用，请添加截图或录屏 -->
<!-- Add screenshots or recordings if applicable -->

---

## ✨ 致谢 / Acknowledgments

**特别感谢 / Special Thanks To:**
- @___ for code review suggestions
- @___ for testing assistance
- @___ for documentation improvements
- @___ for architecture guidance

---

**再次感谢您的贡献！🎉**

> *"From data intelligence emerges."*  
> *"始于数据，终于智能。"*
> *"Every contribution makes our system more robust and intelligent."*

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
**合并权限 / Merge Permission:** @spharx-team/core  
**质量门禁 / Quality Gate:** ✅/❌  

---

🎉 **Thank you again for your contribution!**  
**Intelligence emergence, and nothing less, is the ultimate sublimation of AI.**  
**让我们一起见证AI智能涌现的极尽升华时刻！**