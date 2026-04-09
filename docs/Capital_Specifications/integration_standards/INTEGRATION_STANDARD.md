# Manager 妯″潡涓庣粺涓€閰嶇疆搴撻泦鎴愭爣鍑?
**鐗堟湰**: v1.0.0  
**鏈€鍚庢洿鏂?*: 2026-04-01  
**閫傜敤鑼冨洿**: AgentOS/manager 妯″潡涓?agentos/commons/utils/config_unified 鐨勯泦鎴? 
**鐞嗚鍩虹**: 宸ョ▼涓よ锛堟帴鍙ｅ绾﹀寲锛夈€佷簲缁存浜よ璁★紙鍐呮牳瑙侹-2銆佸伐绋嬭E-4/E-8锛? 
**褰撳墠浣嶇疆**: `agentos/docs/specifications/integration_standards/` (浠?`agentos/manager/` 杩佺Щ)

---

## 涓€銆侀泦鎴愭杩?
### 1.1 闆嗘垚鐩爣

寤虹珛 manager 妯″潡锛堥厤缃鐞嗕腑蹇冿級涓?`agentos/commons/utils/config_unified` 缁熶竴閰嶇疆搴撲箣闂寸殑鏍囧噯鍖栭泦鎴愭満鍒讹紝瀹炵幇锛?
1. **缁熶竴鐨勯厤缃姞杞?*: 鎵€鏈夋ā鍧椾娇鐢ㄧ浉鍚岀殑 API 鍔犺浇 manager 閰嶇疆
2. **鏍囧噯鍖栫殑閰嶇疆璺緞**: 閫氳繃鐜鍙橀噺瀹氫箟閰嶇疆鏍圭洰褰?3. **Schema 楠岃瘉鑷姩鍖?*: 鍔犺浇鏃惰嚜鍔ㄩ獙璇侀厤缃枃浠舵牸寮?4. **鐑洿鏂版敮鎸?*: 鏀寔杩愯鏃堕厤缃彉鏇村拰閫氱煡
5. **鍚戝悗鍏煎**: 淇濇寔鐜版湁浠ｇ爜鐨勫吋瀹规€?
### 1.2 鏋舵瀯鍏崇郴

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?                   AgentOS 鍚勪笟鍔℃ā鍧?                        鈹?鈹? 鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?     鈹?鈹? 鈹?cupolas  鈹?鈹?daemon   鈹?鈹?atoms    鈹?鈹?鍏朵粬妯″潡  鈹?     鈹?鈹? 鈹斺攢鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹?     鈹?鈹?       鈹?           鈹?           鈹?           鈹?            鈹?鈹?       鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹粹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹粹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?            鈹?鈹?                       鈫?缁熶竴璋冪敤                            鈹?鈹?             鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                  鈹?鈹?             鈹? config_unified API     鈹?                  鈹?鈹?             鈹?(agentos/commons/utils/)        鈹?                  鈹?鈹?             鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                  鈹?鈹?                          鈫?                                鈹?鈹?             鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                  鈹?鈹?             鈹?  Manager 閰嶇疆瀛樺偍       鈹?                  鈹?鈹?             鈹?($AGENTOS_CONFIG_DIR)    鈹?                  鈹?鈹?             鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?                  鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

---

## 浜屻€佹爣鍑嗛厤缃矾寰勫畾涔?
### 2.1 鐜鍙橀噺瑙勮寖

#### AGENTOS_CONFIG_DIR锛堝繀闇€锛?
```bash
# 瀹氫箟 Manager 閰嶇疆鐨勬牴鐩綍璺緞
export AGENTOS_CONFIG_DIR="/etc/agentos"          # Linux 鐢熶骇鐜
export AGENTOS_CONFIG_DIR="./AgentOS/manager"     # 寮€鍙戠幆澧?export AGENTOS_CONFIG_DIR="C:\\agentos\\config"   # Windows 鐜
```

**鐢ㄩ€?*: 
- 鎵€鏈夋ā鍧楁煡鎵鹃厤缃枃浠剁殑鍩哄噯璺緞
- 鏇夸唬纭紪鐮佺殑閰嶇疆璺緞
- 鏀寔澶氱幆澧冮儴缃诧紙寮€鍙?棰勫彂/鐢熶骇锛?
**榛樿鍊?*: 濡傛灉鏈缃紝浣跨敤浠ヤ笅鍥為€€绛栫暐锛?1. Linux: `/etc/agentos`
2. Windows: `%APPDATA%\agentos`
3. macOS: `~/Library/Application Support/agentos`
4. 寮€鍙戠幆澧? `./AgentOS/manager`

