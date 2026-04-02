# Manager 模块与统一配置库集成标准

**版本**: v1.0.0  
**最后更新**: 2026-04-01  
**适用范围**: AgentOS/manager 模块与 commons/utils/config_unified 的集成  
**理论基础**: 工程两论（接口契约化）、五维正交设计（内核观K-2、工程观E-4/E-8）  

---

## 一、集成概述

### 1.1 集成目标

建立 manager 模块（配置管理中心）与 `commons/utils/config_unified` 统一配置库之间的标准化集成机制，实现：

1. **统一的配置加载**: 所有模块使用相同的 API 加载 manager 配置
2. **标准化的配置路径**: 通过环境变量定义配置根目录
3. **Schema 验证自动化**: 加载时自动验证配置文件格式
4. **热更新支持**: 支持运行时配置变更和通知
5. **向后兼容**: 保持现有代码的兼容性

### 1.2 架构关系

```
┌─────────────────────────────────────────────────────────────┐
│                    AgentOS 各业务模块                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐      │
│  │ cupolas  │ │ daemon   │ │ atoms    │ │ 其他模块  │      │
│  └─────┬────┘ └─────┬────┘ └─────┬────┘ └─────┬────┘      │
│        │            │            │            │             │
│        └────────────┴────────────┴────────────┘             │
│                        ↓ 统一调用                            │
│              ┌──────────────────────────┐                   │
│              │  config_unified API     │                   │
│              │ (commons/utils/)        │                   │
│              └────────────┬─────────────┘                   │
│                           ↓                                 │
│              ┌──────────────────────────┐                   │
│              │   Manager 配置存储       │                   │
│              │ ($AGENTOS_CONFIG_DIR)    │                   │
│              └──────────────────────────┘                   │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、标准配置路径定义

### 2.1 环境变量规范

#### AGENTOS_CONFIG_DIR（必需）

```bash
# 定义 Manager 配置的根目录路径
export AGENTOS_CONFIG_DIR="/etc/agentos"          # Linux 生产环境
export AGENTOS_CONFIG_DIR="./AgentOS/manager"     # 开发环境
export AGENTOS_CONFIG_DIR="C:\\agentos\\config"   # Windows 环境
```

**用途**: 
- 所有模块查找配置文件的基准路径
- 替代硬编码的配置路径
- 支持多环境部署（开发/预发/生产）

**默认值**: 如果未设置，使用以下回退策略：
1. Linux: `/etc/agentos`
2. Windows: `%APPDATA%\agentos`
3. macOS: `~/Library/Application Support/agentos`
4. 开发环境: `./AgentOS/manager`

### 2.2 配置子目录结构

在 `$AGENTOS_CONFIG_DIR` 下，遵循 manager 模块的目录结构：

```
$AGENTOS_CONFIG_DIR/
├── kernel/
│   └── settings.yaml           # 内核配置
├── model/
│   └── model.yaml              # LLM模型配置
├── agent/
│   └── registry.yaml           # Agent注册表
├── skill/
│   └── registry.yaml           # 技能注册表
├── security/
│   ├── policy.yaml             # 安全策略
│   └── permission_rules.yaml   # 权限规则
├── sanitizer/
│   └── sanitizer_rules.json    # 输入净化规则
├── logging/
│   └── manager.yaml            # 日志配置
├── service/
│   └── tool_d/
│       └── tool.yaml           # 工具服务配置
├── monitoring/
│   ├── alerts/
│   │   └── cupolas-alerts.yml  # 告警配置
│   └── dashboards/
│       └── cupolas-dashboard.json  # 监控面板
├── schema/                     # JSON Schema 验证文件
│   ├── kernel-settings.schema.json
│   ├── model.schema.json
│   └── ...                    # 其他Schema文件
├── environment/                # 环境覆盖配置
│   ├── development.yaml
│   ├── staging.yaml
│   └── production.yaml
└── .env                        # 环境变量模板
```

---

## 三、统一加载API使用规范

### 3.1 基础加载模式

```c
/**
 * @file config_loader_example.c
 * @brief 使用 config_unified 加载 Manager 配置的标准示例
 */

#include "config_unified.h"
#include <stdio.h>
#include <stdlib.h>

/* 获取标准配置路径 */
static const char* get_config_dir(void) {
    const char* config_dir = getenv("AGENTOS_CONFIG_DIR");
    if (!config_dir) {
#ifdef _WIN32
        config_dir = ".\\AgentOS\\manager";
#else
        config_dir = "./AgentOS/manager";
#endif
    }
    return config_dir;
}

