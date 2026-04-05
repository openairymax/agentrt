# AgentOS Manager 配置工具集

本目录包含 Manager 模块的配置管理和运维工具。

## 目录结构

```
tools/
├── config_diff.py              # 配置差异对比工具
├── config_version_cleanup.py   # 配置版本历史清理工具
└── README.md                  # 本文件
```

## config_diff.py

配置差异对比工具，用于比较两个配置文件或目录之间的差异。

### 功能特性

- **单文件比较**: 比较两个配置文件
- **批量比较**: 比较两个目录下的所有配置文件
- **差异统计**: 新增、删除、修改计数
- **彩色输出**: 终端彩色高亮显示
- **JSON报告**: 生成机器可读的差异报告
- **元数据忽略**: 自动忽略 `_` 开头的元数据字段

### 使用方法

#### 单文件比较

```bash
# 基本用法
python3 config_diff.py file1.yaml file2.yaml

# 彩色输出（默认）
python3 config_diff.py file1.yaml file2.yaml

# 禁用彩色
python3 config_diff.py file1.yaml file2.yaml --no-color

# 输出JSON报告
python3 config_diff.py file1.yaml file2.yaml --output diff_report.json
```

#### 批量目录比较

```bash
# 比较两个目录
python3 config_diff.py --dir ./config --baseline ./baseline

# 输出汇总报告
python3 config_diff.py --dir ./config --baseline ./baseline --output batch_report.json

# 仅显示摘要
python3 config_diff.py --dir ./config --baseline ./baseline --quiet
```

### 输出示例

```
======================================================================
配置文件比较:
  文件1: config/kernel/settings.yaml
  文件2: baseline/kernel/settings.yaml
----------------------------------------------------------------------
统计:
  🟢 新增: 2
  🔴 删除: 1
  🟡 修改: 3
----------------------------------------------------------------------
详细差异:

+ kernel.new_feature
    新值: enabled

~ kernel.scheduler.max_concurrency
    旧值: 100
    新值: 200

- kernel.deprecated_setting
    旧值: true

======================================================================
```

### JSON报告格式

```json
{
  "file1": "config/kernel/settings.yaml",
  "file2": "baseline/kernel/settings.yaml",
  "timestamp": "2026-04-04T10:30:00",
  "summary": {
    "added": 2,
    "removed": 1,
    "modified": 3
  },
  "diffs": [
    {
      "path": "kernel.new_feature",
      "type": "added",
      "old_value": null,
      "new_value": "enabled"
    }
  ]
}
```

---

## config_version_cleanup.py

配置版本历史清理工具，用于清理过期的配置版本。

### 功能特性

- **按保留天数清理**: 删除超过N天的旧版本
- **按最大版本数清理**: 保留最近N个版本
- **版本压缩**: 压缩旧版本以节省空间
- **干运行模式**: 不实际删除，仅预览
- **详细报告**: 生成清理操作报告

### 使用方法

#### 干运行模式

```bash
# 预览将删除的版本（不实际删除）
python3 config_version_cleanup.py --dry-run --days 90
```

#### 按保留天数清理

```bash
# 删除90天前的版本
python3 config_version_cleanup.py --days 90

# 输出报告
python3 config_version_cleanup.py --days 90 --output cleanup_report.json
```

#### 按最大版本数清理

```bash
# 保留最近100个版本
python3 config_version_cleanup.py --max-versions 100

# 超过后额外保留10个压缩版本
python3 config_version_cleanup.py --max-versions 100 --compress-after 10
```

### 输出示例

```
======================================================================
AgentOS Manager 配置版本历史清理工具
======================================================================
历史目录: /data/agentos/manager-history
干运行模式: 否
----------------------------------------------------------------------
开始清理，保留最近 100 个版本...
找到 250 个版本
  删除: version_001.json
  压缩: version_101.json
  压缩: version_102.json
  删除: version_150.json

======================================================================
配置版本历史清理报告
======================================================================
清理时间: 2026-04-04T10:30:00
----------------------------------------------------------------------
总版本数: 250
保留版本: 100
删除版本: 100
压缩版本: 20
释放空间: 156.78 MB
======================================================================
```

### 集成到 Cron

```bash
# 每天凌晨2点执行清理
0 2 * * * /usr/bin/python3 /path/to/config_version_cleanup.py --days 90 --output /var/log/config-cleanup.json

# 每周一执行
0 2 * * 1 /usr/bin/python3 /path/to/config_version_cleanup.py --max-versions 50 --compress-after 20
```

---

## 最佳实践

### 配置差异对比

1. **提交前对比**: 提交配置变更前，使用 `config_diff.py` 检查变更内容
2. **版本回滚**: 使用 `config_diff.py` 比较当前配置与历史版本
3. **环境对比**: 比较开发环境与生产环境配置差异

### 版本历史清理

1. **定期清理**: 建议每周执行一次版本清理
2. **监控空间**: 关注历史目录大小，设置告警阈值
3. **保留策略**: 生产环境建议保留90天，测试环境保留30天

### 性能测试

1. **定期基准**: 建议每周运行一次性能基准测试
2. **性能回归**: 性能下降超过20%时触发告警
3. **资源监控**: 结合系统监控，跟踪资源占用趋势

---

Copyright (c) 2026 SPHARX. All Rights Reserved.
