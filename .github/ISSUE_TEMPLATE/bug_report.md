---
name: Bug Report / Bug 报告
description: Report a bug / 报告一个 Bug
title: "[Bug]: "
labels: ["bug"]
assignees: []
---

## 🐛 Bug Description / Bug 描述

<!-- 
A clear and concise description of what the bug is.
清晰简洁地描述这个 Bug 是什么。
-->


## 📋 Expected Behavior / 期望行为

<!-- 
A clear and concise description of what you expected to happen.
清晰简洁地描述您期望发生的行为。
-->


## ❌ Actual Behavior / 实际行为

<!-- 
A clear and concise description of what actually happened.
清晰简洁地描述实际发生的行为。
-->


## 🔍 Steps to Reproduce / 复现步骤

<!-- 
Detailed steps to reproduce the bug behavior. The more detail, the better.
详细的复现步骤。越详细越好。
-->

1. **Environment Setup / 环境设置**:
   - OS / 操作系统:
   - Version / 版本:
   - Build type / 构建类型:

2. **Steps / 步骤**:
   ```
   # Step 1 / 步骤1
   > Command or action
   
   # Step 2 / 步骤2  
   > Command or action
   
   # Step 3 / 步骤3
   > Command or action
   ```

3. **Expected Result / 预期结果**:


4. **Actual Result / 实际结果**:


## 🖥️ Environment Information / 环境信息

<!-- 
Provide complete environment information for reproduction.
提供完整的环境信息以便复现。
-->

### Core System Information / 核心系统信息

| Component / 组件 | Version / 版本 | Status / 状态 |
|-----------------|---------------|---------------|
| **AgentOS / 操作系统** | <!-- e.g., v1.0.0.9 --> | |
| **Operating System / 操作系统** | <!-- e.g., Ubuntu 22.04 LTS --> | |
| **Kernel / 内核** | <!-- e.g., 5.15.0-91-generic --> | |
| **Architecture / 架构** | <!-- e.g., x86_64, arm64 --> | |

### Compiler & Build Tools / 编译器和构建工具

| Tool / 工具 | Version / 版本 | Flags / 标志 |
|-------------|---------------|--------------|
| **C/C++ Compiler / C/C++编译器** | <!-- GCC 11.4, Clang 14, MSVC 2022 --> | `-Wall -Wextra -Werror` |
| **Python / Python** | <!-- 3.10+ --> | |
| **Go / Go** | <!-- 1.21+ --> | |
| **Rust / Rust** | <!-- 1.70+ --> | |
| **CMake / CMake** | <!-- 3.28+ --> | |
| **Build Type / 构建类型** | Debug / Release | |

### Dependencies / 依赖版本

| Library / 库 | Version / 版本 | Purpose / 用途 |
|--------------|---------------|---------------|
| **libcurl / libcurl** | <!-- version --> | HTTP client / HTTP 客户端 |
| **cJSON / cJSON** | <!-- version --> | JSON parsing / JSON 解析 |
| **yaml-cpp / yaml-cpp** | <!-- version --> | YAML config / YAML 配置 |
| **OpenSSL / OpenSSL** | <!-- version --> | Crypto / 加密 |
| **Other / 其他**: | | |

### Runtime Configuration / 运行时配置

```yaml
# AgentOS configuration (if relevant)
# AgentOS 配置（如相关）
agentos:
  daemon_mode: <!-- single/cluster -->
  log_level: <!-- debug/info/warn/error -->
  memory_limit: <!-- e.g., "512MB" -->
```

## 📊 Error Details / 错误详情

### Error Messages / 错误消息

<!-- 
Paste the complete error messages here using code blocks.
在此粘贴完整的错误消息，使用代码块。
-->

