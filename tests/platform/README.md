# 跨平台兼容性测试

`tests/platform/`

## 概述

`platform/` 目录包含 AgentOS 的跨平台兼容性测试，共 **3 个文件**，验证 C 层 API 在不同操作系统（Linux/macOS/Windows）上的行为一致性。

> **版本**：v0.1.0

## 目录结构

```
platform/                          # 共 3 个文件
├── README.md                      # 本文档
└── c/                             # C 平台测试（CMocka，3 个文件）
    ├── CMakeLists.txt
    ├── test_platform_compat.c     # 跨平台 API 兼容性测试
    └── test_mkdir_recursive.c     # 递归目录创建测试
```

## 运行方式

```bash
cd build && ctest -R platform
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
