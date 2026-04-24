"""
AgentOS 测试模板目录

本目录包含各类测试的标准模板文件。

## 模板文件

- `test_template.py` - Python 单元测试模板
- `test_template.c` - C 单元测试模板
- `test_template_integration.py` - 集成测试模板
- `test_template_security.py` - 安全测试模板

## 使用方法

1. 选择合适的模板文件
2. 复制到目标目录
3. 重命名为 `test_<module_name>.py` 或 `test_<module_name>.c`
4. 替换占位符为实际内容
5. 实现测试用例

## 测试命名规范

- 测试文件: `test_<module>.py` 或 `test_<module>.c`
- 测试类: `Test<Module>` 或 `Test<Module><Feature>`
- 测试方法: `test_<action>_<condition>_<expected_result>`

示例:
- 文件: `test_user_manager.py`
- 类: `TestUserManager`
- 方法: `test_create_user_with_valid_data_succeeds`

## 测试标记

使用 pytest 标记分类测试:

- `@pytest.mark.unit` - 单元测试
- `@pytest.mark.integration` - 集成测试
- `@pytest.mark.security` - 安全测试
- `@pytest.mark.slow` - 慢速测试
- `@pytest.mark.smoke` - 冒烟测试

## 最佳实践

1. 每个测试只验证一个行为
2. 使用描述性的测试名称
3. 保持测试独立，不依赖执行顺序
4. 使用 fixture 管理测试资源
5. 清理测试产生的副作用

Version: 2.0.0
"""