```log
# Standard error format expected:
# [ERROR] <MODULE>:<FUNCTION>():<LINE> <CODE> - message (context). Suggestion: <action>

[ERROR] corekern:ipc_send():123 IPC_ERR_TIMEOUT - Failed to send message within timeout (5000ms). Suggestion: Check network connectivity or increase timeout value.
[ERROR] commons:error_chain():45 ERR_CHAIN_BROKEN - Error chain broken at module 'gateway'. Suggestion: Verify error propagation path.
```

### Call Stack / 调用栈

<!-- If available, provide the call stack trace -->
<!-- 如果可用，请提供调用栈追踪 -->

```
# Example format / 示例格式
Thread 0 (main):
  #0 agentos_syscall_dispatch() at syscall/dispatch.c:234
  #1 coreloopthree_plan_execute() at coreloopthree/planning.c:89
  #2 daemon_sched_main() at daemon/sched_d/main.c:45
  #3 main() at main.c:12
```

### Logs / 日志文件

<!-- Attach or paste relevant log excerpts -->
<!-- 附加或粘贴相关的日志片段 -->

```log
# Log file location: /var/log/agentos/
# 日志文件位置：/var/log/agentos/

[2026-04-02 10:30:15] [INFO] [main]: Starting AgentOS v1.0.0.9...
[2026-04-02 10:30:16] [DEBUG] [corekern]: Initializing kernel subsystems...
[2026-04-02 10:30:17] [ERROR] [memoryrovol]: Memory allocation failed in rovol_layer_2...
```

## 🏗️ Architecture Impact Analysis / 架构影响分析

Based on **ARCHITECTURAL_PRINCIPLES.md V1.8** Five-Dimensional Orthogonal System.
基于 **ARCHITECTURAL_PRINCIPLES.md V1.8** 五维正交系统。

### Potentially Violated Principles / 可能违反的原则

<!-- Check which architecture principles this bug may violate -->
<!-- 检查此 bug 可能违反哪些架构原则 -->

#### Dimension 1: System View / 维度一：系统观
- [ ] **S-1: Feedback Loop** / 反馈闭环 - Does the bug break feedback mechanisms? / 此 Bug 是否破坏了反馈机制？
- [ ] **S-2: Hierarchical Decomposition** / 层次分解 - Which layer(s) are affected? / 影响了哪些层级？
- [ ] **S-3: Overall Design Department** / 总体设计部 - Is decision logic affected? / 决策逻辑是否受影响？
- [ ] **S-4: Emergence Management** / 涌现性管理 - Does it cause negative emergence? / 是否导致负面涌现？

#### Dimension 2: Kernel View / 维度二：内核观
- [ ] **K-1: Kernel Minimalism** / 内核极简 - Does it affect core interfaces? / 是否影响核心接口？
- [ ] **K-2: Interface Contract** / 接口契约化 - Are contracts violated? / 契约是否被违反？
- [ ] **K-3: Service Isolation** / 服务隔离 - Does it cross daemon boundaries? / 是否跨越守护进程边界？
- [ ] **K-4: Pluggable Strategy** / 可插拔策略 - Are strategies affected? / 策略是否受影响？

#### Dimension 3: Cognitive View / 维度三：认知观
- [ ] **C-1: Dual-System Synergy** / 双系统协同 - Fast/slow path affected? / 快慢路径受影响？
- [ ] **C-2: Incremental Evolution** / 增量演化 - Regression from recent change? / 近期变更导致的回归？
- [ ] **C-3: Memory Rovol** / 记忆卷载 - Memory layer corruption? / 记忆层损坏？
- [ ] **C-4: Forgetting Mechanism** / 遗忘机制 - Data lifecycle issue? / 数据生命周期问题？

