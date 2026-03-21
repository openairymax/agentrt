# AgentOS C 语言编码规范

**版本：** 1.1.1  
**发布日期：** 2026-03-21  
**适用范围：** AgentOS 中的所有 C 语言模块
---

## 一、核心理念

- **工程控制论：** 通过错误码、日志、健康检查和指标构建反馈闭环，使系统能自我观测并对异常自动响应。
- **系统工程：** 模块化、接口驱动，边界清晰、实现可替换。
- **双系统思维：** 提供 System 1（快速、低延迟）与 System 2（安全、全面）两条路径，并允许运行时策略切换。
- **极简美学：** 接口精炼、命名优雅、注释说明"为什么"，而非"做什么"。

---

## 二、范围与术语

- **agentos_error_t：** 统一错误码类型（int32_t）；提供 `agentos_strerror()` 输出可读字符串。
- **公共头（public headers）：** 位于 `module/include/`，仅包含稳定对外 API。
- **私有实现：** 位于 `module/src/` 或 `module/src/internal/`。
- **Trace/Span：** 遵循 OpenTelemetry 与 W3C Trace Context（`traceparent`）规范。
- **SBOM：** 由 CI 生成的软件物料清单（SPDX 或 CycloneDX），随发布附件提供。

---

## 三、目录模板（强制）

每个模块必须且仅包含以下五项顶级内容（保证模块一致性与可打包性）：
- include/           —— 稳定公共头（导出 API）
- src/               —— 实现（可含 src/internal/）
- tests/             —— 单元与集成测试
- CMakeLists.txt     —— 构建、导出与安装规则
- README.md          —— 模块概述、API 兼容级别、配置与示例

**说明：** include/ 仅放对外接口；实现细节保留在 src/。README.md 须声明 API 版本（MAJOR.MINOR）、支持平台与快速上手示例。tests/ 至少包含一个单元测试与一个可在 CI 中重复运行的集成测试用例。

---

## 四、命名与可见性

- **命名风格：** 函数/变量使用 snake_case；宏/常量使用 UPPER_CASE；类型以 `_t` 结尾。
- **公共符号：** 需带模块前缀（如 `atoms_`、`domes_`、`cognition_`、`llm_`）。
- **私有符号：** 跨文件符号使用 `_module_` 前缀或声明为 `static` 并置于 internal header。
- **可见性控制：** 建议在构建中启用 `-fvisibility=hidden` 并显式导出公共符号。

---

## 五、API 与 ABI 管理

- **版本声明：** 公共头文件顶部必须声明 API 版本宏，例如 `#define MODULE_API_VERSION 1`。
- **ABI 兼容：** 在相同 MAJOR 版本内保证 ABI 兼容；破坏性更改需递增 MAJOR 并发布迁移说明。
- **不透明指针：** 优先使用不透明指针（opaque types）作为导出类型，避免直接暴露结构字段。
- **符号管理：** 建议采用符号版本管理（ELF symbol versioning）或显式导出列表来管理符号演进（推荐但可选）。

---

## 六、函数设计与文档

- **单一职责且短小：** 推荐 ≤50 行；若 >80 行必须重构并添加单元测试。
- **参数数量：** 不超过 5 个，若超过请封装成结构体。
- **文档要求：** 公共 API 必须使用 Doxygen 风格注释，且明确 ownership（谁释放）、线程语义与错误语义。
- **返回约定：** 优先使用 `agentos_error_t`（int32_t），或返回指针（失败返回 NULL 并记录日志）。提供常用错误定义与辅助宏。

**示例：**

```c
/**
 * @brief 提交计划并返回任务 ID。
 * @param engine [in] 引擎句柄（非 NULL）。
 * @param plan   [in] 待调度计划（只读）。
 * @param out_id [out] 输出任务 ID（调用者负责 free）。
 * @return AGENTOS_OK (0) 成功；其他值为错误码。
 */
int cognition_schedule(cognition_engine_t* engine,
                       const task_plan_t* plan,
                       char** out_id);
```

