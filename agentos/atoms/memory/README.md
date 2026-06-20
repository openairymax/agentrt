# Memory — 内存子系统

`agentos/atoms/memory/`

**版本**: v0.1.0

---

## 概述

Memory 是 AgentRT 的免费开源内存子系统，提供 L1 原始存储与 L2 特征索引的基础能力。采用**可拔插提供商架构**，通过统一的 `agentos_memory_provider_t` 函数指针表实现提供商的热替换——内置免费提供商（builtin_provider）提供 L1+L2 基础功能，商业提供商 MemoryRovol 实现 L1-L4 全功能 PRO 记忆系统。

Memory 模块以纯 C 实现，无 FAISS / SQLite / 外部向量数据库依赖，是 AgentRT 八层架构中 Atoms 微内核层的核心存储底座。

---

## 架构定位

```
AgentRT 八层架构：
  Atoms → Commons → Cupolas → Heapstore → Gateway → Protocols → Daemon → OpenLab + Toolkit SDK

Atoms 层内部调用链：
  engine.c → agentos_memory_provider_t* → builtin_provider / MemoryRovol
```

Memory 位于 Atoms 层，是整个 AgentRT 记忆能力的微内核抽象。上层模块（CoreLoopThree、Frameworks 等）通过 `agentos_memory_provider_t*` 指针访问记忆功能，无需感知底层提供商的具体实现。

---

## 设计理念

| 原则 | 说明 |
|------|------|
| **可拔插** | 提供商通过 vtable 函数指针表实现，运行时可切换，上层代码零修改 |
| **能力声明** | 每个提供商声明自身能力标记，上层可据此适配功能路径 |
| **零外部依赖** | 内置提供商纯 C 实现，无需 FAISS / SQLite / 向量数据库 |
| **跨平台** | 存储层兼容 Windows（`_mkdir`/`rand_s`）与 POSIX（`mkdir`/`/dev/urandom`） |
| **渐进增强** | L1 原始存储为基础，L2 特征索引为增强，L3/L4 由商业提供商扩展 |

---

## 目录结构

```
memory/
├── CMakeLists.txt          # 构建配置（静态库 agentos_memory）
├── src/
│   ├── memory_provider.h   # 提供商接口定义（核心头文件）
│   ├── builtin_provider.c  # 内置免费提供商实现
│   ├── builtin_storage.c   # L1 原始存储（文件系统 + JSON 索引）
│   ├── builtin_index.c     # L2 特征索引（双哈希表 + 关键词倒排索引）
│   └── builtin_retrieval.c # 检索策略（BM25 + 余弦相似度）
└── README.md
```

---

## 核心组件

### 1. 提供商接口 (`memory_provider.h`)

定义内存提供商的统一抽象，是整个模块的核心契约。

#### 能力标记 (`agentos_memory_capabilities_t`)

| 字段 | 说明 | builtin | MemoryRovol |
|------|------|---------|-------------|
| `l1_raw` | L1 原始存储 | 1 | 1 |
| `l2_feature` | L2 特征提取/向量索引 | 0 | 1 |
| `l3_structure` | L3 结构绑定/知识图谱 | 0 | 1 |
| `l4_pattern` | L4 模式识别 | 0 | 1 |
| `forgetting` | 遗忘引擎 | 1 | 1 |
| `attractor` | 吸引子网络 | 0 | 1 |
| `persistence` | 持久同调 | 1 | 1 |
| `faiss` | FAISS 加速索引 | 0 | 1 |
| `async_ops` | 异步操作 | 0 | 1 |
| `llm_integration` | LLM 集成 | 0 | 1 |

> **注意**: 内置提供商的 `l2_feature=0`，其 L2 层使用关键词倒排索引而非语义向量搜索。

#### 统计信息 (`agentos_memory_stats_t`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `total_records` | `uint64_t` | 总记录数 |
| `total_bytes` | `uint64_t` | 总字节数 |
| `l1_count` | `uint64_t` | L1 记录数 |
| `l2_indexed` | `uint64_t` | L2 已索引数 |
| `l3_relations` | `uint64_t` | L3 关系数 |
| `l4_patterns` | `uint64_t` | L4 模式数 |
| `index_utilization` | `double` | 索引利用率 |
| `provider_name` | `char[64]` | 提供商名称 |
| `provider_version` | `char[32]` | 提供商版本 |

