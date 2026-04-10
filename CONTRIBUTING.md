# 贡献指南 CONTRIBUTING GUIDE

**版本**: v1.1.0  
**最后更新**: 2026-04-10  
**状态**: 活跃

> 本文件合并自 `.gitcode/CONTRIBUTING.md` 与根目录 `CONTRIBUTING.md`，消除重复并整合双版本独有内容。

感谢您对 AgentOS 项目感兴趣！我们欢迎各种形式的贡献，包括代码提交、文档改进、Bug 报告和 feature 建议。

## 📑 目录

- [行为准则](#行为准则)
- [快速导航](#快速导航)
- [开发环境搭建](#开发环境搭建)
- [分支模型](#分支模型)
- [贡献流程](#贡献流程)
- [编码规范](#编码规范)
- [测试规范](#测试规范)
- [架构原则检查清单](#架构原则检查清单)
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
- 📖 [查看文档](./docs/)
- 💬 [参与讨论 (GitHub 官方)](https://github.com/SpharxTeam/AgentOS/discussions)
- 📐 [编码规范](./docs/Capital_Specifications/)
- 🧪 [测试指南](./tests/README.md)

---

## 开发环境搭建

### 贡献前必读文档

**核心文档**：
- [📘 架构设计原则 V1.8](docs/ARCHITECTURAL_PRINCIPLES.md) - 五维正交体系
- [🔄 CoreLoopThree 架构](agentos/atoms/coreloopthree/README.md) - 三层认知循环
- [🧠 MemoryRovol 架构](agentos/atoms/memoryrovol/README.md) - 四层记忆系统
- [🛡️ cupolas 安全穹顶](agentos/cupolas/README.md) - 安全机制
- [⚙️ 编译指南](docs/Capital_Guides/installation.md) - 构建步骤

### 1. 基础要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2) |
| 编译器 | GCC 11+ / Clang 14+ (C11/C++17) |
| 构建工具 | CMake 3.20+, Ninja |
| Python | 3.10+ (OpenLab需要) |
| Go | 1.21+ (Go SDK) |
| Rust | 1.70+ (Rust SDK) |

系统依赖（通过包管理器安装）：
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

# 或克隆 GitHub fork
# git clone https://github.com/YOUR_USERNAME/AgentOS.git
# cd AgentOS

# 3. 添加上游仓库
git remote add upstream https://atomgit.com/spharx/agentos.git
```

### 3. 安装依赖

#### 使用 Poetry（推荐，适用于 Python 部分）

```bash
curl -sSL https://install.python-poetry.org | python3 -
cd agentos/openlab && poetry install && poetry shell
```

#### 使用 pip

```bash
python3 -m venv venv && source venv/bin/activate
cd agentos/openlab && pip install -r requirements.txt
```

#### 系统依赖（Ubuntu）

```bash
sudo apt install -y build-essential cmake gcc g++ libssl-dev \
    ninja-build python3 python3-pip git
```

### 4. 构建项目

```bash
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build . --parallel $(nproc)
ctest --output-on-failure
```

### 5. IDE 配置推荐

**VS Code** (`/.vscode/settings.json`):
```jsonc
{
  "files.associations": {
    "*.c": "c", "*.h": "c",
    "*.py": "python", "*.go": "go", "*.rs": "rust"
  },
  "editor.formatOnSave": true,
  "editor.tabSize": 4,
  "editor.insertSpaces": true,
  "C_Cpp.clang_format_path": ".clang-format",
  "python.formatting.provider": "black"
}
```

**Git 配置**:
```bash
git config user.name "Your Name"
git config user.email "your.email@example.com"
git config commit.template .gitmessage-template
```

---

## 分支模型

AgentOS 采用对称、简洁、稳定、可追溯、友好的分支模型。

```
main (生产分支，受保护)
  ↑
  │ merge
  │
develop (开发分支)
  ↑
  │ merge PR
  │
feature/xxx (功能分支)
bugfix/xxx (修复分支)
hotfix/xxx (紧急修复)
refactor/xxx (重构分支)
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

在开始工作前，请先查看现有 Issue 列表。如果没有相关 Issue，创建一个新的 Issue 并等待维护者分配。

### 2. 创建分支

从 `develop` 分支创建工作分支：

```bash
git checkout develop && git pull upstream develop
git checkout -b feature/your-feature-name
```

### 3. 开发和测试

按照编码规范编写代码 → 编写单元测试和集成测试 → 确保所有测试通过 → 更新相关文档

```bash
find . -name "*.c" -o -name "*.h" | xargs clang-format -i
find . -name "*.py" | xargs black
cd build && make -j$(nproc)
ctest --output-on-failure
cppcheck --enable=all agentos/atoms/src/
bandit -r agentos/cupolas/
```

### 4. 提交代码

遵循 Conventional Commits 规范：

```bash
git add .
git commit -m "feat: add new memory retrieval algorithm"
```

### 5. 推送并创建 PR

```bash
git push origin feature/your-feature-name
```

然后在 Gitee/GitHub 上创建 Pull Request，填写详细的 PR 描述并关联相关 Issue。

### 6. PR 审查清单

维护者会审查您的代码，请确保以下项目通过：
- [ ] 代码符合编码规范
- [ ] 所有测试通过
- [ ] 无编译器警告
- [ ] 无安全漏洞
- [ ] 文档已更新
- [ ] CHANGELOG 已更新
- [ ] 架构原则检查通过

---

## 编码规范

### C/C++ 规范

#### 命名约定

```c
// 函数: 动词_名词 或 agentos_动词_名词()
int agentos_memory_write(const void* data, size_t len);

// 类型: 名词_t 或 AgentOs_Noun_T
typedef struct memory_record_s memory_record_t;

// 常量: AGENTOS_NOUNN 或 kNounn
#define AGENTOS_MAX_MEMORY_SIZE (1024 * 1024)
static const int kDefaultTimeout = 5000;

// 变量: camelCase 或 snake_case (根据上下文)
int memoryRecordCount;
size_t buffer_size;

// 宏: AGENTOS_MACRO_NAME()
#define AGENTOS_LOG_ERROR(fmt, ...) ...
```

#### 注释规范

所有公共 API 必须有 Doxygen 注释：

```c
/**
 * @brief 写入记忆到MemoryRovol系统
 *
 * @param data 要写入的数据指针
 * @param data_len 数据长度（字节）
 * @param metadata 元数据JSON字符串
 * @param[out] record_id 输出的记录ID（调用者需释放）
 *
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_ERR_INVALID_PARAM 参数无效
 * @return AGENTOS_ERR_NO_MEMORY 内存不足
 *
 * @note 此函数会自动进行L1→L2抽象
 * @warning 调用者必须释放返回的record_id内存
 *
 * @code
 * char* record_id = NULL;
 * int ret = agentos_memory_write(data, len, meta, &record_id);
 * if (ret == AGENTOS_SUCCESS) {
 *     // 使用 record_id
 *     free(record_id);
 * }
 * @endcode
 */
AGENTOS_EXPORT int agentos_memory_write(
    const void* data,
    size_t data_len,
    const char* metadata,
    char** record_id);
```

- 遵循 [C/C++ 编码规范](./docs/Capital_Specifications/)
- 使用 `.clang-format` 和 `.clang-tidy` 进行代码格式化

### Python 规范

- 遵循 PEP 8
- 使用类型注解
- 编写完整 docstring

```python
def memory_write(
    data: bytes,
    metadata: Optional[Dict[str, Any]] = None
) -> str:
    """
    写入记忆到MemoryRovol系统
    
    Args:
        data: 要写入的数据
        metadata: 可选的元数据字典
        
    Returns:
        记录ID字符串
        
    Raises:
        ValueError: 当data为空时
        MemoryError: 当内存不足时
        
    Example:
        >>> record_id = memory_write(b"hello world", {"type": "text"})
        >>> print(record_id)
        'rec_abc123'
    """
    pass
```

### Go 规范

```go
// MemoryWrite 写入记忆到MemoryRovol系统
//
// Parameters:
//   - data: 要写入的数据字节切片
//   - opts: 可选的写入选项
//
// Returns:
//   - string: 记录ID
//   - error: 错误信息
//
// Example:
//
//	recordID, err := memory.Write([]byte("hello"), nil)
//	if err != nil {
//	    log.Fatal(err)
//	}
func MemoryWrite(data []byte, opts *WriteOptions) (string, error) {
	// implementation
}
```

### 通用要求

- **命名语义化**: 名称精确表达语义，避免缩写
- **错误处理**: 所有错误必须处理，不能忽略
- **资源管理**: 使用 RAII 模式，明确所有权
- **线程安全**: 明确标注函数的线程安全性
- **日志规范**: 使用统一日志系统，结构化输出

---

## 测试规范

### 测试分类

| 类型 | 目录 | 说明 |
|------|------|------|
| **单元测试** | tests/unit/ | 单个函数/类测试 |
| **集成测试** | tests/integration/ | 多模块交互测试 |
| **契约测试** | tests/contract/ | 接口契约验证 |
| **性能测试** | tests/perf/ | 性能基准测试 |
| **安全测试** | tests/security/ | 安全漏洞扫描 |

### 测试覆盖率要求

| 模块 | 目标覆盖率 | 当前覆盖率 |
|------|-----------|-----------|
| corekern | ≥95% | 95% |
| coreloopthree | ≥92% | 92% |
| memoryrovol | ≥90% | 90% |
| cupolas | ≥88% | 88% |
| daemon | ≥85% | 85% |
| commons | ≥88% | 88% |

### 测试命名约定

```python
class TestMemoryRovol:
    def test_write_should_return_valid_record_id(self):
        """测试正常写入应返回有效记录ID"""
        
    def test_write_with_empty_data_should_raise_error(self):
        """测试空数据应抛出异常"""
        
    def test_concurrent_writes_should_be_thread_safe(self):
        """测试并发写入应是线程安全的"""
```

### 运行测试

```bash
# 运行所有测试
cd tests && python run_tests.py

# 运行特定测试
python -m pytest tests/unit/coreloopthree/test_loop.py -v

# 生成覆盖率报告
make coverage
```

---

## 架构原则检查清单

在提交 PR 前，请检查是否符合五维正交原则：

### 维度一：系统观 (System View)
- [ ] S-1 反馈闭环: 是否实现完整的感知-决策-执行-反馈循环？
- [ ] S-2 层次分解: 是否保持清晰的层次结构？
- [ ] S-3 总体设计部: 是否有全局协调层？
- [ ] S-4 涌现性管理: 是否抑制负面涌现？

### 维度二：内核观 (Kernel View)
- [ ] K-1 内核极简: 内核是否只保留原子机制？
- [ ] K-2 接口契约化: 公共接口是否有完整契约定义？
- [ ] K-3 服务隔离: 守护进程是否独立运行？
- [ ] K-4 可插拔策略: 策略是否可运行时替换？

### 维度三：认知观 (Cognition View)
- [ ] C-1 双系统协同: 是否实现快慢路径分离？
- [ ] C-2 增量演化: 是否支持增量规划？
- [ ] C-3 记忆卷载: 记忆是否逐层提炼？
- [ ] C-4 遗忘机制: 是否有合理遗忘策略？

### 维度四：工程观 (Engineering View)
- [ ] E-1 安全内生: 安全是否内嵌于每个环节？
- [ ] E-2 可观测性: 是否提供完整指标和追踪？
- [ ] E-3 资源确定性: 资源生命周期是否确定？
- [ ] E-4 跨平台一致性: 多平台行为是否一致？

### 维度五：设计美学 (Aesthetic View)
- [ ] A-1 简约至上: 是否用最少接口提供最大价值？
- [ ] A-2 极致细节: 边界情况是否处理完善？
- [ ] A-3 人文关怀: 开发者体验是否友好？
- [ ] A-4 完美主义: 是否追求极致品质？

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
| `ci` | CI/CD 配置变更 | `ci: add caching for pip dependencies` |
| `revert` | 回滚提交 | `revert: fix IPC race condition` |

### Scope 范围

| Scope | 对应模块 |
|-------|----------|
| `atoms` | 内核层 |
| `daemon` | 服务层 |
| `cupolas` | 安全层 |
| `commons` | 基础库 |
| `toolkit` | SDK |
| `openlab` | 开放生态 |
| `docs` | 文档 |
| `ci` | CI/CD |

### 提交格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

**示例**:
```
feat(memoryrovol): add L4 pattern mining algorithm

Implement persistent homology analysis for pattern detection
in the MemoryRovol system, enabling automatic knowledge
abstraction from raw memory data.

- Add pattern_mining.c module
- Implement PH computation algorithm
- Add unit tests with >90% coverage

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

我们坚信：**开源因贡献而精彩！**

---

## 常见问题

### Q1: 我是新手，可以从哪里开始？

**A**: 欢迎！建议从以下任务开始：
1. 阅读[快速开始指南](docs/Capital_Guides/getting_started.md)
2. 选择一个标记为 `good first issue` 的任务
3. 加入我们的开发者社区获取帮助

### Q2: 我可以提交大型的功能吗？

**A**: 当然可以！但建议先创建 Issue 讨论设计方案，获得社区反馈后再实现。大型功能可能需要分多个 PR 提交。

### Q3: 我的代码风格和现有代码不一致怎么办？

**A**: 请遵循项目的编码规范。如果发现现有代码不符合规范，可以单独提一个 PR 来统一风格。

### Q4: 如何获得代码审查帮助？

**A**: 可以在 PR 中添加审查请求标签，或在社区频道寻求帮助。

---

## 联系方式

如有任何问题，请通过以下方式联系我们：

| 用途 | 联系方式 |
|------|---------|
| **技术支持** | lidecheng@spharx.cn |
| **安全问题** | wangliren@spharx.cn |
| **商务合作** | zhouzhixian@spharx.cn |
| **AtomGit Issues** | https://atomgit.com/spharx/agentos/issues |
| **Gitee Issues** | https://gitee.com/spharx/agentos/issues |
| **GitHub Issues** | https://github.com/SpharxTeam/AgentOS/issues |
| **GitHub Discussions** | https://github.com/SpharxTeam/AgentOS/discussions |

---

<div align="center">

**再次感谢您的贡献！🎉**

> *"From data intelligence emerges."*  
> **始于数据，终于智能。**

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
