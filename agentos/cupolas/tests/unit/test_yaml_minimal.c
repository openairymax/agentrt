/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 */

#include "../src/yaml_minimal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-50s", name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static void test_create_destroy(void)
{
    TEST("Create and destroy");
    yaml_document_t *doc = yaml_create();
    ASSERT(doc != NULL, "yaml_create returned NULL");
    ASSERT(doc->root == NULL, "root should be NULL");
    yaml_destroy(doc);
    PASS();
}

static void test_parse_empty(void)
{
    TEST("Parse empty input");
    yaml_document_t *doc = yaml_create();
    int rc = yaml_parse_string(doc, "", 0);
    ASSERT(rc == 0, "empty input accepted as empty scalar");
    ASSERT(doc->root != NULL, "root should not be NULL");
    yaml_destroy(doc);
    PASS();
}

static void test_parse_scalar(void)
{
    TEST("Parse simple scalar");
    yaml_document_t *doc = yaml_create();
    int rc = yaml_parse_string(doc, "hello world", 11);
    ASSERT(rc == 0, "scalar parse should succeed");
    ASSERT(doc->root != NULL, "root should not be NULL");
    ASSERT(doc->root->type == YAML_NODE_SCALAR, "should be scalar");
    const char *val = yaml_as_string(doc->root, NULL);
    ASSERT(val != NULL, "scalar value should not be NULL");
    yaml_destroy(doc);
    PASS();
}

static void test_parse_sequence(void)
{
    TEST("Parse simple sequence");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "- apple\n- banana\n- cherry\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "sequence parse should succeed");
    ASSERT(doc->root != NULL, "root should not be NULL");
    ASSERT(doc->root->type == YAML_NODE_SEQUENCE, "should be sequence");
    ASSERT(yaml_size(doc->root) == 3, "sequence should have 3 items");

    struct yaml_node *item0 = yaml_get_index(doc->root, 0);
    ASSERT(item0 != NULL, "item 0 should exist");
    struct yaml_node *item1 = yaml_get_index(doc->root, 1);
    ASSERT(item1 != NULL, "item 1 should exist");

    yaml_destroy(doc);
    PASS();
}

static void test_null_lookup(void)
{
    TEST("Null key lookup returns default");
    ASSERT(yaml_as_string(NULL, "default") != NULL, "null node returns default");
    ASSERT(yaml_as_int64(NULL, 999) == 999, "null int64 default");
    ASSERT(yaml_as_double(NULL, 99.9) == 99.9, "null double default");
    ASSERT(yaml_as_bool(NULL, true) == true, "null bool default");
    ASSERT(yaml_has_key(NULL, "x") == false, "has_key on null");
    PASS();
}

static void test_null_document_parse(void)
{
    TEST("Null document param rejected");
    int rc = yaml_parse_string(NULL, "x", 1);
    ASSERT(rc != 0, "null doc should be rejected");
    PASS();
}

static void test_null_input_parse(void)
{
    TEST("Null input param rejected");
    yaml_document_t *doc = yaml_create();
    int rc = yaml_parse_string(doc, NULL, 5);
    ASSERT(rc != 0, "null input should be rejected");
    yaml_destroy(doc);
    PASS();
}

static void test_destroy_null(void)
{
    TEST("Destroy null document");
    yaml_destroy(NULL);
    PASS();
}

static void test_key_with_null_value(void)
{
    TEST("Key with null value");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "key: ~\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "null value parse should succeed");
    ASSERT(yaml_has_key(doc->root, "key") == true, "has_key for null-valued key");
    struct yaml_node *key_node = yaml_get(doc->root, "key");
    ASSERT(key_node != NULL, "key should exist");
    yaml_destroy(doc);
    PASS();
}

static void test_dump_scalar(void)
{
    TEST("Dump scalar node");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "hello";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "parse scalar");

    char buf[4096];
    yaml_dump(doc->root, buf, sizeof(buf), 0);
    ASSERT(strlen(buf) > 0, "dump should produce output");

    char *serialized = yaml_serialize(doc);
    ASSERT(serialized != NULL, "serialize should produce output");
    free(serialized);

    yaml_destroy(doc);
    PASS();
}

static void test_dump_sequence(void)
{
    TEST("Dump sequence node");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "- a\n- b\n- c\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "parse sequence");

    char buf[4096];
    yaml_dump(doc->root, buf, sizeof(buf), 0);
    ASSERT(strlen(buf) > 0, "dump should produce output");

    char *serialized = yaml_serialize(doc);
    ASSERT(serialized != NULL, "serialize should produce output");
    free(serialized);

    yaml_destroy(doc);
    PASS();
}

