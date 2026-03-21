# AgentOS 故障排查指南

**版本**: v1.0.0.5  
**最后更新**: 2026-03-21  
**适用对象**: 开发者、系统运维

---

## 📋 目录

1. [构建问题](#1-构建问题)
2. [运行时错误](#2-运行时错误)
3. [性能问题](#3-性能问题)
4. [诊断工具](#4-诊断工具)
5. [获取帮助](#5-获取帮助)

---

## 1. 构建问题

### 1.1 CMake 配置失败

**症状**:
```bash
CMake Error at CMakeLists.txt:42 (find_package):
<!-- From data intelligence emerges. by spharx -->
  Could not find FAISS
```

**解决方案**:
```bash
# 1. 检查依赖是否安装
sudo apt-get install -y libfaiss-dev libssl-dev

# 2. 清理构建缓存
rm -rf build/
mkdir build && cd build

# 3. 重新配置
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### 1.2 编译错误 - 头文件找不到

**症状**:
```bash
fatal error: agentos.h: No such file or directory
```

**解决方案**:
```bash
# 方法 1: 设置 CPATH
export CPATH=/path/to/agentos/include:$CPATH

# 方法 2: 使用绝对路径
gcc -I/path/to/agentos/include your_code.c

# 方法 3: 安装到系统目录
sudo make install
```

### 1.3 链接错误 - 符号未定义

**症状**:
```bash
undefined reference to `agentos_syscall_invoke'
```

**解决方案**:
```bash
# 1. 确保链接正确的库
gcc your_code.c -L/path/to/lib -lagentos

# 2. 检查库是否存在
ls -la /path/to/lib/libagentos.*

# 3. 设置 LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/lib:$LD_LIBRARY_PATH
```

---

## 2. 运行时错误

### 2.1 共享库加载失败

**症状**:
```bash
error while loading shared libraries: libagentos.so: cannot open shared object file
```

**解决方案**:
```bash
# 方法 1: 临时设置（当前终端）
export LD_LIBRARY_PATH=/path/to/agentos/lib:$LD_LIBRARY_PATH

# 方法 2: 永久设置（添加到 ~/.bashrc）
echo 'export LD_LIBRARY_PATH=/path/to/agentos/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc

# 方法 3: 系统级配置
sudo ldconfig /path/to/agentos/lib
```

### 2.2 权限拒绝错误

**症状**:
```bash
Permission denied: partdata/logs/kernel/agentos.log
```

**解决方案**:
```bash
# 1. 检查文件权限
ls -la partdata/logs/

# 2. 修改所有者
sudo chown -R $USER:$USER partdata/logs/

# 3. 修改权限
chmod -R 755 partdata/logs/
```

### 2.3 服务启动失败

**症状**:
```log
[ERROR] [llm_d] Failed to start: port 18789 already in use
```

**解决方案**:
```bash
# 1. 查找占用端口的进程
sudo lsof -i :18789

# 2. 杀死占用进程
sudo kill -9 <PID>

# 3. 或者修改配置使用其他端口
vim config/config.yaml
# services.llm_d.port: 18790
```

---

## 3. 性能问题

### 3.1 向量检索慢

**症状**: 检索延迟 > 100ms

**诊断步骤**:
```bash
# 1. 查看 FAISS 索引配置
agentos-cli memory stats

# 2. 检查缓存命中率
agentos-cli memory cache-stats

# 3. 监控 CPU 和内存使用
top -p $(pidof agentos)
```

**优化方案**:
```yaml
# config/config.yaml
memory:
  # 调整索引类型（更快但精度略低）
  faiss_index_type: IVF256,PQ32  # 从 IVF1024,PQ64 降级
  
  # 增加缓存
  cache_size: 200000  # 从 100000 增加
  
  # 启用 HNSW（更快速）
  enable_hnsw: true
  hnsw_m: 16
```

### 3.2 内存占用过高

**症状**: 进程 RSS > 4GB

**诊断步骤**:
```bash
# 1. 查看内存分布
agentos-cli memory profile

# 2. 检查是否有内存泄漏
valgrind --leak-check=full ./your_program

# 3. 监控 LRU 缓存使用
watch -n 1 'agentos-cli memory cache-stats'
```

**优化方案**:
```yaml
memory:
  # 限制缓存大小
  cache_size: 50000  # 减少缓存
  
  # 启用自动清理
  auto_gc_interval_sec: 300  # 5 分钟清理一次
  
  # 调整遗忘策略
  forgetting:
    enabled: true
    threshold_days: 30  # 30 天未访问的记忆自动清理
```

### 3.3 CPU 占用过高

**症状**: CPU 使用率持续 > 80%

**诊断步骤**:
```bash
# 1. 使用 perf 分析热点函数
perf top -p $(pidof agentos)

# 2. 生成火焰图
perf record -F 99 -p $(pidof agentos) -- sleep 30
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg

# 3. 查看线程状态
ps -eLo pid,tid,class,pri,ni,cumulative,wchan:20,comm | grep agentos
```

**优化方案**:
```yaml
kernel:
  # 限制工作线程数
  max_workers: 4  # 从 8 减少
  
  # 调整日志级别（减少日志开销）
  log_level: WARN  # 从 INFO 降级
```

---

## 4. 诊断工具

### 4.1 内置 CLI 工具

```bash
# 查看系统状态
agentos-cli status

# 查看内核统计
agentos-cli kernel stats

# 查看记忆系统指标
agentos-cli memory metrics

# 查看活跃任务
agentos-cli task list

# 查看会话信息
agentos-cli session info <session_id>
```

### 4.2 日志分析

```bash
# 1. 实时查看日志
tail -f partdata/logs/kernel/agentos.log

# 2. 搜索错误
grep ERROR partdata/logs/services/*.log

# 3. 追踪特定请求
grep "trace=abc123" partdata/logs/services/*.log

# 4. 使用 jq 解析 JSON 日志
cat partdata/logs/aggregate.jsonl | jq 'select(.level == "ERROR")'
```

### 4.3 性能剖析工具

**perf 使用示例**:
```bash
# 记录性能数据
perf record -F 99 -g -p $(pidof agentos) -- sleep 30

# 生成报告
perf report --stdio

# 查看调用图
perf script | c++filt | less
```

**eBPF 追踪**:
```bash
# 安装 bpfcc
sudo apt-get install -y bpfcc-tools

# 追踪 syscall 调用
sudo execsnoop -p $(pidof agentos)

# 追踪 IPC 通信
sudo bash -c 'echo "tracepoint:syscalls:sys_enter_write { printf(\"%s wrote %d bytes\\n\", comm, args->count); }" | tee /sys/kernel/debug/tracing/events/syscalls/sys_enter_write/format'
```

---

## 5. 获取帮助

### 5.1 自我诊断清单

在寻求帮助之前，请检查：

- [ ] 已阅读相关文档
- [ ] 已搜索 GitHub Issues
- [ ] 已收集错误日志
- [ ] 已尝试重启服务
- [ ] 已检查系统资源（CPU/内存/磁盘）

### 5.2 提交 Issue

**Issue 模板**:
```markdown
### 问题描述
简要描述遇到的问题

### 重现步骤
1. 执行命令：`...`
2. 看到错误：`...`

### 环境信息
- OS: Ubuntu 22.04
- 编译器：GCC 12.2
- AgentOS 版本：v1.0.0.5

### 日志输出
```log
粘贴相关错误日志
```

### 已尝试的解决方案
1. ...
2. ...
```

### 5.3 官方支持渠道

- **GitHub Issues**: https://github.com/spharx/agentos/issues
- **技术论坛**: https://forum.agentos.io
- **Discord**: https://discord.gg/agentos
- **邮件列表**: dev@agentos.io

---

## 📚 附录：常用命令速查

```bash
# 快速重启所有服务
systemctl restart agentos-*

# 查看服务状态
systemctl status agentos-llm_d

# 清理所有日志
find partdata/logs -name "*.log" -delete

# 重置数据库
rm partdata/data/*.db

# 导出性能报告
agentos-cli metrics export > metrics.json

# 健康检查
agentos-cli health-check
```

---

<div align="center">

**© 2026 SPHARX Ltd. All Rights Reserved.**

*From data intelligence emerges*

</div>
