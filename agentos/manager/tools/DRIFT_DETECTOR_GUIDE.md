# AgentOS 配置漂移检测器使用指南

**版本**: V1.0  
**创建日期**: 2026-04-05  
**工具路径**: `agentos/manager/tools/drift_detector.py`  

---

## 目录

1. [概述](#1-概述)
2. [快速开始](#2-快速开始)
3. [功能特性](#3-功能特性)
4. [使用示例](#4-使用示例)
5. [输出格式](#5-输出格式)
6. [集成到CI/CD](#6-集成到cd)
7. [最佳实践](#7-最佳实践)
8. [故障排查](#8-故障排查)

---

## 1. 概述

### 1.1 什么是配置漂移检测？

**配置漂移** (Configuration Drift) 是指运行环境中的配置文件逐渐偏离预期基线状态的现象。这通常由以下原因引起：

- 手动修改配置文件
- 自动化脚本意外更改
- 部署流程不一致
- 紧急修复后未同步到基线

### 1.2 为什么需要检测配置漂移？

- ✅ **安全性**: 防止未授权的安全配置变更
- ✅ **稳定性**: 避免因配置不一致导致的服务中断
- ✅ **合规性**: 满足审计和合规要求
- ✅ **可追溯性**: 追踪配置变更历史

### 1.3 漂移检测器功能

| 功能 | 说明 |
|------|------|
| **基线创建** | 创建配置文件的SHA-256哈希清单 |
| **漂移检测** | 检测文件的修改、删除、新增 |
| **严重程度分级** | 根据文件重要性自动分级（CRITICAL/WARNING/INFO） |
| **报告生成** | 生成JSON和Markdown格式的详细报告 |
| **CI/CD集成** | 支持在CI流水线中自动检测 |

---

## 2. 快速开始

### 2.1 基本用法

```bash
cd agentos/manager/tools

# 创建基线（第一次使用）
python drift_detector.py --action create-baseline

# 检测漂移
python drift_detector.py --action detect

# 一键执行（创建基线 + 检测）
python drift_detector.py --action both
```

### 2.2 输出示例

```
======================================================================
Drift Report Summary:
  Scan Time: 2026-04-05T10:30:00.123456Z
  Config Directory: /path/to/manager
  Baseline Created: 2026-04-05T09:00:00.000000Z
  Total Files Scanned: 15
  Drifted Files: 2 (13.3%)
  Unchanged Files: 13
  Severity Breakdown:
    - Critical: 1
    - Warning: 0
    - Info: 1
======================================================================

Drift Details:

🔴 [CRITICAL] security/policy.yaml (modified)
   File content has been modified
   Baseline: e3b0c44298fc1c14...
   Current:  a7ffc6f8bf1ed766...

🔵 [INFO] agent/registry.yaml (added)
   New file added since baseline
   Baseline: [NEW FILE]
   Current:  b7ffc6f8bf1ed766...

✅ Drift detection completed successfully
```

---

## 3. 功能特性

### 3.1 文件分类

漂移检测器根据文件重要性自动分类：

#### 🔴 CRITICAL（严重）
- `security/policy.yaml` - 安全策略
- `security/permission_rules.yaml` - 权限规则
- `model/model.yaml` - 模型配置
- `kernel/settings.yaml` - 内核配置
- `manager_management.yaml` - Manager管理配置

#### 🟡 WARNING（警告）
- `agent/registry.yaml` - Agent注册表
- `skill/registry.yaml` - 技能注册表
- `logging/manager.yaml` - 日志配置
- `sanitizer/sanitizer_rules.json` - 输入净化规则

#### 🔵 INFO（信息）
- 其他配置文件

### 3.2 检测类型

| 类型 | 说明 | 示例 |
|------|------|------|
| **MODIFIED** | 文件内容被修改 | 配置参数变更 |
| **DELETED** | 文件被删除 | 配置文件丢失 |
| **ADDED** | 新增文件 | 未授权的文件 |

### 3.3 忽略规则

自动忽略以下文件：
- `*.pyc`, `__pycache__/` - Python编译文件
- `.git/`, `.gitignore` - Git相关文件
- `*.log` - 日志文件
- `node_modules/` - Node模块
- `.env*` - 环境变量文件
- `*.tmp`, `*.bak`, `*.swp` - 临时文件

---

## 4. 使用示例

### 4.1 创建基线

```bash
# 基本用法
python drift_detector.py --action create-baseline

# 指定配置目录
python drift_detector.py --config-dir /path/to/config --action create-baseline

# 详细模式
python drift_detector.py --action create-baseline --verbose
```

**输出**:
```
📝 Creating baseline for /path/to/config...
   Found 15 configuration files
   Processed 10 files...
✅ Baseline created with 15 files
   Saved to: /path/to/config/.baseline/manifest.json
```

### 4.2 检测漂移

```bash
# 基本检测
python drift_detector.py --action detect

# 详细模式
python drift_detector.py --action detect --verbose

# 输出到指定文件
python drift_detector.py --action detect --output drift_report.json

# 同时输出Markdown格式
python drift_detector.py --action detect --output report.json --output-md report.md
```

### 4.3 一键执行

```bash
# 创建基线并立即检测（推荐）
python drift_detector.py --action both

# 详细模式
python drift_detector.py --action both --verbose
```

### 4.4 CI/CD集成

```bash
# 在CI中运行，检测到漂移时返回错误码
python drift_detector.py --action detect --fail-on-drift

# 示例：GitHub Actions
- name: Check configuration drift
  run: |
    python drift_detector.py --action detect --fail-on-drift
```

### 4.5 查看报告

```bash
# 查看JSON报告
cat drift_report.json | jq '.'

# 查看Markdown报告
cat drift_report.md

# 在浏览器中查看Markdown
open drift_report.md  # macOS
xdg-open drift_report.md  # Linux
```

---

## 5. 输出格式

### 5.1 JSON报告结构

```json
{
  "scan_time": "2026-04-05T10:30:00.123456Z",
  "config_dir": "/path/to/manager",
  "baseline_created": "2026-04-05T09:00:00.000000Z",
  "total_files_scanned": 15,
  "drifted_files": 2,
  "unchanged_files": 13,
  "has_drift": true,
  "drift_rate": 13.3,
  "drift_items": [
    {
      "file_path": "security/policy.yaml",
      "severity": "critical",
      "drift_type": "modified",
      "baseline_hash": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      "current_hash": "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a",
      "detected_at": "2026-04-05T10:30:00.123456Z",
      "details": "File content has been modified",
      "file_size": 1024,
      "last_modified": "2026-04-05T10:15:00.000000Z"
    }
  ]
}
```

### 5.2 Markdown报告示例

```markdown
# Configuration Drift Report

**Scan Time**: 2026-04-05T10:30:00.123456Z  
**Config Directory**: /path/to/manager  
**Baseline Created**: 2026-04-05T09:00:00.000000Z  

## Summary

| Metric | Value |
|--------|-------|
| Total Files Scanned | 15 |
| Drifted Files | 2 |
| Unchanged Files | 13 |
| Drift Rate | 13.3% |
| Has Drift | Yes ⚠️ |

## Severity Breakdown

| Severity | Count |
|----------|-------|
| 🔴 Critical | 1 |
| 🟡 Warning | 0 |
| 🔵 Info | 1 |

## Drift Details

### CRITICAL

- 🔴 **security/policy.yaml** (modified)
  - Details: File content has been modified
  - Baseline Hash: `e3b0c44298fc1c14...`
  - Current Hash: `a7ffc6f8bf1ed766...`

### INFO

- 🔵 **agent/registry.yaml** (added)
  - Details: New file added since baseline
  - Baseline Hash: `[NEW FILE]`
  - Current Hash: `b7ffc6f8bf1ed766...`
```

---

## 6. 集成到CI/CD

### 6.1 GitHub Actions

```yaml
- name: Check configuration drift
  run: |
    cd agentos/manager/tools
    python drift_detector.py --action detect --fail-on-drift --output drift_report.json
    
- name: Upload drift report
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: drift-report
    path: agentos/manager/tools/drift_report.json
```

### 6.2 Jenkins Pipeline

```groovy
stage('Configuration Drift Check') {
    steps {
        script {
            try {
                sh '''
                    cd agentos/manager/tools
                    python drift_detector.py --action detect --fail-on-drift
                '''
            } catch (Exception e) {
                echo "Configuration drift detected!"
                archiveArtifacts artifacts: 'drift_report.json', allowEmptyArchive: true
                throw e
            }
        }
    }
}
```

### 6.3 GitLab CI

```yaml
drift-check:
  stage: test
  script:
    - cd agentos/manager/tools
    - python drift_detector.py --action detect --fail-on-drift
  artifacts:
    reports:
      drift_report: drift_report.json
    when: always
```

---

## 7. 最佳实践

### 7.1 基线管理

1. **初始基线**: 在部署后立即创建基线
   ```bash
   # 部署完成后
   python drift_detector.py --action create-baseline
   ```

2. **定期更新**: 每次配置变更后更新基线
   ```bash
   # 配置变更并验证后
   python drift_detector.py --action create-baseline
   ```

3. **版本控制**: 将基线文件纳入版本控制
   ```bash
   git add .baseline/manifest.json
   git commit -m "Update configuration baseline"
   ```

### 7.2 检测频率

| 环境 | 频率 | 说明 |
|------|------|------|
| **开发环境** | 每天1次 | 低频率，避免干扰开发 |
| **测试环境** | 每次部署后 | 确保测试环境一致性 |
| **预发布环境** | 每4小时 | 中等频率监控 |
| **生产环境** | 每30分钟 | 高频率监控 |

### 7.3 告警策略

```python
# 示例：根据漂移率设置告警级别
drift_rate = report.drift_rate

if drift_rate > 50:
    # 严重告警 - 立即通知
    send_alert("CRITICAL", f"Configuration drift rate: {drift_rate}%")
elif drift_rate > 20:
    # 警告 - 工作时间内通知
    send_alert("WARNING", f"Configuration drift rate: {drift_rate}%")
elif drift_rate > 0:
    # 信息 - 记录日志
    log_info(f"Minor configuration drift detected: {drift_rate}%")
```

### 7.4 响应流程

```
检测到漂移
    ↓
生成报告
    ↓
评估严重程度
    ↓
┌─────────────────┬─────────────────┬─────────────────┐
│   CRITICAL      │    WARNING      │     INFO        │
│   立即响应      │   24小时内响应  │  记录并跟踪     │
│   回滚配置      │   评估影响      │  定期审查       │
│   通知负责人    │   通知团队      │  无需立即行动   │
└─────────────────┴─────────────────┴─────────────────┘
```

---

## 8. 故障排查

### 8.1 常见问题

#### Q1: 提示"Baseline not found"

**原因**: 尚未创建基线  
**解决**:
```bash
python drift_detector.py --action create-baseline
```

#### Q2: 检测不到预期的漂移

**可能原因**:
1. 基线已过期（包含了漂移后的状态）
2. 文件被忽略模式过滤

**解决**:
```bash
# 重新创建基线
python drift_detector.py --action create-baseline

# 检查文件是否被忽略
python drift_detector.py --action detect --verbose
```

#### Q3: 报告文件无法生成

**可能原因**: 输出目录不存在或无权限  
**解决**:
```bash
# 确保输出目录存在
mkdir -p /path/to/output

# 使用绝对路径
python drift_detector.py --output /absolute/path/drift_report.json
```

### 8.2 调试模式

```bash
# 启用详细输出
python drift_detector.py --action detect --verbose

# 查看完整错误堆栈
python drift_detector.py --action detect --verbose 2>&1 | tee debug.log
```

### 8.3 性能优化

对于大型配置目录（>1000个文件）：

```bash
# 使用增量检测（只检测变更）
python drift_detector.py --action detect --incremental

# 限制检测范围
python drift_detector.py --config-dir agentos/manager/kernel --action detect
```

---

## 附录

### A. 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--config-dir` | 配置目录路径 | `../manager` |
| `--action` | 执行动作 | `both` |
| `--output` | JSON输出路径 | `drift_report.json` |
| `--output-md` | Markdown输出路径 | 自动生成 |
| `--verbose` | 详细模式 | `false` |
| `--fail-on-drift` | 检测到漂移时返回错误码 | `false` |

### B. 相关文件

| 文件 | 路径 | 说明 |
|------|------|------|
| 检测器脚本 | `tools/drift_detector.py` | 主程序 |
| 测试脚本 | `tests/test_drift_detector.py` | 单元测试 |
| 基线文件 | `.baseline/manifest.json` | 基线清单 |
| 检测报告 | `drift_report.json` | JSON报告 |
| 检测报告 | `drift_report.md` | Markdown报告 |

### C. 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| V1.0 | 2026-04-05 | 初始版本 |

---

**文档维护**: AgentOS Team  
**最后更新**: 2026-04-05  
**反馈渠道**: GitHub Issues