---

## 七、错误处理与诊断

- 使用统一错误类型 `agentos_error_t`，并提供 `agentos_strerror()`。
- 错误码命名带模块前缀（例如 `COG_ERR_OUT_OF_MEMORY`）。
- 每条错误路径至少记录 WARN/ERROR 级别日志，日志中须包含 `trace_id` 及关键信息（不可包含敏感数据）。
- 在测试构建中保留 fault-injection 钩子，用以验证错误路径与降级策略。

---

## 八、资源与生命周期管理

- **成对管理：** 所有资源按 create/destroy 成对管理，并在 API 文档中明确释放责任。
- **推荐使用 scope-guard：** 如 cleanup attribute / 宏，以减少手动释放错误。
- **realloc 安全用法：** 必使用临时指针以避免原指针丢失：

```c
void* tmp = realloc(ptr, new_size);
if (!tmp) {
    // 处理失败，ptr 仍然有效
}
ptr = tmp;
```

- **内存配额：** 长期运行服务应有内存配额与回收策略，并在健康检查中暴露相关指标。

---

## 九、并发与线程安全

- **优先使用 C11 atomics：** 使用 `stdatomic.h`，避免使用遗留的 `_sync*`。
- **热路径优化：** 优先无锁/原子实现，并提供锁保护的安全回退（System2）。
- **并发合约：** 为每个 API 明确并发合约：线程安全/不可重入/需外部同步等。
- **锁顺序：** 避免锁嵌套；若必须嵌套，定义并文档化全局锁顺序。
- **线程局部存储：** 使用 `pthread_key_t` 保存 per-request 上下文（如 `trace_id`/`request_id`）。

---

## 十、日志、追踪与指标（可观测性）

- **日志格式：** 使用结构化 JSON（兼容 OpenTelemetry logs），最小字段：`timestamp`、`level`、`module`、`function`、`trace_id`、`request_id`、`message`、`err_code`。
- **追踪规范：** 采用 W3C Trace Context（`traceparent`），并在请求入口生成 `trace_id`。
- **指标格式：** 采用 Prometheus exposition 格式；模块应提供 `/metrics`、`/healthz`（liveness）与 `/readyz`（readiness）端点。
- **日志采样：** 高吞吐场景需实现日志采样或汇总策略以防日志风暴，但错误日志应保持完整。

---

## 十一、配置管理与热重载

- **配置优先级：** `defaults ← config file ← environment variables ← runtime overrides`。
- **配置验证：** 使用 YAML/JSON Schema 验证配置，启动时拒绝非法配置。
- **热重载：** 应以原子方式替换配置句柄（或使用读写锁），确保短期向后兼容。敏感配置应通过 secret manager 注入，不写入普通文件。

---

## 十二、安全实践与运行时加固

### 12.1 编译/链接硬化（release 构建）

建议的编译/链接硬化选项：

- `-fstack-protector-strong`
- `-D_FORTIFY_SOURCE=2`
- `-fPIE -pie`
- `-Wl,-z,relro -Wl,-z,now`
- `-fvisibility=hidden`
- 在适当时启用 LTO

### 12.2 Sanitizers

在 CI 矩阵中运行 Sanitizers：ASAN、UBSAN、TSAN、MSAN（按目标配置）。

### 12.3 模糊测试

对解析器与外部输入持续做模糊测试（libFuzzer/AFL++），并保留回归 corpus。

### 12.4 供应链安全

- CI 生成 SBOM（SPDX/CycloneDX）
- 定期依赖漏洞扫描
- 发布时用 Sigstore/Cosign 签名制品

### 12.5 运行时安全

- **最小权限原则：** 降权运行、限制文件/网络权限、并在支持的平台上使用 seccomp/AppArmor。
- **敏感数据保护：** 严禁在日志中输出密钥或敏感数据，使用脱敏/掩码工具。

---

## 十三、构建、静态/动态分析与 CI

### 13.1 最低 CI 流程

