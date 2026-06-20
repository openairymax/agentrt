# Manager 配置工具集

**模块路径**: `ecosystem/manager/tools/`
**版本**: v0.1.0

## 概述

`manager/tools/` 包含 Manager 模块的配置管理和运维工具集，提供配置差异对比、版本历史清理、配置漂移检测和审计日志生成四大工具。所有工具均支持 CLI 和 Python API 两种调用方式，可独立运行也可集成到 CI/CD 流水线中。

## 目录结构

```
tools/
├── src/                        # 工具实现
│   ├── config_diff.py          # 配置差异对比工具
│   ├── config_version_cleanup.py   # 版本历史清理工具
│   ├── drift_detector.py       # 配置漂移检测器
│   └── audit_log_generator.py  # 审计日志生成器
├── base/                       # 工具基础库
│   ├── __init__.py             # 包初始化
│   └── utils.py                # 公共工具函数
└── README.md                   # 本文件
```

## 工具列表

### 1. 配置差异对比 (`config_diff.py`)

比较两个配置文件的差异，支持 JSON 格式的递归对比。

```bash
python config_diff.py config_v1.json config_v2.json
```

输出格式示例：

```diff
- kernel.log_level: "debug"
+ kernel.log_level: "info"
- model.temperature: 0.8
+ model.temperature: 0.7
+ security.rate_limit: 2000
```

### 2. 版本历史清理 (`config_version_cleanup.py`)

清理配置版本历史，保留指定数量的最新版本或清理指定天数前的版本。

```bash
# 保留最近 10 个版本
python config_version_cleanup.py --keep 10

# 清理 30 天前的版本
python config_version_cleanup.py --older-than 30

# 预览清理结果（不实际执行）
python config_version_cleanup.py --dry-run
```

### 3. 配置漂移检测 (`drift_detector.py`)

检测配置文件是否偏离基线版本，支持创建基线、检测漂移和生成报告。

**核心特性**：

- 基于 SHA-256 哈希的文件完整性校验
- 三种漂移类型：MODIFIED / DELETED / ADDED
- 三级严重程度：INFO / WARNING / CRITICAL
- 敏感文件自动标记为 CRITICAL（security/、kernel/、model/）
- 支持 JSON 和 Markdown 格式报告导出
- 支持 `--fail-on-drift` 退出码，便于 CI/CD 集成

**严重程度规则**：

| 文件 | 严重程度 |
|------|---------|
| `security/policy.yaml` | CRITICAL |
| `security/permission_rules.yaml` | CRITICAL |
| `model/model.yaml` | CRITICAL |
| `kernel/settings.yaml` | CRITICAL |
| `manager_management.yaml` | CRITICAL |
| `agent/registry.yaml` | WARNING |
| `skill/registry.yaml` | WARNING |
| `logging/manager.yaml` | WARNING |
| `sanitizer/sanitizer_rules.json` | WARNING |
| 其他文件 | INFO |

```bash
# 创建基线
python drift_detector.py --action create-baseline

# 检测漂移
python drift_detector.py --action detect

# 一键执行（先建基线再检测）
python drift_detector.py --action both --output drift_report.json

# 详细模式
python drift_detector.py --action detect --verbose

# CI/CD 模式（检测到漂移时返回非零退出码）
python drift_detector.py --action detect --fail-on-drift

# 指定配置目录
python drift_detector.py --config-dir /path/to/config --action both

# 导出 Markdown 报告
python drift_detector.py --action detect --output report.json --output-md report.md
```

**CLI 参数**：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--config-dir` | `../manager` | 配置目录路径 |
| `--action` | `both` | 操作类型：create-baseline / detect / both |
| `--output` | `drift_report.json` | JSON 报告输出路径 |
| `--output-md` | 自动生成 | Markdown 报告输出路径 |
| `--verbose` / `-v` | false | 详细输出 |
| `--fail-on-drift` | false | 检测到漂移时返回退出码 1 |

**Python API**：

```python
from drift_detector import ConfigDriftDetector, DriftSeverity

detector = ConfigDriftDetector(config_dir, verbose=True)

# 创建基线
baseline_path = detector.create_baseline()

# 检测漂移
report = detector.detect_drift()

# 导出报告
detector.export_report_json(report, Path("drift_report.json"))
detector.export_report_markdown(report, Path("drift_report.md"))

# 查看摘要
print(report.summary())
print(f"Drift rate: {report.drift_rate:.1f}%")
print(f"Has drift: {report.has_drift}")
```

### 4. 审计日志生成器 (`audit_log_generator.py`)

生成符合 `config-audit-log.schema.json` 规范的测试用审计日志。

**核心特性**：

- 7 种动作类型：LOAD / RELOAD / CHANGE / ROLLBACK / VALIDATE / EXPORT / IMPORT
- 3 种操作者类型：user / system / ci_cd
- 自动生成 SHA-256 校验和（before/after）
- 支持关联 ID（correlation_id）和工单号（ticket_id）
- 批量生成与单条生成两种模式

```bash
# 生成 10 条随机审计日志
python audit_log_generator.py --count 10 --output audit_log.json

# 生成特定动作的审计日志
python audit_log_generator.py --action CHANGE --file kernel/settings.yaml --reason "调整日志级别"

# 批量生成不同环境的日志
python audit_log_generator.py --count 20 --environment staging --output staging_audit.json

# 不包含变更详情
python audit_log_generator.py --count 5 --no-changes --output simple_audit.json
```

**CLI 参数**：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--config-dir` | `../` | 配置目录路径 |
| `--output` | `sample_audit_log.json` | 输出文件路径 |
| `--count` | 10 | 生成日志条数 |
| `--action` | 随机 | 动作类型：LOAD/RELOAD/CHANGE/ROLLBACK/VALIDATE/EXPORT/IMPORT |
| `--file` | 随机 | 配置文件路径 |
| `--operator-type` | 随机 | 操作者类型：user/system/ci_cd |
| `--environment` | production | 环境名称 |
| `--reason` | 随机 | 变更原因 |
| `--no-changes` | false | 不包含变更详情 |

**Python API**：

```python
from audit_log_generator import AuditLogGenerator

generator = AuditLogGenerator(config_dir)

# 生成单条日志
entry = generator.generate_entry(
    action="CHANGE",
    config_file="kernel/settings.yaml",
    environment="production",
    reason="调整并发参数"
)

# 批量生成
entries = generator.generate_batch(count=20, environment="staging")

# 导出
generator.export_to_json(entries, Path("audit_log.json"))
```

## CI/CD 集成

### GitLab CI 示例

```yaml
config-validation:
  stage: validate
  script:
    - python manager/tools/config_diff.py config.json config.new.json
    - python manager/tools/drift_detector.py --action detect --fail-on-drift
    - python manager/tests/run_all_tests.py --verbose
```

### GitHub Actions 示例

```yaml
- name: Config Drift Check
  run: |
    python manager/tools/src/drift_detector.py \
      --action both \
      --fail-on-drift \
      --output drift_report.json \
      --output-md drift_report.md
```

## 依赖关系

| 组件 | 用途 |
|------|------|
| Python ≥ 3.10 | 运行环境 |
| pytest | 漂移检测器测试依赖 |
| 标准库 hashlib | SHA-256 哈希计算 |
| 标准库 json / yaml | 配置文件解析 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
