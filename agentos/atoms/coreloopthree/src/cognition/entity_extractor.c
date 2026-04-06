/**
 * @file entity_extractor.c
 * @brief 实体提取器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "entity_extractor.h"
#include "intent_utils.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <regex.h>

static int g_extractor_initialized = 0;

const char* agentos_entity_type_name(agentos_entity_type_t type) {
    switch (type) {
        case AGENTOS_ENTITY_UNKNOWN:      return "unknown";
        case AGENTOS_ENTITY_PERSON:       return "person";
        case AGENTOS_ENTITY_ORGANIZATION: return "organization";
        case AGENTOS_ENTITY_LOCATION:     return "location";
        case AGENTOS_ENTITY_TIME:         return "time";
        case AGENTOS_ENTITY_DATE:         return "date";
        case AGENTOS_ENTITY_NUMBER:       return "number";
        case AGENTOS_ENTITY_URL:          return "url";
        case AGENTOS_ENTITY_EMAIL:        return "email";
        case AGENTOS_ENTITY_FILEPATH:     return "filepath";
        case AGENTOS_ENTITY_COMMAND:      return "command";
        case AGENTOS_ENTITY_PARAMETER:    return "parameter";
        default:                          return "unknown";
    }
}

int agentos_entity_extractor_init(void) {
    if (g_extractor_initialized) {
        return 0;
    }
    
    g_extractor_initialized = 1;
    return 0;
}

void agentos_entity_extractor_cleanup(void) {
    g_extractor_initialized = 0;
}

agentos_extraction_result_t* agentos_extraction_result_create(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 10;
    }
    
    agentos_extraction_result_t* result =
        (agentos_extraction_result_t*)AGENTOS_CALLOC(1, sizeof(agentos_extraction_result_t));
    if (!result) {
        return NULL;
    }
    
    result->entities = (agentos_entity_t*)AGENTOS_CALLOC(initial_capacity, sizeof(agentos_entity_t));
    if (!result->entities) {
        AGENTOS_FREE(result);
        return NULL;
    }
    
    result->entity_count = 0;
    result->capacity = initial_capacity;
    
    return result;
}

void agentos_extraction_result_destroy(agentos_extraction_result_t* result) {
    if (!result) {
        return;
    }
    
    if (result->entities) {
        for (size_t i = 0; i < result->entity_count; i++) {
            if (result->entities[i].value) {
                AGENTOS_FREE(result->entities[i].value);
            }
        }
        AGENTOS_FREE(result->entities);
    }
    
    AGENTOS_FREE(result);
}

int agentos_extraction_result_add(agentos_extraction_result_t* result,
                                  const agentos_entity_t* entity) {
    if (!result || !entity) {
        return -1;
    }
    
    /* 扩容检查 */
    if (result->entity_count >= result->capacity) {
        size_t new_capacity = result->capacity * 2;
        agentos_entity_t* new_entities = (agentos_entity_t*)AGENTOS_REALLOC(
            result->entities, new_capacity * sizeof(agentos_entity_t));
        if (!new_entities) {
            return -1;
        }
        
        /* 初始化新空间 */
        memset(new_entities + result->capacity, 0,
               (new_capacity - result->capacity) * sizeof(agentos_entity_t));
        
        result->entities = new_entities;
        result->capacity = new_capacity;
    }
    
    /* 复制实体 */
    agentos_entity_t* target = &result->entities[result->entity_count];
    target->type = entity->type;
    target->type_name = entity->type_name;
    target->start_pos = entity->start_pos;
    target->end_pos = entity->end_pos;
    target->confidence = entity->confidence;
    
    if (entity->value && entity->value_len > 0) {
        target->value = (char*)AGENTOS_MALLOC(entity->value_len + 1);
        if (target->value) {
            memcpy(target->value, entity->value, entity->value_len);
            target->value[entity->value_len] = '\0';
            target->value_len = entity->value_len;
        } else {
            target->value = NULL;
            target->value_len = 0;
        }
    } else {
        target->value = NULL;
        target->value_len = 0;
    }
    
    result->entity_count++;
    return 0;
}

/* 提取数字实体 */
static void extract_numbers(const char* input, size_t input_len,
                            agentos_extraction_result_t* result) {
    const char* p = input;
    int pos = 0;
    
    while (*p && pos < input_len) {
        if (isdigit(*p)) {
            const char* start = p;
            int start_pos = pos;
            
            /* 提取完整数字（包括小数点） */
            while (*p && (isdigit(*p) || *p == '.')) {
                p++;
                pos++;
            }
            
            size_t len = p - start;
            if (len > 0 && len <= 20) { /* 合理的数字长度限制 */
                agentos_entity_t entity;
                memset(&entity, 0, sizeof(entity));
                entity.type = AGENTOS_ENTITY_NUMBER;
                entity.type_name = agentos_entity_type_name(AGENTOS_ENTITY_NUMBER);
                entity.value = (char*)AGENTOS_MALLOC(len + 1);
                if (entity.value) {
                    memcpy(entity.value, start, len);
                    entity.value[len] = '\0';
                    entity.value_len = len;
                    entity.start_pos = start_pos;
                    entity.end_pos = pos - 1;
                    entity.confidence = 0.95f;
                    
                    agentos_extraction_result_add(result, &entity);
                    AGENTOS_FREE(entity.value); /* add 已复制 */
                }
            }
            
            continue;
        }
        
        p++;
        pos++;
    }
}