/* 加载内核配置 */
int load_kernel_config(config_context_t* ctx) {
    const char* config_dir = get_config_dir();
    char file_path[512];
    
    snprintf(file_path, sizeof(file_path), "%s/kernel/settings.yaml", config_dir);
    
    config_file_source_options_t options = {
        .file_path = file_path,
        .format = "yaml",
        .encoding = "utf-8",
        .auto_reload = true,
        .reload_interval_ms = 5000  // 5秒检查一次变更
    };
    
    config_source_t* source = config_source_create_file(&options);
    if (!source) {
        fprintf(stderr, "无法创建配置源: %s\n", file_path);
        return -1;
    }
    
    int result = config_source_load(source, ctx);
    if (result != CONFIG_SUCCESS) {
        fprintf(stderr, "加载配置失败: %s\n", file_path);
        config_source_destroy(source);
        return -1;
    }
    
    config_source_destroy(source);
    return 0;
}

/* 主函数示例 */
int main(void) {
    /* 创建配置上下文 */
    config_context_t* ctx = config_context_create("agentos");
    if (!ctx) {
        fprintf(stderr, "创建配置上下文失败\n");
        return EXIT_FAILURE;
    }
    
    /* 加载各子系统配置 */
    if (load_kernel_config(ctx) != 0) {
        config_context_destroy(ctx);
        return EXIT_FAILURE;
    }
    
    /* 使用配置值 */
    const char* log_level = config_value_as_string(
        config_context_get(ctx, "logging.level"), "info");
    
    printf("日志级别: %s\n", log_level);
    
    /* 清理资源 */
    config_context_destroy(ctx);
    return EXIT_SUCCESS;
}
```

### 3.2 批量加载模式

```c
/**
 * @brief 批量加载所有 Manager 子系统配置
 */
#define MAX_SOURCES 16

typedef struct {
    const char* relative_path;  // 相对于 $AGENTOS_CONFIG_DIR 的路径
    const char* format;         // 文件格式 ("yaml", "json")
    bool required;              // 是否必须存在
} config_source_info_t;

static const config_source_info_t MANAGER_SOURCES[] = {
    {"kernel/settings.yaml", "yaml", true},
    {"model/model.yaml", "yaml", true},
    {"agent/registry.yaml", "yaml", true},
    {"skill/registry.yaml", "yaml", false},
    {"security/policy.yaml", "yaml", true},
    {"logging/manager.yaml", "yaml", true},
    {NULL, NULL, false}  // 结束标记
};

int load_all_manager_configs(config_context_t* ctx) {
    const char* config_dir = get_config_dir();
    int success_count = 0;
    int fail_count = 0;
    
    for (int i = 0; MANAGER_SOURCES[i].relative_path != NULL; i++) {
        const config_source_info_t* info = &MANAGER_SOURCES[i];
        char file_path[512];
        
        snprintf(file_path, sizeof(file_path), "%s/%s", config_dir, info->relative_path);
        
        config_file_source_options_t options = {
            .file_path = file_path,
            .format = info->format,
            .encoding = "utf-8",
            .auto_reload = true,
            .reload_interval_ms = 5000
        };
        
        config_source_t* source = config_source_create_file(&options);
        if (!source) {
            if (info->required) {
                fprintf(stderr, "[ERROR] 无法创建配置源: %s\n", file_path);
                fail_count++;
            } else {
                printf("[WARN] 可选配置源不存在: %s\n", file_path);
            }
            continue;
        }
        
        int result = config_source_load(source, ctx);
        if (result == CONFIG_SUCCESS) {
            success_count++;
            printf("[OK] 加载配置成功: %s\n", info->relative_path);
        } else {
            if (info->required) {
                fprintf(stderr, "[ERROR] 加载配置失败: %s\n", file_path);
                fail_count++;
            } else {
                printf("[WARN] 可选配置加载失败: %s\n", file_path);
            }
        }
        
        config_source_destroy(source);
    }
    
    printf("\n配置加载统计: 成功=%d, 失败=%d\n", success_count, fail_count);
    return (fail_count > 0) ? -1 : 0;
}
```

### 3.3 Schema 验证集成

```c
/**
 * @brief 加载配置并进行 Schema 验证
 */
