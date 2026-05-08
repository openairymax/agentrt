/**
 * @file builtin_index.c
 * @brief AgentOS 内置哈希索引实现（无 FAISS 依赖）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供基于哈希表的记录索引和简单的 TF-IDF 文本索引。
 * 支持精确匹配和关键词检索。
 */

#include "memory_provider.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define HASH_TABLE_SIZE 4096
#define MAX_TOKENS_PER_DOC 256
#define MAX_TOKEN_LEN 64

typedef struct index_entry {
    char record_id[64];
    char metadata_json[1024];
    size_t data_len;
    struct index_entry* next;
} index_entry_t;

typedef struct token_entry {
    char token[MAX_TOKEN_LEN];
    char** record_ids;
    size_t record_count;
    size_t record_capacity;
    struct token_entry* next;
} token_entry_t;

typedef struct builtin_index {
    index_entry_t* id_table[HASH_TABLE_SIZE];
    token_entry_t* token_table[HASH_TABLE_SIZE];
    size_t total_docs;
} builtin_index_t;

static uint32_t hash_str(const char* s) {
    uint32_t h = 5381;
    while (*s) {
        h = ((h << 5) + h) + (unsigned char)*s;
        s++;
    }
    return h % HASH_TABLE_SIZE;
}

builtin_index_t* builtin_index_create(void) {
    builtin_index_t* idx = (builtin_index_t*)calloc(1, sizeof(builtin_index_t));
    return idx;
}

void builtin_index_destroy(builtin_index_t* idx) {
    if (!idx) return;

    for (size_t i = 0; i < HASH_TABLE_SIZE; i++) {
        index_entry_t* e = idx->id_table[i];
        while (e) {
            index_entry_t* next = e->next;
            free(e);
            e = next;
        }

        token_entry_t* t = idx->token_table[i];
        while (t) {
            token_entry_t* next = t->next;
            if (t->record_ids) {
                for (size_t j = 0; j < t->record_count; j++) {
                    free(t->record_ids[j]);
                }
                free(t->record_ids);
            }
            free(t);
            t = next;
        }
    }

    free(idx);
}

