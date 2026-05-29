/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file jsonrpc.c
 * @brief JSON-RPC 2.0 协议工具函数实现
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "jsonrpc.h"

#include "error.h"
#include "gateway_compat.h"
#include "memory_compat.h"

#include <stdlib.h>
#include <string.h>

#ifdef AGENTOS_HAS_CJSON
#include <cjson/cJSON.h>
#endif

static const char *const g_error_messages[] = {
    [JSONRPC_PARSE_ERROR + 32700] = "Parse error",
    [JSONRPC_INVALID_REQUEST + 32700] = "Invalid Request",
    [JSONRPC_METHOD_NOT_FOUND + 32700] = "Method not found",
    [JSONRPC_INVALID_PARAMS + 32700] = "Invalid params",
    [JSONRPC_INTERNAL_ERROR + 32700] = "Internal error",
};

static const char *const g_custom_error_messages[] = {
    "Rate limit exceeded", "Authentication failed", "Session expired", "Service unavailable"};

int jsonrpc_validate_request(const cJSON *json)
{
#ifdef AGENTOS_HAS_CJSON
    AGENTOS_CHECK(json != NULL, AGENTOS_EFAIL, "json is NULL");

    if (!cJSON_HasObjectItem(json, "jsonrpc") || !cJSON_HasObjectItem(json, "method") ||
        !cJSON_HasObjectItem(json, "id")) {
        AGENTOS_ERROR(AGENTOS_EFAIL, "missing required JSON-RPC fields");
    }

    const cJSON *jsonrpc = cJSON_GetObjectItemCaseSensitive(json, "jsonrpc");
    const cJSON *method = cJSON_GetObjectItemCaseSensitive(json, "method");
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");

    if (!cJSON_IsString(jsonrpc)) {
        AGENTOS_ERROR(-2, "jsonrpc field is not a string");
    }
    if (strcmp(jsonrpc->valuestring, "2.0") != 0) {
        AGENTOS_ERROR(-3, "jsonrpc version is not 2.0");
    }

    if (!cJSON_IsString(method)) {
        AGENTOS_ERROR(-2, "method field is not a string");
    }
    if (strlen(method->valuestring) == 0) {
        AGENTOS_ERROR(AGENTOS_EFAIL, "method is empty");
    }

    if (!cJSON_IsNumber(id) && !cJSON_IsString(id) && !cJSON_IsNull(id)) {
        AGENTOS_ERROR(-2, "id field has invalid type");
    }

    return 0;
#else
    (void)json;
    AGENTOS_ERROR(AGENTOS_EFAIL, "cJSON not available");
#endif
}

const char *jsonrpc_get_method(const cJSON *json)
{
#ifdef AGENTOS_HAS_CJSON
    if (!json) {
        return NULL;
    }
    const cJSON *method = cJSON_GetObjectItemCaseSensitive(json, "method");
    if (!cJSON_IsString(method)) {
        return NULL;
    }
    return method->valuestring;
#else
    (void)json;
    return NULL;
#endif
}

const cJSON *jsonrpc_get_params(const cJSON *json)
{
#ifdef AGENTOS_HAS_CJSON
    if (!json) {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive(json, "params");
#else
    (void)json;
    return NULL;
#endif
}

const cJSON *jsonrpc_get_id(const cJSON *json)
{
#ifdef AGENTOS_HAS_CJSON
    if (!json) {
        return NULL;
    }
    return cJSON_GetObjectItemCaseSensitive(json, "id");
#else
    (void)json;
    return NULL;
#endif
}

char *jsonrpc_create_success_response(const cJSON *id, cJSON *result)
{
#ifdef AGENTOS_HAS_CJSON
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        if (result)
            cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");

    if (result) {
        cJSON_AddItemToObject(response, "result", result);
    } else {
        cJSON_AddNullToObject(response, "result");
    }

    if (id) {
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    } else {
        cJSON_AddNullToObject(response, "id");
    }

    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    return json_str;
#else
    (void)id;
    (void)result;
    return NULL;
#endif
}

char *jsonrpc_create_error_response(const cJSON *id, int code, const char *message, cJSON *data)
{
#ifdef AGENTOS_HAS_CJSON
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        if (data)
            cJSON_Delete(data);
        return NULL;
    }

    cJSON *error = cJSON_CreateObject();
    if (!error) {
        cJSON_Delete(response);
        if (data)
            cJSON_Delete(data);
        return NULL;
    }

    cJSON_AddNumberToObject(error, "code", code);

    const char *msg = message;
    if (!msg) {
        msg = jsonrpc_get_error_message(code);
    }
    cJSON_AddStringToObject(error, "message", msg ? msg : "Internal error");

    if (data) {
        cJSON_AddItemToObject(error, "data", data);
    }

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddItemToObject(response, "error", error);

    if (id) {
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    } else {
        cJSON_AddNullToObject(response, "id");
    }

    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    return json_str;
#else
    (void)id;
    (void)code;
    (void)message;
    (void)data;
    return NULL;
#endif
}

