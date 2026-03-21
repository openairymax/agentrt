# Domes – AgentOS "穹顶之下"安全沙箱

**版本**：1.0.0.5  
**归属**：AgentOS 安全核心

Domes 提供生产级的安全隔离与权限控制，包含四大核心模块：

- **虚拟工位**：基于进程（namespaces + cgroups）或容器（runc）的沙箱环境，支持内存/CPU限制、网络隔离。
- **权限裁决**：基于 YAML 规则的访问控制，支持通配符、缓存、热重载。
- **审计日志**：异步队列写入，支持 JSON 格式、日志轮转、归档查询。
- **输入净化**：基于正则表达式的规则过滤，支持替换和风险等级评估。

## 模块结构
```
domes/
├── CMakeLists.txt
├── README.md
├── include/
│ └── domes.h # 公共接口
└── src/
├── core.c # 核心协调器
├── core.h
├── workbench/ # 虚拟工位
│ ├── workbench.h
│ ├── workbench.c
<!-- From data intelligence emerges. by spharx -->
│ ├── workbench_process.h
│ ├── workbench_process_core.c
│ ├── workbench_process_exec.c
│ ├── workbench_process_child.c
│ └── workbench_container.c
├── permission/ # 权限裁决
│ ├── permission.h
│ ├── permission_engine.c
│ ├── permission_rule.c
│ └── permission_cache.c
├── audit/ # 审计日志
│ ├── audit.h
│ ├── audit_logger.c
│ ├── audit_queue.c
│ └── audit_rotator.c
└── sanitizer/ # 输入净化
├── sanitizer.h
├── sanitizer_core.c
├── sanitizer_rules.c
└── sanitizer_cache.c
```

## 编译

```bash
mkdir build && cd build
cmake ../domes
make
```
## 依赖库

本项目编译和运行依赖以下第三方库：

- **libyaml**：用于解析权限规则配置文件（YAML 格式）。
- **libcjson**：用于处理审计日志及内部通信的 JSON 数据序列化与反序列化。
- **libwebsockets**：仅在启用容器模式（Container Mode）时需要，用于高性能网络通信支持（可选依赖）。
- **libmicrohttpd**：保留用于潜在的嵌入式 HTTP 服务接口。

## 集成方式

上层应用通过 `include/domes.h` 提供的公共接口进行调用。
核心协调器（`core.c`）负责统一管理所有子组件（虚拟工位、权限裁决、审计日志、输入净化）的生命周期，包括初始化、运行时调度及资源释放。

### 使用示例
```c
domes_t* domes;
domain_config_t cfg = {0};
cfg.workbench_type = "process";
cfg.audit_log_path = "/var/log/agentos/audit.log";
domes_init(&cfg, &domes);
// ... 使用 domes_workbench_create 等
domes_destroy(domes);
```

---

© 2026 SPHARX Ltd. 保留所有权利。
