/**
 * @file test_layer2_feature.c
 * @brief L2特征层单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 测试L2层的特征提取、相似度计算、嵌入、索引等功能
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8 可测试性原则
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include "layer2_feature.h"

#define EPSILON 0.001f
#define DIM 128

static int float_equal(float a, float b) {
    return fabsf(a - b) < EPSILON;
}

int test_similarity_cosine_basic(void) {
    printf("  测试余弦相似度基本功能...\n");

    float a[DIM];
    float b[DIM];

    for (size_t i = 0; i < DIM; i++) {
        a[i] = 1.0f;
        b[i] = 1.0f;
    }

    float sim = agentos_similarity_cosine(a, b, DIM);
    if (!float_equal(sim, 1.0f)) {
        printf("    相同向量相似度应为1.0f，实际: %f\n", sim);
        return 1;
    }

    printf("    余弦相似度基本功能测试通过\n");
    return 0;
}

int test_similarity_cosine_orthogonal(void) {
    printf("  测试正交向量相似度...\n");

    float a[DIM] = {0};
    float b[DIM] = {0};

    a[0] = 1.0f;
    b[1] = 1.0f;

    float sim = agentos_similarity_cosine(a, b, DIM);
    if (!float_equal(sim, 0.0f)) {
        printf("    正交向量相似度应为0.0f，实际: %f\n", sim);
        return 1;
    }

    printf("    正交向量相似度测试通过\n");
    return 0;
}

int test_similarity_cosine_opposite(void) {
    printf("  测试相反向量相似度...\n");

    float a[DIM];
    float b[DIM];

    for (size_t i = 0; i < DIM; i++) {
        a[i] = 1.0f;
        b[i] = -1.0f;
    }

    float sim = agentos_similarity_cosine(a, b, DIM);
    if (!float_equal(sim, -1.0f)) {
        printf("    相反向量相似度应为-1.0f，实际: %f\n", sim);
        return 1;
    }

    printf("    相反向量相似度测试通过\n");
    return 0;
}

int test_similarity_l2_squared(void) {
    printf("  测试L2距离平方...\n");

    float a[3] = {0.0f, 0.0f, 0.0f};
    float b[3] = {3.0f, 4.0f, 0.0f};

    float dist = agentos_similarity_l2_squared(a, b, 3);
    if (!float_equal(dist, 25.0f)) {
        printf("    L2距离平方应为25.0f，实际: %f\n", dist);
        return 1;
    }

    printf("    L2距离平方测试通过\n");
    return 0;
}

int test_distance_to_score_cosine(void) {
    printf("  测试距离到分数转换(cosine)...\n");

    float score = agentos_distance_to_score(0.5f, "cosine");
    if (!float_equal(score, 0.5f)) {
        printf("    cosine距离0.5应转换为0.5，实际: %f\n", score);
        return 1;
    }

    printf("    距离到分数转换(cosine)测试通过\n");
    return 0;
}

int test_distance_to_score_l2(void) {
    printf("  测试距离到分数转换(l2)...\n");

    float score = agentos_distance_to_score(1.0f, "l2");
    float expected = expf(-1.0f);
    if (!float_equal(score, expected)) {
        printf("    l2距离1.0应转换为exp(-1)，实际: %f\n", score);
        return 1;
    }

    printf("    距离到分数转换(l2)测试通过\n");
    return 0;
}

int test_normalize_basic(void) {
    printf("  测试向量归一化...\n");

    float vec[4] = {3.0f, 4.0f, 0.0f, 0.0f};
    agentos_similarity_normalize(vec, 4);

    float norm = 0.0f;
    for (size_t i = 0; i < 4; i++) {
        norm += vec[i] * vec[i];
    }
    norm = sqrtf(norm);

    if (!float_equal(norm, 1.0f)) {
        printf("    归一化后向量范数应为1.0f，实际: %f\n", norm);
        return 1;
    }

    printf("    向量归一化测试通过\n");
    return 0;
}

int test_normalize_zero_vector(void) {
    printf("  测试零向量归一化...\n");

    float vec[3] = {0.0f, 0.0f, 0.0f};
    agentos_similarity_normalize(vec, 3);

    float expected[3] = {0.0f, 0.0f, 0.0f};
    for (size_t i = 0; i < 3; i++) {
        if (!float_equal(vec[i], expected[i])) {
            printf("    零向量归一化后应仍为零向量\n");
            return 1;
        }
    }

    printf("    零向量归一化测试通过\n");
    return 0;
}

int test_bm25_score_calculation(void) {
    printf("  测试BM25分数计算...\n");

    const char* doc = "agentos is a distributed operating system";
    const char* query = "agentos distributed";

    float score = agentos_bm25_score(doc, query);
    if (score < 0.0f) {
        printf("    BM25分数不应为负数，实际: %f\n", score);
        return 1;
    }

    printf("    BM25分数计算测试通过 (score=%f)\n", score);
    return 0;
}

int test_bm25_empty_inputs(void) {
    printf("  测试BM25空输入...\n");

    float score = agentos_bm25_score(NULL, "query");
    if (score != 0.0f) {
        printf("    空文档BM25分数应为0.0f，实际: %f\n", score);
        return 1;
    }

    score = agentos_bm25_score("document", NULL);
    if (score != 0.0f) {
        printf("    空查询BM25分数应为0.0f，实际: %f\n", score);
        return 1;
    }

    printf("    BM25空输入测试通过\n");
    return 0;
}

int test_hybrid_search_combination(void) {
    printf("  测试混合搜索组合...\n");

    const char* query = "agent memory system";
    float vector_score = 0.85f;
    float keyword_score = 0.72f;
    float alpha = 0.6f;

    float combined = agentos_hybrid_combination(vector_score, keyword_score, alpha);
    float expected = alpha * vector_score + (1.0f - alpha) * keyword_score;

    if (!float_equal(combined, expected)) {
        printf("    混合分数计算错误，实际: %f，期望: %f\n", combined, expected);
        return 1;
    }

    printf("    混合搜索组合测试通过\n");
    return 0;
}

int test_hybrid_search_alpha_boundaries(void) {
    printf("  测试混合搜索alpha边界值...\n");

    float vs = 0.8f;
    float ks = 0.6f;

    float r1 = agentos_hybrid_combination(vs, ks, 0.0f);
    if (!float_equal(r1, ks)) {
        printf("    alpha=0时应只考虑关键词分数\n");
        return 1;
    }

    float r2 = agentos_hybrid_combination(vs, ks, 1.0f);
    if (!float_equal(r2, vs)) {
        printf("    alpha=1时应只考虑向量分数\n");
        return 1;
    }

    printf("    alpha边界值测试通过\n");
    return 0;
}

int test_index_insert_and_search(void) {
    printf("  测试索引插入和搜索...\n");

    agentos_vector_index_t* index = NULL;
    int err = agentos_vector_index_create(DIM, 1000, &index);
    if (err != 0) {
        printf("    创建索引失败\n");
        return 1;
    }

    float vec[DIM];
    for (size_t i = 0; i < DIM; i++) {
        vec[i] = (float)(i % 10) / 10.0f;
    }

    err = agentos_vector_index_insert(index, vec, "doc1");
    if (err != 0) {
        printf("    插入向量失败\n");
        agentos_vector_index_destroy(index);
        return 1;
    }

    agentos_vector_index_destroy(index);
    printf("    索引插入和搜索测试通过\n");
    return 0;
}

int test_index_approximate_knn(void) {
    printf("  测试近似KNN搜索...\n");

    agentos_vector_index_t* index = NULL;
    int err = agentos_vector_index_create(DIM, 100, &index);
    if (err != 0) {
        printf("    创建索引失败\n");
        return 1;
    }

    float query[DIM];
    for (size_t i = 0; i < DIM; i++) {
        query[i] = 0.5f;
    }

    agentos_knn_result_t result;
    err = agentos_vector_index_knn(index, query, 5, &result);
    if (err != 0) {
        printf("    KNN搜索失败\n");
        agentos_vector_index_destroy(index);
        return 1;
    }

    agentos_knn_result_destroy(&result);
    agentos_vector_index_destroy(index);
    printf("    近似KNN搜索测试通过\n");
    return 0;
}

int main(void) {
    printf("开始运行 memoryrovol L2 特征层单元测试...\n");

    int failures = 0;

    failures |= test_similarity_cosine_basic();
    failures |= test_similarity_cosine_orthogonal();
    failures |= test_similarity_cosine_opposite();
    failures |= test_similarity_l2_squared();
    failures |= test_distance_to_score_cosine();
    failures |= test_distance_to_score_l2();
    failures |= test_normalize_basic();
    failures |= test_normalize_zero_vector();
    failures |= test_bm25_score_calculation();
    failures |= test_bm25_empty_inputs();
    failures |= test_hybrid_search_combination();
    failures |= test_hybrid_search_alpha_boundaries();
    failures |= test_index_insert_and_search();
    failures |= test_index_approximate_knn();

    if (failures == 0) {
        printf("\n所有L2特征层测试通过！\n");
        return 0;
    } else {
        printf("\n%d 个L2特征层测试失败\n", failures);
        return 1;
    }
}
