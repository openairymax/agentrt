Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 记忆管理 API

**版本**: v1.0.0.6  
**最后更新**: 2026-03-23  
**状态**: 🟢 生产就绪

---

## 🎯 概述

记忆管理 API 提供 AgentOS 四层记忆系统（MemoryRovol）的完整访问接口。记忆系统采用渐进抽象架构：L1 原始卷 → L2 特征层 → L3 结构层 → L4 模式层，支持语义检索、模式挖掘和长期进化。

---

## 📋 API 索引

| 函数 | 描述 | 复杂度 |
|------|------|--------|
| `agentos_memory_write()` | 写入记忆记录到 L1 原始卷 | O(1) |
| `agentos_memory_query()` | 语义检索记忆记录 | O(log n) |
| `agentos_memory_evolve()` | 触发记忆进化（L1→L4） | O(n log n) |
| `agentos_memory_forget()` | 主动遗忘低价值记忆 | O(log n) |
| `agentos_memory_stats()` | 获取记忆系统统计信息 | O(1) |
| `agentos_memory_layer_stats()` | 获取特定层统计信息 | O(1) |
| `agentos_memory_export()` | 导出记忆数据 | O(n) |
| `agentos_memory_import()` | 导入记忆数据 | O(n) |

---

## 🔧 核心数据结构

### 记忆记录

```c
/**
 * @brief 记忆记录结构体
 * 
 * 描述一个记忆单元，包含内容、元数据和重要性评分。
 */
typedef struct agentos_memory_record {
    /** 记录唯一标识符（UUID 格式） */
    char id[37];  // 36字符UUID + 终止符
    
    /** 记忆内容（UTF-8，最大 64KB） */
    char* content;
    
    /** 内容长度（字节） */
    size_t content_len;
    
    /** 记忆元数据（JSON 格式） */
    char* metadata;
    
    /** 重要性评分（0.0-1.0） */
    float importance;
    
    /** 记忆来源 Agent ID */
    char source_agent[64];
    
    /** 记忆创建时间戳（Unix 毫秒） */
    uint64_t created_at;
    
    /** 最后访问时间戳（Unix 毫秒） */
    uint64_t last_accessed_at;
    
    /** 访问次数 */
    uint32_t access_count;
    
    /** 记忆层级（L1-L4） */
    enum {
        MEMORY_LAYER_L1 = 1,  /* 原始卷：原始数据存储 */
        MEMORY_LAYER_L2 = 2,  /* 特征层：向量化特征 */
        MEMORY_LAYER_L3 = 3,  /* 结构层：关系图谱 */
        MEMORY_LAYER_L4 = 4   /* 模式层：抽象模式 */
    } layer;
    
    /** 记忆标签（用于分类检索） */
    char** tags;
    
    /** 标签数量 */
    uint32_t tag_count;
    
    /** 记忆向量（L2+ 使用，FAISS 索引） */
    float* vector;
    
    /** 向量维度 */
    uint32_t vector_dim;
    
    /** 记忆关联（L3+ 使用，图结构） */
    char** related_ids;
    
    /** 关联数量 */
    uint32_t related_count;
    
    /** 记忆模式（L4 使用，抽象模式描述） */
    char* pattern;
    
    /** trace_id（用于全链路追踪） */
    char trace_id[128];
} agentos_memory_record_t;
```

### 记忆查询条件

```c
/**
 * @brief 记忆查询条件结构体
 */
typedef struct agentos_memory_query {
    /** 查询文本（自然语言） */
    char* text;
    
    /** 查询向量（替代文本查询） */
    float* vector;
    
    /** 向量维度 */
    uint32_t vector_dim;
    
    /** 查询标签 */
    char** tags;
    
    /** 标签数量 */
    uint32_t tag_count;
    
    /** 时间范围：开始时间戳 */
    uint64_t time_start;
    
    /** 时间范围：结束时间戳 */
    uint64_t time_end;
    
    /** 重要性阈值（0.0-1.0） */
    float importance_threshold;
    
    /** 来源 Agent 过滤 */
    char* source_agent;
    
    /** 查询层级（0表示所有层级） */
    uint8_t layer;
    
    /** 返回结果数量限制 */
    uint32_t limit;
    
    /** 相似度阈值（0.0-1.0） */
    float similarity_threshold;
    
    /** 排序方式 */
    enum {
        MEMORY_SORT_BY_TIME_DESC = 0,    /* 时间倒序 */
        MEMORY_SORT_BY_TIME_ASC = 1,     /* 时间正序 */
        MEMORY_SORT_BY_IMPORTANCE = 2,   /* 重要性降序 */
        MEMORY_SORT_BY_SIMILARITY = 3,   /* 相似度降序 */
        MEMORY_SORT_BY_ACCESS_COUNT = 4  /* 访问次数降序 */
    } sort_by;
} agentos_memory_query_t;
```