#### Dimension 4: Engineering View / 维度四：工程观
- [ ] **E-1: Security Built-in** / 安全内生 - Security vulnerability introduced? / 引入安全漏洞？
- [ ] **E-2: Observability** / 可观测性 - Monitoring/logging gap? / 监控/日志缺失？
- [ ] **E-3: Resource Determinism** / 资源确定性 - Memory leak/resource leak? / 内存泄漏/资源泄漏？
- [ ] **E-4: Cross-Platform Consistency** / 跨平台一致性 - Platform-specific bug? / 平台特定 Bug？
- [ ] **E-5: Naming Semantics** / 命名语义化 - Naming convention violation? / 命名规范违反？
- [ ] **E-6: Error Traceability** / 错误可追溯 - Error chain incomplete? / 错误链不完整？
- [ ] **E-7: Documentation as Code** / 文档即代码 - Documentation mismatch? / 文档不一致？
- [ ] **E-8: Testability** / 可测试性 - Test coverage gap? / 测试覆盖缺失？

#### Dimension 5: Aesthetic View / 维度五：设计美学
- [ ] **A-1: Simplicity** / 简约至上 - Complexity causing the bug? / 复杂性导致的 Bug？
- [ ] **A-2: Extreme Details** / 极致细节 - Edge case missed? / 边界情况遗漏？
- [ ] **A-3: Human Care** / 人文关怀 - UX impact? / 用户体验影响？
- [ ] **A-4: Perfectionism** / 完美主义 - Quality standard not met? / 未达到质量标准？

### Affected Modules / 受影响的模块

<!-- List all modules affected by this bug -->
<!-- 列出所有受此 Bug 影响的模块 -->

- [ ] `corekern/` - Core kernel / 核心内核
- [ ] `coreloopthree/` - Cognitive loop / 认知循环
- [ ] `memoryrovol/` - Memory system / 记忆系统
- [ ] `cupolas/` - Security dome / 安全穹顶
- [ ] `syscall/` - Syscalls / 系统调用
- [ ] `commons/` - Common libraries / 公共库
- [ ] `daemon/*/` - Daemons / 守护进程
- [ ] `gateway/` - Gateways / 网关
- [ ] Other / 其他：

## 🛠️ Proposed Fix / 建议修复方案

<!-- If you have a fix suggestion, describe it here -->
<!-- 如果您有修复建议，请在此描述 -->

### Root Cause Analysis / 根因分析


### Fix Strategy / 修复策略
- [ ] Quick workaround / 快速临时方案
- [ ] Proper fix / 正确的修复方案
- [ ] Requires ADR / 需要 ADR（架构决策记录）

### Code Changes Required / 需要的代码变更

```c
// Example code change showing the fix
// 示例代码变更展示修复方案

// Before / 修复前:
// int result = agentos_ipc_send(msg);

// After / 修复后:
int result = agentos_ipc_send_with_timeout(msg, AGENTOS_DEFAULT_TIMEOUT);
if (result != AGENTOS_OK) {
    AGENTOS_LOG_ERROR("IPC send failed: %s", agentos_error_string(result));
    return result;
}
```

## 📸 Screenshots / Demonstrations / 截图或演示

<!-- Add screenshots, screen recordings, or performance graphs if applicable -->
<!-- 如果适用，请添加截图、录屏或性能图表 -->


## ✅ Checklist / 检查清单

### Before Submitting / 提交前检查
- [ ] I have confirmed this is not a duplicate issue / 我已确认这不是重复的 Issue
- [ ] I have provided complete reproduction steps / 我已提供完整的复现步骤
- [ ] I have provided complete environment information / 我已提供完整的环境信息
- [ ] I have included error messages with full context / 我已包含带完整上下文的错误消息
- [ ] I have analyzed the architecture principles impact / 我已分析架构原则的影响
- [ ] I have read the ARCHITECTURAL_PRINCIPLES.md / 我已阅读 ARCHITECTURAL_PRINCIPLES.md
- [ ] I have read the contributing guide / 我已阅读贡献指南
- [ ] I am willing to contribute a fix / 我愿意贡献修复代码

---

**Thank you for your detailed bug report!** This helps us improve AgentOS quality.  
**感谢您的详细 Bug 报告！** 这有助于我们提升 AgentOS 的质量。

> *"Every bug report is an opportunity to make our system more robust."*  
> *每一个 Bug 报告都是让我们的系统更加健壮的机会。*
