/**
 * @file db.c
 * @brief 数据库操作单元（模拟）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct db_unit_data {
    char* connection_string;
    char* metadata_json;
} db_unit_data_t;

/**
 * @brief 验证SQL查询是否仅包含安全操作（SELECT）
 * @param query SQL查询字符串
 * @return 1 安全，0 不安全
 */
static int is_safe_query(const char* query) {
    if (!query) return 0;
    const char* p = query;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (strncasecmp(p, "SELECT", 6) != 0) return 0;
    if (strstr(query, "--") != NULL) return 0;
    if (strstr(query, ";") != NULL) return 0;
    if (strstr(query, "'") != NULL) return 0;
    if (strstr(query, "\"") != NULL) return 0;
    return 1;
}

/**
 * @brief 数据库执行单元的执行方法
 */
static agentos_error_t db_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    (void)unit;
    if (!input || !out_output) return AGENTOS_EINVAL;

    const char* query = (const char*)input;
    if (!is_safe_query(query)) return AGENTOS_EPERM;

    size_t query_len = strlen(query);
    size_t buf_size = query_len + 64;
    char* result = (char*)malloc(buf_size);
    if (!result) return AGENTOS_ENOMEM;
    snprintf(result, buf_size, "Executed query: %.200s, returned 1 row.", query);
    *out_output = result;
    return AGENTOS_SUCCESS;
}

static void db_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    db_unit_data_t* data = (db_unit_data_t*)unit->data;
    if (data) {
        if (data->connection_string) free(data->connection_string);
        if (data->metadata_json) free(data->metadata_json);
        free(data);
    }
    free(unit);
}

static const char* db_get_metadata(agentos_execution_unit_t* unit) {
    db_unit_data_t* data = (db_unit_data_t*)unit->data;
    return data ? data->metadata_json : NULL;
}

agentos_execution_unit_t* agentos_db_unit_create(const char* connection_string) {
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)malloc(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;

    db_unit_data_t* data = (db_unit_data_t*)malloc(sizeof(db_unit_data_t));
    if (!data) {
        free(unit);
        return NULL;
    }

    data->connection_string = connection_string ? strdup(connection_string) : NULL;
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"type\":\"db\",\"conn\":\"%s\"}", connection_string ? connection_string : "");
    data->metadata_json = strdup(meta);

    if (!data->metadata_json || (connection_string && !data->connection_string)) {
        if (data->connection_string) free(data->connection_string);
        if (data->metadata_json) free(data->metadata_json);
        free(data);
        free(unit);
        return NULL;
    }

    unit->data = data;
    unit->execute = db_execute;
    unit->destroy = db_destroy;
    unit->get_metadata = db_get_metadata;

    return unit;
}