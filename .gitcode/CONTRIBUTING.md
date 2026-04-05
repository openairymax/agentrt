# AgentOS GitCode 贡献指南

**版本**: 1.0.0  
**最后更新**: 2026-04-05  

---

## 🎯 欢迎贡献

感谢您考虑为 **AgentOS** 做出贡献！我们热烈欢迎各种形式的贡献，包括但不限于：

- 🐛 Bug 修复
- ✨ 新功能开发
- ⚡ 性能优化
- 📝 文档改进
- 🧪 测试用例
- 🔒 安全增强
- 🌍 国际化

---

## 📋 贡献前准备

### 1️⃣ 了解项目理念

在开始之前，请务必阅读以下文档：

**必读文档**：
- [📘 架构设计原则 V1.8](agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) - 五维正交体系
- [🔄 CoreLoopThree 架构](agentos/manuals/architecture/coreloopthree.md) - 三层认知循环
- [🧠 MemoryRovol 架构](agentos/manuals/architecture/memoryrovol.md) - 四层记忆系统
- [🛡️ cupolas 安全穹顶](agentos/cupolas/README.md) - 安全机制
- [⚙️ 编译指南](agentos/manuals/guides/build.md) - 构建步骤

**选读文档**：
- [🚀 快速开始](agentos/manuals/guides/quickstart.md)
- [🧪 测试指南](agentos/manuals/guides/testing.md)
- [🐳 部署指南](agentos/manuals/guides/deployment.md)

### 2️⃣ 开发环境搭建

#### 系统要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Ubuntu 22.04+ / macOS 13+ / Windows 11 (WSL2) |
| 编译器 | GCC 11+ / Clang 14+ (C11/C++17) |
| 构建工具 | CMake 3.20+, Ninja |
| Python | 3.10+ (OpenLab需要) |
| Go | 1.21+ (Go SDK) |
| Rust | 1.70+ (Rust SDK) |

#### 快速搭建

```bash
# 1. 克隆仓库
git clone https://gitcode.com/spharx/agentos.git && cd agentos

# 2. 安装依赖（Ubuntu）
sudo apt install -y build-essential cmake gcc g++ libssl-dev \
    ninja-build python3 python3-pip git

# 3. 构建项目
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build . --parallel $(nproc)

# 4. 运行测试
ctest --output-on-failure

# 5. （可选）安装OpenLab依赖
cd ../openlab && pip install -e ".[dev]"
```

### 3️⃣ 配置开发环境

#### IDE配置推荐

**VS Code**:
```jsonc
// .vscode/settings.json
{
  "files.associations": {
    "*.c": "c",
    "*.h": "c",
    "*.py": "python",
    "*.go": "go",
    "*.rs": "rust"
  },
  "editor.formatOnSave": true,
  "editor.tabSize": 4,
  "editor.insertSpaces": true,
  "C_Cpp.clang_format_path": ".clang-format",
  "python.formatting.provider": "black"
}
```

**Git配置**:
```bash
git config user.name "Your Name"
git config user.email "your.email@example.com"

# 设置提交模板（可选）
git config commit.template .gitmessage-template
```

---

## 🔄 工作流程

### 分支策略

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

### 提交规范

使用 **Conventional Commits** 规范：

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Type类型**:
- `feat`: 新功能
- `fix`: Bug修复
- `docs`: 文档更新
- `style`: 代码格式调整（不影响功能）
- `refactor`: 重构（既不是新功能也不是修复）
- `perf`: 性能优化
- `test`: 添加或修改测试
- `chore`: 构建过程、辅助工具变动
- `ci`: CI/CD配置变更
- `revert`: 回滚提交

**Scope范围**:
- `atoms`: 内核层
- `daemon`: 服务层
- `cupolas`: 安全层
- `commons`: 基础库
- `toolkit`: SDK
- `openlab`: 开放生态
- `docs`: 文档
- `ci`: CI/CD

**示例**:
```bash
feat(memoryrovol): add L4 pattern mining algorithm

Implement persistent homology analysis for pattern detection
in the MemoryRovol system, enabling automatic knowledge
abstraction from raw memory data.

- Add pattern_mining.c module
- Implement PH computation algorithm
- Add unit tests with >90% coverage

Closes #123
```

