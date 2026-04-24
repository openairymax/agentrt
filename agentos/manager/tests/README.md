# Manager 测试套件

`manager/tests/` 包含 Manager 模块的测试用例，涵盖配置语法校验、Schema 验证和集成测试。

## 测试类型

| 类型 | 说明 |
|------|------|
| **配置语法测试** | 验证配置文件的 JSON 格式正确性 |
| **Schema 验证测试** | 验证配置内容是否符合 JSON Schema 定义 |
| **集成测试** | 验证配置在完整流程中的正确性和一致性 |

## 使用方式

```bash
# 运行所有测试
pytest manager/tests/

# 运行指定测试
pytest manager/tests/test_config_syntax.py

# 详细输出
pytest -v manager/tests/

# 生成测试报告
pytest --html=report.html manager/tests/
```

## 配置-Schema 映射

| 配置文件 | Schema 文件 |
|----------|-------------|
| `kernel_config.json` | `kernel.schema.json` |
| `model_config.json` | `model.schema.json` |
| `security_config.json` | `security.schema.json` |
| `sanitizer_config.json` | `sanitizer.schema.json` |
| `logging_config.json` | `logging.schema.json` |
| `management_config.json` | `management.schema.json` |
| `deployment_config.json` | `deployment.schema.json` |
| `tls_config.json` | `tls.schema.json` |
| `auth_config.json` | `auth.schema.json` |

## 测试示例

```python
def test_kernel_config_schema():
    """验证内核配置符合 Schema 定义"""
    config = load_config("kernel_config.json")
    schema = load_schema("kernel.schema.json")
    assert validate(config, schema) is True
```

---

*AgentOS Manager — Tests*
