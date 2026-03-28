# AgentOS 运维脚本

**版本**: v1.0.0.6  
**最后更新**: 2026-03-21  

---

## 📋 概述

`scripts/ops/` 目录包含 AgentOS 系统的运维监控工具，提供性能基准测试、系统健康检查和契约验证等功能。

---

## 📁 文件清单

| 文件 | 说明 | 类型 | 状态 |
|------|------|------|------|
| `benchmark.py` | 性能基准测试工具 | Python | ✅ 生产就绪 |
| `doctor.py` | 系统健康检查工具 | Python | ✅ 生产就绪 |
| `validate_contracts.py` | 契约验证工具 | Python | 🟡 测试阶段 |

---

## 🔧 工具详解

### 1. benchmark.py - 性能基准测试

**用途**: 评估 AgentOS 系统的各项性能指标

**功能特性**:
- 记忆写入/读取延迟测试
- 向量检索性能测试
- IPC 通信吞吐量测试
- 任务调度延迟测试
- Token 效率分析

**使用方法**:

```bash
cd scripts/ops

# 运行完整基准测试
python benchmark.py --full

# 只测试记忆系统
python benchmark.py --module memory

# 自定义测试参数
python benchmark.py --iterations 1000 --concurrency 10
```

**输出示例**:

```
=== AgentOS 性能基准测试 ===

[记忆系统]
  L1 写入吞吐：10,234 条/秒 ✓
  L2 检索延迟：< 8ms (k=10) ✓
  混合检索延迟：< 45ms (top-100) ✓

[IPC 通信]
  Binder 调用延迟：< 0.5ms ✓
  最大并发连接：1024 ✓

[任务调度]
  加权轮询延迟：< 1ms ✓
  Agent 调度延迟：< 5ms ✓

综合评分：95/100 ⭐⭐⭐⭐⭐
```

**依赖安装**:

```bash
pip install -r requirements.txt
# 或
pip install numpy pandas faiss-cpu
```

---

### 2. doctor.py - 系统健康检查

**用途**: 诊断 AgentOS 系统的常见问题

**功能特性**:
- 环境配置检查
- 依赖项完整性验证
- 服务连通性测试
- 资源使用率监控
- 日志错误分析

**使用方法**:

```bash
cd scripts/ops

# 运行全面健康检查
python doctor.py

# 只检查特定模块
python doctor.py --module kernel
python doctor.py --module services

# 生成诊断报告
python doctor.py --report > diagnosis_$(date +%Y%m%d).txt
```

**检查项目**:

```
✓ 环境变量配置
✓ CMake 版本兼容性
✓ Python 依赖完整性
✓ Docker 服务状态
✓ 数据库连接性
✓ 磁盘空间充足性
✓ 内存使用率
✓ CPU 负载情况
✓ 网络端口可用性
✓ 日志文件错误模式
```

**输出示例**:

```
=== AgentOS 系统健康检查 ===

[环境检查] ✓ 通过
  - CMAKE_VERSION: 3.24.1 ✓
  - PYTHON_VERSION: 3.9.7 ✓
  - GCC_VERSION: 11.2.0 ✓

[服务状态] ⚠ 警告
  - llm_d: 运行中 ✓
  - market_d: 未运行 ⚠
  - monit_d: 运行中 ✓

[资源使用] ✓ 正常
  - CPU: 35% ✓
  - 内存：4.2GB / 16GB ✓
  - 磁盘：120GB / 500GB ✓

诊断建议:
1. 启动 market_d 服务：docker-compose up -d market_d
2. 监控系统日志：tail -f lodges/logs/*.log

总体状态：良好 (2 个警告)
```

---

### 3. validate_contracts.py - 契约验证工具

**用途**: 验证 AgentOS 各模块间接口契约的完整性

**功能特性**:
- Doxygen 注释完整性检查
- 接口签名一致性验证
- 错误码覆盖性分析
- 线程安全标注审查

**使用方法**:

```bash
cd scripts/ops

# 验证所有契约
python validate_contracts.py

# 只验证特定模块
python validate_contracts.py --module corekern
python validate_contracts.py --module syscall

# 生成详细报告
python validate_contracts.py --verbose > contracts_report.txt
```

**检查规则**:

```
✓ 所有公开函数必须有 @brief 注释
✓ 参数必须有 @param 说明
✓ 返回值必须有 @return 说明
✓ 线程安全必须标注 @threadsafe
✓ 错误处理必须有 @see 引用
✓ 所有权语义必须有 @ownership 标注
```

**输出示例**:

