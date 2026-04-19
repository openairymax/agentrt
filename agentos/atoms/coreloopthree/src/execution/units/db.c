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
#include "include/memory_compat.h"

typedef struct db_unit_data {
    char* connection_string;
    char* metadata_json;
} db_unit_data_t;

static const char* DANGEROUS_SQL_KEYWORDS[] = {
    "UNION", "INTO", "OUTFILE", "LOAD_FILE", "DUMPFILE",
    "INFORMATION_SCHEMA", "BENCHMARK", "SLEEP", "WAITFOR",
    "CONCAT", "GROUP_CONCAT", "CONCAT_WS", "CHAR(", "CHR(",
    "NCHAR(", "UNHEX(", "HEX(", "EXTRACTVALUE", "UPDATEXML",
    "PROCEDURE", "ANALYSE", "LOAD_EXTENSION", "ATTACH",
    "DETACH", "REPLACE", "INSERT", "UPDATE", "DELETE",
    "DROP", "CREATE", "ALTER", "EXEC", "EXECUTE",
    "GRANT", "REVOKE", "TRUNCATE", "RENAME",
    NULL
};

static const char* DANGEROUS_SQL_PATTERNS[] = {
    "--", ";", "'", "\"", "`", "/*", "*/", "0x", "0b",
    "X'", "||", "&&", "#",
    NULL
};

static int is_safe_query(const char* query) {
    if (!query) return 0;
    const char* p = query;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (strncasecmp(p, "SELECT", 6) != 0) return 0;
    if (p[6] != ' ' && p[6] != '\t' && p[6] != '\n' && p[6] != '\r' && p[6] != '\0') return 0;

    for (int i = 0; DANGEROUS_SQL_PATTERNS[i] != NULL; i++) {
        if (strstr(query, DANGEROUS_SQL_PATTERNS[i]) != NULL) return 0;
    }

    for (int i = 0; DANGEROUS_SQL_KEYWORDS[i] != NULL; i++) {
        if (strcasestr(query, DANGEROUS_SQL_KEYWORDS[i]) != NULL) return 0;
    }

    for (const char* c = query; *c; c++) {
        if ((unsigned char)*c < 0x20 && *c != '\t' && *c != '\n' && *c != '\r') {
            return 0;
        }
    }

    return 1;
}

static size_t json_escape_string(const char* src, char* dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0) return 0;
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\';
            dst[j++] = c;
        } else if (c == '\n') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else if (c == '\r') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\';
            dst[j++] = 'r';
        } else if (c == '\t') {
            if (j + 2 >= dst_size) break;
            dst[j++] = '\\';
            dst[j++] = 't';
        } else if ((unsigned char)c >= 0x20) {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
    return j;
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
    char escaped_conn[512];
    if (connection_string) {
        json_escape_string(connection_string, escaped_conn, sizeof(escaped_conn));
    } else {
        escaped_conn[0] = '\0';
    }
    char meta[768];
    snprintf(meta, sizeof(meta), "{\"type\":\"db\",\"conn\":\"%s\"}", escaped_conn);
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