int load_and_validate_config(const char* config_path, const char* schema_path) {
    config_context_t* ctx = config_context_create("validation_test");
    if (!ctx) return -1;
    
    /* 加载配置文件 */
    config_file_source_options_t file_opts = {
        .file_path = config_path,
        .format = "yaml",
        .encoding = "utf-8"
    };
    
    config_source_t* source = config_source_create_file(&file_opts);
    if (!source || config_source_load(source, ctx) != CONFIG_SUCCESS) {
        config_context_destroy(ctx);
        return -1;
    }
    config_source_destroy(source);
    
    /* 创建验证器 */
    validator_options_t validator_opts = {
        .type = VALIDATOR_TYPE_JSON_SCHEMA,
        .schema_path = schema_path
    };
    
    config_validator_t* validator = config_validator_create(&validator_opts);
    if (!validator) {
        config_context_destroy(ctx);
        return -1;
    }
    
    /* 执行验证 */
    validation_report_t report;
    int result = config_validator_validate(validator, ctx, &report);
    
    if (result == CONFIG_SUCCESS) {
        printf("[PASS] 配置验证通过: %s\n", config_path);
    } else {
        printf("[FAIL] 配置验证失败: %s\n", config_path);
        printf("错误信息: %s\n", report.error_message);
        printf("错误位置: line %d, column %d\n", report.error_line, report.error_column);
    }
    
    /* 清理资源 */
    config_validator_destroy(validator);
    config_context_destroy(ctx);
    
    return result;
}
```

---

## 四、环境变量引用标准化

### 4.1 引用格式规范

Manager 配置文件中的环境变量引用应统一使用 `${VARIABLE}` 格式：

```yaml
# ✅ 正确格式：使用 ${VARIABLE}
database:
  host: ${DATABASE_HOST:-localhost}
  port: ${DATABASE_PORT:-5432}
  password_env: ${DATABASE_PASSWORD}

# ❌ 错误格式：避免使用 $VARIABLE 或其他格式
# database:
#   host: $DATABASE_HOST  # 不推荐
```

### 4.2 默认值语法

支持 Bash 风格的默认值语法：

| 语法 | 含义 | 示例 |
|------|------|------|
| `${VAR}` | 必需变量，缺失时报错 | `${API_KEY}` |
| `${VAR:-default}` | 缺失时使用默认值 | `${HOST:-localhost}` |
| `${VAR:+alternative}` | 存在时使用替代值 | `${DEBUG:+verbose}` |

### 4.3 环境变量展开选项

```c
/* 在配置源创建时启用环境变量展开 */
config_env_source_options_t env_opts = {
    .prefix = "AGENTOS_",
    .case_sensitive = false,
    .separator = "_",
    .expand_vars = true  // 启用 ${VAR} 展开
};
```

---

## 五、热更新支持

### 5.1 热更新配置

```c
/**
 * @brief 启用配置热更新监控
 */
void setup_hot_reload(config_context_t* ctx) {
    hot_reload_options_t opts = {
        .callback = on_config_changed,
        .user_data = NULL,
        .debounce_ms = 1000,  // 防抖延迟1秒
        .validate_on_reload = true,  // 重载时自动验证
        .schema_path = "$AGENTOS_CONFIG_DIR/schema/"
    };
    
    config_hot_reload_manager_t* hot_reload = 
        config_hot_reload_manager_create(ctx, &opts);
    
    if (hot_reload) {
        /* 每5秒检查一次配置变更 */
        config_hot_reload_start(hot_reload, 5000);
        printf("配置热更新已启动\n");
    }
}

/* 配置变更回调函数 */
void on_config_changed(config_context_t* ctx, const char* changed_key, void* user_data) {
    printf("[HOT RELOAD] 配置已更新: %s\n", changed_key);
    
    /* 根据变更的键执行相应操作 */
    if (strstr(changed_key, "logging.") != NULL) {
        /* 重新初始化日志系统 */
        apply_logging_config(ctx);
    } else if (strstr(changed_key, "scheduler.") != NULL) {
        /* 更新调度器参数 */
        update_scheduler_config(ctx);
    }
}
```

---

## 六、测试覆盖要求

### 6.1 单元测试

每个配置文件的加载和解析都应有对应的单元测试：

```c
/* test_kernel_config.c - 内核配置单元测试 */
void test_load_kernel_settings(void) {
    config_context_t* ctx = config_context_create("test_kernel");
    ASSERT_NOT_NULL(ctx);
    
    /* 测试正常加载 */
    int result = load_kernel_config(ctx);
    ASSERT_EQ(result, 0);
    
    /* 测试配置值访问 */
    const char* scheduler_type = config_value_as_string(
        config_context_get(ctx, "scheduler.type"), NULL);
    ASSERT_STR_EQUAL(scheduler_type, "priority_queue");
    
    /* 测试类型安全 */
    int64_t max_tasks = config_value_as_int(
        config_context_get(ctx, "scheduler.max_concurrent_tasks"), 10);
    ASSERT_TRUE(max_tasks > 0 && max_tasks <= 1000);
    
    config_context_destroy(ctx);
}

