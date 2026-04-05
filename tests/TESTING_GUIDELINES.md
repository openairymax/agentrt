# AgentOS 测试代码规范

**版本**: 1.0.0
**最后更�?*: 2026-03-26
**状�?*: 正式发布

---

## 📋 目录

1. [概述](#概述)
2. [C 测试规范](#c-测试规范)
3. [Python 测试规范](#python-测试规范)
4. [测试组织结构](#测试组织结构)
5. [代码质量标准](#代码质量标准)
6. [最佳实践](#最佳实�?
7. [代码审查清单](#代码审查清单)

---

## 概述

本文档定义了 AgentOS 项目测试代码的编写规范，旨在�?- 提高测试代码的可维护�?- 减少代码重复
- 确保测试质量
- 统一编码风格

---

## C 测试规范

### 1. 文件组织

#### 1.1 文件头注�?
```c
/**
 * @file test_module.c
 * @brief 模块单元测试
 * @details 测试模块的详细描�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */
```

#### 1.2 包含头文件顺�?
```c
// 1. 标准库头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 2. 项目公共头文�?#include "agentos.h"
#include "error.h"

// 3. 模块头文�?#include "module.h"

// 4. 测试工具头文�?#include "test_macros.h"
```

### 2. 使用测试�?
#### 2.1 推荐使用测试宏减少样板代�?
**�?不推�?*:
```c
static void test_engine_create_destroy() {
    engine_t* engine = NULL;
    agentos_error_t err = agentos_engine_create(NULL, &engine);
    printf("test_engine_create_destroy: %d\n", err);
    if (err == AGENTOS_SUCCESS) {
        agentos_engine_destroy(engine);
    }
}
```

**�?推荐**:
```c
static void test_engine_create_destroy() {
    TEST_ENGINE_CREATE_DESTROY(
        engine_t,
        agentos_engine_create,
        agentos_engine_destroy,
        NULL  // manager
    );
}
```

#### 2.2 使用测试套件�?
```c
int main() {
    TEST_SUITE_BEGIN("Engine Module Tests");

    RUN_TEST(test_create_destroy);
    RUN_TEST(test_process);
    RUN_TEST(test_health_check);

    TEST_SUITE_END();
}
```

### 3. 测试函数命名

```c
// 测试模式: test_<功能>_<场景>

// �?好的命名
int test_engine_create_success();
int test_engine_create_failure_null_config();
int test_engine_process_valid_input();
int test_engine_process_invalid_input();

// �?不好的命�?int test1();
int test_engine();
int test_create();
```

### 4. 断言使用

```c
// 使用宏断言，提供清晰的错误信息
TEST_ASSERT(condition, "Condition should be true");
TEST_ASSERT_EQUAL(expected, actual);
TEST_ASSERT_STRING_EQUAL("expected", actual_string);
TEST_ASSERT_NOT_NULL(ptr);
TEST_ASSERT_NULL(ptr);
```

---

## Python 测试规范

### 1. 测试类组�?
```python
class TestModuleFeature:
    """模块功能测试�?""

    @pytest.fixture
    def valid_data(self) -> Dict[str, Any]:
        """提供有效测试数据"""
        return TestDataBuilder() \
            .with_field("name", "test") \
            .with_field("value", 100) \
            .build()

    @pytest.fixture
    def validator(self) -> Validator:
        """提供验证器实�?""
        return Validator()

    def test_valid_data_passes(self, valid_data, validator):
        """测试有效数据通过验证"""
        is_valid = validator.validate(valid_data)
        assert is_valid is True
```

### 2. 使用参数化测�?
**�?不推�?*:
```python
def test_missing_field_a(self, validator):
    data = create_valid_data()
    del data["field_a"]
    assert not validator.validate(data)

def test_missing_field_b(self, validator):
    data = create_valid_data()
    del data["field_b"]
    assert not validator.validate(data)

def test_missing_field_c(self, validator):
    data = create_valid_data()
    del data["field_c"]
    assert not validator.validate(data)
```

**�?推荐**:
```python
@pytest.mark.parametrize("missing_field", ["field_a", "field_b", "field_c"])
def test_missing_required_field(self, validator, missing_field):
    data = create_valid_data()
    del data[missing_field]
    assert not validator.validate(data)
    assert any(missing_field in e for e in validator.errors)
```

### 3. 使用测试辅助工具

```python
from tests.utils.test_helpers import (
    create_mock_response,
    create_mock_session,
    TestDataBuilder,
    ContractTestHelper,
    assert_error_contains
)

# 使用 Mock 工厂
def test_api_call(self):
    mock_session = create_mock_session(
        response_data={"result": "success"},
        status_code=200
    )
    # ... 测试逻辑

# 使用数据构建�?def test_with_builder(self):
    data = TestDataBuilder() \
        .with_field("name", "test") \
        .with_field("value", 100) \
        .build_invalid(missing_field="name")
    # ... 测试逻辑
```

### 4. 测试函数命名

```python
# 测试模式: test_<功能>_<场景>_<预期结果>

# �?好的命名
def test_create_engine_with_valid_config_succeeds():
    pass

def test_create_engine_with_null_config_fails():
    pass

def test_process_task_with_timeout_returns_error():
    pass

# �?不好的命�?def test_create():
    pass

def test_engine():
    pass

def test_1():
    pass
```

---

## 测试组织结构

### 1. 目录结构

```
AgentOS/
├── agentos/atoms/
�?  └── <module>/
�?      └── tests/              # 模块单元测试
�?          ├── test_*.c        # C 测试文件
�?          ├── CMakeLists.txt  # 构建配置
�?          └── README.md       # 测试说明
�?├── agentos/daemon/
�?  └── <service>/
�?      └── tests/              # 服务测试
�?          └── test_*.c
�?├── agentos/commons/
�?  └── tests/
�?      ├── unit/               # 工具库单元测�?�?      └── utils/              # 测试工具
�?          ├── test_macros.h   # C 测试�?�?          └── test_helpers.py # Python 测试辅助
�?└── tests/                      # 顶层测试
    ├── unit/                   # Python 单元测试
    ├── integration/            # 集成测试
    ├── contract/               # 契约测试
    ├── security/               # 安全测试
    ├── performance/            # 性能测试
    └── utils/                  # 测试工具
```

### 2. 测试分类

| 测试类型 | 位置 | 职责 |
|---------|------|------|
| 单元测试 | `agentos/atoms/*/tests/`, `agentos/daemon/*/tests/`, `agentos/commons/tests/unit/` | 测试单个模块/函数 |
| 集成测试 | `tests/integration/` | 测试模块间协�?|
| 契约测试 | `tests/contract/` | 验证接口契约 |
| 安全测试 | `tests/security/` | 安全漏洞检�?|
| 性能测试 | `tests/performance/` | 性能基准测试 |

---

## 代码质量标准

### 1. 重复率标�?
| 评级 | 重复率范�?| 要求 |
|-----|-----------|------|
| �?优秀 | < 20% | 保持现状 |
| �?良好 | 20% - 30% | 可接�?|
| ⚠️ 一�?| 30% - 40% | 需要优�?|
| �?�?| > 40% | 必须重构 |

### 2. 圈复杂度标准

| 评级 | 复杂度范�?| 要求 |
|-----|-----------|------|
| �?优秀 | 1 - 5 | 保持现状 |
| �?良好 | 6 - 10 | 可接�?|
| ⚠️ 一�?| 11 - 15 | 需要优�?|
| �?�?| > 15 | 必须重构 |

### 3. 函数长度标准

| 评级 | 行数范围 | 要求 |
|-----|---------|------|
| �?优秀 | < 20 �?| 保持现状 |
| �?良好 | 20 - 50 �?| 可接�?|
| ⚠️ 一�?| 50 - 100 �?| 需要拆�?|
| �?�?| > 100 �?| 必须拆分 |

---

## 最佳实�?
### 1. 测试原则

#### 1.1 FIRST 原则

- **F**ast（快速）：测试应该快速执�?- **I**ndependent（独立）：测试之间不应相互依�?- **R**epeatable（可重复）：测试应该在任何环境下都能重复执行
- **S**elf-validating（自验证）：测试应该自动判断通过/失败
- **T**imely（及时）：测试应该及时编�?
#### 1.2 AAA 模式

```python
def test_feature():
    # Arrange（准备）
    data = create_test_data()
    validator = Validator()

    # Act（执行）
    result = validator.validate(data)

    # Assert（断言�?    assert result is True
```

### 2. 测试覆盖

#### 2.1 必须覆盖的场�?
- �?正常路径（Happy Path�?- �?边界条件
- �?错误处理
- �?异常情况
- �?性能边界

#### 2.2 测试覆盖率目�?
| 类型 | 目标覆盖�?|
|-----|-----------|
| 单元测试 | �?80% |
| 集成测试 | �?60% |
| 契约测试 | 100% |

### 3. Mock 使用

#### 3.1 何时使用 Mock

- 外部依赖（API、数据库�?- 不确定的行为（时间、随机数�?- 副作用操作（文件写入、网络请求）

#### 3.2 Mock 最佳实�?
```python
# �?使用 Mock 工厂函数
mock_session = create_mock_session(response_data={"result": "success"})

# �?在每个测试中重复 Mock 配置
mock_response = Mock()
mock_response.status_code = 200
mock_response.json.return_value = {"result": "success"}
mock_session_instance = Mock()
mock_session_instance.post.return_value = mock_response
```

---

## 代码审查清单

### C 测试审查清单

- [ ] 文件头注释完整（@file, @brief, @copyright�?- [ ] 使用测试宏减少样板代�?- [ ] 测试函数命名清晰（test_<功能>_<场景>�?- [ ] 使用 TEST_SUITE_BEGIN/END �?- [ ] 函数长度 < 50 �?- [ ] 圈复杂度 < 10
- [ ] 内存泄漏检�?- [ ] 错误处理完整

### Python 测试审查清单

- [ ] 使用参数化测试减少重�?- [ ] 使用测试辅助工具（test_helpers.py�?- [ ] 测试函数命名清晰（test_<功能>_<场景>_<预期>�?- [ ] 使用 fixture 共享测试数据
- [ ] 函数长度 < 50 �?- [ ] 圈复杂度 < 10
- [ ] 使用 Mock 工厂函数
- [ ] 断言清晰明确

### 通用审查清单

- [ ] 测试覆盖所有场�?- [ ] 测试独立可重�?- [ ] 测试命名描述清晰
- [ ] 无重复代�?- [ ] 注释充分
- [ ] 遵循编码规范

---

## 附录

### A. 工具推荐

| 工具 | 用�?| 安装 |
|-----|------|------|
| pytest | Python 测试框架 | `pip install pytest` |
| radon | Python 复杂度检�?| `pip install radon` |
| lizard | C/C++ 复杂度检�?| `pip install lizard` |
| gcov | C 覆盖率检�?| GCC 内置 |
| coverage.py | Python 覆盖率检�?| `pip install coverage` |

### B. 参考资�?
- [pytest 官方文档](https://docs.pytest.org/)
- [Google 测试指南](https://google.github.io/googletest/)
- [C 测试最佳实践](https://www.gnu.org/software/autoconf/manual/autoconf.html#Tests)

---

<div align="center">

**"测试代码也是代码，需要同样的质量标准"**

**AgentOS v1.0.0.6**

</div>