### 2.2 閰嶇疆瀛愮洰褰曠粨鏋?
鍦?`$AGENTOS_CONFIG_DIR` 涓嬶紝閬靛惊 manager 妯″潡鐨勭洰褰曠粨鏋勶細

```
$AGENTOS_CONFIG_DIR/
鈹溾攢鈹€ kernel/
鈹?  鈹斺攢鈹€ settings.yaml           # 鍐呮牳閰嶇疆
鈹溾攢鈹€ model/
鈹?  鈹斺攢鈹€ model.yaml              # LLM妯″瀷閰嶇疆
鈹溾攢鈹€ agent/
鈹?  鈹斺攢鈹€ registry.yaml           # Agent娉ㄥ唽琛?鈹溾攢鈹€ skill/
鈹?  鈹斺攢鈹€ registry.yaml           # 鎶€鑳芥敞鍐岃〃
鈹溾攢鈹€ security/
鈹?  鈹溾攢鈹€ policy.yaml             # 瀹夊叏绛栫暐
鈹?  鈹斺攢鈹€ permission_rules.yaml   # 鏉冮檺瑙勫垯
鈹溾攢鈹€ sanitizer/
鈹?  鈹斺攢鈹€ sanitizer_rules.json    # 杈撳叆鍑€鍖栬鍒?鈹溾攢鈹€ logging/
鈹?  鈹斺攢鈹€ manager.yaml            # 鏃ュ織閰嶇疆
鈹溾攢鈹€ service/
鈹?  鈹斺攢鈹€ tool_d/
鈹?      鈹斺攢鈹€ tool.yaml           # 宸ュ叿鏈嶅姟閰嶇疆
鈹溾攢鈹€ monitoring/
鈹?  鈹溾攢鈹€ alerts/
鈹?  鈹?  鈹斺攢鈹€ cupolas-alerts.yml  # 鍛婅閰嶇疆
鈹?  鈹斺攢鈹€ dashboards/
鈹?      鈹斺攢鈹€ cupolas-dashboard.json  # 鐩戞帶闈㈡澘
鈹溾攢鈹€ schema/                     # JSON Schema 楠岃瘉鏂囦欢
鈹?  鈹溾攢鈹€ kernel-settings.schema.json
鈹?  鈹溾攢鈹€ model.schema.json
鈹?  鈹斺攢鈹€ ...                    # 鍏朵粬Schema鏂囦欢
鈹溾攢鈹€ environment/                # 鐜瑕嗙洊閰嶇疆
鈹?  鈹溾攢鈹€ development.yaml
鈹?  鈹溾攢鈹€ staging.yaml
鈹?  鈹斺攢鈹€ production.yaml
鈹斺攢鈹€ .env                        # 鐜鍙橀噺妯℃澘
```

---

## 涓夈€佺粺涓€鍔犺浇API浣跨敤瑙勮寖

### 3.1 鍩虹鍔犺浇妯″紡

```c
/**
 * @file config_loader_example.c
 * @brief 浣跨敤 config_unified 鍔犺浇 Manager 閰嶇疆鐨勬爣鍑嗙ず渚? */

#include "config_unified.h"
#include <stdio.h>
#include <stdlib.h>

/* 鑾峰彇鏍囧噯閰嶇疆璺緞 */
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

/* 鍔犺浇鍐呮牳閰嶇疆 */
int load_kernel_config(config_context_t* ctx) {
    const char* config_dir = get_config_dir();
    char file_path[512];
    
    snprintf(file_path, sizeof(file_path), "%s/kernel/settings.yaml", config_dir);
    
    config_file_source_options_t options = {
        .file_path = file_path,
        .format = "yaml",
        .encoding = "utf-8",
        .auto_reload = true,
        .reload_interval_ms = 5000  // 5绉掓鏌ヤ竴娆″彉鏇?    };
    
    config_source_t* source = config_source_create_file(&options);
    if (!source) {
        fprintf(stderr, "鏃犳硶鍒涘缓閰嶇疆婧? %s\n", file_path);
        return -1;
    }
    
    int result = config_source_load(source, ctx);
    if (result != CONFIG_SUCCESS) {
        fprintf(stderr, "鍔犺浇閰嶇疆澶辫触: %s\n", file_path);
        config_source_destroy(source);
        return -1;
    }
    
    config_source_destroy(source);
    return 0;
}

/* 涓诲嚱鏁扮ず渚?*/
int main(void) {
    /* 鍒涘缓閰嶇疆涓婁笅鏂?*/
    config_context_t* ctx = config_context_create("agentos");
    if (!ctx) {
        fprintf(stderr, "鍒涘缓閰嶇疆涓婁笅鏂囧け璐n");
        return EXIT_FAILURE;
    }
    
    /* 鍔犺浇鍚勫瓙绯荤粺閰嶇疆 */
    if (load_kernel_config(ctx) != 0) {
        config_context_destroy(ctx);
        return EXIT_FAILURE;
    }
    
    /* 浣跨敤閰嶇疆鍊?*/
    const char* log_level = config_value_as_string(
        config_context_get(ctx, "logging.level"), "info");
    
    printf("鏃ュ織绾у埆: %s\n", log_level);
    
    /* 娓呯悊璧勬簮 */
    config_context_destroy(ctx);
    return EXIT_SUCCESS;
}
```

