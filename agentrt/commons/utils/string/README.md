# String — 字符串工具模块

**模块路径**: `agentrt/commons/utils/string/`
**版本**: v0.1.0

## 概述

String 模块提供安全、高效、统一的字符串处理接口，为 AgentRT 系统提供字符串操作的基础设施。该模块包含两套 API 层：核心层（`agentrt_string.h`）提供完整的字符串缓冲区、视图、列表、编码转换等高级功能，公共层（`string_common.h`）提供常用的字符串复制、连接、比较、查找、转换等基础操作。所有接口均设计为线程安全，避免缓冲区溢出等常见安全问题。

## 设计目标

- **安全第一**：所有字符串操作均提供缓冲区大小检查，防止缓冲区溢出
- **统一接口**：消除项目中分散的字符串处理代码，提供一致的字符串操作策略
- **编码支持**：支持 ASCII、UTF-8、UTF-16、UTF-32、Latin-1 等多种字符编码转换
- **高效操作**：提供字符串缓冲区（动态扩容）、字符串视图（零拷贝）和字符串列表等高级数据结构

## 目录结构

```
string/
├── include/
│   ├── agentrt_string.h       # 核心层字符串处理 API（缓冲区、视图、列表、编码转换）
│   ├── string_common.h        # 公共层字符串基础操作 API
│   └── string_compat.h        # 跨平台兼容性定义（ssize_t、snprintf 等）
├── src/
│   ├── string.c               # 核心层字符串处理实现
│   └── string_common.c        # 公共层字符串操作实现
├── test/
│   └── test_string.c          # 单元测试
└── README.md                  # 本文档
```

## 核心数据结构

### string_buffer_t — 字符串缓冲区

| 字段 | 类型 | 说明 |
|------|------|------|
| `data` | `char *` | 缓冲区数据 |
| `capacity` | `size_t` | 缓冲区容量（包括空字符） |
| `length` | `size_t` | 当前字符串长度（不包括空字符） |
| `encoding` | `string_encoding_t` | 字符串编码 |
| `gateway` | `bool` | 是否为动态分配 |

### string_view_t — 字符串视图（零拷贝）

| 字段 | 类型 | 说明 |
|------|------|------|
| `data` | `const char *` | 字符串数据（不拥有所有权） |
| `length` | `size_t` | 字符串长度 |
| `encoding` | `string_encoding_t` | 字符串编码 |

### string_list_t — 字符串列表

| 字段 | 类型 | 说明 |
|------|------|------|
| `items` | `string_view_t *` | 字符串项数组 |
| `count` | `size_t` | 项数 |
| `capacity` | `size_t` | 数组容量 |

### string_encoding_t — 字符串编码类型

| 枚举值 | 说明 |
|------|------|
| `STRING_ENCODING_ASCII` | ASCII 编码 |
| `STRING_ENCODING_UTF8` | UTF-8 编码 |
| `STRING_ENCODING_UTF16_LE` | UTF-16 小端序 |
| `STRING_ENCODING_UTF16_BE` | UTF-16 大端序 |
| `STRING_ENCODING_UTF32_LE` | UTF-32 小端序 |
| `STRING_ENCODING_UTF32_BE` | UTF-32 大端序 |
| `STRING_ENCODING_LATIN1` | Latin-1 (ISO-8859-1) |
| `STRING_ENCODING_WINDOWS_1252` | Windows-1252 |

### string_compare_option_t — 字符串比较选项

| 枚举值 | 说明 |
|------|------|
| `STRING_COMPARE_CASE_SENSITIVE` | 区分大小写 |
| `STRING_COMPARE_CASE_INSENSITIVE` | 不区分大小写 |
| `STRING_COMPARE_NATURAL` | 自然排序（如 "file10" > "file2"） |
| `STRING_COMPARE_LOCALE_AWARE` | 区域感知比较 |

## 接口说明

### 核心层 API（agentrt_string.h）

#### 安全字符串操作

| 函数 | 说明 |
|------|------|
| `string_copy(dest, src, dest_size)` | 安全复制字符串到缓冲区，返回复制的字符数 |
| `string_copy_n(dest, src, count, dest_size)` | 安全复制指定长度字符串到缓冲区 |
| `string_concat(dest, src, dest_size)` | 安全连接字符串到缓冲区 |
| `string_concat_n(dest, src, count, dest_size)` | 安全连接指定长度字符串到缓冲区 |
| `string_length(str, max_len)` | 计算字符串长度（安全版本，防止无界字符串） |

#### 字符串比较与查找

