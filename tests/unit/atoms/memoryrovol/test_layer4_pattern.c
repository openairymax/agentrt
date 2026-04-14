/**
 * @file test_layer4_pattern.c
 * @brief L4模式层单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 测试L4层的模式挖掘、持久同调、聚类、规则生成、模式验证等功能
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8 可测试性原则
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include "layer4_pattern.h"

#define EPSILON 0.001f

static int float_equal(float a, float b) {
    return fabsf(a - b) < EPSILON;
}

int test_pattern_miner_creation(void) {
    printf("  测试模式挖掘器创建...\n");

    agentos_pattern_miner_t* miner = NULL;
    int err = agentos_pattern_miner_create(&miner);
    if (err != 0 || miner == NULL) {
        printf("    创建模式挖掘器失败\n");
        return 1;
    }

    agentos_pattern_miner_destroy(miner);
    printf("    模式挖掘器创建测试通过\n");
    return 0;
}

int test_pattern_miner_basic_extraction(void) {
    printf("  测试基本模式提取...\n");

    agentos_pattern_miner_t* miner = NULL;
    agentos_pattern_miner_create(&miner);

    const char* sequences[][5] = {
        {"A", "B", "C", "D", "E"},
        {"A", "B", "C", "F", "G"},
        {"A", "B", "D", "E", "H"}
    };
    int seq_count = 3;

    agentos_pattern_t patterns[10];
    size_t pattern_count = 0;

    int err = agentos_pattern_miner_extract(miner, (const char**)sequences, seq_count, 5, patterns, 10, &pattern_count);
    if (err != 0) {
        printf("    模式提取失败\n");
        agentos_pattern_miner_destroy(miner);
        return 1;
    }

    if (pattern_count == 0) {
        printf("    未提取到任何模式\n");
        agentos_pattern_miner_destroy(miner);
        return 1;
    }

    agentos_pattern_miner_destroy(miner);
    printf("    基本模式提取测试通过 (提取到 %zu 个模式)\n", pattern_count);
    return 0;
}

int test_persistence_computation(void) {
    printf("  测试持久同调计算...\n");

    agentos_persistence_t* pers = NULL;
    int err = agentos_persistence_create(&pers);
    if (err != 0 || pers == NULL) {
        printf("    创建持久同调计算器失败\n");
        return 1;
    }

    float points[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f};
    int point_count = 3;
    int max_dimension = 1;

    agentos_persistence_diagram_t diagram;
    err = agentos_persistence_compute(pers, points, point_count, max_dimension, &diagram);
    if (err != 0) {
        printf("    持久同调计算失败\n");
        agentos_persistence_destroy(pers);
        return 1;
    }

    agentos_persistence_destroy(pers);
    printf("    持久同调计算测试通过\n");
    return 0;
}

int test_persistence_empty_input(void) {
    printf("  测试空输入持久同调...\n");

    agentos_persistence_t* pers = NULL;
    agentos_persistence_create(&pers);

    float points[1];
    agentos_persistence_diagram_t diagram;
    int err = agentos_persistence_compute(pers, points, 0, 1, &diagram);

    if (err == 0) {
        printf("    空输入应返回错误\n");
        agentos_persistence_destroy(pers);
        return 1;
    }

    agentos_persistence_destroy(pers);
    printf("    空输入持久同调测试通过\n");
    return 0;
}

int test_clustering_basic(void) {
    printf("  测试基本聚类...\n");

    agentos_clustering_t* cluster = NULL;
    int err = agentos_clustering_create(&cluster);
    if (err != 0 || cluster == NULL) {
        printf("    创建聚类引擎失败\n");
        return 1;
    }

    float data[][2] = {
        {0.0f, 0.0f},
        {0.1f, 0.1f},
        {10.0f, 10.0f},
        {10.1f, 10.1f}
    };
    int data_count = 4;
    int min_cluster_size = 2;

    int labels[4];
    err = agentos_clustering_compute(cluster, (float*)data, data_count, 2, min_cluster_size, labels);
    if (err != 0) {
        printf("    聚类计算失败\n");
        agentos_clustering_destroy(cluster);
        return 1;
    }

    if (labels[0] != labels[1] || labels[2] != labels[3]) {
        printf("    聚类结果不符合预期\n");
        agentos_clustering_destroy(cluster);
        return 1;
    }

    agentos_clustering_destroy(cluster);
    printf("    基本聚类测试通过\n");
    return 0;
}

int test_clustering_single_point(void) {
    printf("  测试单点聚类...\n");

    agentos_clustering_t* cluster = NULL;
    agentos_clustering_create(&cluster);

    float data[][2] = {{5.0f, 5.0f}};
    int labels[1];
    int err = agentos_clustering_compute(cluster, (float*)data, 1, 2, 2, labels);

    if (err == 0) {
        printf("    单点聚类应返回错误或标记为噪声\n");
        agentos_clustering_destroy(cluster);
        return 1;
    }

    agentos_clustering_destroy(cluster);
    printf("    单点聚类测试通过\n");
    return 0;
}

int test_rules_generation(void) {
    printf("  测试规则生成...\n");

    agentos_rules_generator_t* gen = NULL;
    int err = agentos_rules_generator_create(&gen);
    if (err != 0 || gen == NULL) {
        printf("    创建规则生成器失败\n");
        return 1;
    }

    agentos_pattern_t patterns[] = {
        {.items = (char*[]){"A", "B"}, .count = 2, .support = 0.8f},
        {.items = (char*[]){"B", "C"}, .count = 2, .support = 0.7f}
    };
    size_t pattern_count = 2;

    agentos_rule_t rules[10];
    size_t rule_count = 0;

    err = agentos_rules_generate(gen, patterns, pattern_count, rules, 10, &rule_count);
    if (err != 0) {
        printf("    规则生成失败\n");
        agentos_rules_generator_destroy(gen);
        return 1;
    }

    agentos_rules_generator_destroy(gen);
    printf("    规则生成测试通过 (生成 %zu 条规则)\n", rule_count);
    return 0;
}

int test_rules_validation(void) {
    printf("  测试规则验证...\n");

    agentos_rule_validator_t* validator = NULL;
    int err = agentos_rule_validator_create(&validator);
    if (err != 0 || validator == NULL) {
        printf("    创建规则验证器失败\n");
        return 1;
    }

    agentos_rule_t rule = {
        .antecedent = "A",
        .consequent = "B",
        .confidence = 0.9f,
        .support = 0.7f,
        .lift = 1.5f
    };

    agentos_rule_validation_result_t result;
    err = agentos_rule_validate(validator, &rule, &result);
    if (err != 0) {
        printf("    规则验证失败\n");
        agentos_rule_validator_destroy(validator);
        return 1;
    }

    agentos_rule_validator_destroy(validator);
    printf("    规则验证测试通过\n");
    return 0;
}

int test_rules_min_confidence_threshold(void) {
    printf("  测试规则最低置信度阈值...\n");

    agentos_rule_validator_t* validator = NULL;
    agentos_rule_validator_create(&validator);

    agentos_rule_t low_conf_rule = {
        .antecedent = "A",
        .consequent = "B",
        .confidence = 0.3f,
        .support = 0.1f,
        .lift = 0.8f
    };

    agentos_rule_validation_result_t result;
    int err = agentos_rule_validate(validator, &low_conf_rule, &result);

    if (err == 0 && result.valid) {
        printf("    低置信度规则应被拒绝\n");
        agentos_rule_validator_destroy(validator);
        return 1;
    }

    agentos_rule_validator_destroy(validator);
    printf("    规则最低置信度阈值测试通过\n");
    return 0;
}

int test_pattern_generalization(void) {
    printf("  测试模式泛化...\n");

    agentos_pattern_miner_t* miner = NULL;
    agentos_pattern_miner_create(&miner);

    const char* specific_patterns[] = {
        "user_login_mobile_ios",
        "user_login_mobile_android",
        "user_login_web_chrome"
    };

    agentos_pattern_t generalized;
    int err = agentos_pattern_generalize(miner, specific_patterns, 3, &generalized);
    if (err != 0) {
        printf("    模式泛化失败\n");
        agentos_pattern_miner_destroy(miner);
        return 1;
    }

    agentos_pattern_miner_destroy(miner);
    printf("    模式泛化测试通过\n");
    return 0;
}

int test_pattern_matching(void) {
    printf("  测试模式匹配...\n");

    agentos_pattern_miner_t* miner = NULL;
    agentos_pattern_miner_create(&miner);

    agentos_pattern_t pattern = {
        .items = (char*[]){"A", "B", "C"},
        .count = 3,
        .support = 0.8f
    };

    const char* sequence = "X A B C Y Z";
    int matched = agentos_pattern_match(miner, &pattern, sequence);
    if (!matched) {
        printf("    模式匹配失败\n");
        agentos_pattern_miner_destroy(miner);
        return 1;
    }

    agentos_pattern_miner_destroy(miner);
    printf("    模式匹配测试通过\n");
    return 0;
}

int test_l4_layer_integration(void) {
    printf("  测试L4层集成...\n");

    agentos_pattern_miner_t* miner = NULL;
    agentos_clustering_t* cluster = NULL;
    agentos_rules_generator_t* gen = NULL;

    if (agentos_pattern_miner_create(&miner) != 0) {
        printf("    创建模式挖掘器失败\n");
        return 1;
    }

    if (agentos_clustering_create(&cluster) != 0) {
        printf("    创建聚类引擎失败\n");
        agentos_pattern_miner_destroy(miner);
        return 1;
    }

    if (agentos_rules_generator_create(&gen) != 0) {
        printf("    创建规则生成器失败\n");
        agentos_pattern_miner_destroy(miner);
        agentos_clustering_destroy(cluster);
        return 1;
    }

    agentos_pattern_miner_destroy(miner);
    agentos_clustering_destroy(cluster);
    agentos_rules_generator_destroy(gen);

    printf("    L4层集成测试通过\n");
    return 0;
}

int main(void) {
    printf("开始运行 memoryrovol L4 模式层单元测试...\n");

    int failures = 0;

    failures |= test_pattern_miner_creation();
    failures |= test_pattern_miner_basic_extraction();
    failures |= test_persistence_computation();
    failures |= test_persistence_empty_input();
    failures |= test_clustering_basic();
    failures |= test_clustering_single_point();
    failures |= test_rules_generation();
    failures |= test_rules_validation();
    failures |= test_rules_min_confidence_threshold();
    failures |= test_pattern_generalization();
    failures |= test_pattern_matching();
    failures |= test_l4_layer_integration();

    if (failures == 0) {
        printf("\n所有L4模式层测试通过！\n");
        return 0;
    } else {
        printf("\n%d 个L4模式层测试失败\n", failures);
        return 1;
    }
}
