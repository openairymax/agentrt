# CI/CD 流水线与质量

`scripts/ci/`

## 概述

`ci/` 目录包含 AgentOS 项目的 CI/CD 流水线核心脚本、代码质量检查工具、验证脚本和发布管理工具。这些脚本主要被 GitHub Actions 工作流和 pre-commit 钩子调用。

## 目录结构

```
ci/
├── README.md
├── pipeline/                              # CI/CD 流水线脚本
│   ├── ci-run.sh                          # CI 主运行脚本
│   ├── build-module.sh                    # 模块编译
│   ├── run-tests.sh                       # 测试执行
│   ├── quality-gate.sh                    # 代码质量门禁
│   ├── security_check.py                  # 安全扫描
│   ├── security_regression.sh             # 安全回归测试
│   ├── install-deps.sh                    # 依赖安装
│   ├── deploy-artifacts.sh                # 构建产物部署
│   ├── requirements-linux.txt             # Linux 构建依赖
│   └── requirements-macos.txt             # macOS 构建依赖
├── quality/                               # 代码质量工具
│   ├── unified_quality_analyzer.py        # 统一质量分析器
│   ├── analyze_quality.py                 # 质量分析
│   ├── analyze_quality_comprehensive.py   # 综合质量分析
│   ├── check-quality.sh                   # 质量检查入口
│   ├── check_yaml_syntax.py               # YAML 语法检查
│   ├── enhance_coverage.py                # 覆盖率增强
│   ├── update_openlab_paths.py            # 路径更新
│   ├── requirements.txt                   # Python 依赖
│   ├── encoding/                          # 编码问题检测与修复
│   │   ├── check_encoding.py
│   │   ├── fix_bom.py
│   │   └── fix_double_encoding.py
│   └── docs/                              # 文档一致性验证
│       └── verify_consistency.py
├── verify/                                # 构建验证
│   ├── test_build_modes.sh                # 构建模式测试
│   ├── sec017_scan.sh                     # 安全扫描
│   ├── verify_sdks.sh                     # SDK 验证 (Linux/macOS)
│   └── verify_sdks.ps1                    # SDK 验证 (Windows)
└── release/                               # 发布管理
    ├── release.sh                         # 自动化发布
    └── cleanup_builds.sh                  # 构建产物清理
```