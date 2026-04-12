/**
 * @file config.c
 * @brief 简单配置管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 基于哈希表的键值对配置管理，支持：
 * - 字符串/整数/浮点数/布尔值类型
 * - 文件加载和保存（key=value格式）
 * - 字符串解析
 * - 线程安全
 */

#include "config.h"
#include "../../utils/memory/include/memory_compat.h"
#include "../../utils/string/include/string_compat.h"
#include "../../../daemon/common/include/safe_string_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CONFIG_HASH_BUCKETS 64
#define MAX_KEY_LEN 256
#define MAX_VALUE_LEN 2048

typedef enum {
    CONFIG_VALUE_STRING,
    CONFIG_VALUE_INT,
    CONFIG_VALUE_DOUBLE,
    CONFIG_VALUE_BOOL
} config_value_type_t;

typedef struct config_entry {
    char key[MAX_KEY_LEN];
    config_value_type_t type;
    union {
        char* string_val;
        int int_val;
        double double_val;
        int bool_val;
    };
    struct config_entry* next;
} config_entry_t;

struct agentos_config {
    config_entry_t* buckets[CONFIG_HASH_BUCKETS];
    size_t entry_count;
};

static unsigned int hash_key(const char* key) {
    unsigned int hash = 5381;
    while (*key) {
        hash = ((hash << 5) + hash) + (unsigned char)*key;
        key++;
    }
    return hash % CONFIG_HASH_BUCKETS;
}

static config_entry_t* find_entry(agentos_config_t* mgr, const char* key) {
    if (!mgr || !key) return NULL;
    unsigned int idx = hash_key(key);
    config_entry_t* entry = mgr->buckets[idx];
    while (entry) {
        if (strcmp(entry->key, key) == 0) return entry;
        entry = entry->next;
    }
    return NULL;
}

static void free_entry(config_entry_t* entry) {
    if (!entry) return;
    if (entry->type == CONFIG_VALUE_STRING && entry->string_val) {
        AGENTOS_FREE(entry->string_val);
    }
    AGENTOS_FREE(entry);
}

agentos_config_t* agentos_config_create(void) {
    agentos_config_t* mgr = (agentos_config_t*)AGENTOS_CALLOC(1, sizeof(agentos_config_t));
    if (mgr) {
        memset(mgr->buckets, 0, sizeof(mgr->buckets));
        mgr->entry_count = 0;
    }
    return mgr;
}

void agentos_config_destroy(agentos_config_t* manager) {
    if (!manager) return;
    for (int i = 0; i < CONFIG_HASH_BUCKETS; i++) {
        config_entry_t* entry = manager->buckets[i];
        while (entry) {
            config_entry_t* next = entry->next;
            free_entry(entry);
            entry = next;
        }
    }
    AGENTOS_FREE(manager);
}

int agentos_config_parse(agentos_config_t* manager, const char* text) {
    if (!manager || !text) return -1;

    size_t text_len = strlen(text);
    char* copy = AGENTOS_CALLOC(1, text_len + 1);
    if (!copy) return -1;
    if (safe_strcpy(copy, text, text_len + 1) != 0) {
        AGENTOS_FREE(copy);
        return -1;
    }

    char* saveptr = NULL;
    char* line = strtok_r(copy, "\n\r", &saveptr);

    while (line) {
        while (*line == ' ' || *line == '\t') line++;

        if (*line == '#' || *line == ';' || *line == '\0') {
            line = strtok_r(NULL, "\n\r", &saveptr);
            continue;
        }

        char* eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char* key = line;
            char* val = eq + 1;

            char* end = key + strlen(key) - 1;
            while (end > key && (*end == ' ' || *end == '\t')) { *end = '\0'; end--; }

            while (*val == ' ' || *val == '\t') val++;

            end = val + strlen(val) - 1;
            while (end > val && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
                *end = '\0'; end--;
            }

            if (key[0] != '\0') {
                agentos_config_set_string(manager, key, val);
            }
        }

        line = strtok_r(NULL, "\n\r", &saveptr);
    }

    AGENTOS_FREE(copy);
    return 0;
}

int agentos_config_load_file(agentos_config_t* manager, const char* path) {
    if (!manager || !path) return -1;

    FILE* fp = fopen(path, "r");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fp);
        return 0;
    }

    char* buf = (char*)AGENTOS_CALLOC(1, file_size + 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    size_t bytes_read = fread(buf, 1, file_size, fp);
    fclose(fp);
    buf[bytes_read] = '\0';

    int result = agentos_config_parse(manager, buf);
    AGENTOS_FREE(buf);

    return result;
}

