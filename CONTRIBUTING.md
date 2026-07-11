# 贡献指南 (Contributing Guide)

**P4.3.8** | **最后更新**: 2026-06-18

欢迎来到 AgentRT 社区！感谢你对 AgentRT 项目的关注与贡献。无论你是修复 Bug、改进文档、提交新功能，还是参与社区讨论，你的每一份努力都让这个项目变得更好。

在参与贡献之前，请仔细阅读本指南，了解我们的协作方式与规范要求。同时，请遵守 [AgentRT 社区行为准则](./CODE_OF_CONDUCT.md)，共同维护一个开放、包容、互相尊重的社区环境。

---

## 1. 项目结构概览

AgentRT 项目采用分层架构，关键目录如下：

| 目录 | 说明 |
|------|------|
| `agentos/` | 核心系统层，包含 atoms 原子模块、protocols 协议栈、cupolas 安全穹顶、commons 基础库 |
| `ecosystem/` | 生态系统层，包含 OpenLab 开放实验室、Manager 配置管理器等应用 |
| `sdk/` | 多语言 SDK 层，提供 Python / Go / Rust / TypeScript 开发工具包 |
| `scripts/` | 脚本工具集，包含 dev 开发工具、ci 持续集成流水线、release 发布脚本 |
| `tests/` | 测试体系，包含 unit 单元测试、integration 集成测试、benchmarks 基准测试、security 安全测试、contract 契约测试 |
| `deploy/` | 部署相关配置，包含 Docker 镜像定义、Kubernetes 编排文件等 |
| `Docs/` | 项目文档，包含架构设计、快速入门、API 参考、CLI 参考、部署指南等 |

---

## 2. 开发环境搭建

### 2.1 基础要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2) |
| 编译器 | GCC 11+ / Clang 14+（支持 C11/C++17） |
| 构建工具 | CMake 3.20+、Ninja |
| Python | 3.10+（OpenLab 运行必需） |
| Go | 1.21+（Go SDK 开发需要） |
| Rust | 1.70+（Rust SDK 开发需要） |
| Node.js | 18+（TypeScript SDK 开发需要） |

### 2.2 克隆与安装依赖

```bash
# 1. 克隆仓库
git clone https://atomgit.com/openairymax/agentos.git
cd agentos

# 2. 安装系统依赖 (Ubuntu)
sudo apt install -y build-essential cmake gcc g++ libssl-dev \
    libsqlite3-dev ninja-build python3 python3-pip git \
    libevent-dev libcurl4-openssl-dev

# 3. 安装 Python 依赖
cd ecosystem/openlab && pip install -r requirements.txt
```

### 2.3 构建项目

```bash
# 在 AgentRT 源码目录外构建（BAN-33 合规）
cd /path/to/AgentRT
cmake -B ../AgentRT-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build ../AgentRT-build --parallel $(nproc)

# 运行测试
cd ../AgentRT-build && ctest --output-on-failure
```

### 2.4 开发工具配置

推荐使用 VS Code 或 CLion，项目已配置 `.editorconfig`、`.clang-format` 等格式化规则。推荐安装以下扩展：

- C/C++ (Microsoft) / clangd
- Python (Microsoft)
- rust-analyzer
- Go

---

## 3. 编码规范

### 3.1 C 代码风格

- 遵循项目 `.clang-format` 配置，提交前运行 `clang-format -i` 格式化
- 函数命名：`agentrt_动词_名词()` 风格
- 类型命名：`名词_t` 风格
- 常量：`AGENTRT_大写` 宏风格
- 所有公共 API 必须有 Doxygen 注释
- 使用项目统一内存分配器 `AGENTRT_MALLOC` / `AGENTRT_FREE`，禁止直接使用裸 `malloc` / `free`
- 使用安全字符串函数（`strncpy`、`snprintf`），禁止使用 `strcpy`、`gets`、`sprintf`

### 3.2 Python 代码风格

- 遵循 PEP 8 规范
- 使用 Black 进行代码格式化
- 使用 isort 排序 import 语句
- 使用类型注解 (Type Hints)
- 编写 Google 风格的 docstring

### 3.3 提交信息格式

所有提交信息必须遵循以下格式：

```
<type>(<scope>): <subject>
```

**Type（提交类型）**：

| 类型 | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `refactor` | 代码重构 |
| `test` | 测试相关 |
| `docs` | 文档更新 |
| `perf` | 性能优化 |
| `style` | 代码格式调整 |
| `chore` | 构建/工具/依赖更新 |
| `ci` | CI/CD 配置变更 |
| `build` | 构建系统变更 |
| `revert` | 回滚提交 |

**Scope（影响范围）**：

| Scope | 对应模块 |
|-------|----------|
| `core` | 核心内核模块 |
| `memory` | 记忆系统模块 |
| `daemon` | 守护进程服务 |
| `gateway` | 协议网关 |
| `sdk` | 多语言 SDK |
| `cli` | 命令行工具 |
| `docs` | 文档 |
| `deploy` | 部署相关 |
| `test` | 测试体系 |
| `ci` | CI/CD 流水线 |

**Subject（主题描述）**：使用中文描述，不超过 50 个字符。

**示例**：

```
feat(memory): 新增 L4 模式挖掘算法
fix(core): 修复 IPC 竞态条件导致的内存泄漏
refactor(gateway): 重构协议路由层错误处理逻辑
docs(sdk): 更新 Python SDK 快速入门指南
```

---

## 4. 分支策略

AgentRT 采用简洁稳定的 Git 分支模型：

