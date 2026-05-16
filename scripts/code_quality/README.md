# 代码质量工具

`scripts/code-quality/`

## 概述

`code-quality/` 目录提供 AgentOS 代码质量分析和保障工具集，包括代码重复率检测、圈复杂度分析、编码问题修复、测试覆盖率增强、YAML 语法检查、路径引用更新等功能，帮助开发团队维护高质量的代码库。

## 目录结构

```
code-quality/
├── unified_quality_analyzer.py       # 统一代码质量分析器
├── analyze_quality.py                # Python 代码质量分析器
├── analyze_quality_comprehensive.py  # 综合代码质量分析器
├── enhance_coverage.py               # 测试覆盖率增强工具
├── check_yaml_syntax.py              # YAML 语法检查工具
├── update_openlab_paths.py           # 路径引用更新工具
├── check-quality.sh                  # Shell 脚本质量检查入口
├── requirements.txt                  # Python 依赖
├── docs/
│   └── verify_consistency.py         # 文档一致性验证
└── encoding/
    ├── fix_bom.py                    # BOM 清除工具
    ├── fix_double_encoding.py        # 双重编码修复工具
    └── check_encoding.py             # 编码检查和转换工具
```

## 工具列表

| 脚本 | 说明 |
|------|------|
| `unified_quality_analyzer.py` | 统一代码质量分析器：集成 clang-tidy、cppcheck、bandit、mypy 等工具 |
| `analyze_quality.py` | Python 代码质量分析器：检测代码重复率和圈复杂度 |
| `analyze_quality_comprehensive.py` | 综合代码质量分析器：支持 Python/C/C++ 多语言分析 |
| `enhance_coverage.py` | 测试覆盖率增强工具：分析并提升模块测试覆盖率至 90% 以上 |
| `check_yaml_syntax.py` | YAML 语法和非法字符检查工具 |
| `update_openlab_paths.py` | 路径引用更新工具：批量更新 openlab 路径为 agentos/openlab |
| `check-quality.sh` | Shell 脚本：运行 jscpd 和 lizard 进行代码质量检查 |

### encoding/ 子目录

| 脚本 | 说明 |
|------|------|
| `check_encoding.py` | 文件编码检查和转换工具（支持批量转换） |
| `fix_bom.py` | BOM (Byte Order Mark) 清除工具 |
| `fix_double_encoding.py` | 双重编码修复工具（解决 UTF-8/GBK 乱码问题） |

### docs/ 子目录

| 脚本 | 说明 |
|------|------|
| `verify_consistency.py` | 文档一致性验证工具 |

## 使用示例

```bash
# Python 代码质量分析
python3 code-quality/analyze_quality.py --scan-python
python3 code-quality/analyze_quality.py --full-scan --output report.json

# 综合质量分析（多语言）
python3 code-quality/analyze_quality_comprehensive.py --full-scan

# 统一质量分析（集成多种静态分析工具）
python3 code-quality/unified_quality_analyzer.py --scan-all

# Shell 脚本质量检查
./code-quality/check-quality.sh .

# YAML 语法检查
python3 code-quality/check_yaml_syntax.py

# 测试覆盖率增强
python3 code-quality/enhance_coverage.py --module corekern --target 95

# 编码检查和修复
python3 code-quality/encoding/check_encoding.py --convert
python3 code-quality/encoding/fix_bom.py --fix
python3 code-quality/encoding/fix_double_encoding.py --scan-only

# 文档一致性验证
python3 code-quality/docs/verify_consistency.py
```

## 依赖安装

```bash
pip install -r code-quality/requirements.txt
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
