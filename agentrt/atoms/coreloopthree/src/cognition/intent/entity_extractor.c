/**
 * @file entity_extractor.c
 * @brief 实体提取器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "entity_extractor.h"
#include "error.h"

#include "atomic_compat.h"
#include "intent_utils.h"
#include "memory_compat.h"
#include "types.h"

#include <ctype.h>
#ifndef _WIN32
#include <regex.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


static atomic_int g_extractor_initialized = 0;

const char *agentrt_entity_type_name(agentrt_entity_type_t type)
{
    switch (type) {
    case AGENTRT_ENTITY_UNKNOWN:
        return "unknown";
    case AGENTRT_ENTITY_PERSON:
        return "person";
    case AGENTRT_ENTITY_ORGANIZATION:
        return "organization";
    case AGENTRT_ENTITY_LOCATION:
        return "location";
    case AGENTRT_ENTITY_TIME:
        return "time";
    case AGENTRT_ENTITY_DATE:
        return "date";
    case AGENTRT_ENTITY_NUMBER:
        return "number";
    case AGENTRT_ENTITY_URL:
        return "url";
    case AGENTRT_ENTITY_EMAIL:
        return "email";
    case AGENTRT_ENTITY_FILEPATH:
        return "filepath";
    case AGENTRT_ENTITY_COMMAND:
        return "command";
    case AGENTRT_ENTITY_PARAMETER:
        return "parameter";
    default:
        return "unknown";
    }
}

int agentrt_entity_extractor_init(void)
{
    int expected = 0;
    atomic_compare_exchange_strong_explicit(&g_extractor_initialized, &expected, 1,
                                            memory_order_seq_cst, memory_order_seq_cst);
    return 0;
}

void agentrt_entity_extractor_cleanup(void)
{
    atomic_store_explicit(&g_extractor_initialized, 0, memory_order_seq_cst);
}

agentrt_extraction_result_t *agentrt_extraction_result_create(size_t initial_capacity)
{
    if (initial_capacity == 0) {
        initial_capacity = 10;
    }

    agentrt_extraction_result_t *result =
        (agentrt_extraction_result_t *)AGENTRT_CALLOC(1, sizeof(agentrt_extraction_result_t));
    if (!result) {
        AGENTRT_ERROR_NULL(AGENTRT_EUNKNOWN, "validation failed");
    }

    result->entities =
        (agentrt_entity_t *)AGENTRT_CALLOC(initial_capacity, sizeof(agentrt_entity_t));
    if (!result->entities) {
        AGENTRT_FREE(result);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    result->entity_count = 0;
    result->capacity = initial_capacity;

    return result;
}

void agentrt_extraction_result_destroy(agentrt_extraction_result_t *result)
{
    if (!result) {
        return;
    }

    if (result->entities) {
        for (size_t i = 0; i < result->entity_count; i++) {
            if (result->entities[i].value) {
                AGENTRT_FREE(result->entities[i].value);
            }
        }
        AGENTRT_FREE(result->entities);
    }

    AGENTRT_FREE(result);
}

int agentrt_extraction_result_add(agentrt_extraction_result_t *result,
                                  const agentrt_entity_t *entity)
{
    if (!result || !entity) {
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    /* 扩容检查 */
    if (result->entity_count >= result->capacity) {
        size_t new_capacity = result->capacity * 2;
        agentrt_entity_t *new_entities = (agentrt_entity_t *)AGENTRT_REALLOC(
            result->entities, new_capacity * sizeof(agentrt_entity_t));
        if (!new_entities) {
            ATM_RET_ERR(AGENTRT_EINVAL);
        }

        /* 初始化新空间 */
        __builtin_memset(new_entities + result->capacity, 0,
               (new_capacity - result->capacity) * sizeof(agentrt_entity_t));

        result->entities = new_entities;
        result->capacity = new_capacity;
    }

    /* 复制实体 */
    agentrt_entity_t *target = &result->entities[result->entity_count];
    target->type = entity->type;
    target->type_name = entity->type_name;
    target->start_pos = entity->start_pos;
    target->end_pos = entity->end_pos;
    target->confidence = entity->confidence;

    if (entity->value && entity->value_len > 0) {
        target->value = (char *)AGENTRT_MALLOC(entity->value_len + 1);
        if (target->value) {
            __builtin_memcpy(target->value, entity->value, entity->value_len);
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
static void extract_numbers(const char *input, size_t input_len,
                            agentrt_extraction_result_t *result)
{
    const char *p = input;
    size_t pos = 0;

    while (*p && pos < input_len) {
        if (isdigit(*p)) {
            const char *start = p;
            int start_pos = pos;

            /* 提取完整数字（包括小数点） */
            while (*p && (isdigit(*p) || *p == '.')) {
                p++;
                pos++;
            }

            size_t len = p - start;
            if (len > 0 && len <= 20) { /* 合理的数字长度限制 */
                agentrt_entity_t entity;
                __builtin_memset(&entity, 0, sizeof(entity));
                entity.type = AGENTRT_ENTITY_NUMBER;
                entity.type_name = agentrt_entity_type_name(AGENTRT_ENTITY_NUMBER);
                entity.value = (char *)AGENTRT_MALLOC(len + 1);
                if (entity.value) {
                    __builtin_memcpy(entity.value, start, len);
                    entity.value[len] = '\0';
                    entity.value_len = len;
                    entity.start_pos = start_pos;
                    entity.end_pos = pos - 1;
                    entity.confidence = 0.95f;

                    agentrt_extraction_result_add(result, &entity);
                    AGENTRT_FREE(entity.value); /* add 已复制 */
                }
            }

            continue;
        }

        p++;
        pos++;
    }
}

/* 提取 URL 实体 */
#ifndef _WIN32
static void extract_urls(const char *input, size_t input_len, agentrt_extraction_result_t *result)
{
    regex_t regex;
    regmatch_t match;
    const char *pattern = "(https?://[^\\s]+|ftp://[^\\s]+)";

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        return;
    }

    const char *p = input;
    int offset = 0;

    while (regexec(&regex, p, 1, &match, 0) == 0) {
        size_t len = match.rm_eo - match.rm_so;
        if (len > 3 && len <= 256) { /* 合理的 URL 长度限制 */
            agentrt_entity_t entity;
            __builtin_memset(&entity, 0, sizeof(entity));
            entity.type = AGENTRT_ENTITY_URL;
            entity.type_name = agentrt_entity_type_name(AGENTRT_ENTITY_URL);
            entity.value = (char *)AGENTRT_MALLOC(len + 1);
            if (entity.value) {
                __builtin_memcpy(entity.value, p + match.rm_so, len);
                entity.value[len] = '\0';
                entity.value_len = len;
                entity.start_pos = offset + match.rm_so;
                entity.end_pos = offset + match.rm_eo - 1;
                entity.confidence = 0.98f;

                agentrt_extraction_result_add(result, &entity);
                AGENTRT_FREE(entity.value);
            }
        }

        p += match.rm_eo;
        offset += match.rm_eo;
    }

    regfree(&regex);
}

/* 提取邮箱实体 */
static void extract_emails(const char *input, size_t input_len, agentrt_extraction_result_t *result)
{
    regex_t regex;
    regmatch_t match;
    const char *pattern = "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}";

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        return;
    }

    const char *p = input;
    int offset = 0;

    while (regexec(&regex, p, 1, &match, 0) == 0) {
        size_t len = match.rm_eo - match.rm_so;
        if (len > 5 && len <= 128) { /* 合理的邮箱长度限制 */
            agentrt_entity_t entity;
            __builtin_memset(&entity, 0, sizeof(entity));
            entity.type = AGENTRT_ENTITY_EMAIL;
            entity.type_name = agentrt_entity_type_name(AGENTRT_ENTITY_EMAIL);
            entity.value = (char *)AGENTRT_MALLOC(len + 1);
            if (entity.value) {
                __builtin_memcpy(entity.value, p + match.rm_so, len);
                entity.value[len] = '\0';
                entity.value_len = len;
                entity.start_pos = offset + match.rm_so;
                entity.end_pos = offset + match.rm_eo - 1;
                entity.confidence = 0.97f;

                agentrt_extraction_result_add(result, &entity);
                AGENTRT_FREE(entity.value);
            }
        }

        p += match.rm_eo;
        offset += match.rm_eo;
    }

    regfree(&regex);
}