```
=== AgentOS 契约验证 ===

检查文件：156
发现契约：1,234

[完整性检查] ✓ 通过
  - 函数注释：100% ✓
  - 参数说明：100% ✓
  - 返回值说明：100% ✓

[一致性检查] ✓ 通过
  - 接口签名：一致 ✓
  - 错误码定义：一致 ✓
  - 线程安全标注：一致 ✓

违规项：0
警告项：3

验证结果：通过 ✓
```

---

## 🔍 故障排查

### benchmark.py 常见问题

#### 1. FAISS 导入错误

**错误**: `ImportError: No module named 'faiss'`

**解决**:
```bash
# CPU 版本
pip install faiss-cpu

# GPU 版本（需要 CUDA）
pip install faiss-gpu
```

#### 2. 内存不足

**错误**: `MemoryError` 或进程被 kill

**解决**:
```bash
# 减少测试规模
python benchmark.py --small

# 增加 swap 空间
sudo swapon -a
```

### doctor.py 常见问题

#### 1. 权限不足

**错误**: `Permission denied`

**解决**:
```bash
# 使用 sudo 运行（不推荐）
sudo python doctor.py

# 或赋予目录权限
chmod -R u+rwx lodges/logs
```

#### 2. 日志文件不存在

**警告**: `Log file not found: lodges/logs/agentos.log`

**解决**:
```bash
# 创建日志目录
mkdir -p lodges/logs
touch lodges/logs/agentos.log
```

---

## 📊 性能优化建议

基于基准测试结果，提供以下优化建议：

### 记忆系统优化

```python
# 调整 FAISS 参数
# 对于 < 100 万条数据
index_type = "IVF32,SQ8"

# 对于 > 100 万条数据
index_type = "IVF1024,PQ64"

# 批量写入优化
batch_size = 1024  # 推荐值
```

### IPC 通信优化

```c
// 增加 Binder 连接池大小
#define BINDER_POOL_SIZE 256

// 使用异步 IPC
agentos_ipc_set_mode(ASYNC);
```

### 任务调度优化

```yaml
# 调整调度器参数
scheduler:
  strategy: weighted_round_robin
  time_slice_ms: 10
  max_queue_size: 1000
```

---

## 🛡️ 安全建议

### 基准测试安全

1. **隔离测试环境**:
   - 在生产环境外运行基准测试
   - 使用独立的数据集

2. **资源限制**:
```bash
# 限制 CPU 使用
taskset -c 0-3 python benchmark.py

# 限制内存使用
ulimit -v 4194304  # 4GB
```

### 健康检查安全

1. **最小权限原则**:
   - 不要以 root 身份运行 doctor.py
   - 使用专用监控账户

2. **日志保护**:
```bash
# 设置日志权限
chmod 640 lodges/logs/*.log
chown agentos:agentos lodges/logs/*.log
```

---

## 📝 最佳实践

### 定期健康检查

```bash
# 添加到 crontab（每小时执行一次）
0 * * * * cd /path/to/scripts/ops && python doctor.py >> /var/log/agentos_health.log 2>&1
```

### 性能回归测试

```bash
# CI/CD 集成示例（GitHub Actions）
name: Performance Regression Test

on: [push, pull_request]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Run Benchmark
      run: |
        cd scripts/ops
        python benchmark.py --output benchmark_result.json
    
    - name: Compare Results
      uses: benchmark-compare-action@v1
      with:
        current: benchmark_result.json
        baseline: benchmarks/baseline_v1.0.0.6.json
```

### 自动化告警

```python
# 简单的告警脚本示例
#!/usr/bin/env python3

import subprocess
import smtplib
from email.mime.text import MIMEText

def check_and_alert():
    result = subprocess.run(['python', 'doctor.py'], 
                          capture_output=True, text=True)
    
    if '严重' in result.stdout or '失败' in result.stdout:
        msg = MIMEText(result.stdout)
        msg['Subject'] = 'AgentOS 健康检查告警'
        msg['From'] = 'monitor@agentos.org'
        msg['To'] = 'admin@agentos.org'
        
        with smtplib.SMTP('smtp.agentos.org') as server:
            server.send_message(msg)

if __name__ == '__main__':
    check_and_alert()
```

---

## 📞 相关文档

- [主 README](../README.md) - 脚本总览
- [构建指南](../build/README.md) - 编译和安装
- [Docker 部署](../deploy/docker/README.md) - 容器化运维

---

## 🤝 贡献

欢迎提交新的运维工具和改进建议！

**Issue 追踪**: https://github.com/SpharxTeam/AgentOS/issues  
**讨论区**: https://github.com/SpharxTeam/AgentOS/discussions

---

© 2026 SPHARX Ltd. All Rights Reserved.

*From data intelligence emerges.*  
*始于数据，终于智能。*