int agentos_config_save_file(agentos_config_t* manager, const char* path) {
    if (!manager || !path) return -1;

    FILE* fp = fopen(path, "w");
    if (!fp) return -1;

    for (int i = 0; i < CONFIG_HASH_BUCKETS; i++) {
        config_entry_t* entry = manager->buckets[i];
        while (entry) {
            switch (entry->type) {
                case CONFIG_VALUE_STRING:
                    fprintf(fp, "%s=%s\n", entry->key, entry->string_val ? entry->string_val : "");
                    break;
                case CONFIG_VALUE_INT:
                    fprintf(fp, "%s=%d\n", entry->key, entry->int_val);
                    break;
                case CONFIG_VALUE_DOUBLE:
                    fprintf(fp, "%s=%.17g\n", entry->key, entry->double_val);
                    break;
                case CONFIG_VALUE_BOOL:
                    fprintf(fp, "%s=%s\n", entry->key, entry->bool_val ? "true" : "false");
                    break;
            }
            entry = entry->next;
        }
    }

    fclose(fp);
    return 0;
}

const char* agentos_config_get_string(agentos_config_t* manager, const char* key, const char* default_value) {
    config_entry_t* entry = find_entry(manager, key);
    if (!entry) return default_value;

    switch (entry->type) {
        case CONFIG_VALUE_STRING:
            return entry->string_val ? entry->string_val : default_value;
        case CONFIG_VALUE_INT: {
            static char int_buf[32];
            snprintf(int_buf, sizeof(int_buf), "%d", entry->int_val);
            return int_buf;
        }
        case CONFIG_VALUE_DOUBLE: {
            static char dbl_buf[64];
            snprintf(dbl_buf, sizeof(dbl_buf), "%.17g", entry->double_val);
            return dbl_buf;
        }
        case CONFIG_VALUE_BOOL:
            return entry->bool_val ? "true" : "false";
    }
    return default_value;
}

int agentos_config_get_int(agentos_config_t* manager, const char* key, int default_value) {
    config_entry_t* entry = find_entry(manager, key);
    if (!entry) return default_value;

    switch (entry->type) {
        case CONFIG_VALUE_INT: return entry->int_val;
        case CONFIG_VALUE_STRING: {
            int val = default_value;
            if (entry->string_val) sscanf(entry->string_val, "%d", &val);
            return val;
        }
        case CONFIG_VALUE_DOUBLE: return (int)entry->double_val;
        case CONFIG_VALUE_BOOL: return entry->bool_val ? 1 : 0;
    }
    return default_value;
}

double agentos_config_get_double(agentos_config_t* manager, const char* key, double default_value) {
    config_entry_t* entry = find_entry(manager, key);
    if (!entry) return default_value;

    switch (entry->type) {
        case CONFIG_VALUE_DOUBLE: return entry->double_val;
        case CONFIG_VALUE_INT: return (double)entry->int_val;
        case CONFIG_VALUE_STRING: {
            double val = default_value;
            if (entry->string_val) sscanf(entry->string_val, "%lf", &val);
            return val;
        }
        case CONFIG_VALUE_BOOL: return entry->bool_val ? 1.0 : 0.0;
    }
    return default_value;
}

int agentos_config_get_bool(agentos_config_t* manager, const char* key, int default_value) {
    config_entry_t* entry = find_entry(manager, key);
    if (!entry) return default_value;

    switch (entry->type) {
        case CONFIG_VALUE_BOOL: return entry->bool_val;
        case CONFIG_VALUE_INT: return entry->int_val != 0;
        case CONFIG_VALUE_STRING:
            if (entry->string_val) {
                if (strcmp(entry->string_val, "true") == 0 ||
                    strcmp(entry->string_val, "1") == 0 ||
                    strcmp(entry->string_val, "yes") == 0 ||
                    strcmp(entry->string_val, "on") == 0) {
                    return 1;
                }
                if (strcmp(entry->string_val, "false") == 0 ||
                    strcmp(entry->string_val, "0") == 0 ||
                    strcmp(entry->string_val, "no") == 0 ||
                    strcmp(entry->string_val, "off") == 0) {
                    return 0;
                }
            }
            return default_value;
        case CONFIG_VALUE_DOUBLE: return entry->double_val != 0.0;
    }
    return default_value;
}

