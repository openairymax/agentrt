# 贡献指南 CONTRIBUTING GUIDE

感谢您对 AgentOS 项目感兴趣！我们欢迎各种形式的贡献，包括代码提交、文档改进、问题报告和功能建议。

## 📋 目录

- [行为准则](#行为准则)
- [快速导航](#快速导航)
- [开发环境设置](#开发环境设置)
- [贡献流程](#贡献流程)
- [代码规范](#代码规范)
- [测试要求](#测试要求)
- [提交规范](#提交规范)
- [常见问题](#常见问题)

---

## 行为准则

本项目采用 [Contributor Covenant](CODE_OF_CONDUCT.md) 作为行为准则。请确保您的行为符合准则要求，共同维护友好、包容的社区环境。

---

## 快速导航

<!-- From data intelligence emerges. by spharx -->
- 🐛 [报告 Bug](https://gitee.com/spharx/agentos/issues/new?issue_type=bug) (Gitee 官方)
- 💡 [提出功能建议](https://gitee.com/spharx/agentos/issues/new?issue_type=feature) (Gitee 官方)
- 📖 [查看文档](partdocs/)
- 💬 [参与讨论](https://github.com/spharx-team/AgentOS/discussions) (GitHub 镜像)

---

## 开发环境设置

### 1. Fork 和克隆项目

```bash
# 1. 在 Gitee/GitHub上Fork本项目
#    官方仓库：https://gitee.com/spharx/agentos
#    镜像仓库：https://github.com/spharx-team/AgentOS
#
# 2. 克隆您的 fork
git clone https://gitee.com/YOUR_USERNAME/agentos.git
# 或
git clone https://github.com/YOUR_USERNAME/AgentOS.git

cd agentos  # 或 cd AgentOS

# 3. 添加上游仓库
git remote add upstream https://gitee.com/spharx/agentos.git
# 或
git remote add upstream https://github.com/spharx-team/AgentOS.git
```

### 2. 安装依赖

#### 使用 Poetry（推荐）

```bash
# 安装 Poetry
curl -sSL https://install.python-poetry.org | python3 -

# 安装依赖
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
pip install -e .
pip install -e ".[dev]"
```

### 3. 配置预提交钩子

```bash
# 安装 pre-commit
pre-commit install

# 验证安装
pre-commit --version
```

### 4. 验证环境

```bash
# 运行环境验证脚本
./validate.sh

# 或快速体验
./quickstart.sh
```

---

## 贡献流程

### 1. 选择 Issue

- 查看 [GitHub Issues](https://github.com/spharx/spharxworks/issues)
- 寻找标记为 `good first issue` 或 `help wanted` 的问题
- 在 Issue 下评论表示您要接手

### 2. 创建分支

```bash
# 从 main 分支创建新分支
git checkout -b feature/your-feature-name
# 或
git checkout -b fix/issue-123
```

**分支命名规范**:
- `feature/xxx` - 新功能
- `fix/xxx` - Bug 修复
- `docs/xxx` - 文档更新
- `refactor/xxx` - 代码重构
- `test/xxx` - 测试相关
- `chore/xxx` - 构建/工具相关

### 3. 进行修改

- 编写代码和测试
- 确保所有测试通过
- 更新相关文档

### 4. 提交更改

```bash
# 添加文件
git add .

# 提交（遵循提交规范）
git commit -m "feat: add new memory retrieval algorithm"

# 推送到远程
git push origin feature/your-feature-name
```

### 5. 创建 Pull Request

1. 在 GitHub 上访问您的 fork
2. 点击 "Compare & pull request"
3. 填写 PR 模板
4. 等待 CI 检查通过
5. 请求维护者审查

### 6. 代码审查

- 回应审查意见
- 进行必要的修改
- 获得批准后合并

---

## 代码规范

### Python 代码

遵循 [PEP 8](https://pep8.org/) 标准：

```python
# 好的示例
def calculate_memory_similarity(query: str, documents: list[str]) -> float:
    """计算查询与文档列表的相似度"""
    if not documents:
        return 0.0

    # 实现逻辑
    pass


class MemoryIndex:
    """记忆索引管理类"""

    def __init__(self, dimension: int):
        self.dimension = dimension
```

**格式化**:
```bash
# 使用 black 格式化
black .

# 排序 imports
isort .
```

### C++ 代码

遵循 Google C++ Style Guide：

```cpp
// 好的示例
class MemoryManager {
 public:
  explicit MemoryManager(size_t capacity);
  ~MemoryManager();

  bool Store(const std::string& key, const MemoryBlock& block);
  std::optional<MemoryBlock> Retrieve(const std::string& key);

 private:
  size_t capacity_;
  std::unordered_map<std::string, MemoryBlock> cache_;
};
```

**格式化**:
```bash
# 使用 clang-format
clang-format -i src/**/*.cpp
```

### 文档规范

- 使用清晰的中文或英文
- 代码示例要完整可运行
- 包含必要的注释和说明

---

## 测试要求

### 编写测试

```python
# tests/unit/test_memory.py
import pytest
from agentos.memory import MemoryIndex


class TestMemoryIndex:
    """测试记忆索引功能"""

    def test_create_index(self):
        """测试创建索引"""
        index = MemoryIndex(dimension=768)
        assert index.dimension == 768

    def test_add_vectors(self):
        """测试添加向量"""
        index = MemoryIndex(dimension=768)
        vectors = [[0.1] * 768, [0.2] * 768]
        index.add(vectors)
        assert index.size() == 2
```

### 运行测试

```bash
# 运行所有测试
make test

# 运行单元测试
make test-unit

# 运行特定测试
pytest tests/unit/test_memory.py -v

# 生成覆盖率报告
pytest --cov=agentos --cov-report=html
```

### 测试覆盖率

- 新功能必须包含单元测试
- 核心模块覆盖率 > 80%
- 关键路径覆盖率 > 90%

---

## 提交规范

### Commit Message 格式

遵循 [Conventional Commits](https://www.conventionalcommits.org/)：

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Type 类型

- `feat`: 新功能
- `fix`: Bug 修复
- `docs`: 文档更新
- `style`: 代码格式调整
- `refactor`: 代码重构
- `test`: 测试相关
- `chore`: 构建/工具相关

### 示例

```bash
# 新功能
feat(memory): add FAISS vector index support

# Bug 修复
fix(scheduler): resolve race condition in task queue

# 文档更新
docs(readme): update installation instructions

# 代码重构
refactor(core): extract memory management to separate class
```

---

## 常见问题

### Q: 如何开始第一个贡献？

A: 从简单的任务开始，比如：
- 修复文档中的拼写错误
- 改进错误消息
- 添加单元测试
- 优化性能瓶颈

### Q: 遇到问题怎么办？

A: 可以通过以下方式获取帮助：
- 查看现有文档
- 在 Issue 中提问
- 在 Discussions 中讨论
- 联系维护者

### Q: 多久能得到回复？

A: 我们承诺：
- Issue: 48 小时内回复
- PR: 5 个工作日内审查
- 问题咨询：尽快回复

### Q: 可以提交破坏性变更吗？

A: 破坏性变更需要：
1. 提前在 Issue 中讨论
2. 提供迁移指南
3. 获得至少 2 个维护者批准
4. 在主版本更新时引入

---

## 认可

所有贡献者都将被列入 [AUTHORS.md](AUTHORS.md)，并在 CHANGELOG 中被提及。

重要贡献还将被：
- 在官方博客中介绍
- 邀请成为核心贡献者
- 获得项目纪念品（可选）

---

## 联系方式

- **Gitee Issues**: https://gitee.com/spharx/agentos/issues (官方，首选)
- **GitHub Issues**: https://github.com/spharx-team/AgentOS/issues (镜像)
- **技术支持**: lidecheng@spharx.cn
- **安全问题**: wangliren@spharx.cn
- **商务合作**: zhouzhixian@spharx.cn
- **官方网站**: https://spharx.cn

### 仓库链接

- **官方仓库**: https://gitee.com/spharx/agentos
- **镜像仓库**: https://github.com/spharx-team/AgentOS

---

感谢您的贡献！🎉

<div align="center">

**SPHARX 极光感知科技**

*From data intelligence emerges*

</div>

---

© 2026 SPHARX Ltd. 保留所有权利。