### 3.2 鎵归噺鍔犺浇妯″紡

```c
/**
 * @brief 鎵归噺鍔犺浇鎵€鏈?Manager 瀛愮郴缁熼厤缃? */
#define MAX_SOURCES 16

typedef struct {
    const char* relative_path;  // 鐩稿浜?$AGENTOS_CONFIG_DIR 鐨勮矾寰?    const char* format;         // 鏂囦欢鏍煎紡 ("yaml", "json")
    bool required;              // 鏄惁蹇呴』瀛樺湪
} config_source_info_t;

static const config_source_info_t MANAGER_SOURCES[] = {
    {"kernel/settings.yaml", "yaml", true},
    {"model/model.yaml", "yaml", true},
    {"agent/registry.yaml", "yaml", true},
    {"skill/registry.yaml", "yaml", false},
    {"security/policy.yaml", "yaml", true},
    {"logging/manager.yaml", "yaml", true},
    {NULL, NULL, false}  // 缁撴潫鏍囪
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
                fprintf(stderr, "[ERROR] 鏃犳硶鍒涘缓閰嶇疆婧? %s\n", file_path);
                fail_count++;
            } else {
                printf("[WARN] 鍙€夐厤缃簮涓嶅瓨鍦? %s\n", file_path);
            }
            continue;
        }
        
        int result = config_source_load(source, ctx);
        if (result == CONFIG_SUCCESS) {
            success_count++;
            printf("[OK] 鍔犺浇閰嶇疆鎴愬姛: %s\n", info->relative_path);
        } else {
            if (info->required) {
                fprintf(stderr, "[ERROR] 鍔犺浇閰嶇疆澶辫触: %s\n", file_path);
                fail_count++;
            } else {
                printf("[WARN] 鍙€夐厤缃姞杞藉け璐? %s\n", file_path);
            }
        }
        
        config_source_destroy(source);
    }
    
    printf("\n閰嶇疆鍔犺浇缁熻: 鎴愬姛=%d, 澶辫触=%d\n", success_count, fail_count);
    return (fail_count > 0) ? -1 : 0;
}
```

### 3.3 Schema 楠岃瘉闆嗘垚

```c
/**
 * @brief 鍔犺浇閰嶇疆骞惰繘琛?Schema 楠岃瘉
 */
int load_and_validate_config(const char* config_path, const char* schema_path) {
    config_context_t* ctx = config_context_create("validation_test");
    if (!ctx) return -1;
    
    /* 鍔犺浇閰嶇疆鏂囦欢 */
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
    
    /* 鍒涘缓楠岃瘉鍣?*/
    validator_options_t validator_opts = {
        .type = VALIDATOR_TYPE_JSON_SCHEMA,
        .schema_path = schema_path
    };
    
    config_validator_t* validator = config_validator_create(&validator_opts);
    if (!validator) {
        config_context_destroy(ctx);
        return -1;
    }
    
    /* 鎵ц楠岃瘉 */
    validation_report_t report;
    int result = config_validator_validate(validator, ctx, &report);
    
    if (result == CONFIG_SUCCESS) {
        printf("[PASS] 閰嶇疆楠岃瘉閫氳繃: %s\n", config_path);
    } else {
        printf("[FAIL] 閰嶇疆楠岃瘉澶辫触: %s\n", config_path);
        printf("閿欒淇℃伅: %s\n", report.error_message);
        printf("閿欒浣嶇疆: line %d, column %d\n", report.error_line, report.error_column);
    }
    
    /* 娓呯悊璧勬簮 */
    config_validator_destroy(validator);
    config_context_destroy(ctx);
    
    return result;
}
```