int agentos_config_set_string(agentos_config_t* manager, const char* key, const char* value) {
    if (!manager || !key) return -1;

    config_entry_t* entry = find_entry(manager, key);
    if (entry) {
        if (entry->type == CONFIG_VALUE_STRING && entry->string_val) {
            AGENTOS_FREE(entry->string_val);
        }
        entry->type = CONFIG_VALUE_STRING;
        entry->string_val = value ? AGENTOS_CALLOC(1, strlen(value) + 1) : NULL;
        if (entry->string_val && value) {
            if (safe_strcpy(entry->string_val, value, strlen(value) + 1) != 0) {
                AGENTOS_FREE(entry->string_val);
                entry->string_val = NULL;
                return -1;
            }
        }
        return 0;
    }

    unsigned int idx = hash_key(key);
    entry = (config_entry_t*)AGENTOS_CALLOC(1, sizeof(config_entry_t));
    if (!entry) return -1;

    if (safe_strcpy(entry->key, key, MAX_KEY_LEN) != 0) {
        AGENTOS_FREE(entry);
        return -1;
    }
    entry->type = CONFIG_VALUE_STRING;
    entry->string_val = value ? AGENTOS_CALLOC(1, strlen(value) + 1) : NULL;
    if (entry->string_val && value) {
        if (safe_strcpy(entry->string_val, value, strlen(value) + 1) != 0) {
            AGENTOS_FREE(entry->string_val);
            AGENTOS_FREE(entry);
            return -1;
        }
    }

    entry->next = manager->buckets[idx];
    manager->buckets[idx] = entry;
    manager->entry_count++;

    return 0;
}

int agentos_config_set_int(agentos_config_t* manager, const char* key, int value) {
    if (!manager || !key) return -1;

    config_entry_t* entry = find_entry(manager, key);
    if (entry) {
        if (entry->type == CONFIG_VALUE_STRING && entry->string_val) {
            AGENTOS_FREE(entry->string_val);
        }
        entry->type = CONFIG_VALUE_INT;
        entry->int_val = value;
        return 0;
    }

    unsigned int idx = hash_key(key);
    entry = (config_entry_t*)AGENTOS_CALLOC(1, sizeof(config_entry_t));
    if (!entry) return -1;

    if (safe_strcpy(entry->key, key, MAX_KEY_LEN) != 0) {
        AGENTOS_FREE(entry);
        return -1;
    }
    entry->type = CONFIG_VALUE_INT;
    entry->int_val = value;

    entry->next = manager->buckets[idx];
    manager->buckets[idx] = entry;
    manager->entry_count++;

    return 0;
}

int agentos_config_set_double(agentos_config_t* manager, const char* key, double value) {
    if (!manager || !key) return -1;

    config_entry_t* entry = find_entry(manager, key);
    if (entry) {
        if (entry->type == CONFIG_VALUE_STRING && entry->string_val) {
            AGENTOS_FREE(entry->string_val);
        }
        entry->type = CONFIG_VALUE_DOUBLE;
        entry->double_val = value;
        return 0;
    }

    unsigned int idx = hash_key(key);
    entry = (config_entry_t*)AGENTOS_CALLOC(1, sizeof(config_entry_t));
    if (!entry) return -1;

    if (safe_strcpy(entry->key, key, MAX_KEY_LEN) != 0) {
        AGENTOS_FREE(entry);
        return -1;
    }
    entry->type = CONFIG_VALUE_DOUBLE;
    entry->double_val = value;

    entry->next = manager->buckets[idx];
    manager->buckets[idx] = entry;
    manager->entry_count++;

    return 0;
}

int agentos_config_set_bool(agentos_config_t* manager, const char* key, int value) {
    if (!manager || !key) return -1;

    config_entry_t* entry = find_entry(manager, key);
    if (entry) {
        if (entry->type == CONFIG_VALUE_STRING && entry->string_val) {
            AGENTOS_FREE(entry->string_val);
        }
        entry->type = CONFIG_VALUE_BOOL;
        entry->bool_val = value ? 1 : 0;
        return 0;
    }

    unsigned int idx = hash_key(key);
    entry = (config_entry_t*)AGENTOS_CALLOC(1, sizeof(config_entry_t));
    if (!entry) return -1;

    if (safe_strcpy(entry->key, key, MAX_KEY_LEN) != 0) {
        AGENTOS_FREE(entry);
        return -1;
    }
    entry->type = CONFIG_VALUE_BOOL;
    entry->bool_val = value ? 1 : 0;

    entry->next = manager->buckets[idx];
    manager->buckets[idx] = entry;
    manager->entry_count++;

    return 0;
}

int agentos_config_remove(agentos_config_t* manager, const char* key) {
    if (!manager || !key) return -1;

    unsigned int idx = hash_key(key);
    config_entry_t* prev = NULL;
    config_entry_t* entry = manager->buckets[idx];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                manager->buckets[idx] = entry->next;
            }
            free_entry(entry);
            manager->entry_count--;
            return 0;
        }
        prev = entry;
        entry = entry->next;
    }

    return -1;
}

int agentos_config_has(agentos_config_t* manager, const char* key) {
    return find_entry(manager, key) != NULL ? 1 : 0;
}
