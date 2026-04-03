# 贡献指南 CONTRIBUTING GUIDE

感谢您对 AgentOS 项目感兴趣！我们欢迎各种形式的贡献，包括代码提交、文档改进、Bug 报告和 feature 建议。

**版本**: v1.0.0.6  
**最后更新**: 2026-03-31

## 📑 目录

- [行为准则](#行为准则)
- [快速导航](#快速导航)
- [开发环境搭建](#开发环境搭建)
- [分支模型](#分支模型)
- [贡献流程](#贡献流程)
- [编码规范](#编码规范)
- [测试要求](#测试要求)
- [提交规范](#提交规范)
- [代码合并](#代码合并)
- [认可与感谢](#认可与感谢)
- [联系方式](#联系方式)

---

## 行为准则

本项目采用 **Contributor Covenant** 行为准则，确保所有参与者行为符合标准，共同维护友好、包容的开源社区。

---

## 快速导航

- 🐛 [报告 Bug (AtomGit 官方)](https://atomgit.com/spharx/agentos/issues)
- 🐛 [报告 Bug (Gitee 官方)](https://gitee.com/spharx/agentos/issues)
- 🐛 [报告 Bug (GitHub 官方)](https://github.com/SpharxTeam/AgentOS/issues)
- 💡 [提出功能建议 (AtomGit 官方)](https://atomgit.com/spharx/agentos/issues)
- 💡 [提出功能建议 (Gitee 官方)](https://gitee.com/spharx/agentos/issues)
- 💡 [提出功能建议 (GitHub 官方)](https://github.com/SpharxTeam/AgentOS/issues)
- 📖 [查看文档](./manuals/)
- 💬 [参与讨论 (GitHub 官方)](https://github.com/SpharxTeam/AgentOS/discussions)
- 📐 [编码规范](./manuals/specifications/coding_standard/)
- 🧪 [测试指南](./tests/README.md)

---

## 开发环境搭建

### 1. 基础要求

- **操作系统**: Linux (Ubuntu 22.04+), macOS 13+, Windows 11 (WSL2)
- **编译器**: GCC 11+ 或 Clang 14+
- **构建工具**: CMake 3.20+, Ninja 或 Make
- **Python**: 3.9+
- **系统依赖**: 通过系统包管理器安装
  - OpenSSL >= 1.1.1
  - libevent >= 2.1
  - FAISS >= 1.7.0 (可选，用于向量检索)
  - SQLite3 >= 3.35
  - libcurl >= 7.68
  - cJSON >= 1.7.15

### 2. Fork 和克隆项目

```bash
# 1. 在 AtomGit/Gitee/GitHub 上 Fork 本项目
#    AtomGit 官方仓库（推荐）：https://atomgit.com/spharx/agentos
#    Gitee 官方仓库：https://gitee.com/spharx/agentos
#    GitHub 官方仓库：https://github.com/SpharxTeam/AgentOS
#
# 2. 克隆您的 fork
git clone https://atomgit.com/YOUR_USERNAME/agentos.git
cd agentos

# 或克隆 Gitee fork
# git clone https://gitee.com/YOUR_USERNAME/agentos.git
# cd agentos

# 或克隆 GitHub fork
# git clone https://github.com/YOUR_USERNAME/AgentOS.git
# cd AgentOS

# 3. 添加上游仓库（三选一）
git remote add upstream https://atomgit.com/spharx/agentos.git
# 或
# git remote add upstream https://gitee.com/spharx/agentos.git
# 或
# git remote add upstream https://github.com/SpharxTeam/AgentOS.git
```

### 3. 安装依赖

#### 使用 Poetry（推荐，适用于 Python 部分）

```bash
# 安装 Poetry
curl -sSL https://install.python-poetry.org | python3 -

# 安装依赖
cd openlab
poetry install

# 激活虚拟环境
poetry shell
```

#### 使用 pip

```bash
# 创建虚拟环境
python3 -m venv venv
source venv/bin/activate  # Linux/macOS
# 或 .\venv\Scripts\activate  # Windows

# 安装依赖
cd openlab
pip install -r requirements.txt
```

### 4. 构建项目

```bash
# 构建内核层
cd atoms
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build . --parallel $(nproc)

# 运行测试
ctest --output-on-failure
```

---

## 分支模型

AgentOS 采用对称、简洁、稳定、可追溯、友好的分支模型。

```
main (主分支，生产就绪)
  ↑
  │
develop (开发分支，集成分支)
  ↑
  │
  ├─ feature/xxx (新功能)
  ├─ bugfix/xxx (Bug 修复)
  ├─ hotfix/xxx (紧急修复)
  └─ docs/xxx (文档改进)
```

### 分支命名规范

| 分支类型 | 前缀 | 示例 | 说明 |
|---------|------|------|------|
| 功能分支 | `feature/` | `feature/memory-optimization` | 新功能开发 |
| Bug 修复 | `bugfix/` | `bugfix/ipc-race-condition` | 普通 Bug 修复 |
| 紧急修复 | `hotfix/` | `hotfix/security-patch` | 生产环境紧急修复 |
| 文档改进 | `docs/` | `docs/api-improvement` | 文档更新 |
| 性能优化 | `perf/` | `perf/faiss-indexing` | 性能优化 |
| 重构 | `refactor/` | `refactor/error-handling` | 代码重构 |
| 测试 | `test/` | `test/ipc-benchmark` | 测试用例 |

---

## 贡献流程

### 1. 选择或创建 Issue

在开始工作前，请先：
- 查看现有 Issue 列表
- 如果没有相关 Issue，创建一个新的 Issue
- 等待维护者在 Issue 上分配给您

### 2. 创建分支

从 `develop` 分支创建您的工作分支：

```bash
git checkout develop
git pull upstream develop
git checkout -b feature/your-feature-name
```

### 3. 开发和测试

- 按照编码规范编写代码
- 编写单元测试和集成测试
- 确保所有测试通过
- 更新相关文档

### 4. 提交代码

遵循 Conventional Commits 规范：

```bash
git add .
git commit -m "feat: add new memory retrieval algorithm"
```

### 5. 推送并创建 Pull Request

```bash
git push origin feature/your-feature-name
```

然后在 Gitee/GitHub 上创建 Pull Request：
- 填写详细的 PR 描述
- 关联相关 Issue
- 等待代码审查

### 6. 代码审查

维护者会审查您的代码，可能提出修改建议。请：
- 及时响应审查意见
- 根据建议修改代码
- 推动 PR 尽快合并

---

## 编码规范

### C/C++ 代码

- 遵循 [C/C++ 编码规范](./manuals/specifications/coding_standard/C_coding_style_guide.md)
- 使用 `.clang-format` 和 `.clang-tidy` 进行代码格式化
- 所有公共 API 必须有 Doxygen 注释

**示例**:
```cpp
/**
 * @brief 写入原始记忆
 * @param data [in] 数据缓冲区，不能为 NULL
 * @param len [in] 数据长度，必须>0
 * @param metadata [in,opt] JSON 元数据，可为 NULL
 * @param out_record_id [out] 输出记录 ID，调用者负责释放
 * @return agentos_error_t
 * @threadsafe 否，内部使用全局锁
 * @see agentos_sys_memory_search(), agentos_sys_memory_delete()
 */
AGENTOS_API agentos_error_t agentos_sys_memory_write(
    const void* data, 
    size_t len,
    const char* metadata, 
    char** out_record_id);
```

### Python 代码

- 遵循 PEP 8
- 使用类型注解
- 编写 docstring

### 通用要求

- **命名语义化**: 名称精确表达语义，避免缩写
- **错误处理**: 所有错误必须处理，不能忽略
- **资源管理**: 使用 RAII 模式，明确所有权
- **线程安全**: 明确标注函数的线程安全性
- **日志规范**: 使用统一日志系统，结构化输出

---

## 测试要求

### 单元测试

- 核心模块单元测试覆盖率 ≥ 90%
- 使用统一的测试框架
- 测试用例命名清晰

### 集成测试

- 测试模块间接口
- 测试系统调用接口
- 测试端到端场景

### 性能测试

- 基准测试数据必须真实可复现
- 记录测试环境配置
- 提供性能分析报告

### 运行测试

```bash
# 运行所有测试
cd tests
python run_tests.py

# 运行特定测试
python -m pytest tests/unit/coreloopthree/test_loop.py -v

# 生成覆盖率报告
make coverage
```

---

## 提交规范

AgentOS 采用 **Conventional Commits** 规范。

### 提交类型

| 类型 | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat: add L4 pattern layer` |
| `fix` | Bug 修复 | `fix: resolve IPC race condition` |
| `docs` | 文档更新 | `docs: update API documentation` |
| `style` | 代码格式 | `style: format code with clang-format` |
| `refactor` | 重构 | `refactor: simplify error handling` |
| `perf` | 性能优化 | `perf: optimize FAISS indexing` |
| `test` | 测试 | `test: add IPC benchmark tests` |
| `chore` | 构建/工具 | `chore: update CMakeLists.txt` |

### 提交格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

**示例**:
```
feat(memory): add HDBSCAN clustering algorithm

- Implement HDBSCAN clustering for L4 pattern layer
- Add configuration options for min_cluster_size and min_samples
- Integrate with existing memory consolidation pipeline

Closes #123
```

---

## 代码合并

### 合并要求

- ✅ 所有测试通过
- ✅ 代码审查通过
- ✅ 文档已更新
- ✅ 变更日志已更新
- ✅ 无冲突

### 合并策略

- **功能分支**: 使用 `squash and merge`
- **Bug 修复**: 使用 `merge commit`
- **紧急修复**: 使用 `rebase and merge`

---

## 认可与感谢

所有贡献者将被记录在以下位置：

- [AUTHORS.md](./AUTHORS.md) - 核心贡献者名单
- [CHANGELOG.md](./CHANGELOG.md) - 版本变更日志
- [ACKNOWLEDGMENTS.md](./ACKNOWLEDGMENTS.md) - 感谢名单

我们坚信：**开源因贡献而精彩**！

---

## 联系方式

如有任何问题，请通过以下方式联系我们：

- **技术支持**: lidecheng@spharx.cn
- **安全报告**: wangliren@spharx.cn
- **AtomGit Issues**: https://atomgit.com/spharx/agentos/issues
- **Gitee Issues**: https://gitee.com/spharx/agentos/issues
- **GitHub Issues**: https://github.com/SpharxTeam/AgentOS/issues
- **GitHub Discussions**: https://github.com/SpharxTeam/AgentOS/discussions

---

<div align="center">

**再次感谢您的贡献！🎉**

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