```
main (生产分支，受保护)
  ↑ merge
develop (开发分支，日常开发基准)
  ↑ merge PR
  ├── feature/*    (功能开发分支)
  ├── fix/*        (Bug 修复分支)
  └── release/*    (版本发布分支)
```

### 分支操作规范

```bash
# 始终从 develop 分支创建新分支
git checkout develop && git pull origin develop
git checkout -b feature/your-feature-name

# 保持分支同步
git fetch origin
git rebase origin/develop

# 合并后清理本地分支
git branch -d feature/your-feature-name
```

### 分支命名规范

| 分支类型 | 前缀 | 示例 |
|---------|------|------|
| 功能分支 | `feature/` | `feature/memory-l4-pattern` |
| 修复分支 | `fix/` | `fix/ipc-race-condition` |
| 发布分支 | `release/` | `release/v0.2.0` |

---

## 5. Pull Request 流程

### 5.1 提交 PR 前的准备

1. 确保代码已通过格式化（clang-format / Black）
2. 确保所有测试通过（`ctest --output-on-failure`）
3. 确保无编译器警告
4. 确保相关文档已更新

### 5.2 PR 模板

提交 PR 时，请包含以下信息：

- **变更说明**：清晰描述本次变更的内容和原因
- **关联 Issue**：使用 `Closes #xxx` 或 `Fixes #xxx` 关联
- **测试说明**：说明如何验证本次变更
- **影响范围**：说明本次变更影响的模块

### 5.3 审查检查清单

维护者审查 PR 时将检查以下项目：

- [ ] 代码符合编码规范
- [ ] 所有 CI 检查通过（构建、测试、静态分析、安全检查）
- [ ] 无安全漏洞与硬编码敏感信息
- [ ] 新增代码有对应的测试用例
- [ ] 公共 API 变更已更新文档
- [ ] 提交信息格式正确

### 5.4 CI 检查

PR 提交后将自动触发以下 CI 检查流水线：

- 构建验证（Debug / Release 模式）
- 单元测试 + 集成测试
- 代码静态分析（cppcheck / flake8）
- 安全扫描（bandit / 禁止函数检查）
- 质量门禁（quality-gate.sh）

---

## 6. 测试要求

### 6.1 测试类型

| 类型 | 目录 | 说明 |
|------|------|------|
| 单元测试 | `tests/unit/` | 单个函数/模块的独立测试 |
| 集成测试 | `tests/integration/` | 多模块交互的端到端验证 |
| 基准测试 | `tests/benchmarks/` | 性能回归与压力测试 |

### 6.2 测试要求

- 新增功能必须包含对应的单元测试
- 跨模块变更必须包含集成测试
- 性能敏感代码必须包含基准测试
- 测试覆盖率目标：核心模块 ≥ 90%

### 6.3 运行测试

```bash
# C 测试（从构建目录运行）
cd ../AgentRT-build && ctest --output-on-failure

# 运行特定模块测试
cd ../AgentRT-build && ctest -R memory -V

# Python 测试
cd ../AgentRT && python -m pytest tests/ -v

# 基准测试
cd ../AgentRT-build && ctest -R benchmark
```

---

## 7. 文档标准

### 7.1 文档编写规范

- 使用中文编写，技术术语保留英文原文（如 API、SDK、PR、CI）
- 代码示例必须可运行、可验证
- 文档内链接使用相对路径
- 遵循 Markdown 格式规范

### 7.2 文档类型

| 类型 | 位置 | 说明 |
|------|------|------|
| 架构文档 | `Docs/10-architecture/01-system-architecture.md` | 系统整体架构设计 |
| API 参考 | `Docs/30-api/02-api-reference.md` | 接口规范说明 |
| 快速入门 | `Docs/60-guides/AgentRT_quickstart.md` | 新手入门指南 |
| CLI 参考 | `Docs/30-api/03-cli-reference.md` | 命令行工具使用说明 |
| 部署指南 | `Docs/60-guides/AgentRT_deployment.md` | 部署与运维文档 |
| 模块 README | 各模块目录下 | 模块级说明文档 |

---

## 8. Issue 报告指南

### 8.1 Bug 报告

提交 Bug 报告时请包含以下信息：

- **问题描述**：清晰描述 Bug 现象
- **复现步骤**：提供最小复现步骤
- **期望行为**：描述你期望的正确行为
- **环境信息**：操作系统、编译器版本、AgentRT 版本
- **日志/截图**：附上相关错误日志或截图

### 8.2 功能建议

提交功能建议时请包含：

- **需求背景**：描述使用场景与需求动机
- **功能描述**：清晰描述期望的功能
- **替代方案**：是否考虑过其他实现方式
- **影响范围**：预估涉及的模块

---

## 9. 社区渠道

欢迎通过以下渠道参与 AgentRT 社区：

| 渠道 | 用途 | 链接 |
|------|------|------|
| GitHub Issues | Bug 报告 / 功能建议 | [SpharxTeam/AgentRT](https://github.com/SpharxTeam/AgentRT/issues) |
| AtomGit Issues | Bug 报告 / 功能建议 | [openairymax/agentos](https://atomgit.com/openairymax/agentos/issues) |
| Gitee Issues | Bug 报告 / 功能建议 | [spharx/agentos](https://gitee.com/spharx/agentos/issues) |
| 飞书社群 | 技术交流与社区讨论 | 扫描 README 中飞书社群二维码加入 |
| 官方网站 | 项目动态与文档 | [spharx.cn](https://spharx.cn) |

---

<div align="center">

**感谢你的贡献！**

*From data intelligence emerges.*

© 2025-2026 SPHARX Ltd. 保留所有权利。

</div>