---

## 鍥涖€佺幆澧冨彉閲忓紩鐢ㄦ爣鍑嗗寲

### 4.1 寮曠敤鏍煎紡瑙勮寖

Manager 閰嶇疆鏂囦欢涓殑鐜鍙橀噺寮曠敤搴旂粺涓€浣跨敤 `${VARIABLE}` 鏍煎紡锛?
```yaml
# 鉁?姝ｇ‘鏍煎紡锛氫娇鐢?${VARIABLE}
database:
  host: ${DATABASE_HOST:-localhost}
  port: ${DATABASE_PORT:-5432}
  password_env: ${DATABASE_PASSWORD}

# 鉂?閿欒鏍煎紡锛氶伩鍏嶄娇鐢?$VARIABLE 鎴栧叾浠栨牸寮?# database:
#   host: $DATABASE_HOST  # 涓嶆帹鑽?```

### 4.2 榛樿鍊艰娉?
鏀寔 Bash 椋庢牸鐨勯粯璁ゅ€艰娉曪細

| 璇硶 | 鍚箟 | 绀轰緥 |
|------|------|------|
| `${VAR}` | 蹇呴渶鍙橀噺锛岀己澶辨椂鎶ラ敊 | `${API_KEY}` |
| `${VAR:-default}` | 缂哄け鏃朵娇鐢ㄩ粯璁ゅ€?| `${HOST:-localhost}` |
| `${VAR:+alternative}` | 瀛樺湪鏃朵娇鐢ㄦ浛浠ｅ€?| `${DEBUG:+verbose}` |

### 4.3 鐜鍙橀噺灞曞紑閫夐」

```c
/* 鍦ㄩ厤缃簮鍒涘缓鏃跺惎鐢ㄧ幆澧冨彉閲忓睍寮€ */
config_env_source_options_t env_opts = {
    .prefix = "AGENTOS_",
    .case_sensitive = false,
    .separator = "_",
    .expand_vars = true  // 鍚敤 ${VAR} 灞曞紑
};
```

---

## 浜斻€佺儹鏇存柊鏀寔

### 5.1 鐑洿鏂伴厤缃?
```c
/**
 * @brief 鍚敤閰嶇疆鐑洿鏂扮洃鎺? */
void setup_hot_reload(config_context_t* ctx) {
    hot_reload_options_t opts = {
        .callback = on_config_changed,
        .user_data = NULL,
        .debounce_ms = 1000,  // 闃叉姈寤惰繜1绉?        .validate_on_reload = true,  // 閲嶈浇鏃惰嚜鍔ㄩ獙璇?        .schema_path = "$AGENTOS_CONFIG_DIR/schema/"
    };
    
    config_hot_reload_manager_t* hot_reload = 
        config_hot_reload_manager_create(ctx, &opts);
    
    if (hot_reload) {
        /* 姣?绉掓鏌ヤ竴娆￠厤缃彉鏇?*/
        config_hot_reload_start(hot_reload, 5000);
        printf("閰嶇疆鐑洿鏂板凡鍚姩\n");
    }
}

/* 閰嶇疆鍙樻洿鍥炶皟鍑芥暟 */
void on_config_changed(config_context_t* ctx, const char* changed_key, void* user_data) {
    printf("[HOT RELOAD] 閰嶇疆宸叉洿鏂? %s\n", changed_key);
    
    /* 鏍规嵁鍙樻洿鐨勯敭鎵ц鐩稿簲鎿嶄綔 */
    if (strstr(changed_key, "logging.") != NULL) {
        /* 閲嶆柊鍒濆鍖栨棩蹇楃郴缁?*/
        apply_logging_config(ctx);
    } else if (strstr(changed_key, "scheduler.") != NULL) {
        /* 鏇存柊璋冨害鍣ㄥ弬鏁?*/
        update_scheduler_config(ctx);
    }
}
```

---

## 鍏€佹祴璇曡鐩栬姹?
### 6.1 鍗曞厓娴嬭瘯