/* 提取文件路径实体 */
static void extract_filepaths(const char *input, size_t input_len,
                              agentrt_extraction_result_t *result)
{
    regex_t regex;
    regmatch_t match;
    const char *pattern = "(/[a-zA-Z0-9_./-]+|[A-Za-z]:\\\\[a-zA-Z0-9_./\\\\-]+)";

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        return;
    }

    const char *p = input;
    int offset = 0;

    while (regexec(&regex, p, 1, &match, 0) == 0) {
        size_t len = match.rm_eo - match.rm_so;
        if (len > 2 && len <= 512) { /* 合理的路径长度限制 */
            agentrt_entity_t entity;
            __builtin_memset(&entity, 0, sizeof(entity));
            entity.type = AGENTRT_ENTITY_FILEPATH;
            entity.type_name = agentrt_entity_type_name(AGENTRT_ENTITY_FILEPATH);
            entity.value = (char *)AGENTRT_MALLOC(len + 1);
            if (entity.value) {
                __builtin_memcpy(entity.value, p + match.rm_so, len);
                entity.value[len] = '\0';
                entity.value_len = len;
                entity.start_pos = offset + match.rm_so;
                entity.end_pos = offset + match.rm_eo - 1;
                entity.confidence = 0.92f;

                agentrt_extraction_result_add(result, &entity);
                AGENTRT_FREE(entity.value);
            }
        }

        p += match.rm_eo;
        offset += match.rm_eo;
    }

    regfree(&regex);
}
#else
static void extract_urls(const char *input, size_t input_len, agentrt_extraction_result_t *result)
{
    (void)input;
    (void)input_len;
    (void)result;
}
static void extract_emails(const char *input, size_t input_len, agentrt_extraction_result_t *result)
{
    (void)input;
    (void)input_len;
    (void)result;
}
static void extract_filepaths(const char *input, size_t input_len,
                              agentrt_extraction_result_t *result)
{
    (void)input;
    (void)input_len;
    (void)result;
}
#endif

int agentrt_entity_extract(const char *input, size_t input_len, agentrt_extraction_result_t *result)
{
    if (!input || !result || input_len == 0) {
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    if (!atomic_load_explicit(&g_extractor_initialized, memory_order_acquire)) {
        agentrt_entity_extractor_init();
    }

    /* 如果结果为空，创建新的 */
    if (!result->entities) {
        agentrt_extraction_result_t *new_result = agentrt_extraction_result_create(10);
        if (!new_result) {
            ATM_RET_ERR(AGENTRT_EINVAL);
        }
        __builtin_memcpy(result, new_result, sizeof(*result));
        AGENTRT_FREE(new_result);
    }

    /* 按类型提取各类实体 */
    extract_numbers(input, input_len, result);
    extract_urls(input, input_len, result);
    extract_emails(input, input_len, result);
    extract_filepaths(input, input_len, result);

    return 0;
}
