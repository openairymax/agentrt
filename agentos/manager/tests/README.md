# Manager 模块测试套件

**版本**: v1.0.0  
**最后更新**: 2026-04-01  
**适用范围**: AgentOS/manager 模块  
**遵循规范**: ARCHITECTURAL_PRINCIPLES.md E-8（可测试性原则）  

---

## 一、概述

本测试套件为 AgentOS Manager 模块（配置管理中心）提供全面的自动化测试覆盖，确保配置文件的正确性、一致性和生产就绪性。

### 1.1 测试目标

1. **语法正确性**: 验证所有 YAML/JSON 配置文件语法无误
2. **Schema 合规性**: 验证配置符合 JSON Schema 定义
3. **集成一致性**: 验证跨模块配置的完整性和依赖关系
4. **编码标准**: 确保使用 UTF-8 编码，无 BOM，环境变量格式统一

### 1.2 测试架构

```
tests/
├── test_config_syntax.py      # 配置文件语法验证
├── test_schema_validation.py   # JSON Schema 验证
├── test_config_integration.py  # 配置集成测试
└── run_all_tests.py            # 测试套件运行器
```

---

## 二、环境要求

### 2.1 Python 版本

- Python 3.8+ (推荐 3.10+)

### 2.2 依赖库

```bash
# 核心依赖（必需）
pip install pyyaml>=5.0

# 可选依赖（用于 Schema 验证）
pip install jsonschema>=4.0
```

### 2.3 安装检查

```bash
python --version          # 应显示 Python 3.x
python -c "import yaml"   # 应无错误
python -c "import jsonschema"  # 如果安装了 jsonschema
```

---

## 三、快速开始

### 3.1 运行所有测试

```bash
cd AgentOS/agentos/manager/tests
python run_all_tests.py
```

### 3.2 运行单个测试

```bash
# 配置语法验证
python test_config_syntax.py --verbose

# Schema 验证
python test_schema_validation.py --verbose

# 集成测试
python test_config_integration.py --verbose
```

### 3.3 运行特定测试

```bash
# 只运行语法和集成测试
python run_all_tests.py syntax integration

# 只运行 Schema 验证
python run_all_tests.py schema
```

---

## 四、测试详情

### 4.1 配置语法验证 (test_config_syntax.py)

**功能**: 验证所有配置文件的 YAML/JSON 语法、UTF-8 编码、环境变量引用格式

**测试内容**:
- ✅ 必需配置文件存在性检查 (6个核心配置)
- ✅ YAML 文件语法解析
- ✅ JSON 文件语法解析
- ✅ UTF-8 编码验证（无BOM）
- ✅ 环境变量引用格式标准化 (`${VARIABLE}`)

**覆盖范围**: 所有 `.yaml`, `.yml`, `.json` 文件

**预期结果**: 全部通过（0 失败）

---

### 4.2 Schema 验证 (test_schema_validation.py)

**功能**: 验证配置文件是否符合对应的 JSON Schema 定义

**测试内容**:
- ✅ Schema 文件基本格式验证 ($schema, type, properties)
- ✅ 配置与 Schema 匹配验证 (9 个映射)
- ✅ Schema 本身有效性检查
- ✅ 错误信息详细输出（最多前10个错误）

**配置-Schema 映射表**:

| 配置文件 | Schema 文件 |
|---------|------------|
| kernel/settings.yaml | kernel-settings.schema.json |
| model/model.yaml | model.schema.json |
| agent/registry.yaml | agent-registry.schema.json |
| skill/registry.yaml | skill-registry.schema.json |
| security/policy.yaml | security-policy.schema.json |
| sanitizer/sanitizer_rules.json | sanitizer-rules.schema.json |
| logging/manager.yaml | logging.schema.json |
| manager_management.yaml | config-management.schema.json |
| service/tool_d/tool.yaml | tool-service.schema.json |

**依赖**: 需要 `jsonschema` 库（未安装时跳过详细验证）

**预期结果**: 全部通过（0 失败）

---

### 4.3 配置集成测试 (test_config_integration.py)

**功能**: 验证配置完整性、跨模块一致性、架构原则合规性

**测试用例列表**:

| # | 测试名称 | 验证内容 | 关联原则 |
|---|---------|---------|---------|
| 1 | 环境变量模板完整性 | .env.template 定义所有必需变量 | E-4 跨平台一致性 |
| 2 | 配置归属元数据 | 所有核心配置包含 `_owner` 字段 | 双重责任模型 |
| 3 | 版本元数据一致性 | 所有配置包含有效 `_config_version` | K-2 接口契约化 |
| 4 | Schema 引用有效性 | 声明的 Schema 文件存在 | E-6 错误可追溯 |
| 5 | Agent 注册表完整性 | 包含所有预期的 Agent 定义 | S-3 总体设计部 |
| 6 | 双系统认知配置 | System 1 和 System 2 完整定义 | C-1 双系统协同 |
| 7 | 安全默认拒绝策略 | default_policy = "deny" | E-1 安全内生 |
| 8 | 日志 Domain ID 格式 | 符合 Log_guide 规范 | E-2 可观测性 |
| 9 | 模型降级配置 | fallback 和 circuit_breaker 完整 | E-3 资源确定性 |

