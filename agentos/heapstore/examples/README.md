# Heapstore Examples — 使用示例

**模块路径**: `agentos/heapstore/examples/`
**版本**: v0.0.5

## 概述

`heapstore/examples/` 包含 Heapstore 模块的使用示例代码，演示核心 API 的基本用法，包括数据分区初始化、日志写入、注册表操作和批量写入等常见场景。

## 目录结构

```
examples/
├── quick_start.c         # 快速入门示例（初始化 + 日志写入 + 关闭）
├── batch_write.c         # 批量写入操作示例（批量日志 + 事务提交）
├── CMakeLists.txt        # 示例构建配置
└── README.md             # 本文件
```

## 核心组件

### quick_start.c

快速入门示例，演示 Heapstore 的基本使用流程：

1. 初始化数据分区（`heapstore_init`）
2. 快速路径写入日志（`heapstore_log_write_fast`）
3. 慢速路径写入日志（`heapstore_log_write_slow`）
4. 关闭数据分区（`heapstore_shutdown`）

### batch_write.c

批量写入示例，演示批量操作的使用方式：

1. 创建批量写入上下文（`heapstore_batch_begin`）
2. 添加多条日志到批量缓冲（`heapstore_batch_add_log`）
3. 提交批量写入（`heapstore_batch_commit`）
4. 销毁批量写入上下文（`heapstore_batch_context_destroy`）

## 使用说明

```bash
# 构建 Heapstore 模块（含示例）
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make heapstore_examples

# 运行快速入门示例
./build/quick_start

# 运行批量写入示例
./build/batch_write
```

## 依赖关系

| 组件 | 用途 |
|------|------|
| heapstore 核心库 | 数据存储 API |
| CMake ≥ 3.16 | 构建系统 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