char *jsonrpc_create_parse_error_response(void)
{
    return jsonrpc_create_error_response(NULL, JSONRPC_PARSE_ERROR, NULL, NULL);
}

char *jsonrpc_create_invalid_request_response(void)
{
    return jsonrpc_create_error_response(NULL, JSONRPC_INVALID_REQUEST, NULL, NULL);
}

char *jsonrpc_create_method_not_found_response(const cJSON *id)
{
    return jsonrpc_create_error_response(id, JSONRPC_METHOD_NOT_FOUND, NULL, NULL);
}

char *jsonrpc_create_invalid_params_response(const cJSON *id, const char *detail)
{
#ifdef AGENTOS_HAS_CJSON
    cJSON *data = NULL;
    if (detail) {
        data = cJSON_CreateString(detail);
    }
    return jsonrpc_create_error_response(id, JSONRPC_INVALID_PARAMS, NULL, data);
#else
    (void)id;
    (void)detail;
    return NULL;
#endif
}

char *jsonrpc_create_internal_error_response(const cJSON *id, const char *detail)
{
#ifdef AGENTOS_HAS_CJSON
    cJSON *data = NULL;
    if (detail) {
        data = cJSON_CreateString(detail);
    }
    return jsonrpc_create_error_response(id, JSONRPC_INTERNAL_ERROR, NULL, data);
#else
    (void)id;
    (void)detail;
    return NULL;
#endif
}

char *jsonrpc_create_rate_limited_response(const cJSON *id)
{
    return jsonrpc_create_error_response(id, JSONRPC_RATE_LIMITED, NULL, NULL);
}

char *jsonrpc_create_auth_failed_response(const cJSON *id)
{
    return jsonrpc_create_error_response(id, JSONRPC_AUTH_FAILED, NULL, NULL);
}

const char *jsonrpc_get_error_message(int code)
{
    if (code >= -32700 && code <= -32600) {
        int idx = code + 32700;
        if (idx >= 0 && idx < (int)(sizeof(g_error_messages) / sizeof(g_error_messages[0]))) {
            return g_error_messages[idx];
        }
    }

    int cidx = -1;
    switch (code) {
    case JSONRPC_RATE_LIMITED:
        cidx = 0;
        break;
    case JSONRPC_AUTH_FAILED:
        cidx = 1;
        break;
    case JSONRPC_SESSION_EXPIRED:
        cidx = 2;
        break;
    case JSONRPC_SERVICE_UNAVAILABLE:
        cidx = 3;
        break;
    }
    if (cidx >= 0 &&
        cidx < (int)(sizeof(g_custom_error_messages) / sizeof(g_custom_error_messages[0]))) {
        return g_custom_error_messages[cidx];
    }

    return "Unknown error";
}

int jsonrpc_validate_batch_request(const cJSON *batch_json, size_t *out_count)
{
#ifdef AGENTOS_HAS_CJSON
    AGENTOS_CHECK(batch_json != NULL, AGENTOS_EFAIL, "batch_json is NULL");
    AGENTOS_CHECK(out_count != NULL, AGENTOS_EFAIL, "out_count is NULL");
    *out_count = 0;

    AGENTOS_CHECK(cJSON_IsArray(batch_json), AGENTOS_EFAIL, "batch_json is not an array");

    size_t count = cJSON_GetArraySize(batch_json);
    AGENTOS_CHECK(count > 0, AGENTOS_EFAIL, "batch is empty");
    if (count > JSONRPC_MAX_BATCH_SIZE)
        AGENTOS_ERROR(-3, "batch exceeds max size");

    int has_invalid = 0;
    for (size_t i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(batch_json, (int)i);
        if (!cJSON_IsObject(item)) {
            has_invalid = 1;
            continue;
        }
        (*out_count)++;
    }

    return has_invalid ? -4 : 0;
#else
    (void)batch_json;
    (void)out_count;
    AGENTOS_ERROR(AGENTOS_EFAIL, "cJSON not available");
#endif
}

