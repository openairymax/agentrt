Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 白皮书历史版本归档

本目录用于归档 AgentOS 技术白皮书的所有历史正式版本，确保项目演进全程可追溯。

## 归档规则

### 版本命名
- 格式：`V{主版本号}.{次版本号}.{修订号}.{补丁号}`
- 示例：`V1.0.0.6`

### 归档结构
```
history/
├── V1.0/              # 按主版本号分目录
│   ├── zh/           # 中文历史版本
│   │   ├── AgentOS_技术白皮书_V1.0.0.1.md
│   │   ├── AgentOS_技术白皮书_V1.0.0.2.md
│   │   └── ...
│   └── en/           # 英文历史版本
│       ├── AgentOS_Technical_White_Paper_V1.0.0.1.md
│       └── ...
└── README.md         # 本说明文档
```

### 版本升级规则

#### 主版本号（Major）
**升级条件**：架构级重大变更
- 微内核接口不兼容变更
- 认知循环模型重构
- 记忆系统层级调整
- 安全模型根本性变革

**影响**：需要应用层大规模改造

#### 次版本号（Minor）
**升级条件**：功能级新增或改进
- 新增系统调用接口
- 守护进程功能增强
- SDK 语言支持扩展
- 性能优化（>50% 提升）

**影响**：向后兼容，建议升级

#### 修订号（Revision）
**升级条件**：模块级功能迭代
- 局部算法优化
- Bug 修复
- 文档完善
- 测试覆盖提升

**影响**：完全兼容，推荐升级

#### 补丁号（Patch）
**升级条件**：紧急修复
- 安全漏洞修复
- 严重 Bug 热修复
- 文档勘误
- 构建脚本修复

**影响**：立即升级

## 当前归档

| 版本 | 归档日期 | 对应代码版本 | 主要变更 |
|------|---------|-------------|---------|
| - | - | - | 暂无历史版本（首版 V1.0.0.6 发布后归档） |

## 归档流程

### 1. 新版本发布前
- [ ] 完成当前版本的 Markdown 源文件冻结
- [ ] 生成 PDF 正式版本
- [ ] 更新 `white_paper/README.md` 版本映射表
- [ ] 创建 Git Tag：`white-paper-v{version}`

### 2. 历史版本移动
```bash
# 示例：将 V1.0.0.5 归档到 history 目录
mkdir -p history/V1.0/zh
mkdir -p history/V1.0/en

# 移动旧版本文件
git mv white_paper/zh/AgentOS_技术白皮书_V1.0.0.5.md history/V1.0/zh/
git mv white_paper/en/AgentOS_Technical_White_Paper_V1.0.0.5.md history/V1.0/en/

# 提交变更
git commit -m "docs(white-paper): archive V1.0.0.5 to history"
```

### 3. 更新说明
- 在 `white_paper/README.md` 添加版本记录
- 在本文件更新归档表格
- 在 Git Release Notes 中说明版本变更

## 版本兼容性承诺

### 内核层（Atoms）
- **主版本内**：保证二进制兼容性（ABI）
- **跨主版本**：可能需要重新编译

### 服务层（daemon）
- **次版本内**：配置完全兼容
- **跨主版本**：检查配置迁移指南

### SDK 层
- **主版本内**：API 向后兼容
- **跨主版本**：提供迁移工具和指南

## 检索指南

### 查找特定版本
```bash
# 查看某个版本的变更
cd history/V1.0/zh
git show white-paper-v1.0.0.5:AgentOS_技术白皮书_V1.0.0.5.md
```

### 对比版本差异
```bash
# 对比两个版本
git diff white-paper-v1.0.0.5..white-paper-v1.0.0.6 -- white_paper/zh/
```

### 下载历史 PDF
所有历史版本的 PDF 文件将与 Markdown 源文件同目录存放，文件名格式：
```
AgentOS_技术白皮书_V{version}.pdf
```

## 保存期限

**永久保存**：所有正式发布过的版本均永久保留，不得删除。

**理由**：
1. 学术研究需要历史数据
2. 用户可能需要回溯特定版本
3. 技术发展史记录
4. 开源项目透明度要求

## 维护责任

- **责任人**：AgentOS 架构委员会
- **审核频率**：每次新版本发布时
- **联系方式**：architecture@agentos.org（计划中）

---

**最后更新**: 2026-04-09  
**维护者**: AgentOS 架构委员会
