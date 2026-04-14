/**
 * @file test_skill_registry.c
 * @brief Skill注册表单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "market_service.h"

static void test_skill_registry_create_destroy(void) {
    printf("  test_skill_registry_create_destroy...\n");

    skill_registry_t* reg = skill_registry_create();
    assert(reg != NULL);

    skill_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_skill_registry_register_skill(void) {
    printf("  test_skill_registry_register_skill...\n");

    skill_registry_t* reg = skill_registry_create();
    assert(reg != NULL);

    skill_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "test_skill_001";
    meta.name = "Test Skill";
    meta.version = "1.0.0";
    meta.description = "A test skill";

    int ret = skill_registry_register(reg, &meta);
    assert(ret == 0);

    skill_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_skill_registry_get_skill(void) {
    printf("  test_skill_registry_get_skill...\n");

    skill_registry_t* reg = skill_registry_create();
    assert(reg != NULL);

    skill_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "get_test_skill";
    meta.name = "Get Test Skill";
    meta.version = "1.0.0";

    skill_registry_register(reg, &meta);

    const skill_meta_t* found = skill_registry_get(reg, "get_test_skill");
    assert(found != NULL);
    assert(strcmp(found->id, "get_test_skill") == 0);

    skill_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_skill_registry_unregister_skill(void) {
    printf("  test_skill_registry_unregister_skill...\n");

    skill_registry_t* reg = skill_registry_create();
    assert(reg != NULL);

    skill_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "unregister_skill_test";
    meta.name = "Unregister Test Skill";
    meta.version = "1.0.0";

    skill_registry_register(reg, &meta);

    int ret = skill_registry_unregister(reg, "unregister_skill_test");
    assert(ret == 0);

    const skill_meta_t* found = skill_registry_get(reg, "unregister_skill_test");
    assert(found == NULL);

    skill_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_skill_registry_list_skills(void) {
    printf("  test_skill_registry_list_skills...\n");

    skill_registry_t* reg = skill_registry_create();
    assert(reg != NULL);

    skill_meta_t meta1;
    memset(&meta1, 0, sizeof(meta1));
    meta1.id = "list_skill_1";
    meta1.name = "List Skill 1";
    meta1.version = "1.0.0";

    skill_meta_t meta2;
    memset(&meta2, 0, sizeof(meta2));
    meta2.id = "list_skill_2";
    meta2.name = "List Skill 2";
    meta2.version = "1.0.0";

    skill_registry_register(reg, &meta1);
    skill_registry_register(reg, &meta2);

    char** skills = NULL;
    size_t count = 0;
    int ret = skill_registry_list(reg, &skills, &count);
    assert(ret == 0);
    assert(count == 2);

    for (size_t i = 0; i < count; i++) {
        free(skills[i]);
    }
    free(skills);

    skill_registry_destroy(reg);

    printf("    PASSED\n");
}

static void test_skill_registry_by_category(void) {
    printf("  test_skill_registry_by_category...\n");

    skill_registry_t* reg = skill_registry_create();
    assert(reg != NULL);

    skill_meta_t meta1;
    memset(&meta1, 0, sizeof(meta1));
    meta1.id = "cat_skill_1";
    meta1.name = "Category Skill 1";
    meta1.version = "1.0.0";
    meta1.category = "productivity";

    skill_meta_t meta2;
    memset(&meta2, 0, sizeof(meta2));
    meta2.id = "cat_skill_2";
    meta2.name = "Category Skill 2";
    meta2.version = "1.0.0";
    meta2.category = "productivity";

    skill_meta_t meta3;
    memset(&meta3, 0, sizeof(meta3));
    meta3.id = "cat_skill_3";
    meta3.name = "Category Skill 3";
    meta3.version = "1.0.0";
    meta3.category = "development";

    skill_registry_register(reg, &meta1);
    skill_registry_register(reg, &meta2);
    skill_registry_register(reg, &meta3);

    char** skills = NULL;
    size_t count = 0;
    int ret = skill_registry_list_by_category(reg, "productivity", &skills, &count);
    assert(ret == 0);
    assert(count == 2);

    for (size_t i = 0; i < count; i++) {
        free(skills[i]);
    }
    free(skills);

    skill_registry_destroy(reg);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Skill Registry Unit Tests\n");
    printf("=========================================\n");

    test_skill_registry_create_destroy();
    test_skill_registry_register_skill();
    test_skill_registry_get_skill();
    test_skill_registry_unregister_skill();
    test_skill_registry_list_skills();
    test_skill_registry_by_category();

    printf("\n✅ All skill registry tests PASSED\n");
    return 0;
}
