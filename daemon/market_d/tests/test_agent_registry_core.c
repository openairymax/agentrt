/**
 * @file test_agent_registry_core.c
 * @brief Agent注册表核心功能单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 2.0.0
 */

#include "agent_registry_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_PASS(name) printf("✓ %s\n", name)
#define TEST_FAIL(name, reason) do { \
    printf("✗ %s: %s\n", name, reason); \
    return -1; \
} while(0)

static int test_create_destroy(void) {
    agent_registry_t* reg = agent_registry_create();
    if (!reg) {
        TEST_FAIL("create_destroy", "Failed to create registry");
    }

    agent_registry_destroy(reg);
    TEST_PASS("create_destroy");
    return 0;
}

static int test_init_shutdown(void) {
    agent_registry_t* reg = agent_registry_create();
    if (!reg) {
        TEST_FAIL("init_shutdown", "Failed to create registry");
    }

    if (agent_registry_init(reg, NULL) != 0) {
        agent_registry_destroy(reg);
        TEST_FAIL("init_shutdown", "Failed to init registry");
    }

    agent_registry_shutdown(reg);
    agent_registry_destroy(reg);
    TEST_PASS("init_shutdown");
    return 0;
}

static int test_add_get_agent(void) {
    agent_registry_t* reg = agent_registry_create();
    if (!reg) {
        TEST_FAIL("add_get_agent", "Failed to create registry");
    }

    agent_registry_init(reg, NULL);

    agent_entry_t entry = {0};
    strncpy(entry.id, "test-agent-001", sizeof(entry.id) - 1);
    strncpy(entry.name, "Test Agent", sizeof(entry.name) - 1);
    entry.description = strdup("A test agent for unit testing");
    entry.author = strdup("Test Author");
    entry.verified = 1;
    entry.official = 0;

    if (agent_registry_add(reg, &entry) != 0) {
        free(entry.description);
        free(entry.author);
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("add_get_agent", "Failed to add agent");
    }

    free(entry.description);
    free(entry.author);

    const agent_entry_t* found = agent_registry_get(reg, "test-agent-001");
    if (!found) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("add_get_agent", "Agent not found after add");
    }

    if (strcmp(found->id, "test-agent-001") != 0) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("add_get_agent", "Wrong agent ID");
    }

    agent_registry_shutdown(reg);
    agent_registry_destroy(reg);
    TEST_PASS("add_get_agent");
    return 0;
}

static int test_remove_agent(void) {
    agent_registry_t* reg = agent_registry_create();
    agent_registry_init(reg, NULL);

    agent_entry_t entry = {0};
    strncpy(entry.id, "test-agent-002", sizeof(entry.id) - 1);
    strncpy(entry.name, "Test Agent 2", sizeof(entry.name) - 1);
    entry.description = strdup("Another test agent");
    entry.author = strdup("Test Author");

    agent_registry_add(reg, &entry);
    free(entry.description);
    free(entry.author);

    if (agent_registry_remove(reg, "test-agent-002") != 0) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("remove_agent", "Failed to remove agent");
    }

    const agent_entry_t* found = agent_registry_get(reg, "test-agent-002");
    if (found) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("remove_agent", "Agent still found after remove");
    }

    agent_registry_shutdown(reg);
    agent_registry_destroy(reg);
    TEST_PASS("remove_agent");
    return 0;
}

static int test_list_agents(void) {
    agent_registry_t* reg = agent_registry_create();
    agent_registry_init(reg, NULL);

    for (int i = 0; i < 5; i++) {
        agent_entry_t entry = {0};
        char id[64], name[64];
        snprintf(id, sizeof(id), "agent-%03d", i);
        snprintf(name, sizeof(name), "Agent %d", i);
        strncpy(entry.id, id, sizeof(entry.id) - 1);
        strncpy(entry.name, name, sizeof(entry.name) - 1);
        entry.author = strdup("Test");
        agent_registry_add(reg, &entry);
        free(entry.author);
    }

    const agent_entry_t* entries[10];
    size_t count = agent_registry_list(reg, entries, 10);

    if (count != 5) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("list_agents", "Wrong count");
    }

    agent_registry_shutdown(reg);
    agent_registry_destroy(reg);
    TEST_PASS("list_agents");
    return 0;
}

static int test_search_by_tag(void) {
    agent_registry_t* reg = agent_registry_create();
    agent_registry_init(reg, NULL);

    agent_entry_t entry = {0};
    strncpy(entry.id, "search-test-agent", sizeof(entry.id) - 1);
    strncpy(entry.name, "Search Test Agent", sizeof(entry.name) - 1);
    entry.author = strdup("Test");

    agent_registry_add(reg, &entry);
    free(entry.author);

    const agent_entry_t* results[10];
    size_t count = agent_registry_search_by_tag(reg, "python", results, 10);

    agent_registry_shutdown(reg);
    agent_registry_destroy(reg);
    TEST_PASS("search_by_tag");
    return 0;
}

