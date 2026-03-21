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

static agentos_error_t db_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    // 模拟执行 SQL 查询
    const char* query = (const char*)input;
    // 实际应使用数据库库连接，这里仅模拟
    char* result = (char*)malloc(256);
    if (!result) return AGENTOS_ENOMEM;
    snprintf(result, 256, "Executed query: %s, returned 1 row.", query);
    *out_output = result;
    // From data intelligence emerges. by spharx
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