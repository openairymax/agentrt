Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

---
copyright: "Copyright (c) 2026 SPHARX Ltd. All Rights Reserved."
slogan: "From data intelligence emerges 始于数据，终于智能。"
title: "AgentOS 官方技术白皮书"
version: "Doc V1.7"
last_updated: "2026-03-31"
author: "LirenWang"
status: "production_ready"
review_due: "2026-06-30"
theoretical_basis: "工程两论、五维正交系统、双系统认知理论"
target_audience: "技术决策者/架构师/开发者/研究人员"
prerequisites: "了解操作系统基础，熟悉分布式系统概念"
estimated_reading_time: "30分钟"
core_concepts: "白皮书体系, 技术架构, 性能指标, 开源生态"
---

# AgentOS 官方技术白皮书

<div align="center">

**智能体超级操作系统 · 技术权威阐述**

*"From data intelligence emerges 始于数据，终于智能。"*

</div>

## 📋 文档信息卡
- **目标读者**: 技术决策者/架构师/开发者/研究人员
- **前置知识**: 了解操作系统基础，熟悉分布式系统概念
- **预计阅读时间**: 30分钟
- **核心概念**: 白皮书体系, 技术架构, 性能指标, 开源生态
- **文档状态**: 🟢 生产就绪
- **复杂度标识**: ⭐⭐ 中级

---

## 📋 概述

本目录是 AgentOS 项目**官方技术白皮书的唯一权威发布与归档渠道**，所有白皮书文档均与项目代码版本强绑定，确保技术理念、架构设计与工程实现完全一致，全程可追溯、可核验。

### 白皮书定位

AgentOS 技术白皮书系统化阐述了：
- **核心定位**：面向多智能体协作的操作系统内核，而非又一个 Agent 框架
- **架构创新**：微内核 + 三层认知循环 + 四层记忆卷载 + 安全穹顶
- **理论基础**：工程两论、双系统认知理论、微内核哲学、设计美学
- **工程实践**：生产级性能指标、开发者工具链、生态合作模式

### 版本说明

| 版本 | 对应代码 Release | 发布日期 | 状态 | 说明 |
| :-------- | :---------------------- | :------- | :--- | :--- |
| **V1.0.0.6** | v1.0.0.6 | 2026-03-23 | ✅ 正式开源 | 开源孵化首发版本，完整阐述项目核心定位、架构设计与技术创新 |
| （后续版本按行追加） | - | - | - | - |

---

## 📚 正式版本获取

### 核心技术白皮书（全场景通用）

#### 中文正式版（官方首发）
- **PDF 版**：[AgentOS 技术白皮书 V1.0.pdf](./zh/AgentOS_技术白皮书_V1.0.pdf)（44.6KB，正式发布归档格式）
- **Markdown 源文件**：[AgentOS 技术白皮书 V1.0.md](./zh/AgentOS_技术白皮书_V1.0.md)（139.3KB，支持社区协同修订）

#### 英文正式版（翻译完成）
- **PDF 版**：[AgentOS Technical White Paper V1.0.pdf](./en/AgentOS_Technical_White_Paper_V1.0.pdf)
- **Markdown 源文件**：[AgentOS Technical White Paper V1.0.md](./en/AgentOS_Technical_White_Paper_V1.0.md)（39.4KB）

> **注意**：PDF 为不可篡改的标准归档格式，建议使用 PDF 版本进行阅读和引用；Markdown 源文件支持 Git 追溯与社区协同修订。

---

## 🏗️ 核心内容速览

### 微内核架构（~9,000 行代码）

四大原子机制，极简内核设计：
- **IPC Binder**：高性能进程间通信，1024 并发连接
- **内存管理**：智能内存池，碎片率 < 5%
- **任务调度**：加权轮询策略，延迟 < 1ms
- **高精度时间**：纳秒级定时器，误差 < 0.1%

详见：[agentos/atoms/corekern](../../agentos/atoms/corekern/README.md)

### 三层认知循环（CoreLoopThree）⭐

决策与执行分离，认知与记忆融合：
```
认知层 → 规划层 → 执行层
  ↑          ↓
  └──── 反馈闭环 ────┘
```

- **认知层**：意图理解、任务规划（DAG）、Agent 调度、模型协同
- **行动层**：执行引擎、补偿事务（Saga）、责任链追踪
- **记忆层**：MemoryRovol FFI、上下文挂载、LRU 缓存

**性能指标**：
- 任务规划延迟：< 5ms
- Agent 调度延迟：< 5ms
- 实时反馈响应：< 1ms

详见：[agentos/atoms/coreloopthree](../../agentos/atoms/coreloopthree/README.md)

### 四层记忆卷载（MemoryRovol）⭐

从原始数据到高级模式的全栈记忆管理：
```
L4 模式层（持久同调·HDBSCAN 聚类）
    ↑ 抽象进化
L3 结构层（绑定算子·关系编码）
    ↑ 特征提取
L2 特征层（FAISS 向量索引·混合检索）
    ↑ 数据压缩
L1 原始卷（文件系统存储·分片压缩）
```

**性能指标**（Intel i7-12700K, 32GB RAM, NVMe SSD）：
- L1 写入吞吐：**10,000+ 条/秒**
- L2 向量检索延迟：**< 10ms** (k=10)
- 混合检索延迟：**< 50ms** (top-100 重排序)
- L2→L3 抽象速度：**100 条/秒**
- L4 模式挖掘速度：**10 万条/分钟**

详见：[agentos/atoms/memoryrovol](../../agentos/atoms/memoryrovol/README.md)

