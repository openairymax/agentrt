# Manager 配置工具集

`manager/tools/` 包含 Manager 模块的配置管理和运维工具。

## 工具列表

| 工具 | 文件 | 说明 |
|------|------|------|
| **配置差异对比** | `config_diff.py` | 比较两个配置文件的差异 |
| **版本历史清理** | `config_version_cleanup.py` | 清理配置历史版本 |

### 配置差异对比

比较两个配置文件的差异，支持 JSON 格式的递归对比：

```bash
python manager/tools/config_diff.py config_v1.json config_v2.json
```

输出格式示例：

```diff
- kernel.log_level: "debug"
+ kernel.log_level: "info"
- model.temperature: 0.8
+ model.temperature: 0.7
+ security.rate_limit: 2000
```

### 版本历史清理

清理配置版本历史，保留指定数量的最新版本：

```bash
# 保留最近 10 个版本
python manager/tools/config_version_cleanup.py --keep 10

# 清理 30 天前的版本
python manager/tools/config_version_cleanup.py --older-than 30

# 预览清理结果
python manager/tools/config_version_cleanup.py --dry-run
```

## CI/CD 集成

```yaml
# GitLab CI 示例
config-validation:
  script:
    - python manager/tools/config_diff.py config.json config.new.json
    - python manager/scripts/validate_config.py --config config.json
```

---

*AgentOS Manager — Tools*
