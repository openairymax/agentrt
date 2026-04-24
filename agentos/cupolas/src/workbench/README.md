# Workbench — 安全工作台

`cupolas/src/workbench/` 提供安全策略的交互式测试与验证环境，允许安全管理员在正式部署前验证安全规则的有效性。

## 设计目标

- **安全策略模拟**：在不影响生产环境的情况下测试安全策略
- **实时验证**：输入样本数据，即时查看安全策略的判定结果
- **可视化反馈**：直观展示策略匹配过程和决策路径
- **策略调优**：基于测试结果调整安全策略的规则和参数

## 核心功能

| 功能 | 说明 |
|------|------|
| 策略模拟 | 在沙箱环境中执行安全策略评审 |
| 样本测试 | 使用预定义或自定义样本来测试策略 |
| 结果分析 | 分析策略执行结果，包括匹配规则和决策路径 |
| 报告生成 | 生成策略测试报告 |
| 策略导出 | 将验证通过的策略导出到生产环境 |

## 使用方式

```bash
# 启动安全工作台
cupolas-workbench --config security_policies.yaml

# 加载测试样本
cupolas-workbench load-samples ./test-samples/

# 运行策略模拟
cupolas-workbench simulate --policy api_access_control

# 查看测试结果
cupolas-workbench report --format html
```

## 策略模拟流程

```
加载策略配置 → 加载测试样本 → 执行模拟 → 分析结果 → 生成报告
     ↓              ↓              ↓          ↓           ↓
  策略文件     正常/恶意请求    规则引擎    决策树      HTML/JSON
```

## 安全策略示例

```yaml
# workbench/test-policies.yaml
policies:
  - name: "sql_injection_prevention"
    rules:
      - pattern: "'.*OR.*1=1"
        action: "block"
      - pattern: "'.*DROP.*TABLE"
        action: "block"
    default_action: "allow"

test_samples:
  - input: "admin' OR 1=1--"
    expected: "block"
  - input: "SELECT * FROM users"
    expected: "allow"
```

---

*AgentOS Cupolas — Workbench*
