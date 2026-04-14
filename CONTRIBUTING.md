# 贡献指南 CONTRIBUTING GUIDE

**版本**: v2.0.0
**最后更新**: 2026-04-13
**状态**: 活跃

感谢您对 AgentOS 项目感兴趣！我们欢迎各种形式的贡献，包括代码提交、文档改进、Bug 报告和功能建议。

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
- [代码审查](#代码审查)
- [文档贡献](#文档贡献)
- [社区参与](#社区参与)
- [联系方式](#联系方式)

---

## 行为准则

本项目采用 [AgentOS 社区行为准则](./CODE_OF_CONDUCT.md)，请所有参与者遵守。该准则基于中国法律法规和 AgentOS 五维正交设计理念制定。

---

## 快速导航

### 问题反馈

| 平台 | 用途 | 链接 |
|------|------|------|
| AtomGit（推荐） | Bug 报告 / 功能建议 | https://atomgit.com/spharx/agentos/issues |
| Gitee | Bug 报告 / 功能建议 | https://gitee.com/spharx/agentos/issues |
| GitHub | Bug 报告 / 功能建议 | https://github.com/SpharxTeam/AgentOS/issues |

### 文档资源

| 资源 | 链接 |
|------|------|
| 架构设计原则 | [ARCHITECTURAL_PRINCIPLES.md](./docs/ARCHITECTURAL_PRINCIPLES.md) |
| API 规范 | [docs/Capital_API/](./docs/Capital_API/) |
| 编码标准 | [docs/Capital_Specifications/coding_standard/](./docs/Capital_Specifications/coding_standard/) |
| 架构文档 | [docs/Capital_Architecture/](./docs/Capital_Architecture/) |
| 使用指南 | [docs/Capital_Guides/](./docs/Capital_Guides/) |
| 测试指南 | [tests/TESTING_GUIDELINES.md](./tests/TESTING_GUIDELINES.md) |
| 社区治理 | [COMMUNITY.md](./COMMUNITY.md) |

---

## 开发环境搭建

### 贡献前必读文档

| 文档 | 说明 | 优先级 |
|------|------|--------|
| [架构设计原则 V1.8](./docs/ARCHITECTURAL_PRINCIPLES.md) | 五维正交体系 | 必读 |
| [CoreLoopThree 架构](./agentos/atoms/coreloopthree/README.md) | 三层认知循环 | 必读 |
| [MemoryRovol 架构](./agentos/atoms/memoryrovol/README.md) | 四层记忆系统 | 必读 |
| [cupolas 安全穹顶](./agentos/cupolas/README.md) | 安全机制 | 必读 |
| [API 规范](./docs/Capital_API/README.md) | 系统调用接口 | 按需 |
| [编码标准](./docs/Capital_Specifications/coding_standard/) | 各语言编码规范 | 必读 |

### 1. 基础要求

| 项目 | 要求 | 说明 |
|------|------|------|
| 操作系统 | Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2) | 推荐 Ubuntu |
| 编译器 | GCC 11+ / Clang 14+ / MSVC 2022+ | 支持 C11/C++17 |
| 构建工具 | CMake 3.20+, Ninja | |
| Python | 3.10+ | OpenLab/Manager 需要 |
| Go | 1.21+ | Go SDK 开发需要 |
| Rust | 1.70+ | Rust SDK 开发需要 |
| Node.js | 18+ | TypeScript SDK 开发需要 |

系统依赖（Ubuntu）：

```
OpenSSL >= 1.1.1
libevent >= 2.1
FAISS >= 1.7.0 (可选，用于向量检索)
SQLite3 >= 3.35
libcurl >= 7.68
cJSON >= 1.7.15
```

### 2. Fork 和克隆项目

```bash
# 1. 在代码托管平台 Fork 本项目
#    AtomGit（推荐）: https://atomgit.com/spharx/agentos
#    Gitee: https://gitee.com/spharx/agentos
#    GitHub: https://github.com/SpharxTeam/AgentOS

# 2. 克隆您的 fork
git clone https://atomgit.com/YOUR_USERNAME/agentos.git
cd agentos

# 3. 添加上游仓库
git remote add upstream https://atomgit.com/spharx/agentos.git

# 4. 验证远程仓库配置
git remote -v
```

### 3. 安装依赖

#### C/C++ 依赖（Ubuntu）

```bash
sudo apt install -y build-essential cmake gcc g++ libssl-dev \
    ninja-build python3 python3-pip git libevent-dev libsqlite3-dev \
    libcurl4-openssl-dev
```

#### Python 依赖

```bash
# 使用 Poetry（推荐）
curl -sSL https://install.python-poetry.org | python3 -
cd agentos/openlab && poetry install && poetry shell

# 或使用 pip
python3 -m venv venv && source venv/bin/activate
cd agentos/openlab && pip install -r requirements.txt
```

#### vcpkg C++ 包管理

```bash
# 项目使用 vcpkg 管理 C++ 依赖
# 参见 vcpkg.json 中的依赖声明
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.sh
```

### 4. 构建项目

```bash
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build . --parallel $(nproc)
ctest --output-on-failure
```

### 5. 开发工具配置

#### Git 提交模板

```bash
# 配置提交模板（遵循 Conventional Commits 规范）
git config commit.template .gitmessage-template
```

如果项目根目录没有 `.gitmessage-template` 文件，可创建如下模板：

```
<type>(<scope>): <subject>

<body>

<footer>
```

#### VS Code 配置

项目已包含 `.clang-format` 和 `.clang-tidy` 配置文件。推荐 VS Code 扩展：

- C/C++ (Microsoft)
- Python (Microsoft)
- rust-analyzer
- Go
- clangd

#### 代码格式化

```bash
# C/C++
find agentos -name "*.c" -o -name "*.h" | xargs clang-format -i

# Python
black agentos/openlab/ agentos/manager/
isort agentos/openlab/ agentos/manager/

# 检查
flake8 agentos/openlab/ agentos/manager/
mypy agentos/openlab/
```

---

## 分支模型

AgentOS 采用简洁稳定的 Git 分支模型：

```
main (生产分支，受保护)
  ↑
  │ merge (squash)
  │
develop (开发分支)
  ↑
  │ merge PR
  │
feature/xxx    (功能分支)
bugfix/xxx     (修复分支)
hotfix/xxx     (紧急修复)
refactor/xxx   (重构分支)
docs/xxx       (文档分支)
```

### 分支命名规范

| 分支类型 | 前缀 | 示例 | 生命周期 |
|---------|------|------|---------|
| 功能分支 | `feature/` | `feature/memory-l4-pattern` | 合并后删除 |
| Bug 修复 | `bugfix/` | `bugfix/ipc-race-condition` | 合并后删除 |
| 紧急修复 | `hotfix/` | `hotfix/security-patch` | 合并后删除 |
| 文档改进 | `docs/` | `docs/api-reference` | 合并后删除 |
| 性能优化 | `perf/` | `perf/faiss-indexing` | 合并后删除 |
| 重构 | `refactor/` | `refactor/error-handling` | 合并后删除 |
| 测试 | `test/` | `test/syscall-coverage` | 合并后删除 |

### 分支操作规范

```bash
# 创建分支（始终从 develop 创建）
git checkout develop && git pull upstream develop
git checkout -b feature/your-feature-name

# 保持分支同步
git fetch upstream
git rebase upstream/develop

# 清理已合并分支
git branch -d feature/your-feature-name
```

---

## 贡献流程

### 第一步：选择任务

1. 查看 Issue 列表，寻找标记为以下标签的任务：
   - `good first issue` — 适合新贡献者
   - `help wanted` — 需要社区帮助
   - `bug` — Bug 修复
   - `enhancement` — 功能增强
2. 如果没有相关 Issue，先创建 Issue 描述您的计划
3. 等待维护者确认后再开始工作，避免重复劳动

### 第二步：开发实现

```bash
# 从 develop 创建工作分支
git checkout -b feature/your-feature-name

# 编码 → 测试 → 格式化 → 提交（循环）
```

### 第三步：质量保证

在提交 PR 前，确保通过以下检查：

```bash
# 1. 代码格式化
find agentos -name "*.c" -o -name "*.h" | xargs clang-format -i
black agentos/openlab/ && isort agentos/openlab/

# 2. 静态分析
cppcheck --enable=all agentos/
flake8 agentos/openlab/
mypy agentos/openlab/

# 3. 构建验证
cd build && cmake --build . --parallel $(nproc)

# 4. 运行测试
ctest --output-on-failure
cd ../tests && python -m pytest -v

# 5. 安全检查
bandit -r agentos/openlab/
```

### 第四步：提交和推送

```bash
git add .
git commit -m "feat(scope): description of change"
git push origin feature/your-feature-name
```

### 第五步：创建 Pull Request

在 AtomGit/Gitee/GitHub 上创建 Pull Request，填写以下信息：

1. **变更说明**：清晰描述本次变更的内容和原因
2. **关联 Issue**：使用 `Closes #xxx` 或 `Fixes #xxx` 关联
3. **测试说明**：说明如何验证本次变更
4. **影响范围**：说明本次变更影响的模块

### 第六步：代码审查

维护者会审查您的代码。请积极响应审查意见，及时更新代码。

---

## 编码规范

### C/C++ 规范

详细规范参见 [C/C++ 编码标准](./docs/Capital_Specifications/coding_standard/)。

#### 命名约定

```c
// 函数: agentos_动词_名词()
int agentos_memory_write(const void* data, size_t len);

// 类型: 名词_t
typedef struct memory_record_s memory_record_t;

// 常量: AGENTOS_NOUNN 或 kNounn
#define AGENTOS_MAX_MEMORY_SIZE (1024 * 1024)
static const int kDefaultTimeout = 5000;

// 宏: AGENTOS_MACRO_NAME()
#define AGENTOS_LOG_ERROR(fmt, ...) ...

// 文件: 模块_子模块.c/h
// 例: memory_write.c, syscall_table.h
```

#### 注释规范

所有公共 API 必须有 Doxygen 注释：

```c
/**
 * @brief 写入记忆到 MemoryRovol 系统
 *
 * @param data     要写入的数据指针
 * @param data_len 数据长度（字节）
 * @param metadata 元数据 JSON 字符串
 * @param[out] record_id 输出的记录 ID（调用者需释放）
 *
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_ERR_INVALID_PARAM 参数无效
 * @return AGENTOS_ERR_NO_MEMORY 内存不足
 *
 * @note 此函数会自动进行 L1→L2 抽象
 * @warning 调用者必须释放返回的 record_id 内存
 */
AGENTOS_EXPORT int agentos_memory_write(
    const void* data, size_t data_len,
    const char* metadata, char** record_id);
```

#### 内存管理

```c
// 使用项目统一内存分配器
void* ptr = AGENTOS_MALLOC(size);
AGENTOS_FREE(ptr);

char* str = AGENTOS_STRDUP(src);
AGENTOS_FREE(str);

// 禁止使用裸 malloc/free
// void* ptr = malloc(size);   // 错误
// free(ptr);                   // 错误
```

#### 安全编码

```c
// 使用安全字符串函数
strncpy(dst, src, dst_size - 1);
dst[dst_size - 1] = '\0';

// 禁止使用不安全函数
// strcpy(dst, src);           // 禁止
// gets(buf);                   // 禁止
// sprintf(buf, "%s", str);    // 禁止，使用 snprintf
```

### Python 规范

- 遵循 PEP 8，使用 Black 格式化
- 使用类型注解
- 编写完整 docstring（Google 风格）

```python
def memory_write(
    data: bytes,
    metadata: Optional[Dict[str, Any]] = None
) -> str:
    """写入记忆到 MemoryRovol 系统.

    Args:
        data: 要写入的数据.
        metadata: 可选的元数据字典.

    Returns:
        记录 ID 字符串.

    Raises:
        ValueError: 当 data 为空时.
        MemoryError: 当内存不足时.
    """
    pass
```

### Go 规范

- 遵循 Effective Go
- 使用 gofmt 格式化
- 包注释说明包的用途

```go
// Package memory 提供与 MemoryRovol 系统交互的 Go SDK.
package memory

// Write 写入记忆到 MemoryRovol 系统.
func Write(data []byte, opts *WriteOptions) (string, error) {
    // implementation
}
```

### Rust 规范

- 遵循 Rust API Guidelines
- 使用 cargo fmt 格式化
- 使用 cargo clippy 检查

```rust
/// 写入记忆到 MemoryRovol 系统.
///
/// # Arguments
/// * `data` - 要写入的数据字节切片
/// * `opts` - 可选的写入选项
///
/// # Returns
/// 记录 ID 字符串
///
/// # Errors
/// 当数据为空或内存不足时返回错误
pub fn memory_write(data: &[u8], opts: Option<WriteOptions>) -> Result<String, AgentOsError> {
    // implementation
}
```

### 通用要求

| 要求 | 说明 |
|------|------|
| 命名语义化 | 名称精确表达语义，遵循 E-5 原则 |
| 错误处理 | 所有错误必须处理，禁止忽略返回值 |
| 资源管理 | 使用 RAII 模式，明确所有权 |
| 线程安全 | 明确标注函数的线程安全性 |
| 日志规范 | 使用统一日志系统，结构化输出 |
| 安全编码 | 遵循安全编码标准，禁止使用不安全函数 |

---

## 测试规范

详细规范参见 [tests/TESTING_GUIDELINES.md](./tests/TESTING_GUIDELINES.md)。

### 测试目录结构

| 类型 | 目录 | 说明 |
|------|------|------|
| 单元测试 | `tests/unit/` | 单个函数/模块测试 |
| 集成测试 | `tests/integration/` | 多模块交互测试 |
| 契约测试 | `tests/contract/` | 接口契约验证 |
| 端到端测试 | `tests/e2e/` | 完整工作流测试 |
| 安全测试 | `tests/security/` | 安全漏洞扫描 |
| 性能基准 | `tests/benchmarks/` | 性能基准测试 |
| 模糊测试 | `tests/fuzz/` | 模糊测试 |

### 测试覆盖率要求

| 模块 | 目标覆盖率 |
|------|-----------|
| atoms/corekern | ≥95% |
| atoms/coreloopthree | ≥92% |
| atoms/memoryrovol | ≥90% |
| atoms/syscall | ≥95% |
| atoms/taskflow | ≥90% |
| cupolas | ≥88% |
| daemon | ≥85% |
| commons | ≥88% |

### 测试命名约定

```python
class TestMemoryRovol:
    def test_write_should_return_valid_record_id(self):
        """测试正常写入应返回有效记录 ID."""

    def test_write_with_empty_data_should_raise_error(self):
        """测试空数据应抛出异常."""

    def test_concurrent_writes_should_be_thread_safe(self):
        """测试并发写入应是线程安全的."""
```

### 运行测试

```bash
# 运行全部测试
cd tests && python run_tests.py

# 运行特定模块测试
python -m pytest tests/unit/coreloopthree/ -v
python -m pytest tests/integration/syscall/ -v

# 运行 C 测试
cd build && ctest --output-on-failure

# 生成覆盖率报告
cd tests && python -m pytest --cov=agentos --cov-report=html
```

---

## 架构原则检查清单

在提交 PR 前，请检查是否符合五维正交原则：

### 维度一：系统观 (System View)

- [ ] S-1 反馈闭环：是否实现完整的感知-决策-执行-反馈循环？
- [ ] S-2 层次分解：是否保持清晰的层次结构？
- [ ] S-3 总体设计部：是否有全局协调层？
- [ ] S-4 涌现性管理：是否抑制负面涌现？

### 维度二：内核观 (Kernel View)

- [ ] K-1 内核极简：内核是否只保留原子机制？
- [ ] K-2 接口契约化：公共接口是否有完整契约定义？
- [ ] K-3 服务隔离：守护进程是否独立运行？
- [ ] K-4 可插拔策略：策略是否可运行时替换？

### 维度三：认知观 (Cognition View)

- [ ] C-1 双系统协同：是否实现快慢路径分离？
- [ ] C-2 增量演化：是否支持增量规划？
- [ ] C-3 记忆卷载：记忆是否逐层提炼？
- [ ] C-4 遗忘机制：是否有合理遗忘策略？

### 维度四：工程观 (Engineering View)

- [ ] E-1 安全内生：安全是否内嵌于每个环节？
- [ ] E-2 可观测性：是否提供完整指标和追踪？
- [ ] E-3 资源确定性：资源生命周期是否确定？
- [ ] E-4 跨平台一致性：多平台行为是否一致？

### 维度五：设计美学 (Aesthetic View)

- [ ] A-1 简约至上：是否用最少接口提供最大价值？
- [ ] A-2 极致细节：边界情况是否处理完善？
- [ ] A-3 人文关怀：开发者体验是否友好？
- [ ] A-4 完美主义：是否追求极致品质？

---

## 提交规范

AgentOS 采用 **Conventional Commits** 规范。

### 提交类型

| 类型 | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat(syscall): add skill management API` |
| `fix` | Bug 修复 | `fix(ipc): resolve race condition in binder` |
| `docs` | 文档更新 | `docs(api): update memory syscall reference` |
| `style` | 代码格式 | `style(atoms): format with clang-format` |
| `refactor` | 重构 | `refactor(cupolas): simplify permission engine` |
| `perf` | 性能优化 | `perf(memoryrovol): optimize FAISS indexing` |
| `test` | 测试 | `test(daemon): add llm_d unit tests` |
| `chore` | 构建/工具 | `chore(ci): update CMakeLists.txt` |
| `ci` | CI/CD 配置 | `ci: add caching for pip dependencies` |
| `revert` | 回滚提交 | `revert: fix ipc race condition` |

### Scope 范围

| Scope | 对应模块 |
|-------|----------|
| `corekern` | 微内核核心 |
| `coreloopthree` | 三层认知循环 |
| `memoryrovol` | 记忆卷载系统 |
| `syscall` | 系统调用层 |
| `taskflow` | 任务流 |
| `cupolas` | 安全穹顶 |
| `daemon` | 守护进程服务 |
| `gateway` | 协议网关 |
| `heapstore` | 堆存储 |
| `commons` | 基础库 |
| `toolkit` | 多语言 SDK |
| `openlab` | 开放实验室 |
| `manager` | 配置管理器 |
| `docs` | 文档 |
| `ci` | CI/CD |

### 提交格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

**示例**：

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

### 提交规范要点

- subject 不超过 50 个字符
- body 每行不超过 72 个字符
- 一个提交只做一件事
- 禁止提交调试代码、临时文件
- 禁止提交密钥、密码等敏感信息

---

## 代码审查

### PR 审查清单

提交 PR 前，请确保以下项目全部通过：

- [ ] 代码符合编码规范（已通过格式化和静态分析）
- [ ] 所有测试通过（单元测试 + 集成测试）
- [ ] 无编译器警告
- [ ] 无安全漏洞（已通过安全检查）
- [ ] 文档已更新（API 文档、架构文档）
- [ ] 架构原则检查通过
- [ ] 新增代码有对应的测试用例
- [ ] 无硬编码的密钥、路径或 IP 地址

### 合并策略

| 分支类型 | 合并方式 | 说明 |
|---------|---------|------|
| 功能分支 | Squash and merge | 压缩为单个提交 |
| Bug 修复 | Merge commit | 保留完整修复历史 |
| 紧急修复 | Rebase and merge | 保持线性历史 |

### 审查响应时间

| 审查类型 | 目标响应时间 |
|---------|------------|
| 初次审查 | 3 个工作日内 |
| 后续审查 | 1 个工作日内 |
| 紧急修复 | 24 小时内 |

---

## 文档贡献

### 文档结构

AgentOS 文档体系分为以下部分：

| 目录 | 内容 | 贡献方式 |
|------|------|---------|
| `docs/ARCHITECTURAL_PRINCIPLES.md` | 架构设计原则 | 需社区委员会审批 |
| `docs/Capital_Architecture/` | 架构文档 | PR 提交 |
| `docs/Capital_API/` | API 规范 | PR 提交 |
| `docs/Capital_Guides/` | 使用指南 | PR 提交 |
| `docs/Capital_Specifications/` | 编码标准 | 需核心维护者审批 |
| `docs/Basic_Theories/` | 基础理论 | 需核心维护者审批 |
| `docs/White_Paper/` | 白皮书 | 需社区委员会审批 |
| `agentos/*/README.md` | 模块文档 | 随代码 PR 提交 |

### 文档编写规范

1. 使用中文编写，技术术语保留英文原文
2. 代码示例必须可运行、可验证
3. 文档内链接使用相对路径
4. 遵循 Markdown 格式规范
5. 新增文档需在对应目录的 README.md 中添加索引

---

## 社区参与

AgentOS 是一个基于多体控制论智能系统 (MCIS) 理论构建的协作社区。详细社区治理框架请参考 [COMMUNITY.md](./COMMUNITY.md)。

### 贡献者成长路径

| 阶段 | 任务类型 | 典型工作 |
|------|---------|---------|
| 新手 | 文档改进、Bug 报告 | 修复文档错误、补充测试用例 |
| 常规贡献 | 功能开发、代码重构 | 实现新 API、优化算法 |
| 核心贡献 | 架构设计、安全加固 | 设计新模块、审查 PR |

### 社区活动

| 活动 | 频率 | 目标 |
|------|------|------|
| 社区例会 | 每月 | 同步进展、讨论议题 |
| 技术分享 | 每两周 | 技术深度交流 |
| 代码静修 | 每季度 | 集中解决技术债务 |

### 认可与感谢

所有贡献者将被记录在以下位置：

- [AUTHORS.md](./AUTHORS.md) — 核心贡献者名单
- [ACKNOWLEDGMENTS.md](./ACKNOWLEDGMENTS.md) — 感谢名单

---

## 常见问题

### Q1: 我是新手，可以从哪里开始？

建议从以下任务开始：
1. 阅读架构设计原则文档
2. 选择一个标记为 `good first issue` 的任务
3. 从文档改进或测试用例补充入手

### Q2: 我可以提交大型功能吗？

可以，但建议先创建 Issue 讨论设计方案，获得社区反馈后再实现。大型功能应分多个 PR 提交，每个 PR 聚焦一个独立功能。

### Q3: 我的代码风格和现有代码不一致怎么办？

请遵循项目的编码规范。项目提供了 `.clang-format`（C/C++）和 Black 配置（Python），使用这些工具自动格式化即可。

### Q4: 如何处理跨模块的变更？

跨模块变更需要：
1. 在 Issue 中说明变更影响范围
2. 拆分为多个独立 PR，每个 PR 只影响一个模块
3. 在 PR 描述中说明与其他 PR 的依赖关系

### Q5: 报告安全漏洞的流程是什么？

请勿在公开 Issue 中报告安全漏洞。请发送邮件至 wangliren@spharx.cn，详细流程参见 [SECURITY.md](./SECURITY.md)。

---

## 联系方式

| 用途 | 联系方式 |
|------|---------|
| 技术支持 | lidecheng@spharx.cn |
| 安全问题 | wangliren@spharx.cn |
| 商务合作 | zhouzhixian@spharx.cn |
| 行为准则 | conduct@spharx.cn |
| AtomGit Issues | https://atomgit.com/spharx/agentos/issues |
| Gitee Issues | https://gitee.com/spharx/agentos/issues |
| GitHub Issues | https://github.com/SpharxTeam/AgentOS/issues |
| GitHub Discussions | https://github.com/SpharxTeam/AgentOS/discussions |

---

<div align="center">

**感谢您的贡献！**

*From data intelligence emerges*

© 2026 SPHARX Ltd. 保留所有权利。

</div>
