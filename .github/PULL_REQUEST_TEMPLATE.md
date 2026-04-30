## Pull Request 模板

### 变更类型 (请勾选一项)
- [ ] Bug 修复
- [ ] 新功能
- [ ] 重构/代码清理
- [ ] 性能优化
- [ ] 文档更新
- [ ] 测试补充

### 相关 Issue / 任务编号
- 大轮次编号: R-09-__
- 详细任务: R-09-__-__
- 负责团队: [A/B/C]

### 变更摘要
<!-- 简要描述本次变更的内容 -->

### 影响范围
<!-- 列出受影响的模块和文件 -->
- 模块:
- 文件:

### 测试验证
<!-- 描述测试覆盖情况 -->
- [ ] 单元测试通过
- [ ] 集成测试通过
- [ ] 编译零错误零警告
- [ ] 无 BAN 违规（simplified/stub/mock/fake/dummy）
- [ ] CROSS 合规（无 pthread.h/clock_gettime 直接使用）
- [ ] 性能基准无明显退化

### 构建验证命令（符合 v11.14 BAN-33 规范）
```bash
# ✅ 正确：构建产物在 AgentOS-build/ 目录（源码目录保持纯净）
cd AgentOS
cmake -B ../AgentOS-build -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build ../AgentOS-build --parallel 8
cd ../AgentOS-build && ctest --output-on-failure

# ❌ 禁止：以下方式违反 BAN-33（在源码目录内构建）
# cmake -B build
# cmake --build .
```

### Commit Message 格式规范
```
<类型>(<范围>): <简短描述>

<body>

<footer>
```
**类型**: feat/fix/refactor/docs/test/perf/chore
**范围**: checkpoint/multiagent/memoryrovol/coreloopthree/daemon/commons

### 审查清单
- [ ] 代码符合 .clang-format 规范
- [ ] 所有公共 API 有 Doxygen 注释
- [ ] 错误路径正确处理（非桩函数返回）
- [ ] 内存分配失败有安全回退
- [ ] 跨平台兼容（platform.h 抽象层）

### 额外说明
<!-- 其他需要审查者注意的事项 -->