static void test_multi_document(void)
{
    TEST("Multi-document YAML");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "---\ndoc1: first\n...\n---\ndoc2: second\n";
    int rc = yaml_parse_multi(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "multi-doc parse");
    ASSERT(doc->next != NULL, "should have second document");
    struct yaml_node *root2 = yaml_root(doc->next);
    ASSERT(root2 != NULL, "second doc root should exist");
    yaml_destroy_chain(doc);
    PASS();
}

static void test_sequence_out_of_bounds(void)
{
    TEST("Sequence out of bounds access");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "- a\n- b\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "sequence parse");
    ASSERT(yaml_size(doc->root) == 2, "size is 2");
    ASSERT(yaml_get_index(doc->root, 5) == NULL, "out of bounds returns NULL");
    yaml_destroy(doc);
    PASS();
}

static void test_double_parse(void)
{
    TEST("Parse key: double value");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "ratio: 3.14\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "parse should succeed");
    struct yaml_node *ratio = yaml_get(doc->root, "ratio");
    ASSERT(ratio != NULL, "ratio should exist");
    ASSERT(yaml_as_double(ratio, -1.0) == 3.14, "ratio should be 3.14");
    yaml_destroy(doc);
    PASS();
}

static void test_int_parse(void)
{
    TEST("Parse key: int value");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "count: 42\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "parse should succeed");
    struct yaml_node *cnt = yaml_get(doc->root, "count");
    ASSERT(cnt != NULL, "count should exist");
    ASSERT(yaml_as_int64(cnt, -1) == 42, "count should be 42");
    yaml_destroy(doc);
    PASS();
}

static void test_bool_parse(void)
{
    TEST("Parse key: bool values");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "val: true\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "parse should succeed");
    struct yaml_node *val = yaml_get(doc->root, "val");
    ASSERT(val != NULL, "val should exist");
    ASSERT(yaml_as_bool(val, false) == true, "val should be true");
    yaml_destroy(doc);
    PASS();
}

static void test_string_parse(void)
{
    TEST("Parse key: string value");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "name: AgentOS\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "parse should succeed");
    struct yaml_node *name = yaml_get(doc->root, "name");
    ASSERT(name != NULL, "name should exist");
    ASSERT(strcmp(yaml_as_string(name, ""), "AgentOS") == 0, "name should be AgentOS");
    yaml_destroy(doc);
    PASS();
}

static void test_quoted_double(void)
{
    TEST("Parse double-quoted string");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "key: \"quoted value\"\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "parse should succeed");
    struct yaml_node *k = yaml_get(doc->root, "key");
    ASSERT(k != NULL, "key should exist");
    ASSERT(strcmp(yaml_as_string(k, ""), "quoted value") == 0, "quoted value");
    yaml_destroy(doc);
    PASS();
}

static void test_has_key_null_node(void)
{
    TEST("has_key on non-mapping returns false");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "just_a_string\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "parse scalar");
    ASSERT(yaml_has_key(doc->root, "anything") == false, "has_key on scalar returns false");
    yaml_destroy(doc);
    PASS();
}

static void test_sequence_get_index_negative(void)
{
    TEST("Sequence get_index with -1");
    yaml_document_t *doc = yaml_create();
    const char *yaml = "- a\n- b\n";
    int rc = yaml_parse_string(doc, yaml, strlen(yaml));
    ASSERT(rc == 0, "parse sequence");
    ASSERT(yaml_get_index(doc->root, (size_t)-1) == NULL, "huge index returns NULL");
    yaml_destroy(doc);
    PASS();
}

int main(void)
{
    printf("\n=== YAML Minimal Parser Unit Tests ===\n\n");

    test_create_destroy();
    test_parse_empty();
    test_parse_scalar();
    test_parse_sequence();
    test_null_lookup();
    test_null_document_parse();
    test_null_input_parse();
    test_destroy_null();
    test_key_with_null_value();
    test_dump_scalar();
    test_dump_sequence();
    test_multi_document();
    test_sequence_out_of_bounds();
    test_double_parse();
    test_int_parse();
    test_bool_parse();
    test_string_parse();
    test_quoted_double();
    test_has_key_null_node();
    test_sequence_get_index_negative();

    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}