### 安全穹顶（cupolas）⭐

四重防护机制，构建零信任安全体系：
- **虚拟工位**：进程/容器/WASM 沙箱隔离，资源限额
- **权限裁决**：YAML 规则引擎，通配符匹配，热更新
- **输入净化**：正则过滤，风险等级标注（0-3 级）
- **审计追踪**：异步写入，日志轮转，全链路记录

详见：[cupolas](../../agentos/cupolas/README.md)

### Token 效率优势

基于标准工程任务测试（对比行业主流框架）：
- **Token 节省**：约 **60%** 使用量
- **Token 利用率**：领先 **2-3 倍**
- **原因**：分层上下文管理 + 记忆精准检索 + 预算控制机制

详见：[utils/cost](../../agentos/commons/utils/cost/README.md)

---

## 📁 白皮书目录结构

```
white_paper/
├── zh/                         # 中文正式版本（官方首发）
│   ├── AgentOS_技术白皮书_V1.0.pdf    # 正式发布 PDF 版（44.6KB）
│   └── AgentOS_技术白皮书_V1.0.md     # Markdown 源文件（139.3KB）
│
├── en/                         # 英文正式版本（翻译完成）
│   ├── AgentOS_Technical_White_Paper_V1.0.pdf  # PDF 版
│   └── AgentOS_Technical_White_Paper_V1.0.md   # Markdown 源文件（39.4KB）
│
├── history/                    # 历史版本归档目录（计划中）
│   └── V{版本号}/              # 按版本号分目录归档，保留完整演进记录
│
└── README.md                   # 本说明文档
```

---

## 🔧 专项技术白皮书（规划中）

以下专项白皮书按计划推进，预计 2026 年起陆续发布：

| 白皮书名称 | 计划版本 | 预计发布时间 | 主要内容 |
| :-------- | :------- | :----------- | :------- |
| **安全技术白皮书** | V1.0 | 2026-Q2 | cupolas 安全架构、权限模型、审计机制、攻防演练 |
| **硬件适配白皮书** | V1.0 | 2026-Q2 | GPU/NPU 加速、存储优化、边缘设备部署 |
| **应用开发白皮书** | V1.0 | 2026-Q3 | Agent 开发范式、技能插件化、最佳实践案例 |
| **性能优化白皮书** | V1.0 | 2026-Q3 | FAISS 参数调优、LRU 缓存策略、分布式扩展 |

---

## 🤝 贡献指南

我们欢迎全球开发者参与白皮书的修订与优化，贡献流程严格遵循项目开源协同规范：

### 贡献流程

1. **提交 Issue**  
   先通过 [Issue](https://gitee.com/spharx/agentos/issues) 说明修订建议、补充内容或纠错点，经项目维护组评审确认

2. **分支开发**  
   基于主分支创建专属开发分支，仅修改对应 Markdown 源文件，**禁止直接修改 PDF 文件**

3. **提交 PR**  
   完成修订后提交 PR，PR 标题格式为：
   ```
   docs(white-paper): {修订内容简述}
   ```

4. **评审合并**  
   经项目技术委员会评审通过后合并至主分支，PDF 版本将由项目组同步更新发布

### 修订规范

- ✅ **一致性原则**：所有修订必须与项目当前代码能力、架构设计完全一致，禁止出现文档与代码脱节的内容
- ✅ **严谨性原则**：禁止添加营销化、夸大性表述，坚守技术文档的严谨性、客观性、准确性
- ✅ **对齐原则**：中英文版本修订需保持语义完全对齐，同步更新
- ✅ **审慎原则**：正式版本的重大内容变更，需经项目技术委员会评审通过后方可执行

---

## ⚖️ 合规与知识产权声明

### 开源许可

本目录下所有白皮书文档，均采用与项目根目录 [LICENSE](../../LICENSE) 一致的 **Apache License 2.0** 开源许可协议：

- ✅ **免费商用**：可用于闭源商业产品，无需支付授权费用
- ✅ **自由修改**：可修改、改编、二次创作，无需开源修改后的代码
- ✅ **自由分发**：可分发原版或修改版，无需额外授权
- ✅ **专利授权**：获得核心技术的永久专利使用权

### 义务要求

- 📝 **保留声明**：保留原项目的版权声明和许可证文本
- 📝 **修改记录**：若修改核心白皮书内容，需在文件中保留修改记录
- 📝 **不暗示背书**：不得使用 "AgentOS 官方" 或类似误导性表述

### 知识产权

- 本项目自研核心技术、架构设计与创新方案，均已在白皮书中明确披露
- 知识产权归属 **AgentOS 项目组及所有贡献者** 所有
- 禁止任何未经授权的篡改、二次分发与商用篡改行为
- 官方唯一发布渠道：[Gitee](https://gitee.com/spharx/agentos) | [GitHub](https://github.com/SpharxTeam/AgentOS)

---

## 📞 联系与反馈

### 技术支持
- **问题反馈**：通过 [仓库 Issue](https://gitee.com/spharx/agentos/issues) 提交文档相关问题与建议
- **技术咨询**：tech@spharx.cn

### 社区交流
- **官方网站**：https://spharx.cn
- **开发者社区**：筹备中，敬请期待

### 商务合作
- **合作洽谈**：business@spharx.cn

---

<div align="center">

### "Intelligence emergence, and nothing less, is the ultimate sublimation of AI."

### "智能涌现，舍我其谁。"

---

## 📝 版本历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|----------|
| Doc V1.7 | 2026-03-31 | LirenWang | 初始版本，建立白皮书发布体系 |

---

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