char *jsonrpc_process_batch(const cJSON *batch_json,
                            char *(*handler)(const cJSON *request, void *user_data),
                            void *user_data)
{
#ifdef AGENTOS_HAS_CJSON
    if (!batch_json || !handler || !cJSON_IsArray(batch_json)) {
        return NULL;
    }

    size_t count = (size_t)cJSON_GetArraySize(batch_json);
    if (count > JSONRPC_MAX_BATCH_SIZE)
        count = JSONRPC_MAX_BATCH_SIZE;

    cJSON *responses = cJSON_CreateArray();
    if (!responses)
        return NULL;

    for (size_t i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(batch_json, (int)i);

        if (!cJSON_IsObject(item)) {
            char *err_resp = jsonrpc_create_invalid_request_response();
            if (err_resp) {
                cJSON *parsed = cJSON_Parse(err_resp);
                if (parsed) {
                    cJSON_AddItemToArray(responses, parsed);
                }
                AGENTOS_FREE(err_resp);
            }
            continue;
        }

        if (jsonrpc_is_notification(item)) {
            continue;
        }

        int valid = jsonrpc_validate_request(item);
        if (valid != 0) {
            (void)jsonrpc_get_id(item);
            char *err_resp = NULL;
            switch (valid) {
            case -3:
                err_resp = jsonrpc_create_parse_error_response();
                break;
            case -2:
                err_resp = jsonrpc_create_invalid_request_response();
                break;
            default:
                err_resp = jsonrpc_create_invalid_request_response();
                break;
            }
            if (err_resp) {
                cJSON *parsed = cJSON_Parse(err_resp);
                if (parsed) {
                    cJSON_AddItemToArray(responses, parsed);
                }
                AGENTOS_FREE(err_resp);
            }
            continue;
        }

        char *resp_str = handler(item, user_data);
        if (resp_str) {
            cJSON *resp_parsed = cJSON_Parse(resp_str);
            if (resp_parsed) {
                cJSON_AddItemToArray(responses, resp_parsed);
            } else {
                const cJSON *id = jsonrpc_get_id(item);
                cJSON *err_parsed = cJSON_Parse(
                    jsonrpc_create_internal_error_response(id, "Handler returned invalid JSON"));
                if (err_parsed) {
                    cJSON_AddItemToArray(responses, err_parsed);
                }
            }
            AGENTOS_FREE(resp_str);
        } else {
            const cJSON *id = jsonrpc_get_id(item);
            char *err_resp = jsonrpc_create_internal_error_response(id, "Handler returned NULL");
            if (err_resp) {
                cJSON *parsed = cJSON_Parse(err_resp);
                if (parsed) {
                    cJSON_AddItemToArray(responses, parsed);
                }
                AGENTOS_FREE(err_resp);
            }
        }
    }

    char *result = cJSON_PrintUnformatted(responses);
    cJSON_Delete(responses);
    return result;
#else
    (void)batch_json;
    (void)handler;
    (void)user_data;
    return NULL;
#endif
}

char *jsonrpc_create_notification(const char *method, cJSON *params)
{
#ifdef AGENTOS_HAS_CJSON
    if (!method || strlen(method) == 0)
        return NULL;

    cJSON *notif = cJSON_CreateObject();
    if (!notif) {
        if (params)
            cJSON_Delete(params);
        return NULL;
    }

    cJSON_AddStringToObject(notif, "jsonrpc", "2.0");
    cJSON_AddStringToObject(notif, "method", method);

    if (params) {
        cJSON_AddItemToObject(notif, "params", params);
    }

    char *json_str = cJSON_PrintUnformatted(notif);
    cJSON_Delete(notif);

    return json_str;
#else
    (void)method;
    (void)params;
    return NULL;
#endif
}

bool jsonrpc_is_notification(const cJSON *json)
{
#ifdef AGENTOS_HAS_CJSON
    if (!json || !cJSON_IsObject(json))
        return false;

    return !cJSON_HasObjectItem(json, "id");
#else
    (void)json;
    return false;
#endif
}

char *jsonrpc_create_notification_params(const char *method, const char *params_json)
{
#ifdef AGENTOS_HAS_CJSON
    cJSON *params = NULL;
    if (params_json && strlen(params_json) > 0) {
        params = cJSON_Parse(params_json);
        if (!params)
            return NULL;
    }

    return jsonrpc_create_notification(method, params);
#else
    (void)method;
    (void)params_json;
    return NULL;
#endif
}
