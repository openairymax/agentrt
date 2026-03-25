# AgentOS Partdata 模块 CI/CD 文档

| 文档信息 | |
|---------|---------|
| **版本** | v1.0.0 |
| **编制日期** | 2026-03-24 |
| **适用模块** | AgentOS Partdata 数据分区模块 |
| **文档状态** | 正式发布 |

---

## 一、概述

本文档描述了 `partdata` 模块的完整 CI/CD 流程，涵盖代码提交验证、自动化构建、单元测试、集成测试、安全扫描、质量检查、版本控制、部署策略等完整环节。

### 1.1 CI/CD 流程架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Partdata CI/CD Pipeline                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────┐    ┌──────────────────────────────────────────────────┐ │
│  │  代码提交 │───▶│              Stage 0: Code Quality               │ │
│  └──────────┘    │  • cppcheck 静态分析  • clang-format 代码风格     │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              Stage 1: Cross-Platform Build        │ │
│                   │  • Ubuntu/macOS/Windows  • GCC/Clang 编译器       │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              Stage 2: Unit Tests                  │ │
│                   │  • 7 个测试文件  • 覆盖率报告  • 集成测试         │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              Stage 3: Security Scan               │ │
│                   │  • Semgrep  • TruffleHog  • 秘钥检测              │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              Stage 4: Performance Benchmark       │ │
│                   │  • 8 项性能基准测试  • 结果归档                    │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              Stage 5: Quality Gate                │ │
│                   │  • 所有检查必须通过  • 失败则阻断流程              │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              Stage 6: Release Artifacts           │ │
│                   │  • 静态库打包  • 头文件归档  • 版本标记           │ │
│                   └──────────────────────────────────────────────────┘ │
│                                      │                                   │
│                                      ▼                                   │
│                   ┌──────────────────────────────────────────────────┐ │
│                   │              Stage 7: Deploy                      │ │
│                   │  • staging/production  • 手动审批  • 回滚支持     │ │
│                   └──────────────────────────────────────────────────┘ │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 二、触发条件

### 2.1 自动触发

| 事件 | 条件 | 触发阶段 |
|------|------|----------|
| Push | `main` 或 `develop` 分支 | 全流程 |
| Pull Request | 目标分支为 `main` | 全流程（不含部署） |
| 路径匹配 | `partdata/**` 变更 | 全流程 |

### 2.2 手动触发

```yaml
workflow_dispatch:
  inputs:
    build_type:        # Release | Debug | RelWithDebInfo
    run_benchmark:     # true | false
    deploy_target:     # none | staging | production
```

---

## 三、构建配置

### 3.1 构建矩阵

| 操作系统 | 编译器 | 构建类型 |
|----------|--------|----------|
| ubuntu-latest | GCC | Release |
| ubuntu-latest | Clang | Release |
| macos-latest | GCC | Release |
| macos-latest | Clang | Release |
| windows-latest | MSVC | Release |

### 3.2 依赖项

**Ubuntu/macOS:**
```bash
# 必需依赖
cmake >= 3.15
ninja
gcc >= 9.0 或 clang >= 10.0
libsqlite3-dev

# 可选依赖
lcov          # 覆盖率报告
cppcheck      # 静态分析
clang-format  # 代码格式化
clang-tidy    # 代码检查
```

**Windows:**
```powershell
# 必需依赖
cmake >= 3.15
ninja
Visual Studio 2019+ (MSVC)
sqlite3

# 可选依赖
clang-format
clang-tidy
```

### 3.3 本地构建

**Linux/macOS:**
```bash
cd partdata
./scripts/build.sh all           # 完整构建流程
BUILD_TYPE=Debug ./scripts/build.sh build  # Debug 构建
./scripts/build.sh test          # 运行测试
./scripts/build.sh package       # 打包
```

**Windows:**
```cmd
cd partdata
scripts\build.bat all            # 完整构建流程
set BUILD_TYPE=Debug
scripts\build.bat build          # Debug 构建
scripts\build.bat test           # 运行测试
scripts\build.bat package        # 打包
```

---

## 四、测试覆盖

### 4.1 测试文件列表

| 文件 | 类型 | 覆盖内容 |
|------|------|----------|
| `test_partdata_core.c` | 单元测试 | 核心功能：初始化、路径管理、统计、清理 |
| `test_partdata_log.c` | 单元测试 | 日志系统：写入、轮转、级别过滤 |
| `test_partdata_registry.c` | 单元测试 | 注册表：Agent/技能/会话 CRUD |
| `test_partdata_trace.c` | 单元测试 | 追踪系统：Span 写入、批量、刷新 |
| `test_partdata_ipc.c` | 单元测试 | IPC 存储：通道、缓冲区管理 |
| `test_partdata_memory.c` | 单元测试 | 内存管理：池、分配记录 |
| `test_partdata_integration.c` | 集成测试 | 完整生命周期、多子系统协同 |
| `benchmark_partdata.c` | 性能测试 | 8 项性能基准 |

### 4.2 运行测试

```bash
# 运行所有测试
cd build
ctest --output-on-failure

# 运行特定测试
./tests/partdata_tests

# 运行性能基准
./tests/partdata_benchmark
```

### 4.3 覆盖率要求

| 指标 | 目标值 | 当前状态 |
|------|--------|----------|
| 行覆盖率 | ≥ 80% | 待测量 |
| 分支覆盖率 | ≥ 70% | 待测量 |
| 函数覆盖率 | ≥ 90% | 待测量 |

---

## 五、安全扫描

### 5.1 扫描工具