1. 格式检查（clang-format）
2. 构建（CMake）
3. 静态分析（clang-tidy / cppcheck）
4. 单元测试（coverage）
5. Sanitizer jobs（ASAN/UBSAN/TSAN/MSAN）
6. Fuzz job（持续/定期运行）
7. 依赖与许可检查、SBOM 生成、制品签名与发布

### 13.2 pre-commit 钩子

建议的 pre-commit 钩子：

- clang-format
- license header 检查
- 脚本 shellcheck
- commit-msg（Conventional Commits）

### 13.3 CI 阻断策略

CI 在发现高危安全问题或格式/静态分析回归时应阻止合并。

---

## 十四、测试策略

- **单元测试：** 使用 CMocka、Criterion 或等效框架，覆盖成功、失败与边界条件。
- **集成测试：** 容器化或隔离的测试环境，模拟外部依赖（如 LLM mock、数据库 mock 等）。
- **模糊测试：** 对解析器、反序列化器和网络协议持续 fuzz，并保存回归样本。
- **性能基准：** 对关键路径提供 microbenchmarks，并对比回归。
- **覆盖率目标：** 关键模块 ≥ 85%（可根据模块重要性分级）；CI 可根据策略阻断或发出警告。

---

## 十五、发布与供应链合规

- **语义化版本：** 使用 SemVer。每次发布附带 changelog、SBOM 与签名文件。
- **漏洞阈值：** 若依赖漏洞超过阈值（高危），应阻断发布。
- **可重现构建：** 每次发布应保留可重现构建元数据（构建日志、编译标志、SBOM、签名）。

---

## 十六、开发流程与代码审查

- **提交规范：** 提交与 PR 遵循 Conventional Commits。PR 必包含相应测试与文档更新。
- **CODEOWNERS：** 在仓库中设置 CODEOWNERS，关键模块由指定人员审查；关键模块 PR 至少需要两位 reviewer。
- **Review 清单（必检项）：** API/ABI 影响、测试覆盖、格式/静态分析/sanitizer 结果、SBOM/第三方许可影响与安全考虑。

---

## 附录 A — 推荐编译选项（CMake 示例）

```cmake
# Release 构建建议 flags
target_compile_options(${target} PRIVATE
  -Wall -Wextra -Werror
  -Wformat -Wformat-security
  -fstack-protector-strong
  -D_FORTIFY_SOURCE=2
  -fPIE
  -fno-omit-frame-pointer
)

target_link_options(${target} PRIVATE
  -Wl,-z,relro -Wl,-z,now -pie
)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON) # 视情况启用 LTO
```

---

## 附录 B — 示例代码片段（精简且惯用）

### B.1 资源分配与清理

```c
service_t* service_create(const char* path) {
    if (!path) return NULL;
    
    service_t* svc = calloc(1, sizeof(*svc));
    if (!svc) return NULL;
    
    svc->file = fopen(path, "rb");
    if (!svc->file) goto fail;
    
    // ... 初始化 ...
    
    return svc;
    
fail:
    service_destroy(svc);
    return NULL;
}
```

### B.2 无锁环形缓冲骨架（简化）

```c
typedef struct ring_buffer {
    void** buffer;
    size_t mask; // capacity = mask + 1，且为 2 的幂
    atomic_size_t head;
    atomic_size_t tail;
} ring_buffer_t;
```

### B.3 健康检查示例

```c
char* module_health_check(void) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "healthy");
    cJSON_AddNumberToObject(root, "requests_total",
                           (double)atomic_load(&requests_total));
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json; // 调用者需 free
}
```

---

## 附录 C — 常用错误码（示例，请集中管理并扩展）

```c
#define AGENTOS_OK           0
#define AGENTOS_EINVAL      -22
#define AGENTOS_ENOMEM      -12
#define AGENTOS_EBUSY       -16
#define AGENTOS_ETIMEDOUT  -110
```

---

## 版权

© 2026 SPHARX Ltd. All Rights Reserved.