agentos_error_t builtin_index_add(
    builtin_index_t* idx,
    const char* record_id,
    const char* metadata_json,
    size_t data_len) {

    if (!idx || !record_id) return AGENTOS_EINVAL;

    uint32_t h = hash_str(record_id);
    index_entry_t* entry = (index_entry_t*)calloc(1, sizeof(index_entry_t));
    if (!entry) return AGENTOS_ENOMEM;

    snprintf(entry->record_id, sizeof(entry->record_id), "%s", record_id);
    if (metadata_json) {
        snprintf(entry->metadata_json, sizeof(entry->metadata_json), "%s", metadata_json);
    }
    entry->data_len = data_len;
    entry->next = idx->id_table[h];
    idx->id_table[h] = entry;
    idx->total_docs++;

    if (metadata_json && metadata_json[0]) {
        const char* p = metadata_json;
        char token[MAX_TOKEN_LEN];
        int ti = 0;

        while (*p) {
            if (isalnum((unsigned char)*p) || *p == '_') {
                if (ti < MAX_TOKEN_LEN - 1) {
                    token[ti++] = (char)tolower((unsigned char)*p);
                }
            } else {
                if (ti > 0) {
                    token[ti] = '\0';
                    if (strlen(token) >= 2) {
                        uint32_t th = hash_str(token);
                        token_entry_t* te = idx->token_table[th];
                        while (te) {
                            if (strcmp(te->token, token) == 0) break;
                            te = te->next;
                        }

                        if (!te) {
                            te = (token_entry_t*)calloc(1, sizeof(token_entry_t));
                            if (te) {
                                snprintf(te->token, sizeof(te->token), "%s", token);
                                te->record_capacity = 16;
                                te->record_ids = (char**)calloc(te->record_capacity, sizeof(char*));
                                te->next = idx->token_table[th];
                                idx->token_table[th] = te;
                            }
                        }

                        if (te) {
                            if (te->record_count >= te->record_capacity) {
                                te->record_capacity *= 2;
                                char** tmp = (char**)realloc(te->record_ids, te->record_capacity * sizeof(char*));
                                if (tmp) te->record_ids = tmp;
                            }
                            if (te->record_count < te->record_capacity) {
                                te->record_ids[te->record_count++] = strdup(record_id);
                            }
                        }
                    }
                    ti = 0;
                }
            }
            p++;
        }

        if (ti > 0) {
            token[ti] = '\0';
            if (strlen(token) >= 2) {
                uint32_t th = hash_str(token);
                token_entry_t* te = idx->token_table[th];
                while (te) {
                    if (strcmp(te->token, token) == 0) break;
                    te = te->next;
                }
                if (te && te->record_count < te->record_capacity) {
                    te->record_ids[te->record_count++] = strdup(record_id);
                }
            }
        }
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t builtin_index_remove(builtin_index_t* idx, const char* record_id) {
    if (!idx || !record_id) return AGENTOS_EINVAL;

    uint32_t h = hash_str(record_id);
    index_entry_t** pp = &idx->id_table[h];
    while (*pp) {
        if (strcmp((*pp)->record_id, record_id) == 0) {
            index_entry_t* del = *pp;
            *pp = del->next;
            free(del);
            idx->total_docs--;
            return AGENTOS_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    return AGENTOS_ENOENT;
}

agentos_error_t builtin_index_lookup(
    const builtin_index_t* idx,
    const char* record_id,
    int* out_found) {

    if (!idx || !record_id || !out_found) return AGENTOS_EINVAL;

    *out_found = 0;
    uint32_t h = hash_str(record_id);
    index_entry_t* e = idx->id_table[h];
    while (e) {
        if (strcmp(e->record_id, record_id) == 0) {
            *out_found = 1;
            return AGENTOS_SUCCESS;
        }
        e = e->next;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t builtin_index_search(
    const builtin_index_t* idx,
    const char* query,
    uint32_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count) {

    if (!idx || !query || !out_record_ids || !out_scores || !out_count)
        return AGENTOS_EINVAL;

    *out_record_ids = NULL;
    *out_scores = NULL;
    *out_count = 0;

    if (idx->total_docs == 0) return AGENTOS_SUCCESS;

    size_t result_cap = (limit > 0 && limit < idx->total_docs) ? limit : idx->total_docs;
    if (result_cap == 0) result_cap = 16;

    char** results = (char**)calloc(result_cap, sizeof(char*));
    float* scores = (float*)calloc(result_cap, sizeof(float));
    size_t result_count = 0;

    char token[MAX_TOKEN_LEN];
    int ti = 0;
    const char* p = query;

    while (*p && result_count < result_cap) {
        if (isalnum((unsigned char)*p) || *p == '_') {
            if (ti < MAX_TOKEN_LEN - 1) {
                token[ti++] = (char)tolower((unsigned char)*p);
            }
        } else {
            if (ti > 0) {
                token[ti] = '\0';
                if (strlen(token) >= 2) {
                    uint32_t th = hash_str(token);
                    token_entry_t* te = idx->token_table[th];
                    while (te) {
                        if (strcmp(te->token, token) == 0) break;
                        te = te->next;
                    }
                    if (te) {
                        for (size_t i = 0; i < te->record_count && result_count < result_cap; i++) {
                            int already = 0;
                            for (size_t j = 0; j < result_count; j++) {
                                if (strcmp(results[j], te->record_ids[i]) == 0) {
                                    scores[j] += 1.0f;
                                    already = 1;
                                    break;
                                }
                            }
                            if (!already) {
                                results[result_count] = strdup(te->record_ids[i]);
                                scores[result_count] = 1.0f;
                                result_count++;
                            }
                        }
                    }
                }
                ti = 0;
            }
        }
        p++;
    }

    if (ti > 0 && result_count < result_cap) {
        token[ti] = '\0';
        if (strlen(token) >= 2) {
            uint32_t th = hash_str(token);
            token_entry_t* te = idx->token_table[th];
            while (te) {
                if (strcmp(te->token, token) == 0) break;
                te = te->next;
            }
            if (te) {
                for (size_t i = 0; i < te->record_count && result_count < result_cap; i++) {
                    int already = 0;
                    for (size_t j = 0; j < result_count; j++) {
                        if (strcmp(results[j], te->record_ids[i]) == 0) {
                            scores[j] += 1.0f;
                            already = 1;
                            break;
                        }
                    }
                    if (!already) {
                        results[result_count] = strdup(te->record_ids[i]);
                        scores[result_count] = 1.0f;
                        result_count++;
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < result_count; i++) {
        for (size_t j = i + 1; j < result_count; j++) {
            if (scores[j] > scores[i]) {
                float ts = scores[i]; scores[i] = scores[j]; scores[j] = ts;
                char* tr = results[i]; results[i] = results[j]; results[j] = tr;
            }
        }
    }

    *out_record_ids = results;
    *out_scores = scores;
    *out_count = result_count;
    return AGENTOS_SUCCESS;
}

size_t builtin_index_total_docs(const builtin_index_t* idx) {
    return idx ? idx->total_docs : 0;
}

agentos_error_t builtin_index_compact(builtin_index_t* idx) {
    if (!idx) return AGENTOS_EINVAL;

    for (size_t b = 0; b < HASH_TABLE_SIZE; b++) {
        index_entry_t** prev = &idx->id_table[b];
        index_entry_t* cur = idx->id_table[b];
        while (cur) {
            if (cur->record_id[0] == '\0') {
                *prev = cur->next;
                index_entry_t* dead = cur;
                cur = cur->next;
                free(dead);
            } else {
                prev = &cur->next;
                cur = cur->next;
            }
        }
    }

    for (size_t b = 0; b < HASH_TABLE_SIZE; b++) {
        token_entry_t** prev = &idx->token_table[b];
        token_entry_t* cur = idx->token_table[b];
        while (cur) {
            if (cur->record_count == 0) {
                *prev = cur->next;
                token_entry_t* dead = cur;
                cur = cur->next;
                free(dead->record_ids);
                free(dead);
            } else {
                prev = &cur->next;
                cur = cur->next;
            }
        }
    }

    return AGENTOS_SUCCESS;
}