**预期结果**: 全部通过（0 错误，允许警告）

---

## 五、输出示例

### 5.1 成功输出

```
================================================================================
AgentOS Manager 模块测试报告
================================================================================
测试时间: 2026-04-01 12:00:00
配置目录: D:\Spharx\SpharxWorks\AgentOS\manager

--------------------------------------------------------------------------------
测试名称                    状态       退出码    耗时(秒)
--------------------------------------------------------------------------------
配置语法验证                ✅ PASS     0         1.23
Schema 验证                 ✅ PASS     0         0.89
配置集成测试                ✅ PASS     0         1.56
--------------------------------------------------------------------------------

总计: 3 个测试
通过: 3 ✅ (100.0%)
失败: 0 ❌ (0.0%)
```

### 5.2 失败输出示例

```
❌ FAIL: Schema 验证 → security/policy.yaml
  - [security.rules] 'default_policy' 的值必须是 "deny" 或 "allow"
  - [security.audit.encryption.algorithm] 枚举值无效: 'aes-128'
```

---

## 六、CI/CD 集成

### 6.1 GitHub Actions 示例

```yaml
name: Manager Config Tests

on:
  push:
    paths:
      - 'AgentOS/agentos/manager/**'
  pull_request:
    paths:
      - 'AgentOS/agentos/manager/**'

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Set up Python
      uses: actions/setup-python@v4
      with:
        python-version: '3.10'
    
    - name: Install dependencies
      run: |
        pip install pyyaml jsonschema
    
    - name: Run tests
      working-directory: ./AgentOS/agentos/manager/tests
      run: python run_all_tests.py --verbose
```

### 6.2 GitLab CI 示例

```yaml
test_manager_config:
  stage: test
  image: python:3.10-slim
  
  script:
    - pip install pyyaml jsonschema
    - cd AgentOS/agentos/manager/tests
    - python run_all_tests.py --verbose
  
  only:
    changes:
      - AgentOS/agentos/manager/**
```

---

## 七、故障排除

### 7.1 常见问题

**Q: Python 命令未找到**
```
解决方案:
Windows: 确保 Python 已添加到 PATH
Linux/macOS: 使用 python3 或创建符号链接
```

**Q: ImportError: No module named 'yaml'**
```
解决方案:
pip install pyyaml
或
pip3 install pyyaml
```

**Q: Schema 验证跳过**
```
原因: jsonschema 库未安装
解决: pip install jsonschema
注意: 不影响基本测试，仅跳过详细 Schema 验证
```

**Q: 测试超时**
```
原因: 配置文件过大或系统性能问题
解决: 
1. 检查是否有超大配置文件 (>10MB)
2. 增加 timeout 参数（当前为120秒）
3. 排除不必要的测试目录
```

### 7.2 调试模式

```bash
# 启用详细输出
python run_all_tests.py --verbose

# 单独调试某个测试
python test_config_syntax.py --config-dir /path/to/configs --verbose

# 检查 Python 环境
python -c "import sys; print(sys.version)"
python -c "import yaml; print('PyYAML:', yaml.__version__)"
```

---

## 八、扩展指南

### 8.1 添加新测试用例

在 `test_config_integration.py` 中添加新方法：

```python
def test_your_new_check(self) -> IntegrationTestResult:
    """你的新测试"""
    # ... 实现逻辑 ...
    
    return IntegrationTestResult(
        test_name="你的测试名称",
        passed=is_success,
        details="详细信息",
        severity="error" if not is_success else "info"
    )
```

然后在 `run_all_tests()` 方法的 `tests` 列表中注册：

```python
tests = [
    # ... 已有测试 ...
    self.test_your_new_check,  # 添加你的测试
]
```

### 8.2 添加新的配置-Schema 映射

在 `test_schema_validation.py` 中更新 `CONFIG_SCHEMA_MAP`:

```python
CONFIG_SCHEMA_MAP = {
    # ... 已有映射 ...
    'your/new_config.yaml': 'schema/your-new-config.schema.json',
}
```

---

## 九、最佳实践

### 9.1 开发阶段

- [ ] 提交代码前运行完整测试套件
- [ ] 新增配置文件必须对应添加 Schema
- [ ] 更新 .env.template 添加新的环境变量
- [ ] 确保 `_owner` 和 `_config_version` 元数据完整

### 9.2 CI/CD 阶段

- [ ] 将测试纳入持续集成流水线
- [ ] 设置测试失败阻断合并请求（PR）
- [ ] 定期清理过时的测试用例
- [ ] 监控测试执行时间趋势

### 9.3 发布阶段

- [ ] 生产部署前必须通过全部测试
- [ ] 记录测试结果到发布日志
- [ ] 对失败的测试进行根因分析
- [ ] 更新测试覆盖率指标

---

## 十、参考文档

- [ARCHITECTURAL_PRINCIPLES.md](../../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md)
- [INTEGRATION_STANDARD.md](../INTEGRATION_STANDARD.md)
- [CONFIG_CHANGE_PROCESS.md](../CONFIG_CHANGE_PROCESS.md)
- [config_unified README](../../../agentos/commons/utils/config_unified/README.md)

---

## 十一、版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0.0 | 2026-04-01 | 初始版本，包含三个核心测试和运行器 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."