| 函数 | 说明 |
|------|------|
| `string_compare(str1, str2, options)` | 比较两个字符串，支持大小写不敏感、自然排序等选项 |
| `string_compare_n(str1, str2, len, options)` | 比较两个字符串（指定长度） |
| `string_find(haystack, needle, options)` | 查找子字符串 |
| `string_find_last(haystack, needle, options)` | 从末尾开始查找子字符串 |
| `string_find_char(str, ch)` | 查找字符第一次出现的位置 |
| `string_find_char_last(str, ch)` | 查找字符最后一次出现的位置 |

#### 字符串修剪与转换

| 函数 | 说明 |
|------|------|
| `string_trim(str)` | 修剪字符串开头和结尾的空白字符 |
| `string_trim_start(str)` | 修剪字符串开头的空白字符 |
| `string_trim_end(str)` | 修剪字符串结尾的空白字符 |
| `string_to_lower(str)` | 转换字符串为小写 |
| `string_to_upper(str)` | 转换字符串为大写 |

#### 字符串替换与分割

| 函数 | 说明 |
|------|------|
| `string_replace(str, old_substr, new_substr, result, result_size)` | 替换字符串中的子字符串 |
| `string_split(str, delimiter, options, limit)` | 分割字符串，返回 `string_list_t` |
| `string_join(list, delimiter, result, result_size)` | 连接字符串列表 |

#### 字符串检查

| 函数 | 说明 |
|------|------|
| `string_starts_with(str, prefix, options)` | 检查字符串是否以指定前缀开头 |
| `string_ends_with(str, suffix, options)` | 检查字符串是否以指定后缀结尾 |
| `string_is_blank(str)` | 检查字符串是否只包含空白字符 |
| `string_is_digit(str)` | 检查字符串是否只包含数字字符 |
| `string_is_alpha(str)` | 检查字符串是否只包含字母字符 |
| `string_is_alnum(str)` | 检查字符串是否只包含字母数字字符 |

#### 字符串格式化

| 函数 | 说明 |
|------|------|
| `string_format(buffer, buffer_size, format, ...)` | 安全格式化字符串 |
| `string_format_v(buffer, buffer_size, format, args)` | 安全格式化字符串（va_list 版本） |
| `string_alloc_format(format, ...)` | 分配并格式化字符串（需手动释放） |
| `string_alloc_format_v(format, args)` | 分配并格式化字符串（va_list 版本） |

#### 字符串内存分配

| 函数 | 说明 |
|------|------|
| `string_alloc_copy(str)` | 复制字符串（分配新内存） |
| `string_alloc_copy_n(str, len)` | 复制指定长度字符串（分配新内存） |
| `string_alloc_concat(str1, str2)` | 连接字符串（分配新内存） |

#### 字符串缓冲区操作

| 函数 | 说明 |
|------|------|
| `string_buffer_create(initial_capacity, encoding)` | 创建字符串缓冲区 |
| `string_buffer_destroy(buffer)` | 销毁字符串缓冲区 |
| `string_buffer_append(buffer, str)` | 向缓冲区追加字符串 |
| `string_buffer_append_n(buffer, str, len)` | 向缓冲区追加指定长度字符串 |
| `string_buffer_append_format(buffer, format, ...)` | 向缓冲区追加格式化字符串 |
| `string_buffer_append_char(buffer, ch)` | 向缓冲区追加字符 |
| `string_buffer_clear(buffer)` | 清空缓冲区 |
| `string_buffer_cstr(buffer)` | 获取 C 字符串（只读） |
| `string_buffer_length(buffer)` | 获取缓冲区长度 |

#### 字符串视图操作

| 函数 | 说明 |
|------|------|
| `string_view_create(str, encoding)` | 创建字符串视图 |
| `string_view_create_n(str, len, encoding)` | 从指定长度创建字符串视图 |
| `string_view_compare(view1, view2, options)` | 比较两个字符串视图 |
| `string_view_find(haystack, needle, options)` | 查找子字符串视图 |
| `string_view_to_cstr(view)` | 字符串视图转换为 C 字符串 |

#### 字符串列表操作

| 函数 | 说明 |
|------|------|
| `string_list_create(initial_capacity)` | 创建字符串列表 |
| `string_list_destroy(list)` | 销毁字符串列表 |
| `string_list_add(list, item)` | 向列表添加字符串视图 |
| `string_list_add_cstr(list, str)` | 向列表添加 C 字符串 |
| `string_list_clear(list)` | 清空列表 |
| `string_list_size(list)` | 获取列表大小 |
| `string_list_get(list, index)` | 获取列表项 |

#### 编码转换

