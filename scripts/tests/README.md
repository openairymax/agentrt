# AgentOS Scripts Tests

## 测试结构

```
tests/
├── shell/                    # Shell 脚本测试
│   ├── test_framework.sh     # 测试框架
│   └── test_common_utils.sh  # 通用工具测试
└── python/                   # Python 脚本测试
    ├── conftest.py           # pytest 配置
    └── test_core.py         # 核心模块测试
```

## 运行测试

### Python 测试

```bash
# 运行所有测试
pytest scripts/tests/python/ -v

# 带覆盖率
pytest scripts/tests/python/ --cov=scripts/core --cov-report=html

# 只运行特定测试
pytest scripts/tests/python/test_core.py::TestPluginRegistry -v
```

### Shell 测试

```bash
# 运行所有 Shell 测试
for test in scripts/tests/shell/test_*.sh; do
    bash "$test"
done

# 运行特定测试
bash scripts/tests/shell/test_common_utils.sh
```

## 测试框架

### Shell 测试框架

提供断言函数：
- `assert_true`, `assert_false`
- `assert_equal`, `assert_contains`
- `assert_file_exists`, `assert_dir_exists`
- `assert_command_exists`, `assert_not_empty`

### Python 测试框架

使用 pytest 框架，支持：
- fixtures
- parametrize
- async tests
- coverage

## CI/CD

GitHub Actions 工作流 `.github/workflows/scripts-ci.yml` 包含：

- Shell 脚本语法检查 (ShellCheck)
- Python 测试 (pytest)
- 安全扫描 (Bandit)
- 跨平台测试
- 性能基准测试