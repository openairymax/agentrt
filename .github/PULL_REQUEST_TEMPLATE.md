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

### S-01~S-08 统一工作标准检查 (v11.14)

| # | 标准 | 检查项 | 状态 |
|---|------|--------|------|
| S-01 | 统一构建输出目录 | 构建产物在 `AgentOS-build/`，源码目录无 `.o`/`.a`/`.exe` | [ ] |
| S-02 | 统一CMake命令 | 使用 `cmake -B ../AgentOS-build` 构建 | [ ] |
| S-03 | 统一代码风格 | 所有 `.c`/`.h` 通过 `clang-format --dry-run --Werror` | [ ] |
| S-04 | 统一PR模板 | 使用本模板，填写完整 | [ ] |
| S-05 | 统一测试命令 | 使用 `cd ../AgentOS-build && ctest --output-on-failure` | [ ] |
| S-06 | 统一commit前缀 | `feat:`/`fix:`/`refactor:`/`docs:`/`test:`/`chore:` | [ ] |
| S-07 | 统一分支命名 | `feature/<name>-<issue>` / `fix/<name>-<issue>` | [ ] |
| S-08 | 统一BAN检查 | 运行 `scripts/pipeline/quality-gate.sh`，BAN-01~BAN-36 零违规 | [ ] |

### BAN 违规检查 (BAN-01~BAN-36)

- [ ] 无 BAN-01~BAN-13 违规（基础铁律）
- [ ] 无 BAN-17 违规（src/ 下无"简化实现"标记）
- [ ] 无 BAN-18 违规（src/ 下无返回固定值桩函数体）
- [ ] 无 BAN-19 违规（src/ 下无 mock/fake 降级）
- [ ] 无 BAN-20 违规（解析器无功能缺失）
- [ ] 无 BAN-25 违规（二进制无明文 License 字符串）
- [ ] 无 BAN-26 违规（Trial 版未正确限制）
- [ ] 无 BAN-28~BAN-29 违规（声明即实现，无桩函数）
- [ ] 无 BAN-30~BAN-31 违规（无资源泄漏，getter无副作用）
- [ ] 无 BAN-32 违规（无硬编码平台路径）
- [ ] 无 BAN-33 违规（构建产物不在源码目录内）
- [ ] 无 BAN-34 违规（代码已 clang-format 格式化）
- [ ] 无 BAN-35 违规（CI 无硬编码路径）
- [ ] 无 BAN-36 违规（无私有构建脚本）
- [ ] 无 TODO/FIXME/HACK 标记

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

### 跨平台验证
- [ ] Linux GCC 编译通过
- [ ] Windows MSVC 编译通过（如适用）
- [ ] 无 CROSS-01 违规（无 pthread.h/clock_gettime 直接使用）

### Commit Message 格式规范
```
<类型>(<范围>): <简短描述>

<body>

<footer>
```
**类型**: feat/fix/refactor/docs/test/chore
**范围**: checkpoint/multiagent/memoryrovol/coreloopthree/daemon/commons

### 审查清单
- [ ] 代码符合 .clang-format 规范
- [ ] 所有公共 API 有 Doxygen 注释
- [ ] 错误路径正确处理（非桩函数返回）
- [ ] 内存分配失败有安全回退
- [ ] 跨平台兼容（platform.h 抽象层）

### 额外说明
<!-- 其他需要审查者注意的事项 -->
