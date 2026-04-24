# 测试脚本

`scripts/tests/`

## 概述

`tests/` 目录包含 AgentOS 项目的各类测试执行脚本，提供功能测试、集成测试、端到端测试的统一运行入口，支持 CI/CD 流水线自动化和本地手动测试。

## 脚本列表

| 脚本 | 说明 |
|------|------|
| `run_tests.sh` | 测试执行入口，支持选择测试范围和模式 |
| `run_integration.sh` | 集成测试，验证多组件协同工作 |
| `run_e2e.sh` | 端到端测试，模拟真实用户场景 |
| `test_report.sh` | 测试报告生成与汇总 |

## 使用示例

```bash
# 运行所有测试
./tests/run_tests.sh

# 仅运行单元测试
./tests/run_tests.sh --type unit

# 运行集成测试
./tests/run_tests.sh --type integration

# 生成测试报告
./tests/test_report.sh --format html --output ./reports/
```

## 测试类型

| 类型 | 说明 | 覆盖范围 |
|------|------|----------|
| 单元测试 | 验证单个函数或模块的正确性 | C (CMockery2), Python (pytest) |
| 集成测试 | 验证多组件间的数据流和协议交互 | 跨守护进程通信 |
| E2E 测试 | 模拟用户从入口到返回的全链路场景 | 完整系统流程 |

## CI/CD 集成

```yaml
# GitLab CI 示例
test:
  script:
    - scripts/tests/run_tests.sh --ci
  artifacts:
    reports:
      junit: reports/junit.xml
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