| 工具 | 用途 | 配置文件 |
|------|------|----------|
| cppcheck | C/C++ 静态分析 | 内置规则 |
| Semgrep | 安全漏洞检测 | `.semgrep.yml` |
| TruffleHog | 秘钥泄露检测 | 内置规则 |
| clang-tidy | 代码质量检查 | `.clang-tidy` |

### 5.2 自定义规则

`.semgrep.yml` 包含以下自定义规则：

- `partdata-sql-injection`: SQL 注入检测
- `partdata-buffer-overflow`: 缓冲区溢出检测
- `partdata-format-string`: 格式化字符串漏洞检测
- `partdata-memory-leak`: 内存泄漏检测
- `partdata-uninitialized-pointer`: 未初始化指针检测

### 5.3 安全扫描结果

扫描结果存储于 GitHub Actions Artifacts：
- `partdata-security-reports/`
- 保留期限：30 天

---

## 六、版本控制

### 6.1 版本号规范

```
主版本.次版本.修订版本.构建号
  │        │        │        │
  │        │        │        └── 日常修复 (1.0.0.6)
  │        │        └── 功能新增,向下兼容 (1.0.1)
  │        └── 重大功能更新,向下兼容 (1.1.0)
  └── 不兼容的API变更 (2.0.0)
```

### 6.2 制品命名规范

```
agentos-partdata-{版本}-{操作系统}-{架构}.{格式}

示例:
agentos-partdata-1.0.0.6-linux-x86_64.tar.gz
agentos-partdata-1.0.0.6-macos-arm64.tar.gz
agentos-partdata-1.0.0.6-windows-x64.zip
```

### 6.3 制品存储

| 类型 | 存储位置 | 保留期限 |
|------|----------|----------|
| CI 临时制品 | GitHub Actions Artifacts | 7 天 |
| 测试报告 | GitHub Actions Artifacts | 7 天 |
| 安全报告 | GitHub Actions Artifacts | 30 天 |
| 发布制品 | GitHub Releases | 永久 |

---

## 七、部署策略

### 7.1 环境层次

```
dev (本地) ──▶ test (CI) ──▶ staging ──▶ production
   │              │              │              │
   └── 开发者     └── 自动化     └── 手动触发   └── 需审批
```

### 7.2 部署命令

```bash
# 部署到 staging
DEPLOY_DIR=/opt/agentos ./scripts/deploy.sh deploy

# 部署到 production（需要审批）
VERSION=1.0.0.7 DEPLOY_DIR=/opt/agentos ./scripts/deploy.sh deploy

# 验证部署
./scripts/deploy.sh verify

# 健康检查
./scripts/deploy.sh health

# 查看部署历史
./scripts/deploy.sh history

# 回滚到上一版本
./scripts/deploy.sh rollback

# 清理旧备份
./scripts/deploy.sh clean 5
```

### 7.3 回滚机制

1. **自动备份**: 每次部署前自动备份当前版本
2. **版本记录**: `deployments.log` 记录所有部署版本
3. **一键回滚**: `deploy.sh rollback` 恢复到上一版本
4. **备份保留**: 默认保留最近 5 个备份

---

## 八、监控告警

### 8.1 CI/CD 监控指标

| 指标 | 描述 | 告警阈值 |
|------|------|----------|
| 构建成功率 | 成功构建占比 | < 95% |
| 构建时长 | 平均构建时间 | > 30 分钟 |
| 测试覆盖率 | 代码覆盖率 | < 80% |
| 安全漏洞 | 高危漏洞数量 | > 0 |

### 8.2 通知渠道

- GitHub Actions 状态徽章
- PR 检查状态
- Workflow Summary 报告

---

## 九、故障排查

### 9.1 常见问题

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 构建失败 | 依赖缺失 | 检查 `cmake` 输出，安装缺失依赖 |
| 测试失败 | 代码缺陷 | 查看 `ctest` 输出，定位失败测试 |
| 安全扫描失败 | 漏洞检测 | 查看 `bandit_report.json`，修复漏洞 |
| 部署失败 | 权限问题 | 检查 `DEPLOY_DIR` 权限 |

### 9.2 日志位置

| 日志类型 | 位置 |
|----------|------|
| CI 构建日志 | GitHub Actions → Workflow → Jobs |
| 测试输出 | `partdata/build/test_output/` |
| 部署日志 | `${DEPLOY_DIR}/logs/` |
| 安全报告 | GitHub Artifacts → `partdata-security-reports` |

---

## 十、最佳实践

### 10.1 代码提交

1. 提交前运行 `pre-commit` 钩子
2. 确保本地测试通过
3. 遵循代码风格规范（`.clang-format`）

### 10.2 PR 流程

1. 创建功能分支
2. 提交 PR 到 `main` 分支
3. 等待 CI 检查通过
4. 代码审查通过后合并

### 10.3 发布流程

1. 更新版本号
2. 合并到 `main` 分支
3. CI 自动构建发布制品
4. 手动触发部署到 staging
5. 验证通过后部署到 production

---

## 十一、相关文档

| 文档 | 路径 |
|------|------|
| 项目 CI/CD 方案 | `.本地总结/CI_CD解决方案.md` |
| 模块 README | `partdata/README.md` |
| 构建脚本 | `partdata/scripts/build.sh` |
| 部署脚本 | `partdata/scripts/deploy.sh` |
| GitHub Actions | `.github/workflows/partdata-ci.yml` |

---

*本文档由 AgentOS CTO (SOLO Coder) 基于《工程控制论》《论系统工程》思想设计*
*© 2026 SPHARX Ltd. 保留所有权利*