### Pull Request 流程

#### 1. 创建分支

```bash
# 功能分支
git checkout -b feature/add-pattern-mining develop

# 修复分支
git checkout -b bugfix/fix-memory-leak develop

# 紧急修复
git checkout -b hotfix/critical-security-fix main
```

#### 2. 开发和测试

```bash
# 编写代码
# ... (遵循编码规范)

# 格式化代码
find . -name "*.c" -o -name "*.h" | xargs clang-format -i
find . -name "*.py" | xargs black

# 构建测试
cd build && make -j$(nproc)

# 运行测试
ctest --output-on-failure

# 静态分析
cppcheck --enable=all agentos/atoms/src/
bandit -r agentos/cupolas/
```

#### 3. 提交PR

1. 推送到远程仓库
2. 在GitCode创建Pull Request
3. 填写PR模板（[PULL_REQUEST_TEMPLATE.md](PULL_REQUEST_TEMPLATE.md)）
4. 等待CI通过和代码审查

#### 4. PR审查清单

- [ ] 代码符合编码规范
- [ ] 所有测试通过
- [ ] 无编译器警告
- [ ] 无安全漏洞
- [ ] 文档已更新
- [ ] CHANGELOG已更新
- [ ] 架构原则检查通过

---

## 📏 编码规范

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
    char** record_id
);
```

### Python 规范

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

---

## 🧪 测试规范

### 测试分类

| 类型 | 目录 | 说明 |
|------|------|------|
| **单元测试** | tests/unit/ | 单个函数/类测试 |
| **集成测试** | tests/integration/ | 多模块交互测试 |
| **契约测试** | tests/contract/ | 接口契约验证 |
| **性能测试** | tests/perf/ | 性能基准测试 |
| **安全测试** | tests/security/ | 安全漏洞扫描 |

### 测试命名

```python
# Python测试命名
class TestMemoryRovol:
    def test_write_should_return_valid_record_id(self):
        """测试正常写入应返回有效记录ID"""
        
    def test_write_with_empty_data_should_raise_error(self):
        """测试空数据应抛出异常"""
        
    def test_concurrent_writes_should_be_thread_safe(self):
        """测试并发写入应是线程安全的"""
```

### 测试覆盖率要求

| 模块 | 目标覆盖率 | 当前覆盖率 |
|------|-----------|-----------|
| corekern | ≥95% | 95% |
| coreloopthree | ≥92% | 92% |
| memoryrovol | ≥90% | 90% |
| cupolas | ≥88% | 88% |
| daemon | ≥85% | 85% |
| commons | ≥88% | 88% |

---

## 🏗️ 架构原则检查清单

在提交PR前，请检查是否符合五维正交原则：

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

## ❓ 常见问题

### Q1: 我是新手，可以从哪里开始？

**A**: 欢迎！建议从以下任务开始：
1. 阅读[快速开始指南](agentos/manuals/guides/quickstart.md)
2. 选择一个标记为`good first issue`的任务
3. 加入我们的开发者社区获取帮助

### Q2: 我可以提交大型的功能吗？

**A**: 当然可以！但建议先创建Issue讨论设计方案，获得社区反馈后再实现。大型功能可能需要分多个PR提交。

### Q3: 我的代码风格和现有代码不一致怎么办？

**A**: 请遵循项目的编码规范。如果发现现有代码不符合规范，可以单独提一个PR来统一风格。

### Q4: 如何获得代码审查帮助？

**A**: 可以在PR中添加`@spharx-team/reviewers`标签请求审查，或在社区频道寻求帮助。

---

## 📞 联系方式

- **技术问题**: lidecheng@spharx.cn
- **安全问题**: wangliren@spharx.cn
- **商务合作**: zhouzhixian@spharx.cn
- **GitCode讨论区**: https://gitcode.com/spharx/agentos/issues

---

## 📜 许可证

通过贡献代码，您同意您的贡献将采用 **Apache License 2.0** 许可证授权。

---

**再次感谢您为AgentOS做出贡献！** 🎉

> *"From data intelligence emerges."*  
> **始于数据，终于智能。**

---

© 2026 SPHARX Ltd. All Rights Reserved.
