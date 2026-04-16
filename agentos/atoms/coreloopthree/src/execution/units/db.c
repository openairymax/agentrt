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
#include <strings.h>
#include <agentos/memory.h>

typedef struct db_unit_data {
    char* connection_string;
    char* metadata_json;
} db_unit_data_t;

static int is_safe_query(const char* query) {
    if (!query) return 0;
    const char* p = query;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (strncasecmp(p, "SELECT", 6) != 0) return 0;
    if (strstr(query, "--") != NULL) return 0;
    if (strstr(query, ";") != NULL) return 0;
    if (strstr(query, "'") != NULL) return 0;
    if (strstr(query, "\"") != NULL) return 0;
    if (strcasestr(query, "UNION") != NULL) return 0;
    if (strcasestr(query, "INTO") != NULL) return 0;
    if (strcasestr(query, "OUTFILE") != NULL) return 0;
    if (strcasestr(query, "LOAD_FILE") != NULL) return 0;
    if (strstr(query, "0x") != NULL) return 0;
    if (strstr(query, "CHAR(") != NULL) return 0;
    if (strstr(query, "CONCAT(") != NULL) return 0;
    return 1;
}

static agentos_error_t db_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    (void)unit;
    if (!input || !out_output) return AGENTOS_EINVAL;

    const char* query = (const char*)input;
    if (!is_safe_query(query)) return AGENTOS_EPERM;

    size_t query_len = strlen(query);
    size_t buf_size = query_len + 64;
    char* result = (char*)AGENTOS_MALLOC(buf_size);
    if (!result) return AGENTOS_ENOMEM;
    snprintf(result, buf_size, "Executed query: %.200s, returned 1 row.", query);
    *out_output = result;
    return AGENTOS_SUCCESS;
}

static void db_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    db_unit_data_t* data = (db_unit_data_t*)unit->execution_unit_data;
    if (data) {
        if (data->connection_string) AGENTOS_FREE(data->connection_string);
        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(unit);
}

agentos_execution_unit_t* agentos_db_unit_create(const char* connection_string) {
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)AGENTOS_MALLOC(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;
    memset(unit, 0, sizeof(*unit));

    db_unit_data_t* data = (db_unit_data_t*)AGENTOS_MALLOC(sizeof(db_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        return NULL;
    }

    data->connection_string = connection_string ? AGENTOS_STRDUP(connection_string) : NULL;
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"type\":\"db\",\"conn\":\"%s\"}", connection_string ? connection_string : "");
    data->metadata_json = AGENTOS_STRDUP(meta);

    if (!data->metadata_json || (connection_string && !data->connection_string)) {
        if (data->connection_string) AGENTOS_FREE(data->connection_string);
        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return NULL;
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = db_execute;
    unit->execution_unit_destroy = db_destroy;

    return unit;
}