static int test_search(void) {
    agent_registry_t* reg = agent_registry_create();
    agent_registry_init(reg, NULL);

    agent_entry_t entry = {0};
    strncpy(entry.id, "searchable-agent", sizeof(entry.id) - 1);
    strncpy(entry.name, "Searchable Agent", sizeof(entry.name) - 1);
    entry.description = strdup("This agent is searchable by text");
    entry.author = strdup("Test");

    agent_registry_add(reg, &entry);
    free(entry.description);
    free(entry.author);

    const agent_entry_t* results[10];
    size_t count = agent_registry_search(reg, "searchable", results, 10);

    if (count != 1) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("search", "Search returned wrong count");
    }

    agent_registry_shutdown(reg);
    agent_registry_destroy(reg);
    TEST_PASS("search");
    return 0;
}

static int test_count(void) {
    agent_registry_t* reg = agent_registry_create();
    agent_registry_init(reg, NULL);

    if (agent_registry_count(reg) != 0) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("count", "Initial count should be 0");
    }

    for (int i = 0; i < 3; i++) {
        agent_entry_t entry = {0};
        char id[64];
        snprintf(id, sizeof(id), "count-test-%d", i);
        strncpy(entry.id, id, sizeof(entry.id) - 1);
        strncpy(entry.name, "Count Test", sizeof(entry.name) - 1);
        entry.author = strdup("Test");
        agent_registry_add(reg, &entry);
        free(entry.author);
    }

    if (agent_registry_count(reg) != 3) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("count", "Count mismatch after adding agents");
    }

    agent_registry_shutdown(reg);
    agent_registry_destroy(reg);
    TEST_PASS("count");
    return 0;
}

static int test_add_version(void) {
    agent_registry_t* reg = agent_registry_create();
    agent_registry_init(reg, NULL);

    agent_entry_t entry = {0};
    strncpy(entry.id, "version-test-agent", sizeof(entry.id) - 1);
    strncpy(entry.name, "Version Test Agent", sizeof(entry.name) - 1);
    entry.author = strdup("Test");
    agent_registry_add(reg, &entry);
    free(entry.author);

    agent_version_t version = {0};
    version.version = "1.0.0";
    version.download_url = "https://example.com/agent.tar.gz";
    version.checksum = "abc123";

    if (agent_registry_add_version(reg, "version-test-agent", &version) != 0) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("add_version", "Failed to add version");
    }

    const char* latest = agent_registry_get_latest_version(reg, "version-test-agent");
    if (!latest || strcmp(latest, "1.0.0") != 0) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("add_version", "Latest version mismatch");
    }

    agent_registry_shutdown(reg);
    agent_registry_destroy(reg);
    TEST_PASS("add_version");
    return 0;
}

static int test_invalid_params(void) {
    agent_registry_t* reg = agent_registry_create();
    agent_registry_init(reg, NULL);

    agent_entry_t entry = {0};
    strncpy(entry.name, "No ID Agent", sizeof(entry.name) - 1);

    if (agent_registry_add(NULL, &entry) == 0) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("invalid_params", "Should reject NULL registry");
    }

    if (agent_registry_add(reg, &entry) == 0) {
        agent_registry_shutdown(reg);
        agent_registry_destroy(reg);
        TEST_FAIL("invalid_params", "Should reject entry without ID");
    }

    agent_registry_shutdown(reg);
    agent_registry_destroy(reg);
    TEST_PASS("invalid_params");
    return 0;
}

int main(void) {
    int failed = 0;

    printf("\n=== Agent Registry Core Unit Tests ===\n\n");

    if (test_create_destroy() != 0) failed++;
    if (test_init_shutdown() != 0) failed++;
    if (test_add_get_agent() != 0) failed++;
    if (test_remove_agent() != 0) failed++;
    if (test_list_agents() != 0) failed++;
    if (test_search_by_tag() != 0) failed++;
    if (test_search() != 0) failed++;
    if (test_count() != 0) failed++;
    if (test_add_version() != 0) failed++;
    if (test_invalid_params() != 0) failed++;

    printf("\n=== Test Summary ===\n");
    printf("Total: 10 tests\n");
    printf("Passed: %d\n", 10 - failed);
    printf("Failed: %d\n", failed);

    return (failed == 0) ? 0 : 1;
}