void test_invalid_config_path(void) {
    config_context_t* ctx = config_context_create("test_invalid");
    ASSERT_NOT_NULL(ctx);
    
    /* 测试不存在的路径 */
    int result = load_config_from_path(ctx, "/nonexistent/path/config.yaml");
    ASSERT_NEQ(result, 0);  // 应该失败
    
    config_context_destroy(ctx);
}
```

### 6.2 集成测试

测试多个配置文件的协同加载和验证：

```c
/* test_integration.c - 配置系统集成测试 */
void test_full_system_config_load(void) {
    config_context_t* ctx = config_context_create("integration_test");
    ASSERT_NOT_NULL(ctx);
    
    /* 加载所有管理器配置 */
    int result = load_all_manager_configs(ctx);
    ASSERT_EQ(result, 0);
    
    /* 验证跨配置依赖关系 */
    const char* agent_name = config_value_as_string(
        config_context_get(ctx, "agents.planner.name"), NULL);
    ASSERT_NOT_NULL(agent_name);
    
    /* 验证 Schema 验证通过 */
    result = validate_all_configs(ctx);
    ASSERT_EQ(result, 0);
    
    config_context_destroy(ctx);
}
```

### 6.3 Schema 验证测试

```c
/* test_schema_validation.c - Schema 验证测试 */
void test_valid_kernel_config_passes_validation(void) {
    int result = load_and_validate_config(
        "../manager/kernel/settings.yaml",
        "../manager/schema/kernel-settings.schema.json"
    );
    ASSERT_EQ(result, 0);  // 应该通过验证
}

void test_invalid_config_fails_validation(void) {
    /* 故意修改配置使其无效，然后验证失败 */
    create_temp_invalid_config();
    
    int result = load_and_validate_config(
        "temp_invalid_config.yaml",
        "../manager/schema/kernel-settings.schema.json"
    );
    ASSERT_NEQ(result, 0);  // 应该验证失败
    
    cleanup_temp_files();
}
```

---

## 七、最佳实践清单

### 7.1 开发阶段

- [ ] 使用 `AGENTOS_CONFIG_DIR` 环境变量指向开发目录
- [ ] 所有新配置文件添加对应的 JSON Schema
- [ ] 配置文件使用 UTF-8 编码，无 BOM
- [ ] 环境变量引用使用 `${VARIABLE}` 格式
- [ ] 为每个配置项添加注释说明用途

### 7.2 部署阶段

- [ ] 设置生产环境的 `AGENTOS_CONFIG_DIR`
- [ ] 敏感配置字段使用加密或环境变量引用
- [ ] 启用配置热更新（可选）
- [ ] 配置备份到版本控制系统
- [ ] 运行部署前配置验证脚本

### 7.3 运维阶段

- [ ] 监控配置加载性能指标
- [ ] 记录所有配置变更审计日志
- [ ] 定期验证配置完整性
- [ ] 制定紧急配置回滚流程

---

## 八、故障排除

### 8.1 常见问题

**问题 1**: 配置文件找不到
```
解决方案:
1. 检查 AGENTOS_CONFIG_DIR 环境变量是否正确设置
2. 确认配置文件存在于指定路径
3. 检查文件权限是否允许读取
```

**问题 2**: Schema 验证失败
```
解决方案:
1. 查看详细错误信息和位置
2. 对照 Schema 文件检查配置格式
3. 确保必填字段都已填写
4. 检查数据类型是否匹配
```

**问题 3**: 环境变量未展开
```
解决方案:
1. 确认使用 ${VARIABLE} 格式（不是 $VARIABLE）
2. 检查环境变量是否已导出
3. 在配置源选项中启用 expand_vars=true
```

### 8.2 调试工具

```bash
# 检查配置目录结构
ls -la $AGENTOS_CONFIG_DIR/

# 验证 YAML 语法
python -c "import yaml; yaml.safe_load(open('settings.yaml'))"

# 验证 JSON Schema
ajv validate -s schema.json -d data.json --spec=draft2020-12

# 测试配置加载
export AGENTOS_DEBUG=1
./your_application
```

---

## 九、版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0.0 | 2026-04-01 | 初始版本，定义集成标准和最佳实践 |

---

## 十、参考文档

- [ARCHITECTURAL_PRINCIPLES.md](../../manuals/ARCHITECTURAL_PRINCIPLES.md)
- [config_unified README.md](../../../commons/utils/config_unified/README.md)
- [CONFIG_CHANGE_PROCESS.md](../CONFIG_CHANGE_PROCESS.md)
- [error_code_reference.md](../../manuals/specifications/project_erp/error_code_reference.md)

---

© 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."