### 记忆查询结果

```c
/**
 * @brief 记忆查询结果结构体
 */
typedef struct agentos_memory_result {
    /** 记忆记录 */
    agentos_memory_record_t* record;
    
    /** 查询相似度得分（0.0-1.0） */
    float similarity_score;
    
    /** 结果排名 */
    uint32_t rank;
    
    /** 下一结果指针（链表结构） */
    struct agentos_memory_result* next;
} agentos_memory_result_t;
```

### 记忆统计信息

```c
/**
 * @brief 记忆统计信息结构体
 */
typedef struct agentos_memory_statistics {
    /** 总记录数 */
    uint64_t total_records;
    
    /** 各层记录数 */
    uint64_t layer_counts[4];  /* L1-L4 */
    
    /** 总存储大小（字节） */
    uint64_t total_storage_bytes;
    
    /** 各层存储大小 */
    uint64_t layer_storage_bytes[4];
    
    /** 平均重要性评分 */
    float avg_importance;
    
    /** 查询命中率（%） */
    float query_hit_rate_percent;
    
    /** 平均查询延迟（毫秒） */
    float avg_query_latency_ms;
    
    /** 记忆进化次数 */
    uint64_t evolution_count;
    
    /** 最后进化时间戳 */
    uint64_t last_evolution_at;
    
    /** 主动遗忘记录数 */
    uint64_t forgotten_records;
    
    /** FAISS 索引状态 */
    struct {
        /** 索引类型 */
        char index_type[64];
        
        /** 索引大小 */
        uint64_t index_size_bytes;
        
        /** 索引构建时间（毫秒） */
        uint32_t build_time_ms;
        
        /** 索引准确率（%） */
        float accuracy_percent;
    } faiss_stats;
    
    /** 图数据库状态（L3） */
    struct {
        /** 节点数量 */
        uint64_t node_count;
        
        /** 边数量 */
        uint64_t edge_count;
        
        /** 平均度 */
        float avg_degree;
        
        /** 连通分量数 */
        uint32_t connected_components;
    } graph_stats;
} agentos_memory_statistics_t;
```

---

## 📖 API 详细说明

### `agentos_memory_write()`

```c
/**
 * @brief 写入记忆记录到 L1 原始卷
 * 
 * 将记忆内容写入记忆系统的 L1 层，后续会自动触发特征提取和向量化。
 * 
 * @param content 记忆内容（UTF-8）
 * @param content_len 内容长度（字节）
 * @param metadata 元数据（JSON 格式，可选）
 * @param importance 重要性评分（0.0-1.0）
 * @param tags 标签数组（可选）
 * @param tag_count 标签数量
 * @param out_id 输出参数，返回记录 ID
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_id 由调用者负责释放
 * @threadsafe 是
 * @reentrant 否
 * @see agentos_memory_query(), agentos_memory_evolve()
 */
AGENTOS_API agentos_error_t
agentos_memory_write(const char* content,
                     size_t content_len,
                     const char* metadata,
                     float importance,
                     const char** tags,
                     uint32_t tag_count,
                     char** out_id);
```

**参数说明**：
- `content`: 记忆内容文本
- `content_len`: 内容长度（最大 64KB）
- `metadata`: JSON 格式元数据，如 `{"type": "conversation", "emotion": "positive"}`
- `importance`: 重要性评分，影响记忆保留优先级
- `tags`: 标签数组，用于分类检索
- `tag_count`: 标签数量
- `out_id`: 输出参数，返回生成的 UUID

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 写入成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效
- `AGENTOS_ERROR_NO_MEMORY` (0x1002): 内存不足
- `AGENTOS_ERROR_STORAGE_FULL` (0x2001): 存储空间不足
- `AGENTOS_ERROR_CONTENT_TOO_LARGE` (0x2002): 内容过大

