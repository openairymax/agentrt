# IO — I/O 工具模块

**模块路径**: `agentos/commons/utils/io/`
**版本**: v0.1.0

## 概述

IO 模块提供文件与目录操作的基础工具集，包括文件读写、目录创建和文件列表枚举。该模块是 AgentOS 中进行文件系统操作的标准接口，提供简洁的跨平台 API。

## 设计目标

- **简洁 API**：提供最小化但完整的文件 I/O 接口，读写文件只需一行代码
- **跨平台**：屏蔽不同操作系统的文件系统差异，统一目录创建和文件枚举行为
- **安全可靠**：返回明确的错误码，支持自动资源管理（如 `agentos_io_free_list`）
- **便捷性强**：自动处理文件全部内容的读取，允许自动计算字符串长度

## 目录结构

```
io/
├── include/
│   └── io.h                     # I/O 工具接口定义
├── src/
│   └── file_utils.c             # 文件与流操作实现
└── README.md                    # 本文档
```

## 接口说明

### 文件读写

| 函数 | 说明 |
|------|------|
| `agentos_io_read_file(path, out_len)` | 读取文件全部内容，返回分配的内存（需调用者 `free`），失败返回 NULL |
| `agentos_io_write_file(path, data, len)` | 写入数据到文件，`len` 为 `-1` 时自动计算字符串长度，返回 0 成功 |

### 目录操作

| 函数 | 说明 |
|------|------|
| `agentos_io_ensure_dir(path)` | 确保目录存在，不存在则创建 |
| `agentos_io_mkdir_p(path, mode)` | 递归创建目录（跨平台），`mode` 为 Unix 风格权限（Windows 忽略） |
| `agentos_io_list_files(path, out_files, out_count)` | 列出目录下所有文件（不包含子目录），返回 0 成功 |
| `agentos_io_free_list(files, count)` | 释放 `agentos_io_list_files` 返回的文件列表 |

## 使用示例

```c
#include "io.h"

// 读取文件全部内容
size_t len = 0;
char *content = agentos_io_read_file("/tmp/config.json", &len);
if (content != NULL) {
    printf("Read %zu bytes: %s\n", len, content);
    free(content);
}

// 写入文件
const char *data = "Hello, AgentOS!";
if (agentos_io_write_file("/tmp/output.txt", data, (size_t)-1) == 0) {
    printf("Write successful\n");
}

// 确保目录存在
if (agentos_io_ensure_dir("/tmp/agentos/logs/") == 0) {
    printf("Directory ready\n");
}

// 递归创建目录
if (agentos_io_mkdir_p("/tmp/agentos/data/2026", 0755) == 0) {
    printf("Directories created\n");
}

// 列出目录文件
char **files = NULL;
size_t count = 0;
if (agentos_io_list_files("/tmp/agentos/logs/", &files, &count) == 0) {
    for (size_t i = 0; i < count; i++) {
        printf("  %s\n", files[i]);
    }
    agentos_io_free_list(files, count);
}
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `stddef.h` | 标准类型定义（`size_t` 等） |

---

© 2026 SPHARX Ltd. All Rights Reserved.