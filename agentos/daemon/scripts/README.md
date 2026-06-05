# Daemon Scripts

AgentOS 守护进程级别的 CI/CD 与质量保障脚本。

## 脚本清单

| 脚本 | 说明 |
|------|------|
| `ci.sh` | CI 构建入口脚本 |
| `local-ci.sh` | 本地 CI 模拟运行 |
| `static-analysis.sh` | 静态代码分析 |
| `verify-coverage.sh` | 代码覆盖率验证 |

## 用法

```bash
bash scripts/ci.sh
bash scripts/static-analysis.sh
```

## 版本

v0.1.0