| 函数 | 说明 |
|------|------|
| `string_convert_encoding(src, src_enc, dest, dest_size, dest_enc)` | 编码转换 |
| `string_utf8_char_count(str, max_len)` | 计算 UTF-8 字符串的字符数（码点） |
| `string_utf8_next_char(str, ch)` | 获取 UTF-8 字符串的下一个字符 |
| `string_utf8_validate(str, len)` | 检查字符串是否为有效的 UTF-8 |

### 公共层 API（string_common.h）

| 函数 | 说明 |
|------|------|
| `string_common_strlcpy(dest, dest_size, src)` | 安全的字符串复制 |
| `string_common_strlcat(dest, dest_size, src)` | 安全的字符串连接 |
| `string_common_strdup(str)` | 字符串复制（动态内存分配） |
| `string_common_strndup(str, n)` | 字符串复制（指定长度，动态内存分配） |
| `string_common_strcasecmp(s1, s2)` | 大小写不敏感的字符串比较 |
| `string_common_strncasecmp(s1, s2, n)` | 大小写不敏感的字符串比较（指定长度） |
| `string_common_strstr(haystack, needle)` | 字符串查找 |
| `string_common_strsplit(str, delim)` | 字符串分割，返回字符串数组 |
| `string_common_strsplit_free(arr)` | 释放字符串数组 |
| `string_common_strtoint(str, base, result)` | 字符串转换为整数 |
| `string_common_strtouint(str, base, result)` | 字符串转换为无符号整数 |
| `string_common_strtod(str, result)` | 字符串转换为双精度浮点数 |
| `string_common_itoa(value, base, buf, buf_size)` | 整数转换为字符串 |
| `string_common_utoa(value, base, buf, buf_size)` | 无符号整数转换为字符串 |
| `string_common_ftoa(value, precision, buf, buf_size)` | 双精度浮点数转换为字符串 |
| `string_common_strtrim(str)` | 字符串修剪（去除首尾空白字符） |
| `string_common_strtolower(str)` | 字符串转小写 |
| `string_common_strtoupper(str)` | 字符串转大写 |
| `string_common_json_escape(src, out)` | JSON 字符串转义（动态分配） |
| `string_common_json_escape_buf(src, dst, dst_size)` | JSON 字符串转义（固定缓冲区） |

## 使用示例

```c
#include "agentrt_string.h"

/* === 安全字符串复制 === */
char dest[64];
if (string_copy(dest, "Hello, AgentRT!", sizeof(dest)) < 0) {
    fprintf(stderr, "Buffer too small\n");
    return;
}

/* === 字符串比较 === */
if (string_compare("agent", "Agent", STRING_COMPARE_CASE_INSENSITIVE) == 0) {
    printf("Strings are equal (case-insensitive)\n");
}

/* === 字符串分割 === */
string_list_t list = string_split("a,b,c", ",", STRING_SPLIT_TRIM_WHITESPACE, 0);
for (size_t i = 0; i < string_list_size(&list); i++) {
    string_view_t item = string_list_get(&list, i);
    printf("Item %zu: %.*s\n", i, (int)item.length, item.data);
}
string_list_destroy(&list);

/* === 字符串缓冲区 === */
string_buffer_t *buf = string_buffer_create(64, STRING_ENCODING_UTF8);
string_buffer_append(buf, "Hello");
string_buffer_append(buf, " ");
string_buffer_append(buf, "World");
string_buffer_append_format(buf, "! (v%d.%d)", 1, 0);
printf("Buffer: %s\n", string_buffer_cstr(buf));
string_buffer_destroy(buf);

/* === 动态格式化字符串 === */
char *formatted = string_alloc_format("Task #%d completed in %.2f ms", 42, 123.45);
printf("%s\n", formatted);
free(formatted);

/* === 前缀/后缀检查 === */
if (string_starts_with("agentrt_task_001", "agentrt_", STRING_COMPARE_CASE_SENSITIVE)) {
    printf("Valid task ID prefix\n");
}
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `stdbool.h` | 布尔类型支持 |
| `stddef.h` | `size_t`、`ssize_t` 等类型 |
| `stdint.h` | 固定宽度整数类型 |
| `stdarg.h` | 可变参数支持（格式化函数） |

## 配置选项

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `initial_buffer_size` | `size_t` | 64 | 字符串缓冲区初始容量 |
| `max_buffer_size` | `size_t` | 0（无限制） | 字符串缓冲区最大容量 |
| `locale_aware` | `bool` | false | 是否区域感知格式化 |
| `null_string` | `const char *` | `"(null)"` | NULL 指针的替代字符串 |

---

© 2026 SPHARX Ltd. All Rights Reserved.