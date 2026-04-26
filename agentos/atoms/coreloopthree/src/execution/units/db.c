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
#include "memory_compat.h"
#include <sqlite3.h>

typedef struct db_unit_data {
    char* connection_string;
    char* metadata_json;
    sqlite3* db;
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
    if (!unit || !unit->execution_unit_data || !input || !out_output) return AGENTOS_EINVAL;

    db_unit_data_t* data = (db_unit_data_t*)unit->execution_unit_data;
    const char* query = (const char*)input;
    if (!is_safe_query(query)) return AGENTOS_EPERM;

    if (!data->db) {
        *out_output = AGENTOS_STRDUP("{\"error\":\"no_database_connection\"}");
        return *out_output ? AGENTOS_EPERM : AGENTOS_ENOMEM;
    }

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(data->db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        size_t buf_size = strlen(sqlite3_errmsg(data->db)) + 64;
        char* err_result = (char*)AGENTOS_MALLOC(buf_size);
        if (err_result) {
            snprintf(err_result, buf_size, "{\"error\":\"prepare_failed\",\"detail\":\"%s\"}", sqlite3_errmsg(data->db));
        }
        *out_output = err_result;
        return *out_output ? AGENTOS_EIO : AGENTOS_ENOMEM;
    }

    size_t result_cap = 4096;
    char* result = (char*)AGENTOS_MALLOC(result_cap);
    if (!result) { sqlite3_finalize(stmt); return AGENTOS_ENOMEM; }

    size_t pos = 0;
    pos += snprintf(result + pos, result_cap - pos, "{\"columns\":[");

    int col_count = sqlite3_column_count(stmt);
    for (int i = 0; i < col_count; i++) {
        const char* col_name = sqlite3_column_name(stmt, i);
        if (i > 0 && pos < result_cap - 2) result[pos++] = ',';
        pos += snprintf(result + pos, result_cap - pos, "\"%s\"", col_name ? col_name : "");
    }
    pos += snprintf(result + pos, result_cap - pos, "],\"rows\":[");

    int row_count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (row_count > 0 && pos < result_cap - 2) result[pos++] = ',';
        if (pos + 256 > result_cap) {
            result_cap *= 2;
            char* new_result = (char*)AGENTOS_REALLOC(result, result_cap);
            if (!new_result) { AGENTOS_FREE(result); sqlite3_finalize(stmt); return AGENTOS_ENOMEM; }
            result = new_result;
        }
        result[pos++] = '[';
        for (int i = 0; i < col_count; i++) {
            if (i > 0 && pos < result_cap - 2) result[pos++] = ',';
            const char* val = (const char*)sqlite3_column_text(stmt, i);
            if (val) {
                char escaped[512];
                json_escape_string(val, escaped, sizeof(escaped));
                pos += snprintf(result + pos, result_cap - pos, "\"%s\"", escaped);
            } else {
                pos += snprintf(result + pos, result_cap - pos, "null");
            }
        }
        if (pos < result_cap - 2) result[pos++] = ']';
        row_count++;
        if (row_count >= 1000) break;
    }

    if (pos + 32 > result_cap) {
        result_cap += 64;
        char* new_result = (char*)AGENTOS_REALLOC(result, result_cap);
        if (!new_result) { AGENTOS_FREE(result); sqlite3_finalize(stmt); return AGENTOS_ENOMEM; }
        result = new_result;
    }
    pos += snprintf(result + pos, result_cap - pos, "],\"row_count\":%d}", row_count);

    sqlite3_finalize(stmt);
    *out_output = result;
    return AGENTOS_SUCCESS;
}

static void db_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    db_unit_data_t* data = (db_unit_data_t*)unit->execution_unit_data;
    if (data) {
        if (data->db) sqlite3_close(data->db);
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

    if (connection_string) {
        int rc = sqlite3_open(connection_string, &data->db);
        if (rc != SQLITE_OK) {
            if (data->db) {
                sqlite3_close(data->db);
                data->db = NULL;
            }
        } else {
            sqlite3_exec(data->db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
            sqlite3_exec(data->db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
        }
    } else {
        data->db = NULL;
    }

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
