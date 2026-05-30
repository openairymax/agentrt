# Manager 测试套件

**模块路径**: `agentos/manager/tests/`
**版本**: v0.0.5

## 概述

`manager/tests/` 包含 Manager 模块的完整测试套件，涵盖配置语法校验、JSON Schema 验证、配置集成测试、审计日志验证和配置漂移检测器测试。测试套件遵循 ARCHITECTURAL_PRINCIPLES.md 的 **E-8 可测试性原则**，提供统一的测试运行器，支持选择性执行和详细报告输出。

## 目录结构

```
tests/
├── test_config_syntax.py         # 配置文件语法验证测试
├── test_schema_validation.py     # JSON Schema 校验测试
├── test_config_integration.py    # 配置集成测试
├── test_audit_log_validation.py  # 审计日志验证测试
├── test_drift_detector.py        # 配置漂移检测器测试
├── run_all_tests.py              # 测试套件运行器
└── README.md                     # 本文件
```

## 测试类型

### 1. 配置语法验证 (`test_config_syntax.py`)

验证所有配置文件的 JSON/YAML 格式正确性：

- YAML/JSON 语法合法性
- UTF-8 编码检查
- 环境变量格式验证
- 文件完整性检查

### 2. Schema 验证 (`test_schema_validation.py`)

验证配置内容是否符合 JSON Schema 定义：

- 9 个 Schema 文件的校验规则覆盖
- 272 项校验规则逐一验证
- 必填字段检查
- 值域约束验证
- 类型一致性检查

### 3. 配置集成测试 (`test_config_integration.py`)

验证配置在完整流程中的正确性和一致性：

- 跨模块配置依赖关系验证
- 配置合并（Base + Environment + Runtime）正确性
- 热重载后配置一致性
- 环境变量覆盖优先级

### 4. 审计日志验证 (`test_audit_log_validation.py`)

验证审计日志是否符合 `config-audit-log.schema.json` 规范：

- 7 种动作类型的日志格式验证
- 操作者信息完整性
- 校验和（SHA-256）正确性
- 变更项（changes）结构验证

### 5. 漂移检测器测试 (`test_drift_detector.py`)

验证配置漂移检测器的功能：

- 基线创建（SHA-256 哈希、文件元数据）
- 文件修改检测（DriftType.MODIFIED）
- 文件删除检测（DriftType.DELETED）
- 文件新增检测（DriftType.ADDED）
- 敏感文件严重程度（CRITICAL/WARNING/INFO）
- 报告导出（JSON / Markdown）
- 忽略模式验证
- CLI 接口测试

## 使用方式

### 通过 pytest 运行

```bash
# 运行所有测试
pytest manager/tests/ -v

# 运行指定测试
pytest manager/tests/test_config_syntax.py
pytest manager/tests/test_schema_validation.py
pytest manager/tests/test_drift_detector.py

# 生成 HTML 测试报告
pytest --html=report.html manager/tests/
```

### 通过统一运行器

```bash
# 运行所有测试
python manager/tests/run_all_tests.py

# 详细模式
python manager/tests/run_all_tests.py --verbose

# 指定配置目录
python manager/tests/run_all_tests.py --config-dir ./configs

# 只运行指定测试
python manager/tests/run_all_tests.py syntax schema
python manager/tests/run_all_tests.py integration
```

### 运行器参数

| 参数 | 缩写 | 说明 |
|------|------|------|
| `--config-dir` | `-c` | Manager 配置根目录路径（默认: `../`） |
| `--verbose` | `-v` | 输出详细测试信息 |
| `tests` | - | 要运行的测试名称关键词（可选：syntax / schema / integration） |

### 退出码

| 退出码 | 含义 |
|--------|------|
| 0 | 所有测试通过 |
| 1 | 存在失败的测试 |
| 2 | 参数错误或执行异常 |

## 配置-Schema 映射

| 配置文件 | Schema 文件 | 测试覆盖 |
|----------|-------------|---------|
| `kernel/settings.yaml` | `kernel-settings.schema.json` | 语法 + Schema + 集成 |
| `model/model.yaml` | `model.schema.json` | 语法 + Schema + 集成 |
| `security/policy.yaml` | `security-policy.schema.json` | 语法 + Schema + 集成 |
| `sanitizer/sanitizer_rules.json` | `sanitizer-rules.schema.json` | 语法 + Schema + 集成 |
| `logging/manager.yaml` | `logging.schema.json` | 语法 + Schema + 集成 |
| `manager_management.yaml` | `config-management.schema.json` | 语法 + Schema + 集成 |
| `service/tool_d/tool.yaml` | `tool-service.schema.json` | 语法 + Schema + 集成 |
| `agent/registry.yaml` | `agent-registry.schema.json` | 语法 + Schema + 集成 |
| `skill/registry.yaml` | `skill-registry.schema.json` | 语法 + Schema + 集成 |

## 测试示例

### 配置语法测试

```python
def test_yaml_syntax():
    """验证 YAML 文件语法正确性"""
    config_path = Path("kernel/settings.yaml")
    content = config_path.read_text(encoding='utf-8')
    data = yaml.safe_load(content)
    assert data is not None
```

### Schema 验证测试

```python
def test_kernel_config_schema():
    """验证内核配置符合 Schema 定义"""
    config = load_config("kernel/settings.yaml")
    schema = load_schema("kernel-settings.schema.json")
    assert validate(config, schema) is True
```

### 漂移检测测试

```python
def test_detect_modified_file():
    """测试检测文件修改"""
    detector = ConfigDriftDetector(config_dir)
    detector.create_baseline()

    # 修改文件
    modify_config("kernel/settings.yaml")

    report = detector.detect_drift()
    assert report.has_drift
    assert report.drifted_files == 1
```

## 依赖关系

| 组件 | 用途 |
|------|------|
| Python ≥ 3.10 | 运行环境 |
| pytest | 测试框架 |
| jsonschema | Schema 校验 |
| PyYAML | YAML 解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
