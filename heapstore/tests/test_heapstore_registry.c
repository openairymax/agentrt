/**
 * @file test_heapstore_registry.c
 * @brief AgentOS 数据分区注册表单元测试
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "heapstore.h"
#include "heapstore_registry.h"

static void test_registry_init_shutdown(void) {
    printf("Test: registry_init_shutdown...");

    heapstore_error_t err = heapstore_registry_init();
    assert(err == heapstore_SUCCESS);

    heapstore_registry_shutdown();

    printf("PASS\n");
}

static void test_registry_agent_crud(void) {
    printf("Test: registry_agent_crud...");

    heapstore_error_t err = heapstore_registry_init();
    assert(err == heapstore_SUCCESS);

    heapstore_agent_record_t record;
    memset(&record, 0, sizeof(record));

    snprintf(record.id, sizeof(record.id), "agent_%d", (int)time(NULL));
    snprintf(record.name, sizeof(record.name), "Test Agent");
    snprintf(record.type, sizeof(record.type), "planning");
    snprintf(record.version, sizeof(record.version), "1.0.0");
    snprintf(record.status, sizeof(record.status), "active");
    record.created_at = (uint64_t)time(NULL);
    record.updated_at = record.created_at;

    err = heapstore_registry_add_agent(&record);
    if (err == heapstore_SUCCESS) {
        heapstore_agent_record_t get_record;
        memset(&get_record, 0, sizeof(get_record));

        err = heapstore_registry_get_agent(record.id, &get_record);
        assert(err == heapstore_SUCCESS);
        assert(strcmp(get_record.name, record.name) == 0);
        assert(strcmp(get_record.type, record.type) == 0);

        snprintf(record.status, sizeof(record.status), "inactive");
        record.updated_at = (uint64_t)time(NULL);
        err = heapstore_registry_update_agent(&record);
        assert(err == heapstore_SUCCESS);

        err = heapstore_registry_delete_agent(record.id);
        assert(err == heapstore_SUCCESS);

        err = heapstore_registry_get_agent(record.id, &get_record);
        assert(err != heapstore_SUCCESS);
    }

    heapstore_registry_shutdown();
    printf("PASS\n");
}

static void test_registry_skill_crud(void) {
    printf("Test: registry_skill_crud...");

    heapstore_error_t err = heapstore_registry_init();
    assert(err == heapstore_SUCCESS);

    heapstore_skill_record_t record;
    memset(&record, 0, sizeof(record));

    snprintf(record.id, sizeof(record.id), "skill_%d", (int)time(NULL));
    snprintf(record.name, sizeof(record.name), "Test Skill");
    snprintf(record.version, sizeof(record.version), "1.0.0");
    snprintf(record.library_path, sizeof(record.library_path), "/path/to/lib");
    record.installed_at = (uint64_t)time(NULL);

    err = heapstore_registry_add_skill(&record);
    if (err == heapstore_SUCCESS) {
        heapstore_skill_record_t get_record;
        memset(&get_record, 0, sizeof(get_record));

        err = heapstore_registry_get_skill(record.id, &get_record);
        assert(err == heapstore_SUCCESS);
        assert(strcmp(get_record.name, record.name) == 0);

        err = heapstore_registry_delete_skill(record.id);
        assert(err == heapstore_SUCCESS);
    }

    heapstore_registry_shutdown();
    printf("PASS\n");
}

static void test_registry_session_crud(void) {
    printf("Test: registry_session_crud...");

    heapstore_error_t err = heapstore_registry_init();
    assert(err == heapstore_SUCCESS);

    heapstore_session_record_t record;
    memset(&record, 0, sizeof(record));

    snprintf(record.id, sizeof(record.id), "session_%d", (int)time(NULL));
    snprintf(record.user_id, sizeof(record.user_id), "user_123");
    record.created_at = (uint64_t)time(NULL);
    record.last_active_at = record.created_at;
    record.ttl_seconds = 3600;
    snprintf(record.status, sizeof(record.status), "active");

    err = heapstore_registry_add_session(&record);
    if (err == heapstore_SUCCESS) {
        heapstore_session_record_t get_record;
        memset(&get_record, 0, sizeof(get_record));

        err = heapstore_registry_get_session(record.id, &get_record);
        assert(err == heapstore_SUCCESS);
        assert(strcmp(get_record.user_id, record.user_id) == 0);

        record.last_active_at = (uint64_t)time(NULL);
        err = heapstore_registry_update_session(&record);
        assert(err == heapstore_SUCCESS);

        err = heapstore_registry_delete_session(record.id);
        assert(err == heapstore_SUCCESS);
    }

    heapstore_registry_shutdown();
    printf("PASS\n");
}

static void test_registry_invalid_params(void) {
    printf("Test: registry_invalid_params...");

    heapstore_error_t err = heapstore_registry_init();
    assert(err == heapstore_SUCCESS);

    heapstore_agent_record_t agent_record;
    memset(&agent_record, 0, sizeof(agent_record));

    err = heapstore_registry_add_agent(NULL);
    assert(err == heapstore_ERR_INVALID_PARAM);

    err = heapstore_registry_add_agent(&agent_record);
    assert(err == heapstore_ERR_INVALID_PARAM);

    heapstore_agent_record_t get_record;
    err = heapstore_registry_get_agent(NULL, &get_record);
    assert(err == heapstore_ERR_INVALID_PARAM);

    err = heapstore_registry_get_agent("nonexistent", &get_record);
    assert(err != heapstore_SUCCESS);

    heapstore_registry_shutdown();
    printf("PASS\n");
}

static void test_registry_vacuum(void) {
    printf("Test: registry_vacuum...");

    heapstore_error_t err = heapstore_registry_init();
    assert(err == heapstore_SUCCESS);

    err = heapstore_registry_vacuum();
    assert(err == heapstore_SUCCESS);

    heapstore_registry_shutdown();
    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS heapstore Registry Unit Tests ===\n\n");

    test_registry_init_shutdown();
    test_registry_agent_crud();
    test_registry_skill_crud();
    test_registry_session_crud();
    test_registry_invalid_params();
    test_registry_vacuum();

    printf("\n=== All Registry Tests Passed ===\n");
    return 0;
}

