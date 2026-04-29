/**
 * @file builtin_retrieval.c
 * @brief AgentOS 内置检索实现（BM25 + 余弦相似度）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供基于 BM25 的文本检索和基于余弦相似度的向量检索。
 * 无 FAISS 依赖，纯 C 实现。
 */

#include "memory_provider.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define BM25_K1 1.2
#define BM25_B 0.75
#define MAX_QUERY_TOKENS 64
#define MAX_TOKEN_LEN 64

typedef struct doc_freq {
    char token[MAX_TOKEN_LEN];
    size_t df;
    struct doc_freq* next;
} doc_freq_t;

typedef struct builtin_retrieval {
    doc_freq_t* freq_table[1024];
    size_t total_docs;
    double avg_dl;
} builtin_retrieval_t;

static uint32_t simple_hash(const char* s) {
    uint32_t h = 5381;
    while (*s) { h = ((h << 5) + h) + (unsigned char)*s; s++; }
    return h % 1024;
}

builtin_retrieval_t* builtin_retrieval_create(void) {
    builtin_retrieval_t* ret = (builtin_retrieval_t*)calloc(1, sizeof(builtin_retrieval_t));
    return ret;
}

void builtin_retrieval_destroy(builtin_retrieval_t* ret) {
    if (!ret) return;
    for (size_t i = 0; i < 1024; i++) {
        doc_freq_t* df = ret->freq_table[i];
        while (df) {
            doc_freq_t* next = df->next;
            free(df);
            df = next;
        }
    }
    free(ret);
}

void builtin_retrieval_update_stats(
    builtin_retrieval_t* ret,
    size_t total_docs,
    double avg_doc_length) {
    if (!ret) return;
    ret->total_docs = total_docs;
    ret->avg_dl = avg_doc_length > 0 ? avg_doc_length : 1.0;
}

static size_t count_token_in_text(const char* text, const char* token) {
    size_t count = 0;
    size_t tlen = strlen(token);
    const char* p = text;
    while (*p) {
        if (strncasecmp(p, token, tlen) == 0) {
            const char before = (p > text) ? *(p - 1) : ' ';
            const char after = *(p + tlen);
            if (!isalnum((unsigned char)before) && !isalnum((unsigned char)after)) {
                count++;
            }
        }
        p++;
    }
    return count;
}

float builtin_retrieval_bm25_score(
    builtin_retrieval_t* ret,
    const char* query_tokens[],
    size_t query_token_count,
    const char* doc_text,
    size_t doc_length) {

    if (!ret || !query_tokens || !doc_text) return 0.0f;

    float score = 0.0f;
    double dl = (double)doc_length;
    double N = (double)ret->total_docs > 0 ? ret->total_docs : 1;
    double avgdl = ret->avg_dl;

    for (size_t i = 0; i < query_token_count; i++) {
        if (!query_tokens[i]) continue;

        size_t tf = count_token_in_text(doc_text, query_tokens[i]);

        uint32_t h = simple_hash(query_tokens[i]);
        doc_freq_t* df = ret->freq_table[h];
        size_t doc_freq = 1;
        while (df) {
            if (strcmp(df->token, query_tokens[i]) == 0) {
                doc_freq = df->df;
                break;
            }
            df = df->next;
        }

        double idf = log((N - (double)doc_freq + 0.5) / ((double)doc_freq + 0.5) + 1.0);
        double tf_norm = ((double)tf * (BM25_K1 + 1.0)) /
            ((double)tf + BM25_K1 * (1.0 - BM25_B + BM25_B * dl / avgdl));

        score += (float)(idf * tf_norm);
    }

    return score > 0 ? score : 0.0f;
}

float builtin_retrieval_cosine_similarity(
    const float* vec_a,
    const float* vec_b,
    size_t dim) {

    if (!vec_a || !vec_b || dim == 0) return 0.0f;

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        dot += vec_a[i] * vec_b[i];
        norm_a += vec_a[i] * vec_a[i];
        norm_b += vec_b[i] * vec_b[i];
    }

    if (norm_a < 1e-8f || norm_b < 1e-8f) return 0.0f;
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

size_t builtin_retrieval_tokenize(
    const char* text,
    char tokens[][MAX_TOKEN_LEN],
    size_t max_tokens) {

    if (!text || !tokens || max_tokens == 0) return 0;

    size_t count = 0;
    int ti = 0;
    const char* p = text;

    while (*p && count < max_tokens) {
        if (isalnum((unsigned char)*p) || *p == '_') {
            if (ti < MAX_TOKEN_LEN - 1) {
                tokens[count][ti++] = (char)tolower((unsigned char)*p);
            }
        } else {
            if (ti >= 2) {
                tokens[count][ti] = '\0';
                count++;
            }
            ti = 0;
        }
        p++;
    }

    if (ti >= 2 && count < max_tokens) {
        tokens[count][ti] = '\0';
        count++;
    }

    return count;
}
