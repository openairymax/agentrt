# Cache — 缓存管理模块

**模块路径**: `agentos/commons/utils/cache/`
**版本**: v0.1.0

## 概述

Cache 模块提供通用的缓存实现，支持基于哈希表的键值对存储、检索、过期淘汰和容量控制。该模块是 AgentRT 中高频数据访问优化的基础设施，通过回调机制支持任意类型的键和值，并内置字符串键的便捷接口。

## 设计目标

- **高性能存储**：基于哈希表实现，支持 O(1) 平均查找时间的键值存储，冲突处理采用链表法
- **灵活的键值类型**：通过回调函数（哈希、比较、复制、释放）支持任意类型的键和值
- **容量控制**：可配置缓存容量上限，支持动态调整
- **过期机制**：支持 TTL（Time To Live）过期时间配置，自动淘汰过期条目
- **便捷接口**：内置字符串键的默认处理和便捷构造函数

## 目录结构

```
cache/
├── include/
│   └── cache_common.h            # 缓存模块公共接口定义
├── src/
│   └── cache_common.c            # 哈希表缓存实现
└── README.md                     # 本文档
```

## 核心数据结构

### cache_config_t — 缓存配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `capacity` | `size_t` | 缓存容量上限 |
| `ttl_sec` | `int` | 条目过期时间（秒） |
| `hash_func` | `cache_hash_func_t` | 键哈希函数指针 |
| `compare_func` | `cache_compare_func_t` | 键比较函数指针 |
| `key_free_func` | `cache_free_func_t` | 键释放函数指针 |
| `value_free_func` | `cache_free_func_t` | 值释放函数指针 |
| `key_copy_func` | `cache_copy_func_t` | 键复制函数指针 |
| `value_copy_func` | `cache_copy_func_t` | 值复制函数指针 |

### 回调函数类型

| 类型 | 签名 | 说明 |
|------|------|------|
| `cache_free_func_t` | `void (*)(void *data)` | 释放键/值的内存 |
| `cache_copy_func_t` | `void *(*)(const void *data)` | 深拷贝键/值 |
| `cache_compare_func_t` | `int (*)(const void *a, const void *b)` | 比较两个键 |
| `cache_hash_func_t` | `unsigned int (*)(const void *key)` | 计算键的哈希值 |

## 接口说明

### 生命周期管理

| 函数 | 说明 |
|------|------|
| `cache_create_default_config()` | 创建默认缓存配置 |
| `cache_create(config)` | 创建缓存实例，返回缓存句柄 |
| `cache_destroy(cache)` | 销毁缓存，释放所有资源 |
| `cache_create_string_cache(capacity, ttl_sec)` | 创建字符串键缓存（便捷方法） |

### 数据操作

| 函数 | 说明 |
|------|------|
| `cache_get(cache, key, out_value)` | 获取键对应的值，返回 1 找到 / 0 未找到 / -1 错误 |
| `cache_put(cache, key, value)` | 存入键值对，若键已存在则替换 |
| `cache_delete(cache, key)` | 删除指定键的条目 |
| `cache_clear(cache)` | 清空缓存中所有条目 |
| `cache_get_size(cache)` | 获取当前缓存条目数 |
| `cache_get_capacity(cache)` | 获取缓存容量上限 |
| `cache_set_capacity(cache, capacity)` | 动态调整缓存容量 |
| `cache_get_ttl(cache)` | 获取缓存过期时间 |
| `cache_set_ttl(cache, ttl_sec)` | 动态调整过期时间 |

### 字符串键便捷接口

| 函数 | 说明 |
|------|------|
| `cache_get_string(cache, key, out_value)` | 获取字符串键对应的字符串值 |
| `cache_put_string(cache, key, value)` | 存入字符串键值对 |
| `cache_string_hash(key)` | 字符串默认哈希函数 |
| `cache_string_compare(a, b)` | 字符串默认比较函数 |
| `cache_string_copy(data)` | 字符串默认复制函数 |
| `cache_string_free(data)` | 字符串默认释放函数 |

## 使用示例

```c
#include "cache_common.h"

// 方式一：使用字符串便捷接口
cache_t str_cache = cache_create_string_cache(100, 300);
cache_put_string(str_cache, "user:42", "{\"name\":\"Alice\"}");

char *value = NULL;
if (cache_get_string(str_cache, "user:42", &value) == 1) {
    printf("Cached: %s\n", value);
}
cache_destroy(str_cache);

// 方式二：自定义类型的通用缓存
cache_config_t config = cache_create_default_config();
config.capacity = 1000;
config.ttl_sec = 600;
config.hash_func = my_hash_func;
config.compare_func = my_compare_func;
config.key_free_func = my_free_func;
config.key_copy_func = my_copy_func;
config.value_free_func = my_free_func;
config.value_copy_func = my_copy_func;

cache_t cache = cache_create(&config);

void *result = NULL;
int rc = cache_get(cache, &my_key, &result);
if (rc == 1) {
    // 缓存命中
} else if (rc == 0) {
    // 缓存未命中，计算并存入
    cache_put(cache, &my_key, &computed_value);
}

cache_destroy(cache);
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `stdbool.h` | 布尔类型支持 |
| `stddef.h` | 标准类型定义（`size_t` 等） |
| `stdint.h` | 固定宽度整数类型 |

---

© 2026 SPHARX Ltd. All Rights Reserved.