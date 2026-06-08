# Pull Request

## 变更类型

- [ ] Bug 修复
- [ ] 新功能
- [ ] 重构 / 代码清理
- [ ] 性能优化
- [ ] 文档更新
- [ ] 测试补充

## 相关 Issue

Closes #____ (如有)

## 变更摘要

<!-- 一两句话说明改了什么、为什么改 -->

## 检查清单

提交前请确认以下事项：

### 构建 & 测试

- [ ] 本地构建通过：
  ```bash
  cmake -B ../AgentOS-build -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
  cmake --build ../AgentOS-build --parallel 8
  ```
- [ ] 测试通过：`cd ../AgentOS-build && ctest --output-on-failure`
- [ ] 代码风格通过：`clang-format --dry-run --Werror` 对所有变更的 `.c`/`.h` 文件

### 代码质量

- [ ] Commit 遵循 [Conventional Commits](https://www.conventionalcommits.org/) 格式（`feat:` / `fix:` / `refactor:` / `docs:` / `test:` / `chore:`）
- [ ] 分支命名规范：`feature/<描述>` 或 `fix/<描述>`
- [ ] 公共 API 附带 Doxygen 注释
- [ ] 无 `TODO` / `FIXME` / `HACK` 残留标记
- [ ] 无硬编码路径或平台相关代码（跨平台兼容使用 `platform.h` 抽象层）

## 影响范围

<!-- 列出受影响的模块和关键文件 -->

| 模块 | 说明 |
|------|------|
|      |      |

---

> **提示**：CI 会自动运行质量检查和测试。如果 CI 报错，请根据日志修复后重新推送即可。
>
> 如有疑问，欢迎在 PR 中直接提问。