**使用示例**：
```c
char* record_id = NULL;
const char* tags[] = {"conversation", "user_feedback", "positive"};
const char* metadata = "{\"type\": \"user_feedback\", \"rating\": 5}";

agentos_error_t err = agentos_memory_write(
    "用户表示对产品的新功能非常满意，特别是实时协作功能",
    68,  // 内容长度（字节）
    metadata,
    0.8f,  // 重要性评分
    tags,
    3,  // 标签数量
    &record_id
);

if (err == AGENTOS_SUCCESS) {
    printf("记忆写入成功，ID: %s\n", record_id);
    free(record_id);
}
```

### `agentos_memory_query()`

```c
/**
 * @brief 语义检索记忆记录
 * 
 * 根据自然语言查询或向量查询检索相关记忆记录。
 * 支持跨层级检索（L1-L4）和混合检索。
 * 
 * @param query 查询条件
 * @param out_results 输出参数，返回结果链表
 * @param out_count 输出参数，返回结果数量
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @ownership out_results 由调用者负责释放
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t
agentos_memory_query(const agentos_memory_query_t* query,
                     agentos_memory_result_t** out_results,
                     uint32_t* out_count);
```

**参数说明**：
- `query`: 查询条件结构体
- `out_results`: 输出参数，返回结果链表头指针
- `out_count`: 输出参数，返回匹配结果数量

**返回值**：
- `AGENTOS_SUCCESS` (0x0000): 查询成功
- `AGENTOS_ERROR_INVALID_ARGUMENT` (0x1001): 参数无效
- `AGENTOS_ERROR_NO_RESULTS` (0x2003): 无匹配结果
- `AGENTOS_ERROR_INDEX_NOT_READY` (0x2004): 索引未就绪

**使用示例**：
```c
// 构建查询条件
agentos_memory_query_t query = {0};
query.text = "用户对协作功能的反馈";
query.limit = 10;
query.similarity_threshold = 0.6f;
query.sort_by = MEMORY_SORT_BY_SIMILARITY;

// 执行查询
agentos_memory_result_t* results = NULL;
uint32_t result_count = 0;
agentos_error_t err = agentos_memory_query(&query, &results, &result_count);

if (err == AGENTOS_SUCCESS) {
    printf("找到 %u 条相关记忆:\n", result_count);
    
    // 遍历结果链表
    agentos_memory_result_t* current = results;
    uint32_t i = 1;
    while (current != NULL) {
        printf("%u. [相似度: %.2f] %s\n", 
               i++, 
               current->similarity_score,
               current->record->content);
        current = current->next;
    }
    
    // 释放结果链表
    agentos_memory_result_free(results);
}
```

### `agentos_memory_evolve()`

```c
/**
 * @brief 触发记忆进化（L1→L4）
 * 
 * 手动触发记忆系统的进化过程，将 L1 原始数据提升到更高抽象层级。
 * 进化过程包括：特征提取、向量化、关系挖掘、模式发现。
 * 
 * @param config 进化配置（JSON 格式）
 * @param out_stats 输出参数，返回进化统计信息
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 否（进化过程独占资源）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_memory_evolve(const char* config,
                      agentos_memory_evolution_stats_t* out_stats);
```

**进化配置示例**：
```json
{
    "trigger": "manual",
    "layers": ["L1", "L2", "L3", "L4"],
    "batch_size": 1000,
    "similarity_threshold": 0.7,
    "max_evolution_time_ms": 300000,
    "force_rebuild": false
}
```

### `agentos_memory_forget()`

```c
/**
 * @brief 主动遗忘低价值记忆
 * 
 * 根据艾宾浩斯遗忘曲线主动清理低重要性、低访问频率的记忆记录。
 * 支持配置遗忘策略和保留规则。
 * 
 * @param policy 遗忘策略（JSON 格式）
 * @param out_forgotten 输出参数，返回遗忘记录数
 * @return AGENTOS_SUCCESS 成功；其他值表示错误
 * 
 * @threadsafe 是
 * @reentrant 否
 */
AGENTOS_API agentos_error_t
agentos_memory_forget(const char* policy,
                      uint64_t* out_forgotten);
```