姣忎釜閰嶇疆鏂囦欢鐨勫姞杞藉拰瑙ｆ瀽閮藉簲鏈夊搴旂殑鍗曞厓娴嬭瘯锛?
```c
/* test_kernel_config.c - 鍐呮牳閰嶇疆鍗曞厓娴嬭瘯 */
void test_load_kernel_settings(void) {
    config_context_t* ctx = config_context_create("test_kernel");
    ASSERT_NOT_NULL(ctx);
    
    /* 娴嬭瘯姝ｅ父鍔犺浇 */
    int result = load_kernel_config(ctx);
    ASSERT_EQ(result, 0);
    
    /* 娴嬭瘯閰嶇疆鍊艰闂?*/
    const char* scheduler_type = config_value_as_string(
        config_context_get(ctx, "scheduler.type"), NULL);
    ASSERT_STR_EQUAL(scheduler_type, "priority_queue");
    
    /* 娴嬭瘯绫诲瀷瀹夊叏 */
    int64_t max_tasks = config_value_as_int(
        config_context_get(ctx, "scheduler.max_concurrent_tasks"), 10);
    ASSERT_TRUE(max_tasks > 0 && max_tasks <= 1000);
    
    config_context_destroy(ctx);
}

void test_invalid_config_path(void) {
    config_context_t* ctx = config_context_create("test_invalid");
    ASSERT_NOT_NULL(ctx);
    
    /* 娴嬭瘯涓嶅瓨鍦ㄧ殑璺緞 */
    int result = load_config_from_path(ctx, "/nonexistent/path/config.yaml");
    ASSERT_NEQ(result, 0);  // 搴旇澶辫触
    
    config_context_destroy(ctx);
}
```

### 6.2 闆嗘垚娴嬭瘯

娴嬭瘯澶氫釜閰嶇疆鏂囦欢鐨勫崗鍚屽姞杞藉拰楠岃瘉锛?
```c
/* test_integration.c - 閰嶇疆绯荤粺闆嗘垚娴嬭瘯 */
void test_full_system_config_load(void) {
    config_context_t* ctx = config_context_create("integration_test");
    ASSERT_NOT_NULL(ctx);
    
    /* 鍔犺浇鎵€鏈夌鐞嗗櫒閰嶇疆 */
    int result = load_all_manager_configs(ctx);
    ASSERT_EQ(result, 0);
    
    /* 楠岃瘉璺ㄩ厤缃緷璧栧叧绯?*/
    const char* agent_name = config_value_as_string(
        config_context_get(ctx, "agents.planner.name"), NULL);
    ASSERT_NOT_NULL(agent_name);
    
    /* 楠岃瘉 Schema 楠岃瘉閫氳繃 */
    result = validate_all_configs(ctx);
    ASSERT_EQ(result, 0);
    
    config_context_destroy(ctx);
}
```

### 6.3 Schema 楠岃瘉娴嬭瘯

```c
/* test_schema_validation.c - Schema 楠岃瘉娴嬭瘯 */
void test_valid_kernel_config_passes_validation(void) {
    int result = load_and_validate_config(
        "../agentos/manager/kernel/settings.yaml",
        "../agentos/manager/schema/kernel-settings.schema.json"
    );
    ASSERT_EQ(result, 0);  // 搴旇閫氳繃楠岃瘉
}

void test_invalid_config_fails_validation(void) {
    /* 鏁呮剰淇敼閰嶇疆浣垮叾鏃犳晥锛岀劧鍚庨獙璇佸け璐?*/
    create_temp_invalid_config();
    
    int result = load_and_validate_config(
        "temp_invalid_config.yaml",
        "../agentos/manager/schema/kernel-settings.schema.json"
    );
    ASSERT_NEQ(result, 0);  // 搴旇楠岃瘉澶辫触
    
    cleanup_temp_files();
}
```

---

## 涓冦€佹渶浣冲疄璺垫竻鍗?
### 7.1 寮€鍙戦樁娈?
- [ ] 浣跨敤 `AGENTOS_CONFIG_DIR` 鐜鍙橀噺鎸囧悜寮€鍙戠洰褰?- [ ] 鎵€鏈夋柊閰嶇疆鏂囦欢娣诲姞瀵瑰簲鐨?JSON Schema
- [ ] 閰嶇疆鏂囦欢浣跨敤 UTF-8 缂栫爜锛屾棤 BOM
- [ ] 鐜鍙橀噺寮曠敤浣跨敤 `${VARIABLE}` 鏍煎紡
- [ ] 涓烘瘡涓厤缃」娣诲姞娉ㄩ噴璇存槑鐢ㄩ€?
### 7.2 閮ㄧ讲闃舵

- [ ] 璁剧疆鐢熶骇鐜鐨?`AGENTOS_CONFIG_DIR`
- [ ] 鏁忔劅閰嶇疆瀛楁浣跨敤鍔犲瘑鎴栫幆澧冨彉閲忓紩鐢?- [ ] 鍚敤閰嶇疆鐑洿鏂帮紙鍙€夛級
- [ ] 閰嶇疆澶囦唤鍒扮増鏈帶鍒剁郴缁?- [ ] 杩愯閮ㄧ讲鍓嶉厤缃獙璇佽剼鏈?
### 7.3 杩愮淮闃舵