/* 提取 URL 实体 */
static void extract_urls(const char* input, size_t input_len,
                         agentos_extraction_result_t* result) {
    regex_t regex;
    regmatch_t match;
    const char* pattern = "(https?://[^\\s]+|ftp://[^\\s]+)";
    
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        return;
    }
    
    const char* p = input;
    int offset = 0;
    
    while (regexec(&regex, p, 1, &match, 0) == 0) {
        size_t len = match.rm_eo - match.rm_so;
        if (len > 3 && len <= 256) { /* 合理的 URL 长度限制 */
            agentos_entity_t entity;
            memset(&entity, 0, sizeof(entity));
            entity.type = AGENTOS_ENTITY_URL;
            entity.type_name = agentos_entity_type_name(AGENTOS_ENTITY_URL);
            entity.value = (char*)AGENTOS_MALLOC(len + 1);
            if (entity.value) {
                memcpy(entity.value, p + match.rm_so, len);
                entity.value[len] = '\0';
                entity.value_len = len;
                entity.start_pos = offset + match.rm_so;
                entity.end_pos = offset + match.rm_eo - 1;
                entity.confidence = 0.98f;
                
                agentos_extraction_result_add(result, &entity);
                AGENTOS_FREE(entity.value);
            }
        }
        
        p += match.rm_eo;
        offset += match.rm_eo;
    }
    
    regfree(&regex);
}

/* 提取邮箱实体 */
static void extract_emails(const char* input, size_t input_len,
                           agentos_extraction_result_t* result) {
    regex_t regex;
    regmatch_t match;
    const char* pattern = "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}";
    
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        return;
    }
    
    const char* p = input;
    int offset = 0;
    
    while (regexec(&regex, p, 1, &match, 0) == 0) {
        size_t len = match.rm_eo - match.rm_so;
        if (len > 5 && len <= 128) { /* 合理的邮箱长度限制 */
            agentos_entity_t entity;
            memset(&entity, 0, sizeof(entity));
            entity.type = AGENTOS_ENTITY_EMAIL;
            entity.type_name = agentos_entity_type_name(AGENTOS_ENTITY_EMAIL);
            entity.value = (char*)AGENTOS_MALLOC(len + 1);
            if (entity.value) {
                memcpy(entity.value, p + match.rm_so, len);
                entity.value[len] = '\0';
                entity.value_len = len;
                entity.start_pos = offset + match.rm_so;
                entity.end_pos = offset + match.rm_eo - 1;
                entity.confidence = 0.97f;
                
                agentos_extraction_result_add(result, &entity);
                AGENTOS_FREE(entity.value);
            }
        }
        
        p += match.rm_eo;
        offset += match.rm_eo;
    }
    
    regfree(&regex);
}

/* 提取文件路径实体 */
static void extract_filepaths(const char* input, size_t input_len,
                              agentos_extraction_result_t* result) {
    regex_t regex;
    regmatch_t match;
    const char* pattern = "(/[a-zA-Z0-9_./-]+|[A-Za-z]:\\\\[a-zA-Z0-9_./\\\\-]+)";
    
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        return;
    }
    
    const char* p = input;
    int offset = 0;
    
    while (regexec(&regex, p, 1, &match, 0) == 0) {
        size_t len = match.rm_eo - match.rm_so;
        if (len > 2 && len <= 512) { /* 合理的路径长度限制 */
            agentos_entity_t entity;
            memset(&entity, 0, sizeof(entity));
            entity.type = AGENTOS_ENTITY_FILEPATH;
            entity.type_name = agentos_entity_type_name(AGENTOS_ENTITY_FILEPATH);
            entity.value = (char*)AGENTOS_MALLOC(len + 1);
            if (entity.value) {
                memcpy(entity.value, p + match.rm_so, len);
                entity.value[len] = '\0';
                entity.value_len = len;
                entity.start_pos = offset + match.rm_so;
                entity.end_pos = offset + match.rm_eo - 1;
                entity.confidence = 0.92f;
                
                agentos_extraction_result_add(result, &entity);
                AGENTOS_FREE(entity.value);
            }
        }
        
        p += match.rm_eo;
        offset += match.rm_eo;
    }
    
    regfree(&regex);
}

int agentos_entity_extract(const char* input, size_t input_len,
                          agentos_extraction_result_t* result) {
    if (!input || !result || input_len == 0) {
        return -1;
    }
    
    if (!g_extractor_initialized) {
        agentos_entity_extractor_init();
    }
    
    /* 如果结果为空，创建新的 */
    if (!result->entities) {
        agentos_extraction_result_t* new_result = agentos_extraction_result_create(10);
        if (!new_result) {
            return -1;
        }
        memcpy(result, new_result, sizeof(*result));
        AGENTOS_FREE(new_result);
    }
    
    /* 按类型提取各类实体 */
    extract_numbers(input, input_len, result);
    extract_urls(input, input_len, result);
    extract_emails(input, input_len, result);
    extract_filepaths(input, input_len, result);
    
    return 0;
}
