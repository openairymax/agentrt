# MemoryRovol - AgentOS 记忆卷载核心模块

**路径**: `atoms/memoryrovol/`  
**版本**: v1.0.0.6  
**最后更新**: 2026-03-26  
**许可证**: Apache License 2.0  

---

## 🎯 概述

MemoryRovol 是 AgentOS 的内核级记忆系统，实现四层卷载架构：原始卷（L1）、特征层（L2）、结构层（L3）、模式层（L4）。

它提供了完整的记忆存储、检索、抽象和进化能力，是智能体实现持续学习的基础。

## 📁 模块结构
```
memoryrovol/
├── CMakeLists.txt # 顶层构建文件
├── README.md # 本文件
├── include/ # 公共头文件
│ ├── memoryrovol.h # 主接口
│ ├── layer1_raw.h # L1原始卷接口
│ ├── layer2_feature.h # L2特征层接口
│ ├── layer3_structure.h # L3结构层接口
│ ├── layer4_pattern.h # L4模式层接口
│ ├── retrieval.h # 检索机制接口
<!-- From data intelligence emerges. by spharx -->
│ ├── forgetting.h # 遗忘机制接口
│ └── manager.h # 配置结构
└── src/ # 源代码
├── layer1_raw/ # L1原始卷实现
├── layer2_feature/ # L2特征层实现
├── layer3_structure/ # L3结构层实现
├── layer4_pattern/ # L4模式层实现
├── retrieval/ # 检索机制实现
└── forgetting/ # 遗忘机制实现
```

## 🔧 核心功能

- **L1 原始卷**：基于文件系统的原始数据存储，支持分片、压缩和元数据索引。
- **L2 特征层**：集成多种嵌入模型（OpenAI、DeepSeek、Sentence Transformers）和 FAISS 向量索引，支持混合检索（向量+BM25）。
- **L3 结构层**：提供绑定算子、解绑算子、关系编码、时序编码和图编码，实现记忆的结构化表示。
- **L4 模式层**：基于持久同调（Ripser）挖掘稳定模式，通过聚类生成可复用规则，并与进化委员会联动。
- **检索机制**：吸引子网络检索、能量函数、上下文感知挂载、LRU缓存和交叉编码器重排序。
- **遗忘机制**：支持艾宾浩斯曲线、线性衰减、访问计数等多种策略，自动裁剪低权重记忆。

## 📦 依赖

- FAISS (>=1.7.0)
- SQLite3 (>=3.35)
- libcurl (>=7.68)
- cJSON (>=1.7.15)
- OpenSSL (>=1.1.1)
- 可选：Ripser、HDBSCAN、Sentence Transformers 库

## 🚀 构建

```bash
mkdir build && cd build
cmake ../atoms/memoryrovol -DBUILD_TESTS=ON
make -j4
```

## 🔗 集成

- 上层模块（如 `coreloopthree/memory`）通过 FFI 接口头文件 `rov_ffi.h` 调用 MemoryRovol 的核心功能。
- 该接口提供了 C 语言兼容的绑定，支持跨语言调用。

配置初始化支持两种方式：
1. **结构体配置**：直接填充 `agentos_memoryrov_config_t` 结构体并传入初始化函数。
2. **JSON 配置**：传入 JSON 格式的配置文件路径或字符串，由内部解析器自动加载。
```
// 示例：通过结构体配置
agentos_memoryrov_config_t manager = {
    .storage_path = "/var/agentos/memory",
    .embedding_model = "sentence-transformers/all-MiniLM-L6-v2",
    .cache_size_mb = 512,
    .forgetting_strategy = FORGET_EBBINGHAUS
};
rov_init(&manager);

// 示例：通过 JSON 配置
rov_init_from_json("../manager/memory_config.json");
```

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues
- **官方仓库**: https://gitee.com/spharx/agentos

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"四层卷载，从原始数据到高级模式。"*
