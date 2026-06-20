# Data Analysis Skill

> **版本**: 1.0.0 | **类别**: analytics | **标签**: data-analysis, statistics, analytics, insights

## 概述

对结构化/半结构化数据执行统计分析，生成洞察报告。支持 JSON 数组、CSV 文本和键值对字典三种输入格式。

## 分析维度

| 维度 | 说明 |
|------|------|
| 描述性统计 (descriptive) | 均值、中位数、标准差、分位数(P25/P50/P75/P95/P99)、IQR、偏度 |
| 异常检测 (outliers) | IQR 方法（1.5×IQR）和 Z-score 方法（阈值3.0）双重检测 |
| 趋势识别 (trend) | 简单线性回归，计算斜率、R²、变化率百分比 |

## 输入参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|:---:|------|------|
| data | string | ✅ | — | 待分析数据 (JSON数组/CSV文本/JSON对象) |
| format | string | ❌ | auto | 数据格式 (json/csv/auto) |
| analysis_type | string | ❌ | all | 分析类型 (descriptive/distribution/outliers/trend/all) |
| target_fields | array | ❌ | [] | 重点分析的字段列表 (空=全部数值字段) |

## 输出格式

```json
{
  "summary": "Analyzed 20 records across 2 numeric field(s). Key insight: x shows a upward trend (+100.00% per period, R²=1.00)",
  "record_count": 20,
  "fields_analyzed": ["x", "y"],
  "descriptive_stats": {
    "x": {
      "count": 20,
      "mean": 9.5,
      "median": 9.5,
      "std_dev": 5.766,
      "min": 0,
      "max": 19,
      "p25": 4.75,
      "p75": 14.25,
      "p95": 18.05,
      "iqr": 9.5,
      "skewness": 0.0
    }
  },
  "outliers": {},
  "trends": {
    "x": {
      "direction": "upward",
      "slope": 1.0,
      "r_squared": 1.0,
      "change_rate_percent": 10.53
    }
  },
  "insights": [
    "x shows a upward trend (+10.5% per period, R²=1.00)",
    "y shows a upward trend (+5.3% per period, R²=1.00)"
  ]
}
```

## 数据格式说明

### JSON 数组（自动检测）
```json
[{"x": 1, "y": 10}, {"x": 2, "y": 20}]
```

### CSV 文本（自动检测）
```
x,y
1,10
2,20
```

## 洞察生成规则

1. **样本量评估**: <30 条标记低置信度
2. **异常值洞察**: 汇总各字段 IQR 离群值数量
3. **趋势洞察**: R² > 0.5 的显著趋势被报告
4. **分布洞察**: |偏度| > 1 的偏态分布被标记

## 使用示例

```python
from ecosystem.skills import DataAnalysisSkill
import asyncio, json

skill = DataAnalysisSkill()
data = json.dumps([{"x": i, "y": i*2+1} for i in range(20)])
result = asyncio.run(skill.execute({"data": data}))
print(result["record_count"])    # 20
print(result["fields_analyzed"]) # ["x", "y"]
print(result["insights"][0])     # 趋势洞察
```

## 实现文件

- [data_analysis.py](data_analysis.py) — SkillPlugin 实现
