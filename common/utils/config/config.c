/**
 * @file config.c
 * @brief 简单配置管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_MAX_ENTRIES 256
#define CONFIG_MAX_KEY_LEN 128
#define CONFIG_MAX_VALUE_LEN 1024

typedef struct {
    char key[CONFIG_MAX_KEY_LEN];
    char value[CONFIG_MAX_VALUE_LEN];
    int used;
} config_entry_t;

struct agentos_config {
    config_entry_t entries[CONFIG_MAX_ENTRIES];
    int count;
};

agentos_config_t* agentos_config_create(void) {
    agentos_config_t* config = (agentos_config_t*)calloc(1, sizeof(agentos_config_t));
    return config;
}

void agentos_config_destroy(agentos_config_t* config) {
    if (config) {
        free(config);
    }
}

int agentos_config_parse(agentos_config_t* config, const char* text) {
    if (!config || !text) {
        return -1;
    }

    char* text_copy = strdup(text);
    if (!text_copy) {
        return -1;
    }

    char* line = strtok(text_copy, "\n\r");
    while (line && config->count < CONFIG_MAX_ENTRIES) {
        while (*line == ' ' || *line == '\t') line++;

        if (*line == '#' || *line == '\0' || (*line == '/' && *(line + 1) == '/')) {
            line = strtok(NULL, "\n\r");
            continue;
        }

        char* eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char* key = line;
            char* value = eq + 1;

            while (*key == ' ' || *key == '\t') key++;
            char* key_end = key + strlen(key) - 1;
            while (key_end > key && (*key_end == ' ' || *key_end == '\t')) *key_end-- = '\0';

            while (*value == ' ' || *value == '\t') value++;

            config_entry_t* entry = &config->entries[config->count];
            snprintf(entry->key, CONFIG_MAX_KEY_LEN, "%s", key);
            snprintf(entry->value, CONFIG_MAX_VALUE_LEN, "%s", value);
            entry->used = 1;
            config->count++;
        }

        line = strtok(NULL, "\n\r");
    }

    free(text_copy);
    return 0;
}

int agentos_config_load_file(agentos_config_t* config, const char* path) {
    if (!config || !path) {
        return -1;
    }

    FILE* fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    char line[2048];
    while (fgets(line, sizeof(line), fp) && config->count < CONFIG_MAX_ENTRIES) {
        char* ptr = line;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        if (*ptr == '#' || *ptr == '\0' || (*ptr == '/' && *(ptr + 1) == '/')) {
            continue;
        }

        char* eq = strchr(ptr, '=');
        if (eq) {
            *eq = '\0';
            char* key = ptr;
            char* value = eq + 1;

            while (*key == ' ' || *key == '\t') key++;
            char* key_end = key + strlen(key) - 1;
            while (key_end > key && (*key_end == ' ' || *key_end == '\t')) *key_end-- = '\0';

            while (*value == ' ' || *value == '\t') value++;
            value[strcspn(value, "\n\r")] = '\0';

            config_entry_t* entry = &config->entries[config->count];
            snprintf(entry->key, CONFIG_MAX_KEY_LEN, "%s", key);
            snprintf(entry->value, CONFIG_MAX_VALUE_LEN, "%s", value);
            entry->used = 1;
            config->count++;
        }
    }

    fclose(fp);
    return 0;
}

int agentos_config_save_file(agentos_config_t* config, const char* path) {
    if (!config || !path) {
        return -1;
    }

    FILE* fp = fopen(path, "w");
    if (!fp) {
        return -1;
    }

    for (int i = 0; i < config->count; i++) {
        if (config->entries[i].used) {
            fprintf(fp, "%s=%s\n", config->entries[i].key, config->entries[i].value);
        }
    }

    fclose(fp);
    return 0;
}

const char* agentos_config_get_string(agentos_config_t* config, const char* key, const char* default_value) {
    if (!config || !key) {
        return default_value;
    }

    for (int i = 0; i < config->count; i++) {
        if (config->entries[i].used && strcmp(config->entries[i].key, key) == 0) {
            return config->entries[i].value;
        }
    }
    return default_value;
}

int agentos_config_get_int(agentos_config_t* config, const char* key, int default_value) {
    const char* value = agentos_config_get_string(config, key, NULL);
    if (value) {
        return atoi(value);
    }
    return default_value;
}

double agentos_config_get_double(agentos_config_t* config, const char* key, double default_value) {
    const char* value = agentos_config_get_string(config, key, NULL);
    if (value) {
        return atof(value);
    }
    return default_value;
}

int agentos_config_get_bool(agentos_config_t* config, const char* key, int default_value) {
    const char* value = agentos_config_get_string(config, key, NULL);
    if (value) {
        if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 || strcmp(value, "yes") == 0) {
            return 1;
        }
        if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0 || strcmp(value, "no") == 0) {
            return 0;
        }
    }
    return default_value;
}

int agentos_config_set_string(agentos_config_t* config, const char* key, const char* value) {
    if (!config || !key || !value) {
        return -1;
    }

    for (int i = 0; i < config->count; i++) {
        if (config->entries[i].used && strcmp(config->entries[i].key, key) == 0) {
            snprintf(config->entries[i].value, CONFIG_MAX_VALUE_LEN, "%s", value);
            return 0;
        }
    }

    if (config->count >= CONFIG_MAX_ENTRIES) {
        return -1;
    }

    config_entry_t* entry = &config->entries[config->count];
    snprintf(entry->key, CONFIG_MAX_KEY_LEN, "%s", key);
    snprintf(entry->value, CONFIG_MAX_VALUE_LEN, "%s", value);
    entry->used = 1;
    config->count++;

    return 0;
}

int agentos_config_set_int(agentos_config_t* config, const char* key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    return agentos_config_set_string(config, key, buf);
}

int agentos_config_set_double(agentos_config_t* config, const char* key, double value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", value);
    return agentos_config_set_string(config, key, buf);
}

int agentos_config_set_bool(agentos_config_t* config, const char* key, int value) {
    return agentos_config_set_string(config, key, value ? "true" : "false");
}

int agentos_config_remove(agentos_config_t* config, const char* key) {
    if (!config || !key) {
        return -1;
    }

    for (int i = 0; i < config->count; i++) {
        if (config->entries[i].used && strcmp(config->entries[i].key, key) == 0) {
            config->entries[i].used = 0;
            return 0;
        }
    }
    return -1;
}

int agentos_config_has(agentos_config_t* config, const char* key) {
    if (!config || !key) {
        return 0;
    }

    for (int i = 0; i < config->count; i++) {
        if (config->entries[i].used && strcmp(config->entries[i].key, key) == 0) {
            return 1;
        }
    }
    return 0;
}
