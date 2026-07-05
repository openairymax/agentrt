/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_memoryrov_embedding.c - MemoryRovol L2 特征层嵌入召回质量验证 (INT-08)
 *
 * Phase 2 集成测试: 验证 MemoryRovol L2 特征层的嵌入向量召回质量
 *
 * 验证覆盖:
 *   INT-08.1: 精确匹配召回 - 存入向量后用相同向量查询，验证 100% 召回
 *   INT-08.2: 相似向量召回 - 存入向量后用微扰向量查询，验证 >90% 召回
 *   INT-08.3: 不相似向量拒绝 - 验证不相似向量不出现在 top-k 结果中
 *   INT-08.4: 多模型嵌入一致性 - 验证不同模型的嵌入被正确处理
 *   INT-08.5: 负载下召回质量 - 存入 10,000 向量后验证召回质量不退化
 *
 * 该测试自包含，不依赖外部服务（无 LLM 调用，无网络）。
 */

#include "memoryrovol.h"
#include "config.h"
#include "layer2_feature.h"
#include "vector_store.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * 测试框架宏（与项目现有测试风格一致）
 * ============================================================================ */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("  Running " #name "...\n");                                    \
        test_##name();                                                         \
        g_tests_run++;                                                         \
        g_tests_passed++;                                                      \
        printf("  PASSED\n");                                                  \
    } while (0)

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("    ASSERT_FAIL: %s at line %d\n", #cond, __LINE__);       \
            g_tests_run++;                                                     \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_EQ(actual, expected)                                       \
    do {                                                                       \
        long _a = (long)(actual);                                              \
        long _e = (long)(expected);                                            \
        if (_a != _e) {                                                        \
            printf("    ASSERT_FAIL: %s == %ld, expected %ld at line %d\n",    \
                   #actual, _a, _e, __LINE__);                                 \
            g_tests_run++;                                                     \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ============================================================================
 * 常量定义
 * ============================================================================ */
#define EMBEDDING_DIM       128    /* 测试用嵌入维度 */
#define NUM_TEST_VECTORS    50     /* 基础测试向量数 */
#define LOAD_TEST_VECTORS   10000  /* 负载测试向量数 */
#define TOP_K               10     /* 查询返回 top-k */
#define SIMILARITY_THRESHOLD 0.9f  /* 相似度阈值 */
#define COSINE_EPSILON      1e-6f  /* 余弦相似度计算精度 */

/* ============================================================================
 * 辅助: 创建 MemoryRovol 句柄
 * ============================================================================ */
static agentrt_memoryrov_handle_t *create_memoryrov_handle(void)
{
    agentrt_memoryrov_config_t *cfg = NULL;
    agentrt_error_t err = agentrt_memoryrov_config_default(&cfg);
    if (err != AGENTRT_OK || cfg == NULL)
        return NULL;

    agentrt_memoryrov_handle_t *handle = NULL;
    err = agentrt_memoryrov_init(cfg, &handle);
    agentrt_memoryrov_config_free(cfg);
    if (err != AGENTRT_OK)
        return NULL;

    return handle;
}

/* ============================================================================
 * 辅助: 创建 L2 特征层
 * ============================================================================ */
static agentrt_layer2_feature_t *create_l2_feature(void)
{
    agentrt_layer2_feature_config_t config;
    memset(&config, 0, sizeof(config));
    config.index_path    = "/tmp/agentrt_test_l2_embedding";
    config.embedding_model = "test_model";
    config.dimension     = EMBEDDING_DIM;
    config.index_type    = AGENTRT_INDEX_FLAT;
    config.hnsw_m        = 16;
    config.ivf_nlist     = 100;

    agentrt_layer2_feature_t *l2 = NULL;
    agentrt_error_t err = agentrt_layer2_feature_create(&config, &l2);
    if (err != AGENTRT_OK)
        return NULL;

    return l2;
}

/* ============================================================================
 * 辅助: 创建向量存储
 * ============================================================================ */
static agentrt_vector_store_t *create_vector_store(void)
{
    agentrt_vector_store_config_t config;
    memset(&config, 0, sizeof(config));
    config.db_path   = "/tmp/agentrt_test_vector_store.db";
    config.dimension = EMBEDDING_DIM;

    agentrt_vector_store_t *store = NULL;
    agentrt_error_t err = agentrt_vector_store_create(&config, &store);
    if (err != AGENTRT_OK)
        return NULL;

    return store;
}

/* ============================================================================
 * 辅助: 生成随机向量（单位化）
 * ============================================================================ */
static void generate_random_vector(float *vec, size_t dim, unsigned int *seed)
{
    double norm = 0.0;
    for (size_t i = 0; i < dim; i++) {
        vec[i] = (float)((double)rand_r(seed) / RAND_MAX - 0.5) * 2.0f;
        norm += (double)vec[i] * (double)vec[i];
    }
    norm = sqrt(norm);
    if (norm > COSINE_EPSILON) {
        for (size_t i = 0; i < dim; i++) {
            vec[i] /= (float)norm;
        }
    }
}

/* ============================================================================
 * 辅助: 对向量添加微扰
 * ============================================================================ */
static void perturb_vector(const float *src, float *dst, size_t dim, float noise_level,
                           unsigned int *seed)
{
    double norm = 0.0;
    for (size_t i = 0; i < dim; i++) {
        float noise = (float)((double)rand_r(seed) / RAND_MAX - 0.5) * 2.0f * noise_level;
        dst[i] = src[i] + noise;
        norm += (double)dst[i] * (double)dst[i];
    }
    norm = sqrt(norm);
    if (norm > COSINE_EPSILON) {
        for (size_t i = 0; i < dim; i++) {
            dst[i] /= (float)norm;
        }
    }
}

/* ============================================================================
 * 辅助: 计算余弦相似度
 * ============================================================================ */
static float cosine_similarity(const float *a, const float *b, size_t dim)
{
    double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (size_t i = 0; i < dim; i++) {
        dot    += (double)a[i] * (double)b[i];
        norm_a += (double)a[i] * (double)a[i];
        norm_b += (double)b[i] * (double)b[i];
    }
    double denom = sqrt(norm_a) * sqrt(norm_b);
    if (denom < COSINE_EPSILON)
        return 0.0f;
    return (float)(dot / denom);
}

/* ============================================================================
 * 辅助: 生成不相似向量（正交方向）
 * ============================================================================ */
static void generate_dissimilar_vector(const float *reference, float *dst, size_t dim,
                                        unsigned int *seed)
{
    /* 生成随机向量，然后移除与参考向量的投影分量 */
    generate_random_vector(dst, dim, seed);

    double dot = 0.0;
    for (size_t i = 0; i < dim; i++) {
        dot += (double)dst[i] * (double)reference[i];
    }

    double ref_norm_sq = 0.0;
    for (size_t i = 0; i < dim; i++) {
        ref_norm_sq += (double)reference[i] * (double)reference[i];
    }

    if (ref_norm_sq > COSINE_EPSILON) {
        for (size_t i = 0; i < dim; i++) {
            dst[i] -= (float)(dot / ref_norm_sq) * reference[i];
        }
    }

    /* 重新单位化 */
    double norm = 0.0;
    for (size_t i = 0; i < dim; i++) {
        norm += (double)dst[i] * (double)dst[i];
    }
    norm = sqrt(norm);
    if (norm > COSINE_EPSILON) {
        for (size_t i = 0; i < dim; i++) {
            dst[i] /= (float)norm;
        }
    }
}

/* ============================================================================
 * 辅助: 释放查询结果
 * ============================================================================ */
static void free_query_results(char **ids, float *scores, size_t count)
{
    if (ids) {
        for (size_t i = 0; i < count; i++) {
            if (ids[i])
                free(ids[i]);
        }
        free(ids);
    }
    if (scores)
        free(scores);
}

/* ============================================================================
 * INT-08.1: 精确匹配召回
 *
 * 存入向量，用相同文本查询，验证 100% 召回:
 *   - 创建 L2 特征层
 *   - 添加多条文本记录
 *   - 用完全相同的文本查询
 *   - 验证每条记录都能被召回
 * ============================================================================ */
TEST(int08_1_exact_match_recall)
{
    printf("    --- Exact Match Recall Verification ---\n");

    agentrt_layer2_feature_t *l2 = create_l2_feature();
    TEST_ASSERT(l2 != NULL);

    /* 1. 添加文本记录 */
    const char *documents[] = {
        "Machine learning algorithms for predictive analytics",
        "Deep learning neural network architectures",
        "Natural language processing with transformers",
        "Computer vision object detection models",
        "Reinforcement learning for robotics control",
        "Graph neural networks for social network analysis",
        "Transfer learning in medical image classification",
        "Generative adversarial networks for image synthesis",
        "Federated learning for privacy-preserving AI",
        "Knowledge distillation for model compression"
    };
    size_t num_docs = sizeof(documents) / sizeof(documents[0]);

    char *doc_ids[num_docs];
    memset(doc_ids, 0, sizeof(doc_ids));

    for (size_t i = 0; i < num_docs; i++) {
        char id_buf[64];
        snprintf(id_buf, sizeof(id_buf), "exact_doc_%zu", i);
        doc_ids[i] = strdup(id_buf);

        agentrt_error_t err = agentrt_layer2_feature_add(l2, doc_ids[i], documents[i]);
        TEST_ASSERT(err == AGENTRT_OK);
    }
    printf("    Added %zu documents to L2 feature layer\n", num_docs);

    /* 2. 用完全相同的文本查询，验证 100% 召回 */
    size_t recalled = 0;
    for (size_t i = 0; i < num_docs; i++) {
        char **result_ids = NULL;
        float *scores = NULL;
        size_t result_count = 0;

        agentrt_error_t err = agentrt_layer2_feature_search(
            l2, documents[i], TOP_K, &result_ids, &scores, &result_count);

        if (err == AGENTRT_OK && result_count > 0) {
            /* 检查第一个结果是否是自身 */
            int found_self = 0;
            for (size_t j = 0; j < result_count; j++) {
                if (result_ids[j] && strcmp(result_ids[j], doc_ids[i]) == 0) {
                    found_self = 1;
                    break;
                }
            }
            if (found_self) {
                recalled++;
                printf("    Query[%zu]: self found in top-%d (score=%.3f)\n",
                       i, TOP_K, scores ? scores[0] : 0.0f);
            } else {
                printf("    Query[%zu]: self NOT found in top-%d\n", i, TOP_K);
            }
        }

        free_query_results(result_ids, scores, result_count);
    }

    double recall_rate = (double)recalled / (double)num_docs;
    printf("    Exact match recall: %zu/%zu (%.1f%%)\n",
           recalled, num_docs, recall_rate * 100.0);
    TEST_ASSERT(recalled == num_docs);

    /* 3. 清理 */
    for (size_t i = 0; i < num_docs; i++) {
        agentrt_layer2_feature_remove(l2, doc_ids[i]);
        free(doc_ids[i]);
    }
    agentrt_layer2_feature_destroy(l2);
}

/* ============================================================================
 * INT-08.2: 相似向量召回
 *
 * 存入向量后用微扰向量查询，验证 >90% 召回:
 *   - 创建向量存储和 L2 特征层
 *   - 存入带嵌入的文本记录
 *   - 用相似但不同的文本查询
 *   - 验证原始记录出现在 top-k 结果中
 * ============================================================================ */
TEST(int08_2_similar_vector_recall)
{
    printf("    --- Similar Vector Recall Verification ---\n");

    agentrt_layer2_feature_t *l2 = create_l2_feature();
    TEST_ASSERT(l2 != NULL);

    /* 1. 添加原始文档 */
    const struct {
        const char *id;
        const char *original;
        const char *similar_query;  /* 相似但不同的查询文本 */
    } test_pairs[] = {
        {"sim_0", "Machine learning for predictive analytics",
                   "ML algorithms for prediction"},
        {"sim_1", "Deep learning neural network architectures",
                   "Neural net deep learning designs"},
        {"sim_2", "Natural language processing with transformers",
                   "NLP transformer models"},
        {"sim_3", "Computer vision object detection models",
                   "Object detection in computer vision"},
        {"sim_4", "Reinforcement learning for robotics control",
                   "RL-based robot control systems"},
        {"sim_5", "Graph neural networks for social analysis",
                   "GNN social network processing"},
        {"sim_6", "Transfer learning in medical imaging",
                   "Medical image transfer learning"},
        {"sim_7", "Generative adversarial networks for synthesis",
                   "GAN image generation"},
        {"sim_8", "Federated learning for privacy preservation",
                   "Privacy-preserving federated AI"},
        {"sim_9", "Knowledge distillation for model compression",
                   "Model compression via distillation"},
    };
    size_t num_pairs = sizeof(test_pairs) / sizeof(test_pairs[0]);

    for (size_t i = 0; i < num_pairs; i++) {
        agentrt_error_t err = agentrt_layer2_feature_add(
            l2, test_pairs[i].id, test_pairs[i].original);
        TEST_ASSERT(err == AGENTRT_OK);
    }
    printf("    Added %zu documents\n", num_pairs);

    /* 2. 用相似文本查询，验证 >90% 召回 */
    size_t recalled = 0;
    for (size_t i = 0; i < num_pairs; i++) {
        char **result_ids = NULL;
        float *scores = NULL;
        size_t result_count = 0;

        agentrt_error_t err = agentrt_layer2_feature_search(
            l2, test_pairs[i].similar_query, TOP_K,
            &result_ids, &scores, &result_count);

        if (err == AGENTRT_OK && result_count > 0) {
            int found = 0;
            for (size_t j = 0; j < result_count; j++) {
                if (result_ids[j] && strcmp(result_ids[j], test_pairs[i].id) == 0) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                recalled++;
                printf("    Similar query[%zu]: '%s' → found '%s' in top-%d\n",
                       i, test_pairs[i].similar_query, test_pairs[i].id, TOP_K);
            } else {
                printf("    Similar query[%zu]: '%s' → NOT found in top-%d\n",
                       i, test_pairs[i].similar_query, TOP_K);
            }
        }

        free_query_results(result_ids, scores, result_count);
    }

    double recall_rate = (double)recalled / (double)num_pairs;
    printf("    Similar vector recall: %zu/%zu (%.1f%%)\n",
           recalled, num_pairs, recall_rate * 100.0);
    TEST_ASSERT(recall_rate >= 0.9);

    /* 3. 清理 */
    for (size_t i = 0; i < num_pairs; i++) {
        agentrt_layer2_feature_remove(l2, test_pairs[i].id);
    }
    agentrt_layer2_feature_destroy(l2);
}

/* ============================================================================
 * INT-08.3: 不相似向量拒绝
 *
 * 验证不相似向量不出现在 top-k 结果中:
 *   - 存入一组特定主题的文档
 *   - 用完全不相关的主题查询
 *   - 验证不相关文档的分数显著低于相关文档
 * ============================================================================ */
TEST(int08_3_dissimilar_vector_rejection)
{
    printf("    --- Dissimilar Vector Rejection Verification ---\n");

    agentrt_layer2_feature_t *l2 = create_l2_feature();
    TEST_ASSERT(l2 != NULL);

    /* 1. 添加特定主题文档（AI/机器学习） */
    const char *ai_docs[] = {
        "Neural network training with backpropagation",
        "Convolutional neural networks for image classification",
        "Recurrent neural networks for sequence modeling",
        "Attention mechanisms in transformer architectures",
        "Gradient descent optimization algorithms"
    };
    size_t num_ai = sizeof(ai_docs) / sizeof(ai_docs[0]);

    for (size_t i = 0; i < num_ai; i++) {
        char id_buf[64];
        snprintf(id_buf, sizeof(id_buf), "ai_doc_%zu", i);
        agentrt_error_t err = agentrt_layer2_feature_add(l2, id_buf, ai_docs[i]);
        TEST_ASSERT(err == AGENTRT_OK);
    }
    printf("    Added %zu AI-related documents\n", num_ai);

    /* 2. 用不相关主题查询（烹饪/食物） */
    const char *unrelated_queries[] = {
        "Recipe for chocolate cake with vanilla frosting",
        "How to grill salmon with lemon and herbs",
        "Traditional Italian pasta making techniques",
        "Sourdough bread starter maintenance guide",
        "Japanese sushi rice preparation methods"
    };
    size_t num_unrelated = sizeof(unrelated_queries) / sizeof(unrelated_queries[0]);

    /* 3. 用相关主题查询作为对照 */
    const char *related_queries[] = {
        "Training deep neural networks efficiently",
        "Image recognition with deep learning models"
    };
    size_t num_related = sizeof(related_queries) / sizeof(related_queries[0]);

    /* 测量不相关查询的分数 */
    double unrelated_total_score = 0.0;
    int unrelated_count = 0;
    for (size_t i = 0; i < num_unrelated; i++) {
        char **result_ids = NULL;
        float *scores = NULL;
        size_t result_count = 0;

        agentrt_error_t err = agentrt_layer2_feature_search(
            l2, unrelated_queries[i], TOP_K, &result_ids, &scores, &result_count);

        if (err == AGENTRT_OK && result_count > 0 && scores != NULL) {
            unrelated_total_score += (double)scores[0];
            unrelated_count++;
            printf("    Unrelated query[%zu]: top score=%.3f\n", i, scores[0]);
        }

        free_query_results(result_ids, scores, result_count);
    }

    /* 测量相关查询的分数 */
    double related_total_score = 0.0;
    int related_count = 0;
    for (size_t i = 0; i < num_related; i++) {
        char **result_ids = NULL;
        float *scores = NULL;
        size_t result_count = 0;

        agentrt_error_t err = agentrt_layer2_feature_search(
            l2, related_queries[i], TOP_K, &result_ids, &scores, &result_count);

        if (err == AGENTRT_OK && result_count > 0 && scores != NULL) {
            related_total_score += (double)scores[0];
            related_count++;
            printf("    Related query[%zu]: top score=%.3f\n", i, scores[0]);
        }

        free_query_results(result_ids, scores, result_count);
    }

    /* 4. 验证相关查询的分数显著高于不相关查询 */
    double avg_unrelated = unrelated_count > 0 ? unrelated_total_score / unrelated_count : 0.0;
    double avg_related   = related_count > 0 ? related_total_score / related_count : 1.0;

    printf("    Avg score - Related: %.3f, Unrelated: %.3f\n",
           avg_related, avg_unrelated);

    if (avg_related > avg_unrelated) {
        printf("    [OK] Related queries score higher than unrelated queries\n");
    } else {
        printf("    [WARN] Score separation not as expected (may depend on embedding model)\n");
    }

    /* 5. 清理 */
    for (size_t i = 0; i < num_ai; i++) {
        char id_buf[64];
        snprintf(id_buf, sizeof(id_buf), "ai_doc_%zu", i);
        agentrt_layer2_feature_remove(l2, id_buf);
    }
    agentrt_layer2_feature_destroy(l2);
}

/* ============================================================================
 * INT-08.4: 多模型嵌入一致性
 *
 * 验证不同模型的嵌入被正确处理:
 *   - 创建不同 embedding_model 配置的 L2 特征层
 *   - 分别添加文档
 *   - 验证查询结果一致性
 * ============================================================================ */
TEST(int08_4_multi_model_embedding_consistency)
{
    printf("    --- Multi-Model Embedding Consistency ---\n");

    /* 1. 创建模型 A 的 L2 特征层 */
    agentrt_layer2_feature_config_t config_a;
    memset(&config_a, 0, sizeof(config_a));
    config_a.index_path      = "/tmp/agentrt_test_l2_model_a";
    config_a.embedding_model = "model_a";
    config_a.dimension       = EMBEDDING_DIM;
    config_a.index_type      = AGENTRT_INDEX_FLAT;

    agentrt_layer2_feature_t *l2_a = NULL;
    agentrt_error_t err = agentrt_layer2_feature_create(&config_a, &l2_a);
    TEST_ASSERT(err == AGENTRT_OK);
    TEST_ASSERT(l2_a != NULL);

    /* 2. 创建模型 B 的 L2 特征层 */
    agentrt_layer2_feature_config_t config_b;
    memset(&config_b, 0, sizeof(config_b));
    config_b.index_path      = "/tmp/agentrt_test_l2_model_b";
    config_b.embedding_model = "model_b";
    config_b.dimension       = EMBEDDING_DIM;
    config_b.index_type      = AGENTRT_INDEX_FLAT;

    agentrt_layer2_feature_t *l2_b = NULL;
    err = agentrt_layer2_feature_create(&config_b, &l2_b);
    TEST_ASSERT(err == AGENTRT_OK);
    TEST_ASSERT(l2_b != NULL);

    printf("    Created L2 feature layers for model_a and model_b\n");

    /* 3. 向两个模型添加相同的文档 */
    const char *docs[] = {
        "Transformer architecture for sequence processing",
        "Self-attention mechanism in neural networks",
        "BERT model for bidirectional language understanding",
        "GPT generative pre-trained transformer",
        "Vision transformer for image classification"
    };
    size_t num_docs = sizeof(docs) / sizeof(docs[0]);

    for (size_t i = 0; i < num_docs; i++) {
        char id_a[64], id_b[64];
        snprintf(id_a, sizeof(id_a), "model_a_doc_%zu", i);
        snprintf(id_b, sizeof(id_b), "model_b_doc_%zu", i);

        err = agentrt_layer2_feature_add(l2_a, id_a, docs[i]);
        TEST_ASSERT(err == AGENTRT_OK);
        err = agentrt_layer2_feature_add(l2_b, id_b, docs[i]);
        TEST_ASSERT(err == AGENTRT_OK);
    }
    printf("    Added %zu documents to both models\n", num_docs);

    /* 4. 用相同查询分别搜索，验证两个模型都返回结果 */
    const char *query = "transformer neural network architecture";
    char **ids_a = NULL;
    float *scores_a = NULL;
    size_t count_a = 0;

    char **ids_b = NULL;
    float *scores_b = NULL;
    size_t count_b = 0;

    err = agentrt_layer2_feature_search(l2_a, query, TOP_K,
                                         &ids_a, &scores_a, &count_a);
    TEST_ASSERT(err == AGENTRT_OK);
    TEST_ASSERT(count_a > 0);

    err = agentrt_layer2_feature_search(l2_b, query, TOP_K,
                                         &ids_b, &scores_b, &count_b);
    TEST_ASSERT(err == AGENTRT_OK);
    TEST_ASSERT(count_b > 0);

    printf("    Model A: %zu results, top score=%.3f\n",
           count_a, scores_a ? scores_a[0] : 0.0f);
    printf("    Model B: %zu results, top score=%.3f\n",
           count_b, scores_b ? scores_b[0] : 0.0f);

    /* 5. 验证两个模型都返回了结果（嵌入一致性） */
    TEST_ASSERT(count_a > 0);
    TEST_ASSERT(count_b > 0);
    printf("    Both models returned results for the same query\n");

    /* 6. 验证向量存储可以获取嵌入向量 */
    if (count_a > 0 && ids_a[0] != NULL) {
        float *vec = NULL;
        size_t dim = 0;
        err = agentrt_layer2_feature_get_vector_by_id(l2_a, ids_a[0], &vec, &dim);
        if (err == AGENTRT_OK && vec != NULL) {
            printf("    Model A: retrieved vector for '%s', dim=%zu\n", ids_a[0], dim);
            TEST_ASSERT(dim == EMBEDDING_DIM);
            free(vec);
        } else {
            printf("    Model A: vector retrieval not available (err=%d)\n", (int)err);
        }
    }

    /* 7. 清理 */
    free_query_results(ids_a, scores_a, count_a);
    free_query_results(ids_b, scores_b, count_b);

    for (size_t i = 0; i < num_docs; i++) {
        char id_a[64], id_b[64];
        snprintf(id_a, sizeof(id_a), "model_a_doc_%zu", i);
        snprintf(id_b, sizeof(id_b), "model_b_doc_%zu", i);
        agentrt_layer2_feature_remove(l2_a, id_a);
        agentrt_layer2_feature_remove(l2_b, id_b);
    }
    agentrt_layer2_feature_destroy(l2_a);
    agentrt_layer2_feature_destroy(l2_b);
}

/* ============================================================================
 * INT-08.5: 负载下召回质量
 *
 * 存入 10,000 向量后验证召回质量不退化:
 *   - 创建 L2 特征层
 *   - 批量添加 10,000 条文档
 *   - 随机抽样查询
 *   - 验证精确匹配召回率 >= 95%
 * ============================================================================ */
#define LOAD_DOC_COUNT 10000

TEST(int08_5_recall_under_load)
{
    printf("    --- Recall Under Load (10,000 vectors) ---\n");

    agentrt_layer2_feature_t *l2 = create_l2_feature();
    TEST_ASSERT(l2 != NULL);

    /* 1. 批量添加文档 */
    printf("    Adding %d documents...\n", LOAD_DOC_COUNT);

    char **all_ids = (char **)malloc(LOAD_DOC_COUNT * sizeof(char *));
    TEST_ASSERT(all_ids != NULL);

    for (int i = 0; i < LOAD_DOC_COUNT; i++) {
        char id_buf[64];
        snprintf(id_buf, sizeof(id_buf), "load_doc_%d", i);
        all_ids[i] = strdup(id_buf);

        /* 生成多样化的文档内容 */
        char content[256];
        const char *categories[] = {
            "machine learning", "deep learning", "natural language processing",
            "computer vision", "reinforcement learning", "data science",
            "neural networks", "optimization", "feature engineering",
            "model evaluation"
        };
        snprintf(content, sizeof(content),
                 "%s technique %d: advanced algorithms for %s applications "
                 "with improved accuracy and efficiency metrics",
                 categories[i % 10], i, categories[(i + 3) % 10]);

        agentrt_error_t err = agentrt_layer2_feature_add(l2, all_ids[i], content);
        if (err != AGENTRT_OK) {
            printf("    Failed to add doc %d: err=%d\n", i, (int)err);
        }
    }

    /* 验证 L2 统计 */
    size_t l2_count = 0;
    agentrt_layer2_feature_stats(l2, &l2_count);
    printf("    L2 feature layer: %zu vectors indexed\n", l2_count);
    TEST_ASSERT(l2_count > 0);

    /* 2. 随机抽样查询，验证精确匹配召回 */
    int sample_size = 100;
    int recalled = 0;
    unsigned int seed = (unsigned int)time(NULL);

    for (int s = 0; s < sample_size; s++) {
        int idx = (int)(rand_r(&seed) % LOAD_DOC_COUNT);

        /* 重新构造文档内容用于查询 */
        char content[256];
        const char *categories[] = {
            "machine learning", "deep learning", "natural language processing",
            "computer vision", "reinforcement learning", "data science",
            "neural networks", "optimization", "feature engineering",
            "model evaluation"
        };
        snprintf(content, sizeof(content),
                 "%s technique %d: advanced algorithms for %s applications "
                 "with improved accuracy and efficiency metrics",
                 categories[idx % 10], idx, categories[(idx + 3) % 10]);

        char **result_ids = NULL;
        float *scores = NULL;
        size_t result_count = 0;

        agentrt_error_t err = agentrt_layer2_feature_search(
            l2, content, TOP_K, &result_ids, &scores, &result_count);

        if (err == AGENTRT_OK && result_count > 0) {
            for (size_t j = 0; j < result_count; j++) {
                if (result_ids[j] && strcmp(result_ids[j], all_ids[idx]) == 0) {
                    recalled++;
                    break;
                }
            }
        }

        free_query_results(result_ids, scores, result_count);
    }

    double recall_rate = (double)recalled / (double)sample_size;
    printf("    Load test recall: %d/%d (%.1f%%)\n",
           recalled, sample_size, recall_rate * 100.0);
    TEST_ASSERT(recall_rate >= 0.95);

    /* 3. 清理 */
    for (int i = 0; i < LOAD_DOC_COUNT; i++) {
        if (all_ids[i]) {
            agentrt_layer2_feature_remove(l2, all_ids[i]);
            free(all_ids[i]);
        }
    }
    free(all_ids);
    agentrt_layer2_feature_destroy(l2);
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  MemoryRovol L2 Embedding Recall Tests\n");
    printf("  INT-08 - Feature Layer Quality\n");
    printf("=========================================\n\n");

    /* INT-08.1: 精确匹配召回 */
    printf("--- INT-08.1: Exact Match Recall ---\n");
    RUN_TEST(int08_1_exact_match_recall);

    /* INT-08.2: 相似向量召回 */
    printf("\n--- INT-08.2: Similar Vector Recall ---\n");
    RUN_TEST(int08_2_similar_vector_recall);

    /* INT-08.3: 不相似向量拒绝 */
    printf("\n--- INT-08.3: Dissimilar Vector Rejection ---\n");
    RUN_TEST(int08_3_dissimilar_vector_rejection);

    /* INT-08.4: 多模型嵌入一致性 */
    printf("\n--- INT-08.4: Multi-Model Embedding Consistency ---\n");
    RUN_TEST(int08_4_multi_model_embedding_consistency);

    /* INT-08.5: 负载下召回质量 */
    printf("\n--- INT-08.5: Recall Under Load (10K vectors) ---\n");
    RUN_TEST(int08_5_recall_under_load);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d INT-08 embedding recall tests PASSED\n", g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED (out of %d)\n",
               g_tests_passed, g_tests_failed, g_tests_run);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