**遗忘策略示例**：
```json
{
    "strategy": "ebbinghaus",
    "max_age_days": 30,
    "importance_threshold": 0.3,
    "min_access_count": 1,
    "preserve_tags": ["important", "core_knowledge"],
    "dry_run": false
}
```

---

## 🚀 使用示例

### 完整记忆管理流程

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agentos/memory.h"

// 辅助函数：释放结果链表
void free_memory_results(agentos_memory_result_t* results) {
    agentos_memory_result_t* current = results;
    while (current != NULL) {
        agentos_memory_result_t* next = current->next;
        free(current->record->content);
        free(current->record->metadata);
        free(current->record);
        free(current);
        current = next;
    }
}

int main(void)
{
    agentos_error_t err;
    
    // 1. 写入多条记忆记录
    const char* conversations[] = {
        "用户询问如何导出项目数据，需要 CSV 格式",
        "用户报告登录时遇到验证码显示问题",
        "用户建议增加深色模式主题选项",
        "用户称赞客服响应速度快，问题解决及时"
    };
    
    char* record_ids[4];
    for (int i = 0; i < 4; i++) {
        const char* tags[] = {"conversation", "user_feedback"};
        err = agentos_memory_write(
            conversations[i],
            strlen(conversations[i]),
            "{\"type\": \"customer_service\"}",
            0.7f + (i * 0.1f),  // 重要性递增
            tags,
            2,
            &record_ids[i]
        );
        
        if (err != AGENTOS_SUCCESS) {
            fprintf(stderr, "记忆写入失败: 0x%04X\n", err);
            return 1;
        }
        printf("记忆 %d 写入成功: %s\n", i, record_ids[i]);
    }
    
    // 2. 语义检索相关记忆
    agentos_memory_query_t query = {0};
    query.text = "用户反馈和问题报告";
    query.limit = 5;
    query.similarity_threshold = 0.5f;
    query.sort_by = MEMORY_SORT_BY_SIMILARITY;
    
    agentos_memory_result_t* results = NULL;
    uint32_t result_count = 0;
    err = agentos_memory_query(&query, &results, &result_count);
    
    if (err == AGENTOS_SUCCESS) {
        printf("\n语义检索结果 (%u 条):\n", result_count);
        
        agentos_memory_result_t* current = results;
        while (current != NULL) {
            printf("• [相似度: %.2f] %s\n", 
                   current->similarity_score,
                   current->record->content);
            current = current->next;
        }
        
        free_memory_results(results);
    }
    
    // 3. 获取记忆统计信息
    agentos_memory_statistics_t stats;
    err = agentos_memory_stats(&stats);
    
    if (err == AGENTOS_SUCCESS) {
        printf("\n记忆系统统计:\n");
        printf("  总记录数: %llu\n", stats.total_records);
        printf("  L1 记录: %llu\n", stats.layer_counts[0]);
        printf("  L2 记录: %llu\n", stats.layer_counts[1]);
        printf("  L3 记录: %llu\n", stats.layer_counts[2]);
        printf("  L4 记录: %llu\n", stats.layer_counts[3]);
        printf("  查询命中率: %.1f%%\n", stats.query_hit_rate_percent);
        printf("  平均查询延迟: %.1f ms\n", stats.avg_query_latency_ms);
    }
    
    // 4. 清理资源
    for (int i = 0; i < 4; i++) {
        free(record_ids[i]);
    }
    
    return 0;
}
```

### 高级记忆操作

```c
// 向量查询示例
void vector_query_example(void)
{
    // 假设已有预计算的查询向量
    float query_vector[384];  // 384维向量
    // ... 填充向量数据 ...
    
    agentos_memory_query_t query = {0};
    query.vector = query_vector;
    query.vector_dim = 384;
    query.limit = 10;
    query.similarity_threshold = 0.7f;
    
    agentos_memory_result_t* results = NULL;
    uint32_t count = 0;
    agentos_memory_query(&query, &results, &count);
    
    // 处理结果...
    free_memory_results(results);
}