- [ ] 鐩戞帶閰嶇疆鍔犺浇鎬ц兘鎸囨爣
- [ ] 璁板綍鎵€鏈夐厤缃彉鏇村璁℃棩蹇?- [ ] 瀹氭湡楠岃瘉閰嶇疆瀹屾暣鎬?- [ ] 鍒跺畾绱ф€ラ厤缃洖婊氭祦绋?
---

## 鍏€佹晠闅滄帓闄?
### 8.1 甯歌闂

**闂 1**: 閰嶇疆鏂囦欢鎵句笉鍒?```
瑙ｅ喅鏂规:
1. 妫€鏌?AGENTOS_CONFIG_DIR 鐜鍙橀噺鏄惁姝ｇ‘璁剧疆
2. 纭閰嶇疆鏂囦欢瀛樺湪浜庢寚瀹氳矾寰?3. 妫€鏌ユ枃浠舵潈闄愭槸鍚﹀厑璁歌鍙?```

**闂 2**: Schema 楠岃瘉澶辫触
```
瑙ｅ喅鏂规:
1. 鏌ョ湅璇︾粏閿欒淇℃伅鍜屼綅缃?2. 瀵圭収 Schema 鏂囦欢妫€鏌ラ厤缃牸寮?3. 纭繚蹇呭～瀛楁閮藉凡濉啓
4. 妫€鏌ユ暟鎹被鍨嬫槸鍚﹀尮閰?```

**闂 3**: 鐜鍙橀噺鏈睍寮€
```
瑙ｅ喅鏂规:
1. 纭浣跨敤 ${VARIABLE} 鏍煎紡锛堜笉鏄?$VARIABLE锛?2. 妫€鏌ョ幆澧冨彉閲忔槸鍚﹀凡瀵煎嚭
3. 鍦ㄩ厤缃簮閫夐」涓惎鐢?expand_vars=true
```

### 8.2 璋冭瘯宸ュ叿

```bash
# 妫€鏌ラ厤缃洰褰曠粨鏋?ls -la $AGENTOS_CONFIG_DIR/

# 楠岃瘉 YAML 璇硶
python -c "import yaml; yaml.safe_load(open('settings.yaml'))"

# 楠岃瘉 JSON Schema
ajv validate -s schema.json -d data.json --spec=draft2020-12

# 娴嬭瘯閰嶇疆鍔犺浇
export AGENTOS_DEBUG=1
./your_application
```

---

## 涔濄€佺増鏈巻鍙?
| 鐗堟湰 | 鏃ユ湡 | 鍙樻洿璇存槑 |
|------|------|---------|
| v1.0.0 | 2026-04-01 | 鍒濆鐗堟湰锛屽畾涔夐泦鎴愭爣鍑嗗拰鏈€浣冲疄璺?|
| v1.0.1 | 2026-04-03 | 鏂囨。杩佺Щ鑷?agentos/docs/specifications/integration_standards/ |

---

## 鍗併€佸弬鑰冩枃妗?
- [ARCHITECTURAL_PRINCIPLES.md](../ARCHITECTURAL_PRINCIPLES.md)
- [config_unified README.md](../../../agentos/commons/utils/config_unified/README.md)
- [CONFIG_CHANGE_PROCESS.md](../../../agentos/manager/CONFIG_CHANGE_PROCESS.md)
- [error_code_reference.md](../project_erp/error_code_reference.md)
- [Integration Standards README](./README.md)

---

## 馃摑 杩佺Щ璁板綍 / Migration Record

| 鏃ユ湡 | 鎿嶄綔 | 鍘熶綅缃?| 鏂颁綅缃?|
|------|------|--------|--------|
| 2026-04-03 | 绉诲姩鏂囨。 | `agentos/manager/INTEGRATION_STANDARD.md` | `agentos/docs/specifications/integration_standards/INTEGRATION_STANDARD.md` |

**杩佺Щ鍘熷洜**: 闆嗘垚鏍囧噯灞炰簬椤圭洰绾ц鑼冩枃妗ｏ紝搴旈泦涓鐞嗗湪 `agentos/docs/specifications/` 涓嬶紝渚夸簬璺ㄦā鍧楁煡闃呭拰缁存姢銆?
---

漏 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."