#### 查询结果 (`agentos_memory_query_result_t`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `record_ids` | `char**` | 记录 ID 数组 |
| `scores` | `float*` | 相关性分数数组 |
| `count` | `size_t` | 结果数量 |

#### 提供商 vtable (`agentos_memory_provider_t`)

18 个函数指针构成完整的提供商协议：

| 分类 | 函数指针 | 说明 |
|------|----------|------|
| **生命周期** | `init` / `destroy` | 初始化与销毁 |
| **L1 原始存储** | `write_raw` / `get_raw` / `delete_raw` | 写入、读取、删除原始数据 |
| **查询检索** | `query` / `retrieve` | 关键词查询与语义检索 |
| **演化遗忘** | `evolve` / `forget` | 索引演化与遗忘策略 |
| **状态监控** | `stats` / `health_check` | 统计信息与健康检查 |
| **上下文挂载** | `mount` | 将记录挂载到当前上下文 |
| **便捷写入** | `add_memory` | 无需 metadata 的快捷写入 |
| **同步** | `sync_push` / `sync_pull` / `has_active_sync` | 提供商间记录同步 |

其他字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | `const char*` | 提供商名称 |
| `version` | `const char*` | 提供商版本 |
| `capabilities` | `agentos_memory_capabilities_t` | 能力标记 |
| `impl` | `void*` | 提供商私有实现数据 |
| `sync_target` | `agentos_memory_provider_t*` | 同步目标提供商 |

---

### 2. 内置提供商 (`builtin_provider.c`)

实现 `agentos_memory_provider_t` 接口的免费提供商，提供 L1+L2 基础功能。

**能力声明**: `l1_raw=1, persistence=1, forgetting=1`，其余为 0。

**内部结构** (`builtin_provider_impl_t`):

```
builtin_provider_impl_t
├── storage    → builtin_storage_t*   (L1 原始存储)
├── index      → builtin_index_t*     (L2 关键词索引)
├── retrieval  → builtin_retrieval_t* (检索策略)
└── stats      → agentos_memory_stats_t (统计信息)
```

**默认数据路径**: `./data/agentos/memory`

**启动日志**: `[AgentRT] using built-in provider (free) - storage: <path>`

**遗忘策略**: 淘汰 10% 最旧记录（按 `updated_at` 排序，选择最久未更新的记录删除）。

**同步支持**: 通过 `sync_target` 指针实现提供商间记录推送（push）与拉取（pull）。

---

### 3. L1 原始存储 (`builtin_storage.c`)

基于文件系统的原始数据存储，无外部数据库依赖。

**存储格式**: 每条记录存储为 `.bin` 文件，元数据保存在内存中的 `storage_record_t` 结构。

**记录元数据** (`storage_record_t`):

| 字段 | 类型 | 说明 |
|------|------|------|
| `record_id` | `char[64]` | 记录 ID，格式 `rec-{timestamp}-{random_hex}` |
| `file_path` | `char[512]` | 数据文件路径 |
| `data_len` | `size_t` | 数据长度 |
| `created_at` | `time_t` | 创建时间 |
| `updated_at` | `time_t` | 更新时间 |
| `metadata_json` | `char[1024]` | 元数据 JSON |

**平台兼容**:

| 平台 | 目录创建 | 安全随机数 |
|------|----------|------------|
| Windows | `_mkdir()` | `rand_s()` |
| POSIX | `mkdir(path, 0755)` | `/dev/urandom` |

**容量**: 最大 1,000,000 条记录，初始容量 1,024 条，动态 2 倍扩容。

---

### 4. L2 特征索引 (`builtin_index.c`)

基于双哈希表的关键词倒排索引，提供精确匹配与关键词检索。

**数据结构**:

```
builtin_index_t
├── id_table[4096]     → record_id → metadata_json 映射
├── token_table[4096]  → token → record_ids 倒排索引
└── total_docs         → 总文档数
```

**哈希函数**: DJB2（`h = ((h << 5) + h) + c`，初始值 5381）

**分词策略**: 从 `metadata_json` 中提取 ≥ 2 字符的字母数字 token，转小写后建立倒排索引。

**搜索流程**: 查询文本分词 → 倒排索引查找 → 分数累加 → 按分数降序排序。