// 批量导入记忆数据
void batch_import_example(void)
{
    const char* import_data = 
        "[\n"
        "  {\n"
        "    \"content\": \"项目需求文档 v1.0\",\n"
        "    \"metadata\": \"{\\\"type\\\": \\\"document\\\", \\\"version\\\": \\\"1.0\\\"}\",\n"
        "    \"importance\": 0.9,\n"
        "    \"tags\": [\"document\", \"requirements\", \"core\"]\n"
        "  },\n"
        "  {\n"
        "    \"content\": \"团队会议纪要：讨论架构设计\",\n"
        "    \"metadata\": \"{\\\"type\\\": \\\"meeting\\\", \\\"date\\\": \\\"2026-03-20\\\"}\",\n"
        "    \"importance\": 0.7,\n"
        "    \"tags\": [\"meeting\", \"architecture\", \"team\"]\n"
        "  }\n"
        "]";
    
    uint64_t imported_count = 0;
    agentos_memory_import(import_data, &imported_count);
    printf("导入 %llu 条记忆记录\n", imported_count);
}
```

---

## ⚠️ 注意事项

### 1. 内存管理
- `agentos_memory_write()` 返回的 `out_id` 需要调用者释放
- `agentos_memory_query()` 返回的结果链表需要调用 `agentos_memory_result_free()` 释放
- 记忆记录中的 `content` 和 `metadata` 字段由 API 内部管理，调用者不应直接修改

### 2. 性能考虑
- 记忆写入是 O(1) 操作，但后续的特征提取是异步的
- 语义检索使用 FAISS 索引，复杂度 O(log n)
- 记忆进化是计算密集型操作，建议在低负载时段执行
- 批量操作时使用 `agentos_memory_import()` 比多次调用 `agentos_memory_write()` 更高效

### 3. 数据一致性
- 记忆写入后不会立即出现在查询结果中（需要特征提取完成）
- 使用 `agentos_memory_evolve()` 可以强制更新所有层级
- 系统重启后记忆数据会从持久化存储恢复，但索引需要重建

### 4. 错误处理
- 始终检查 API 返回值
- 使用 `agentos_strerror()` 获取错误描述
- 记录 `trace_id` 便于问题排查

### 5. 安全考虑
- 记忆内容可能包含敏感信息，确保适当的数据加密
- 使用 `agentos_memory_forget()` 定期清理不需要的记忆
- 配置适当的访问控制策略

---

## 🔍 错误码参考

| 错误码 | 值 | 描述 |
|--------|-----|------|
| `AGENTOS_SUCCESS` | 0x0000 | 操作成功 |
| `AGENTOS_ERROR_INVALID_ARGUMENT` | 0x1001 | 参数无效 |
| `AGENTOS_ERROR_NO_MEMORY` | 0x1002 | 内存不足 |
| `AGENTOS_ERROR_STORAGE_FULL` | 0x2001 | 存储空间不足 |
| `AGENTOS_ERROR_CONTENT_TOO_LARGE` | 0x2002 | 内容过大（>64KB） |
| `AGENTOS_ERROR_NO_RESULTS` | 0x2003 | 无匹配结果 |
| `AGENTOS_ERROR_INDEX_NOT_READY` | 0x2004 | FAISS 索引未就绪 |
| `AGENTOS_ERROR_EVOLUTION_IN_PROGRESS` | 0x2005 | 记忆进化进行中 |
| `AGENTOS_ERROR_IMPORT_FORMAT` | 0x2006 | 导入数据格式错误 |
| `AGENTOS_ERROR_EXPORT_FAILED` | 0x2007 | 导出失败 |
| `AGENTOS_ERROR_PERMISSION_DENIED` | 0x1009 | 权限不足 |

---

## 📚 相关文档

- [系统调用总览](../README.md) - 所有系统调用 API 概览
- [任务管理 API](task.md) - 任务生命周期管理 API
- [会话管理 API](session.md) - 会话生命周期管理 API
- [可观测性 API](telemetry.md) - 指标、日志和追踪 API
- [Agent 管理 API](agent.md) - Agent 生命周期管理 API
- [架构设计：记忆系统](../../architecture/folder/memoryrovol.md) - MemoryRovol 架构详解

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*