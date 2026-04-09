# Tools - 通用工具集

此目录包含各种开发和运维辅助工具脚本。

## 📁 工具列表

### Python 工具
- **analyze_quality.py** - 代码质量分析（复杂度、重复率、风格检查）
- **enhance_coverage.py** - 测试覆盖率增强工具

### 编码工具
- **encoding/check_encoding.py** - 文件编码检查和转换工具
- **encoding/fix_bom.py** - BOM (Byte Order Mark) 修复工具

### Shell 工具
- **check-quality.sh** - 快速质量检查入口脚本

### 配置文件
- **requirements.txt** - Python 依赖包列表

## 🚀 使用方法

### 代码质量分析
```bash
cd scripts/tools
python analyze_quality.py --path ../../agentos/atoms/
```

### 质量检查
```bash
cd scripts/tools
./check-quality.sh
```

### 清理BOM标记
```bash
cd scripts/tools/encoding
python fix_bom.py --fix
```

### 编码检查
```bash
cd scripts/tools/encoding
python check_encoding.py --convert
```

## 🔧 依赖安装

```bash
pip install -r requirements.txt
```

主要依赖：
- `radon` - Python 复杂度分析
- `pylint` - 代码风格检查
- `jscpd` - 重复代码检测

## 📊 输出示例

**analyze_quality.py 输出：**
```
==========================================
AgentOS Code Quality Analysis Report
==========================================

📊 总体评分: A- (87.5/100)

📈 指标详情:
├── Cyclomatic Complexity: 平均 3.2 ✅
├── Code Duplication: 2.8% ✅  
├── Style Consistency: 97% ✅
└── Documentation Coverage: 85% ⚠️

🔍 文件统计:
├── 分析文件数: 451
├── 总代码行数: 128,456
├── 问题文件数: 12
└── 建议优化项: 23
```
