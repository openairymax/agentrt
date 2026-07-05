# Heapstore Scripts — 运维工具脚本

**模块路径**: `agentrt/heapstore/scripts/`
**版本**: v0.1.0

## 概述

`heapstore/scripts/` 包含 Heapstore 模块的运维工具脚本，提供性能回归检测和版本标签管理功能，用于保障模块的代码质量和性能稳定性。

## 目录结构

```
scripts/
├── performance_regression_detector.py   # 性能回归检测工具
├── add_since_tags.py                    # 版本标签添加工具
└── README.md                            # 本文件
```

## 核心组件

### performance_regression_detector.py

性能回归检测工具，用于自动检测 Heapstore 模块的性能退化：

- 对比历史基准数据与当前性能指标
- 检测吞吐量下降、延迟增加等回归现象
- 生成性能回归报告

### add_since_tags.py

版本标签添加工具，为代码中的 API 函数自动添加 `@since` 版本标签：

- 扫描头文件中的函数声明
- 自动插入 `@since vN.N.N` 标签
- 保持代码文档与版本号同步

## 使用说明

```bash
# 运行性能回归检测
python scripts/performance_regression_detector.py

# 添加版本标签
python scripts/add_since_tags.py
```

## 依赖关系

| 组件 | 用途 |
|------|------|
| Python ≥ 3.10 | 脚本运行环境 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
