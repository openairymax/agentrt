# App - 应用示例

**版本**: v1.0.0.9  
**最后更新**: 2026-03-26  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

### 1. docgen - 文档生成应用

自动生成 API 文档和 Markdown 文档站点。

**功能特性：**
- 从源代码提取代码注释
- 生成结构化 API 文档
- 创建 Markdown 文档站点
- 支持多种编程语言

**使用方法：**
```bash
cd docgen
./run.sh
```

**文件结构：**
- `src/` - 源代码目录
  - `main.py` - 主程序入口
  <!-- From data intelligence emerges. by spharx -->
  - `generator.py` - 文档生成器
- `manager.yaml` - 配置文件
- `manifest.json` - 应用清单
- `run.sh` - 运行脚本

---

### 2. ecommerce - 电子商务应用

电子商务平台示例，展示如何使用 AgentOS 实现复杂的业务流程。

**功能特性：**
- 商品管理
- 订单处理
- 支付集成
- 客户服务

**使用方法：**
```bash
cd ecommerce
./run.sh
```

**文件结构：**
- `src/` - 源代码目录
  - `main.py` - 主程序入口
  - `requirements.txt` - Python 依赖
  - `utils.py` - 工具函数
- `manager.yaml` - 配置文件
- `manifest.json` - 应用清单
- `run.sh` - 运行脚本

---

### 3. research - 研究应用

学术研究辅助工具，提供文献检索、数据分析等功能。

**功能特性：**
- 文献检索和管理
- 数据分析可视化
- 研究笔记组织
- 协作研究支持

**文件结构：**
- `README.md` - 详细说明文档

---

### 4. videoedit - 视频编辑应用

智能视频编辑工具，利用 AI 技术简化视频编辑流程。

**功能特性：**
- 智能场景检测
- 自动剪辑优化
- 特效添加
- 导出和分享

**使用方法：**
```bash
cd videoedit
./run.sh
```

**文件结构：**
- `src/` - 源代码目录
  - `main.py` - 主程序入口
  - `edit_pipeline.py` - 编辑流水线
  - `requirements.txt` - Python 依赖
- `manager.yaml` - 配置文件
- `manifest.json` - 应用清单
- `run.sh` - 运行脚本

---

## 🚀 开发指南

### 创建新应用

1. 在 `app/` 目录下创建新的应用目录
2. 添加必要的应用文件：
   - `src/` - 源代码
   - `manager.yaml` - 配置文件
   - `manifest.json` - 应用清单
   - `run.sh` - 运行脚本
   - `README.md` - 说明文档

### manifest.json 格式

```json
{
  "name": "应用名称",
  "version": "1.0.0",
  "description": "应用描述",
  "author": "作者信息",
  "license": "许可证",
  "dependencies": {
    "agentos": "^1.0.0"
  },
  "scripts": {
    "start": "python src/main.py"
  }
}
```

### manager.yaml 格式

```yaml
app:
  name: 应用名称
  version: 1.0.0
  
agentos:
  version: "^1.0.0"
  
services:
  # 使用的服务配置
  llm:
    provider: "default"
  memory:
    enabled: true
    
logging:
  level: "INFO"
  format: "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
```

## 📖 学习资源

- [AgentOS 核心概念](../paper/guides/)
- [系统调用参考](../atoms/syscall/README.md)
- [SDK 使用指南](../../toolkit/README.md)

## 🤝 贡献

欢迎提交新的示例应用！请确保：
1. 应用能够正常运行
2. 提供完整的文档
3. 遵循项目的代码规范
4. 包含必要的测试用例

## 📄 许可证

Apache License 2.0 - 详见 [LICENSE](../../LICENSE)

---

© 2026 SPHARX Ltd. 保留所有权利。