**紧凑化** (`compact`): 清理空记录和零引用 token 条目，回收内存。

---

### 5. 检索策略 (`builtin_retrieval.c`)

提供 BM25 文本检索与余弦相似度向量检索两种策略。

**BM25 参数**: `k1=1.2, b=0.75`

**BM25 评分公式**:

```
score = Σ IDF(qi) × (tf × (k1 + 1)) / (tf + k1 × (1 - b + b × dl / avgdl))
IDF(qi) = log((N - df + 0.5) / (df + 0.5) + 1.0)
```

**余弦相似度**: 标准点积 / 模长乘积，内置零范数保护（`norm < 1e-8` 时返回 0.0）。

**分词**: 与 `builtin_index.c` 一致，提取 ≥ 2 字符的字母数字 token 并转小写。

---

## API 概览

### 全局提供商注册

| 接口 | 说明 |
|------|------|
| `agentos_memory_provider_register(provider)` | 注册内存提供商 |
| `agentos_memory_provider_get_active()` | 获取当前活跃提供商 |
| `agentos_memory_provider_set_active(provider)` | 设置活跃提供商 |
| `agentos_memory_provider_unregister()` | 注销并销毁活跃提供商 |

### 内置提供商工厂

| 接口 | 说明 |
|------|------|
| `agentos_builtin_memory_provider_init(storage_path)` | 初始化内置提供商并注册为活跃 |
| `agentos_builtin_provider_create()` | 创建内置提供商实例（不自动注册） |

### 资源释放

| 接口 | 说明 |
|------|------|
| `agentos_memory_provider_free_query_results(ids, scores, count)` | 释放查询结果 |
| `agentos_memory_query_result_free(result)` | 释放查询结果结构体 |

### 使用示例

```c
#include "memory_provider.h"

int main(void) {
    /* 初始化内置提供商 */
    agentos_error_t err = agentos_builtin_memory_provider_init(NULL);
    if (err != AGENTOS_SUCCESS) return 1;

    agentos_memory_provider_t *mem = agentos_memory_provider_get_active();

    /* 写入原始数据 */
    char *record_id = NULL;
    mem->write_raw(mem, "hello world", 11, "{\"type\":\"greeting\"}", &record_id);

    /* 查询 */
    char **ids = NULL;
    float *scores = NULL;
    size_t count = 0;
    mem->query(mem, "greeting", 10, &ids, &scores, &count);

    /* 读取原始数据 */
    void *data = NULL;
    size_t len = 0;
    mem->get_raw(mem, record_id, &data, &len);

    /* 遗忘 */
    mem->forget(mem);

    /* 健康检查 */
    char *health_json = NULL;
    mem->health_check(mem, &health_json);

    /* 清理 */
    agentos_memory_provider_free_query_results(ids, scores, count);
    agentos_memory_provider_unregister();
    return 0;
}
```

---

## 构建说明

Memory 模块编译为静态库 `agentos_memory`，依赖 commons 层的 error/compat/platform 头文件。

```bash
# 标准构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 仅构建 memory 模块
cmake --build build --target agentos_memory
```

**编译选项**:

| 平台 | 选项 |
|------|------|
| MSVC | `/W4`, 定义 `_CRT_SECURE_NO_WARNINGS` / `_CRT_NONSTDC_NO_DEPRECATE` |
| GCC/Clang | `-Wall -Wextra -Wpedantic` |

**安装**: 头文件安装至 `include/agentos/memory/`，库文件安装至 `lib/`。

---

## 与其他模块的关系

| 模块 | 关系 |
|------|------|
| **memoryrovol** | 商业提供商，实现同一 `agentos_memory_provider_t` 接口，提供 L1-L4 全功能 |
| **coreloopthree** | 上层消费者，在认知-执行循环中读写记忆 |
| **frameworks** | 通过 Frameworks 层的 `AGENTOS_FW_MEMORY` 框架类型封装 Memory |
| **commons/error** | 提供错误码定义（`agentos_error_t`） |
| **commons/compat** | 提供内存兼容宏（`AGENTOS_MALLOC/CALLOC/FREE/STRDUP`） |
| **commons/platform** | 提供平台抽象（线程、睡眠等） |

---

© 2026 SPHARX Ltd. All Rights Reserved.
