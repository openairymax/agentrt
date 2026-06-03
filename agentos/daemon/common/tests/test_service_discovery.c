/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 * test_service_discovery.c - Service Discovery Module Unit Tests
 */

#include "../include/service_discovery.h"
#include "../include/safe_string_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-50s", name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); return; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)

static int callback_fired = 0;
static sd_event_type_t last_event = 0;

static void event_callback(sd_event_type_t event, const char *svc,
                           const sd_instance_t *inst, void *user_data)
{
    (void)svc;
    (void)inst;
    (void)user_data;
    callback_fired++;
    last_event = event;
}

static sd_instance_t make_instance(const char *id, const char *endpoint)
{
    sd_instance_t inst;
    memset(&inst, 0, sizeof(inst));
    safe_strcpy(inst.instance_id, id, SD_MAX_NAME_LEN);
    safe_strcpy(inst.endpoint, endpoint, SD_MAX_ENDPOINT_LEN);
    inst.healthy = true;
    inst.weight = 100;
    inst.max_connections = 1000;
    return inst;
}

/* ==================== 1. sd_create_default_config ==================== */

static void test_sd_create_default_config(void)
{
    TEST("sd_create_default_config - verify defaults");
    sd_config_t cfg = sd_create_default_config();
    ASSERT(cfg.heartbeat_interval_ms == SD_DEFAULT_HEARTBEAT_MS,
           "default heartbeat_interval_ms");
    ASSERT(cfg.expire_timeout_ms == SD_DEFAULT_EXPIRE_MS,
           "default expire_timeout_ms");
    ASSERT(cfg.default_lb_strategy == SD_LB_ROUND_ROBIN,
           "default lb strategy should be ROUND_ROBIN");
    ASSERT(cfg.enable_auto_expire == true,
           "auto expire should be enabled by default");
    ASSERT(cfg.enable_health_propagation == true,
           "health propagation should be enabled by default");
    ASSERT(cfg.shm_size == 1024 * 1024,
           "default shm_size should be 1MB");
    ASSERT(strcmp(cfg.shm_name, SD_SHM_NAME) == 0,
           "default shm_name should match SD_SHM_NAME");
    PASS();
}

/* ==================== 2. sd_create(NULL) ==================== */

static void test_sd_create_null_config(void)
{
    TEST("sd_create(NULL) - create with defaults");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "sd_create(NULL) should succeed");
    sd_destroy(sd);
    PASS();
}

/* ==================== 3. sd_create(config) ==================== */

static void test_sd_create_with_config(void)
{
    TEST("sd_create(config) - create with custom config");
    sd_config_t cfg = sd_create_default_config();
    cfg.heartbeat_interval_ms = 5000;
    cfg.expire_timeout_ms = 15000;
    cfg.default_lb_strategy = SD_LB_RANDOM;
    cfg.enable_auto_expire = false;
    cfg.enable_health_propagation = false;

    service_discovery_t sd = sd_create(&cfg);
    ASSERT(sd != NULL, "sd_create with config should succeed");
    sd_destroy(sd);
    PASS();
}

/* ==================== 4. sd_destroy(NULL) ==================== */

static void test_sd_destroy_null(void)
{
    TEST("sd_destroy(NULL) - safe null handling");
    sd_destroy(NULL);
    PASS();
}

/* ==================== 5. sd_start / sd_stop / sd_is_running lifecycle ==================== */

static void test_sd_lifecycle(void)
{
    TEST("sd_start/sd_stop/sd_is_running lifecycle");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    ASSERT(sd_is_running(sd) == false, "should not be running before start");

    int ret = sd_start(sd);
    ASSERT(ret == 0, "sd_start should succeed");
    ASSERT(sd_is_running(sd) == true, "should be running after start");

    ret = sd_stop(sd);
    ASSERT(ret == 0, "sd_stop should succeed");
    ASSERT(sd_is_running(sd) == false, "should not be running after stop");

    sd_destroy(sd);
    PASS();
}

/* ==================== 6. sd_start idempotent ==================== */

static void test_sd_start_idempotent(void)
{
    TEST("sd_start on already started - idempotent");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    int ret = sd_start(sd);
    ASSERT(ret == 0, "first start");
    ASSERT(sd_is_running(sd) == true, "should be running");

    ret = sd_start(sd);
    ASSERT(ret == 0, "second start should be idempotent");
    ASSERT(sd_is_running(sd) == true, "should still be running");

    sd_destroy(sd);
    PASS();
}

/* ==================== 7. sd_stop(NULL) ==================== */

static void test_sd_stop_null(void)
{
    TEST("sd_stop(NULL) - rejected");
    int ret = sd_stop(NULL);
    ASSERT(ret != 0, "sd_stop(NULL) should be rejected");
    PASS();
}

/* ==================== 8. sd_register - normal ==================== */

static void test_sd_register_normal(void)
{
    TEST("sd_register - normal registration");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    int ret = sd_register(sd, "auth-service", "auth",
                          &inst, "critical,prod", "db-service,log-service");
    ASSERT(ret == 0, "registration should succeed");

    sd_service_count(sd);
    ASSERT(sd_service_count(sd) == 1, "service count should be 1");

    sd_destroy(sd);
    PASS();
}

/* ==================== 9. sd_register - NULL parameter validation ==================== */

static void test_sd_register_null_params(void)
{
    TEST("sd_register - NULL parameter validation");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");
    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");

    int ret = sd_register(NULL, "svc", "type", &inst, "tags", "deps");
    ASSERT(ret != 0, "null sd should be rejected");

    ret = sd_register(sd, NULL, "type", &inst, "tags", "deps");
    ASSERT(ret != 0, "null service_name should be rejected");

    ret = sd_register(sd, "svc", NULL, &inst, "tags", "deps");
    ASSERT(ret != 0, "null service_type should be rejected");

    ret = sd_register(sd, "svc", "type", NULL, "tags", "deps");
    ASSERT(ret != 0, "null instance should be rejected");

    sd_destroy(sd);
    PASS();
}

/* ==================== 10. sd_deregister - normal ==================== */

static void test_sd_deregister_normal(void)
{
    TEST("sd_deregister - normal deregistration");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    int ret = sd_register(sd, "auth-service", "auth", &inst, "", "");
    ASSERT(ret == 0, "register");
    ASSERT(sd_service_count(sd) == 1, "count after register");

    ret = sd_deregister(sd, "auth-service", "inst-001");
    ASSERT(ret == 0, "deregister should succeed");

    sd_destroy(sd);
    PASS();
}

/* ==================== 11. sd_deregister - nonexistent service ==================== */

static void test_sd_deregister_nonexistent(void)
{
    TEST("sd_deregister - nonexistent service");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    int ret = sd_deregister(sd, "nonexistent", "inst-001");
    ASSERT(ret != 0, "deregister nonexistent should fail");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "real-svc", "type", &inst, "", "");

    ret = sd_deregister(sd, "real-svc", "nonexistent-inst");
    ASSERT(ret != 0, "deregister nonexistent instance should fail");

    sd_destroy(sd);
    PASS();
}

/* ==================== 12. sd_deregister_all ==================== */

static void test_sd_deregister_all(void)
{
    TEST("sd_deregister_all - deregister all instances");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst1 = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_instance_t inst2 = make_instance("inst-002", "tcp://127.0.0.1:8081");

    sd_register(sd, "web-service", "web", &inst1, "", "");
    sd_register(sd, "web-service", "web", &inst2, "", "");

    uint32_t found = 0;
    sd_instance_t discovered[8];
    sd_discover(sd, "web-service", discovered, 8, &found);
    ASSERT(found == 2, "should have 2 instances before deregister_all");

    int ret = sd_deregister_all(sd, "web-service");
    ASSERT(ret == 0, "deregister_all should succeed");

    sd_discover(sd, "web-service", discovered, 8, &found);
    ASSERT(found == 0, "should have 0 instances after deregister_all");

    sd_destroy(sd);
    PASS();
}

/* ==================== 13. sd_discover - discover registered instances ==================== */

static void test_sd_discover_normal(void)
{
    TEST("sd_discover - discover registered instances");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst1 = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_instance_t inst2 = make_instance("inst-002", "tcp://127.0.0.1:8081");

    sd_register(sd, "api-gateway", "gateway", &inst1, "", "");
    sd_register(sd, "api-gateway", "gateway", &inst2, "", "");

    sd_instance_t instances[8];
    uint32_t found = 0;
    int ret = sd_discover(sd, "api-gateway", instances, 8, &found);
    ASSERT(ret == 0, "discover should succeed");
    ASSERT(found == 2, "should find 2 instances");

    sd_destroy(sd);
    PASS();
}

/* ==================== 14. sd_discover - nonexistent service ==================== */

static void test_sd_discover_nonexistent(void)
{
    TEST("sd_discover - nonexistent service");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t instances[8];
    uint32_t found = 99;
    int ret = sd_discover(sd, "nonexistent", instances, 8, &found);
    ASSERT(ret != 0, "discover nonexistent should return error");
    ASSERT(found == 0, "found count should be 0 for nonexistent");

    sd_destroy(sd);
    PASS();
}

/* ==================== 15. sd_discover_by_type ==================== */

static void test_sd_discover_by_type(void)
{
    TEST("sd_discover_by_type - find services by type");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst1 = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_instance_t inst2 = make_instance("inst-002", "tcp://127.0.0.1:9090");

    sd_register(sd, "auth-svc-1", "auth", &inst1, "", "");
    sd_register(sd, "auth-svc-2", "auth", &inst2, "", "");
    sd_register(sd, "web-svc-1", "web", &inst1, "", "");

    sd_service_entry_t entries[8];
    uint32_t found = 0;
    int ret = sd_discover_by_type(sd, "auth", entries, 8, &found);
    ASSERT(ret == 0, "discover_by_type should succeed");
    ASSERT(found == 2, "should find 2 auth services");

    ret = sd_discover_by_type(sd, "web", entries, 8, &found);
    ASSERT(ret == 0, "discover_by_type web should succeed");
    ASSERT(found == 1, "should find 1 web service");

    ret = sd_discover_by_type(sd, "nonexistent", entries, 8, &found);
    ASSERT(ret == 0, "discover_by_type nonexistent should succeed with 0");
    ASSERT(found == 0, "should find 0 for unknown type");

    sd_destroy(sd);
    PASS();
}

/* ==================== 16. sd_discover_by_tags ==================== */

static void test_sd_discover_by_tags(void)
{
    TEST("sd_discover_by_tags - find services by tags");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");

    sd_register(sd, "svc-a", "typeA", &inst, "critical,frontend", "");
    sd_register(sd, "svc-b", "typeB", &inst, "critical,backend", "");
    sd_register(sd, "svc-c", "typeC", &inst, "optional,frontend", "");

    sd_service_entry_t entries[8];
    uint32_t found = 0;
    int ret = sd_discover_by_tags(sd, "critical", entries, 8, &found);
    ASSERT(ret == 0, "discover_by_tags should succeed");
    ASSERT(found == 2, "should find 2 critical services");

    ret = sd_discover_by_tags(sd, "frontend", entries, 8, &found);
    ASSERT(ret == 0, "discover_by_tags frontend should succeed");
    ASSERT(found == 2, "should find 2 frontend services");

    ret = sd_discover_by_tags(sd, "backend", entries, 8, &found);
    ASSERT(ret == 0, "discover_by_tags backend should succeed");
    ASSERT(found == 1, "should find 1 backend service");

    sd_destroy(sd);
    PASS();
}

/* ==================== 17. sd_select_instance - round_robin ==================== */

static void test_sd_select_instance_round_robin(void)
{
    TEST("sd_select_instance - round_robin strategy");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst1 = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_instance_t inst2 = make_instance("inst-002", "tcp://127.0.0.1:8081");

    sd_register(sd, "lb-svc", "test", &inst1, "", "");
    sd_register(sd, "lb-svc", "test", &inst2, "", "");

    sd_instance_t selected;
    memset(&selected, 0, sizeof(selected));
    int ret = sd_select_instance(sd, "lb-svc", SD_LB_ROUND_ROBIN, &selected);
    ASSERT(ret == 0, "first select should succeed");
    ASSERT(selected.instance_id[0] != '\0', "should select an instance");

    ret = sd_select_instance(sd, "lb-svc", SD_LB_ROUND_ROBIN, &selected);
    ASSERT(ret == 0, "second select should succeed");

    sd_destroy(sd);
    PASS();
}

/* ==================== 18. sd_select_instance - random ==================== */

static void test_sd_select_instance_random(void)
{
    TEST("sd_select_instance - random strategy");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "rand-svc", "test", &inst, "", "");

    sd_instance_t selected;
    memset(&selected, 0, sizeof(selected));
    int ret = sd_select_instance(sd, "rand-svc", SD_LB_RANDOM, &selected);
    ASSERT(ret == 0, "random select should succeed");
    ASSERT(strcmp(selected.instance_id, "inst-001") == 0,
           "should select the only instance");

    sd_destroy(sd);
    PASS();
}

/* ==================== 19. sd_select_instance - nonexistent ==================== */

static void test_sd_select_instance_nonexistent(void)
{
    TEST("sd_select_instance - nonexistent service");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t selected;
    int ret = sd_select_instance(sd, "nonexistent", SD_LB_ROUND_ROBIN, &selected);
    ASSERT(ret != 0, "select nonexistent should fail");

    sd_destroy(sd);
    PASS();
}

/* ==================== 20. sd_heartbeat - normal ==================== */

static void test_sd_heartbeat_normal(void)
{
    TEST("sd_heartbeat - normal heartbeat");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "hb-svc", "test", &inst, "", "");

    int ret = sd_heartbeat(sd, "hb-svc", "inst-001");
    ASSERT(ret == 0, "heartbeat should succeed");

    sd_destroy(sd);
    PASS();
}

/* ==================== 21. sd_heartbeat - nonexistent ==================== */

static void test_sd_heartbeat_nonexistent(void)
{
    TEST("sd_heartbeat - nonexistent service");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    int ret = sd_heartbeat(sd, "nonexistent", "inst-001");
    ASSERT(ret != 0, "heartbeat on nonexistent should fail");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "hb-svc", "test", &inst, "", "");

    ret = sd_heartbeat(sd, "hb-svc", "nonexistent-inst");
    ASSERT(ret != 0, "heartbeat on nonexistent instance should fail");

    sd_destroy(sd);
    PASS();
}

/* ==================== 22. sd_update_health ==================== */

static void test_sd_update_health(void)
{
    TEST("sd_update_health - mark unhealthy then healthy");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    inst.healthy = true;
    sd_register(sd, "health-svc", "test", &inst, "", "");

    int ret = sd_update_health(sd, "health-svc", "inst-001", false);
    ASSERT(ret == 0, "mark unhealthy should succeed");

    sd_instance_t instances[8];
    uint32_t found = 0;
    sd_discover(sd, "health-svc", instances, 8, &found);
    ASSERT(found == 0, "unhealthy instance should not be discoverable");

    ret = sd_update_health(sd, "health-svc", "inst-001", true);
    ASSERT(ret == 0, "mark healthy should succeed");

    sd_discover(sd, "health-svc", instances, 8, &found);
    ASSERT(found == 1, "healthy instance should be discoverable again");

    sd_destroy(sd);
    PASS();
}

/* ==================== 23. sd_update_connections ==================== */

static void test_sd_update_connections(void)
{
    TEST("sd_update_connections - update connections");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "conn-svc", "test", &inst, "", "");

    int ret = sd_update_connections(sd, "conn-svc", "inst-001", 42);
    ASSERT(ret == 0, "update connections should succeed");

    ret = sd_update_connections(sd, "nonexistent", "inst-001", 10);
    ASSERT(ret != 0, "update connections on nonexistent should fail");

    sd_destroy(sd);
    PASS();
}

/* ==================== 24. sd_get_dependencies ==================== */

static void test_sd_get_dependencies(void)
{
    TEST("sd_get_dependencies - retrieve dependencies");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "dep-svc", "test", &inst, "", "db-service,log-service,cache");

    char deps[SD_MAX_DEPS_LEN];
    memset(deps, 0, sizeof(deps));
    int ret = sd_get_dependencies(sd, "dep-svc", deps, sizeof(deps));
    ASSERT(ret == 0, "get_dependencies should succeed");
    ASSERT(strstr(deps, "db-service") != NULL, "should contain db-service");
    ASSERT(strstr(deps, "log-service") != NULL, "should contain log-service");
    ASSERT(strstr(deps, "cache") != NULL, "should contain cache");

    ret = sd_get_dependencies(sd, "nonexistent", deps, sizeof(deps));
    ASSERT(ret != 0, "get_dependencies on nonexistent should fail");

    sd_destroy(sd);
    PASS();
}

/* ==================== 25. sd_check_dependencies ==================== */

static void test_sd_check_dependencies(void)
{
    TEST("sd_check_dependencies - check dependency health");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "app-svc", "app", &inst, "", "db-service,cache-service");

    sd_instance_t db_inst = make_instance("db-001", "tcp://127.0.0.1:5432");
    sd_register(sd, "db-service", "db", &db_inst, "", "");

    char missing[SD_MAX_DEPS_LEN];
    memset(missing, 0, sizeof(missing));
    int ret = sd_check_dependencies(sd, "app-svc", missing, sizeof(missing));
    ASSERT(ret != 0, "check_deps should indicate missing deps");

    ret = sd_check_dependencies(sd, "app-svc", NULL, 0);
    ASSERT(ret != 0, "check_deps with NULL buffer should still report missing");

    sd_instance_t cache_inst = make_instance("cache-001", "tcp://127.0.0.1:6379");
    sd_register(sd, "cache-service", "cache", &cache_inst, "", "");

    memset(missing, 0, sizeof(missing));
    ret = sd_check_dependencies(sd, "app-svc", missing, sizeof(missing));
    ASSERT(ret == 0, "check_deps should succeed when all deps are present");

    sd_destroy(sd);
    PASS();
}

/* ==================== 26. sd_register_event_callback ==================== */

static void test_sd_register_event_callback(void)
{
    TEST("sd_register_event_callback - register callback");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    callback_fired = 0;
    last_event = 0;

    int ret = sd_register_event_callback(sd, event_callback, NULL);
    ASSERT(ret == 0, "register callback should succeed");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "cb-svc", "test", &inst, "", "");
    ASSERT(callback_fired >= 1, "callback should have been fired for register");
    ASSERT(last_event == SD_EVENT_REGISTERED, "should be REGISTERED event");

    int prev_fired = callback_fired;
    sd_deregister(sd, "cb-svc", "inst-001");
    ASSERT(callback_fired > prev_fired, "callback should fire for deregister");
    ASSERT(last_event == SD_EVENT_DEREGISTERED, "should be DEREGISTERED event");

    ret = sd_register_event_callback(NULL, event_callback, NULL);
    ASSERT(ret != 0, "register callback on NULL sd should fail");

    ret = sd_register_event_callback(sd, NULL, NULL);
    ASSERT(ret != 0, "register NULL callback should fail");

    sd_destroy(sd);
    PASS();
}

/* ==================== 27. sd_get_stats ==================== */

static void test_sd_get_stats(void)
{
    TEST("sd_get_stats - retrieve stats");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "stats-svc", "test", &inst, "", "");

    sd_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int ret = sd_get_stats(sd, &stats);
    ASSERT(ret == 0, "get_stats should succeed");
    ASSERT(stats.registrations >= 1, "should have at least 1 registration");
    ASSERT(stats.active_services >= 1, "should have at least 1 active service");
    ASSERT(stats.active_instances >= 1, "should have at least 1 active instance");

    ret = sd_get_stats(NULL, &stats);
    ASSERT(ret != 0, "get_stats on NULL should fail");

    ret = sd_get_stats(sd, NULL);
    ASSERT(ret != 0, "get_stats with NULL stats should fail");

    sd_destroy(sd);
    PASS();
}

/* ==================== 28. sd_service_count ==================== */

static void test_sd_service_count(void)
{
    TEST("sd_service_count - service count tracking");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    uint32_t count = sd_service_count(NULL);
    ASSERT(count == 0, "NULL sd should return 0");

    count = sd_service_count(sd);
    ASSERT(count == 0, "initial count should be 0");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "svc1", "type1", &inst, "", "");
    count = sd_service_count(sd);
    ASSERT(count == 1, "count should be 1 after first registration");

    sd_instance_t inst2 = make_instance("inst-002", "tcp://127.0.0.1:9090");
    sd_register(sd, "svc2", "type2", &inst2, "", "");
    count = sd_service_count(sd);
    ASSERT(count == 2, "count should be 2 after second registration");

    sd_deregister_all(sd, "svc1");
    count = sd_service_count(sd);
    ASSERT(count == 2, "deregister_all should not remove service entry");

    sd_destroy(sd);
    PASS();
}

/* ==================== 29. sd_lb_strategy_to_string ==================== */

static void test_sd_lb_strategy_to_string(void)
{
    TEST("sd_lb_strategy_to_string - all strategies");
    const char *s;

    s = sd_lb_strategy_to_string(SD_LB_ROUND_ROBIN);
    ASSERT(s != NULL && strcmp(s, "ROUND_ROBIN") == 0,
           "ROUND_ROBIN string");

    s = sd_lb_strategy_to_string(SD_LB_WEIGHTED);
    ASSERT(s != NULL && strcmp(s, "WEIGHTED") == 0,
           "WEIGHTED string");

    s = sd_lb_strategy_to_string(SD_LB_LEAST_CONNECTION);
    ASSERT(s != NULL && strcmp(s, "LEAST_CONNECTION") == 0,
           "LEAST_CONNECTION string");

    s = sd_lb_strategy_to_string(SD_LB_RANDOM);
    ASSERT(s != NULL && strcmp(s, "RANDOM") == 0,
           "RANDOM string");

    s = sd_lb_strategy_to_string(SD_LB_LEAST_LOAD);
    ASSERT(s != NULL && strcmp(s, "LEAST_LOAD") == 0,
           "LEAST_LOAD string");

    s = sd_lb_strategy_to_string((sd_lb_strategy_t)999);
    ASSERT(s != NULL && strcmp(s, "UNKNOWN") == 0,
           "invalid strategy should return UNKNOWN");

    s = sd_lb_strategy_to_string((sd_lb_strategy_t)-1);
    ASSERT(s != NULL && strcmp(s, "UNKNOWN") == 0,
           "negative strategy should return UNKNOWN");

    PASS();
}

/* ==================== 30. Multiple services and instances stress ==================== */

static void test_sd_multiple_services_stress(void)
{
    TEST("Multiple services and instances stress");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    const int num_services = 5;
    const int num_instances = 4;
    char svc_name[SD_MAX_NAME_LEN];
    char inst_id[SD_MAX_NAME_LEN];
    char endpoint[SD_MAX_ENDPOINT_LEN];

    for (int s = 0; s < num_services; s++) {
        snprintf(svc_name, sizeof(svc_name), "stress-svc-%d", s);
        for (int i = 0; i < num_instances; i++) {
            snprintf(inst_id, sizeof(inst_id), "inst-%d-%d", s, i);
            snprintf(endpoint, sizeof(endpoint), "tcp://127.0.0.1:%d", 8000 + s * 100 + i);
            sd_instance_t inst = make_instance(inst_id, endpoint);
            int ret = sd_register(sd, svc_name, "stress", &inst, "", "");
            ASSERT(ret == 0, "register should succeed");
        }
    }

    uint32_t count = sd_service_count(sd);
    ASSERT(count == (uint32_t)num_services, "should have all services");

    sd_instance_t instances[SD_MAX_INSTANCES];
    uint32_t found = 0;
    int ret = sd_discover(sd, "stress-svc-0", instances, SD_MAX_INSTANCES, &found);
    ASSERT(ret == 0, "discover should succeed");
    ASSERT(found == (uint32_t)num_instances, "should find all instances");

    sd_service_entry_t entries[SD_MAX_SERVICES];
    ret = sd_discover_by_type(sd, "stress", entries, SD_MAX_SERVICES, &found);
    ASSERT(ret == 0, "discover_by_type should succeed");
    ASSERT(found == (uint32_t)num_services, "should find all stress services");

    for (int s = 0; s < num_services; s++) {
        snprintf(svc_name, sizeof(svc_name), "stress-svc-%d", s);
        sd_deregister_all(sd, svc_name);
    }

    sd_destroy(sd);
    PASS();
}

/* ==================== 31. All null parameter safety checks ==================== */

static void test_sd_null_parameter_safety(void)
{
    TEST("All null parameter safety checks for each API");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    ASSERT(sd_start(NULL) != 0, "sd_start(NULL) rejected");
    ASSERT(sd_stop(NULL) != 0, "sd_stop(NULL) rejected");
    ASSERT(sd_is_running(NULL) == false, "sd_is_running(NULL) returns false");

    sd_instance_t inst = make_instance("inst-x", "tcp://1.2.3.4:80");
    ASSERT(sd_register(NULL, "s", "t", &inst, "", "") != 0, "sd_register null sd");
    ASSERT(sd_register(sd, NULL, "t", &inst, "", "") != 0, "sd_register null name");
    ASSERT(sd_register(sd, "s", NULL, &inst, "", "") != 0, "sd_register null type");
    ASSERT(sd_register(sd, "s", "t", NULL, "", "") != 0, "sd_register null inst");

    ASSERT(sd_deregister(NULL, "s", "i") != 0, "sd_deregister null sd");
    ASSERT(sd_deregister(sd, NULL, "i") != 0, "sd_deregister null name");
    ASSERT(sd_deregister(sd, "s", NULL) != 0, "sd_deregister null inst_id");

    ASSERT(sd_deregister_all(NULL, "s") != 0, "sd_deregister_all null sd");
    ASSERT(sd_deregister_all(sd, NULL) != 0, "sd_deregister_all null name");

    sd_instance_t buf[4];
    uint32_t found;
    ASSERT(sd_discover(NULL, "s", buf, 4, &found) != 0, "sd_discover null sd");
    ASSERT(sd_discover(sd, NULL, buf, 4, &found) != 0, "sd_discover null name");
    ASSERT(sd_discover(sd, "s", NULL, 4, &found) != 0, "sd_discover null buf");
    ASSERT(sd_discover(sd, "s", buf, 4, NULL) != 0, "sd_discover null found");

    sd_service_entry_t entries[4];
    ASSERT(sd_discover_by_type(NULL, "t", entries, 4, &found) != 0,
           "sd_discover_by_type null sd");
    ASSERT(sd_discover_by_type(sd, NULL, entries, 4, &found) != 0,
           "sd_discover_by_type null type");
    ASSERT(sd_discover_by_type(sd, "t", NULL, 4, &found) != 0,
           "sd_discover_by_type null entries");
    ASSERT(sd_discover_by_type(sd, "t", entries, 4, NULL) != 0,
           "sd_discover_by_type null found");

    ASSERT(sd_discover_by_tags(NULL, "t", entries, 4, &found) != 0,
           "sd_discover_by_tags null sd");
    ASSERT(sd_discover_by_tags(sd, NULL, entries, 4, &found) != 0,
           "sd_discover_by_tags null tags");
    ASSERT(sd_discover_by_tags(sd, "t", NULL, 4, &found) != 0,
           "sd_discover_by_tags null entries");
    ASSERT(sd_discover_by_tags(sd, "t", entries, 4, NULL) != 0,
           "sd_discover_by_tags null found");

    sd_instance_t sel;
    ASSERT(sd_select_instance(NULL, "s", SD_LB_ROUND_ROBIN, &sel) != 0,
           "sd_select_instance null sd");
    ASSERT(sd_select_instance(sd, NULL, SD_LB_ROUND_ROBIN, &sel) != 0,
           "sd_select_instance null name");
    ASSERT(sd_select_instance(sd, "s", SD_LB_ROUND_ROBIN, NULL) != 0,
           "sd_select_instance null instance");

    ASSERT(sd_heartbeat(NULL, "s", "i") != 0, "sd_heartbeat null sd");
    ASSERT(sd_heartbeat(sd, NULL, "i") != 0, "sd_heartbeat null name");
    ASSERT(sd_heartbeat(sd, "s", NULL) != 0, "sd_heartbeat null inst_id");

    ASSERT(sd_update_health(NULL, "s", "i", true) != 0, "sd_update_health null sd");
    ASSERT(sd_update_health(sd, NULL, "i", true) != 0, "sd_update_health null name");
    ASSERT(sd_update_health(sd, "s", NULL, true) != 0, "sd_update_health null inst_id");

    ASSERT(sd_update_connections(NULL, "s", "i", 10) != 0,
           "sd_update_connections null sd");
    ASSERT(sd_update_connections(sd, NULL, "i", 10) != 0,
           "sd_update_connections null name");
    ASSERT(sd_update_connections(sd, "s", NULL, 10) != 0,
           "sd_update_connections null inst_id");

    ASSERT(sd_get_dependencies(NULL, "s", NULL, 0) != 0,
           "sd_get_dependencies null sd");
    ASSERT(sd_get_dependencies(sd, NULL, NULL, 0) != 0,
           "sd_get_dependencies null name");

    ASSERT(sd_check_dependencies(NULL, "s", NULL, 0) != 0,
           "sd_check_dependencies null sd");
    ASSERT(sd_check_dependencies(sd, NULL, NULL, 0) != 0,
           "sd_check_dependencies null name");

    ASSERT(sd_register_event_callback(NULL, event_callback, NULL) != 0,
           "sd_register_event_callback null sd");
    ASSERT(sd_register_event_callback(sd, NULL, NULL) != 0,
           "sd_register_event_callback null callback");

    ASSERT(sd_get_stats(NULL, NULL) != 0, "sd_get_stats null sd");

    ASSERT(sd_service_count(NULL) == 0, "sd_service_count null returns 0");

    sd_destroy(sd);
    PASS();
}

/* ==================== 32. sd_select_instance - weighted ==================== */

static void test_sd_select_instance_weighted(void)
{
    TEST("sd_select_instance - weighted strategy");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    inst.weight = 100;
    sd_register(sd, "weight-svc", "test", &inst, "", "");

    sd_instance_t selected;
    memset(&selected, 0, sizeof(selected));
    int ret = sd_select_instance(sd, "weight-svc", SD_LB_WEIGHTED, &selected);
    ASSERT(ret == 0, "weighted select should succeed");
    ASSERT(strcmp(selected.instance_id, "inst-001") == 0,
           "should select the only instance");

    sd_destroy(sd);
    PASS();
}

/* ==================== 33. sd_select_instance - least_connection ==================== */

static void test_sd_select_instance_least_connection(void)
{
    TEST("sd_select_instance - least_connection strategy");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst1 = make_instance("inst-001", "tcp://127.0.0.1:8080");
    inst1.active_connections = 10;
    sd_register(sd, "lc-svc", "test", &inst1, "", "");

    sd_instance_t inst2 = make_instance("inst-002", "tcp://127.0.0.1:8081");
    inst2.active_connections = 5;
    sd_register(sd, "lc-svc", "test", &inst2, "", "");

    sd_instance_t selected;
    memset(&selected, 0, sizeof(selected));
    int ret = sd_select_instance(sd, "lc-svc", SD_LB_LEAST_CONNECTION, &selected);
    ASSERT(ret == 0, "least_connection select should succeed");
    ASSERT(strcmp(selected.instance_id, "inst-002") == 0,
           "should select instance with fewer connections");

    sd_destroy(sd);
    PASS();
}

/* ==================== 34. sd_select_instance - least_load ==================== */

static void test_sd_select_instance_least_load(void)
{
    TEST("sd_select_instance - least_load strategy");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst1 = make_instance("inst-001", "tcp://127.0.0.1:8080");
    inst1.max_connections = 100;
    inst1.active_connections = 10;
    sd_register(sd, "ll-svc", "test", &inst1, "", "");

    sd_instance_t inst2 = make_instance("inst-002", "tcp://127.0.0.1:8081");
    inst2.max_connections = 100;
    inst2.active_connections = 50;
    sd_register(sd, "ll-svc", "test", &inst2, "", "");

    sd_instance_t selected;
    memset(&selected, 0, sizeof(selected));
    int ret = sd_select_instance(sd, "ll-svc", SD_LB_LEAST_LOAD, &selected);
    ASSERT(ret == 0, "least_load select should succeed");
    ASSERT(strcmp(selected.instance_id, "inst-001") == 0,
           "should select instance with lower load percentage");

    sd_destroy(sd);
    PASS();
}

/* ==================== 35. sd_discover with max_count limit ==================== */

static void test_sd_discover_max_count(void)
{
    TEST("sd_discover - max_count limit");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst1 = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_instance_t inst2 = make_instance("inst-002", "tcp://127.0.0.1:8081");
    sd_instance_t inst3 = make_instance("inst-003", "tcp://127.0.0.1:8082");

    sd_register(sd, "limit-svc", "test", &inst1, "", "");
    sd_register(sd, "limit-svc", "test", &inst2, "", "");
    sd_register(sd, "limit-svc", "test", &inst3, "", "");

    sd_instance_t instances[2];
    uint32_t found = 0;
    int ret = sd_discover(sd, "limit-svc", instances, 2, &found);
    ASSERT(ret == 0, "discover should succeed");
    ASSERT(found == 2, "should return at most 2 instances");

    sd_destroy(sd);
    PASS();
}

/* ==================== 36. Callback on health changes ==================== */

static void test_sd_callback_health_changes(void)
{
    TEST("Callback on health changes");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    callback_fired = 0;
    last_event = 0;
    sd_register_event_callback(sd, event_callback, NULL);

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    inst.healthy = true;
    sd_register(sd, "hc-svc", "test", &inst, "", "");

    int prev = callback_fired;
    sd_update_health(sd, "hc-svc", "inst-001", false);
    ASSERT(callback_fired > prev, "callback should fire on health down");
    ASSERT(last_event == SD_EVENT_INSTANCE_DOWN, "should be INSTANCE_DOWN");

    prev = callback_fired;
    sd_update_health(sd, "hc-svc", "inst-001", true);
    ASSERT(callback_fired > prev, "callback should fire on health up");
    ASSERT(last_event == SD_EVENT_INSTANCE_UP, "should be INSTANCE_UP");

    sd_destroy(sd);
    PASS();
}

/* ==================== 37. sd_deregister_all - nonexistent service ==================== */

static void test_sd_deregister_all_nonexistent(void)
{
    TEST("sd_deregister_all - nonexistent service");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    int ret = sd_deregister_all(sd, "nonexistent");
    ASSERT(ret != 0, "deregister_all on nonexistent should fail");

    sd_destroy(sd);
    PASS();
}

/* ==================== 38. sd_discover returns only healthy ==================== */

static void test_sd_discover_only_healthy(void)
{
    TEST("sd_discover - returns only healthy instances");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t inst_healthy = make_instance("inst-h", "tcp://127.0.0.1:8080");
    inst_healthy.healthy = true;

    sd_instance_t inst_unhealthy = make_instance("inst-u", "tcp://127.0.0.1:8081");
    inst_unhealthy.healthy = false;

    sd_register(sd, "mixed-svc", "test", &inst_healthy, "", "");
    sd_register(sd, "mixed-svc", "test", &inst_unhealthy, "", "");

    sd_instance_t instances[8];
    uint32_t found = 0;
    int ret = sd_discover(sd, "mixed-svc", instances, 8, &found);
    ASSERT(ret == 0, "discover should succeed");
    ASSERT(found == 1, "should return only healthy instances");
    ASSERT(strcmp(instances[0].instance_id, "inst-h") == 0,
           "returned instance should be the healthy one");

    sd_destroy(sd);
    PASS();
}

/* ==================== 39. Multiple callbacks ==================== */

static void test_sd_multiple_callbacks(void)
{
    TEST("Multiple event callbacks");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    callback_fired = 0;

    int ret = sd_register_event_callback(sd, event_callback, NULL);
    ASSERT(ret == 0, "register first callback");
    ret = sd_register_event_callback(sd, event_callback, NULL);
    ASSERT(ret == 0, "register second callback");

    sd_instance_t inst = make_instance("inst-001", "tcp://127.0.0.1:8080");
    sd_register(sd, "multi-cb-svc", "test", &inst, "", "");

    ASSERT(callback_fired >= 2, "both callbacks should be fired");

    sd_destroy(sd);
    PASS();
}

/* ==================== 40. sd_discover_by_type - null params ==================== */

static void test_sd_discover_null_params(void)
{
    TEST("sd_discover - null parameter safety");
    service_discovery_t sd = sd_create(NULL);
    ASSERT(sd != NULL, "create");

    sd_instance_t buf[4];
    uint32_t found;
    ASSERT(sd_discover(NULL, "s", buf, 4, &found) != 0,
           "sd_discover null sd should fail");
    ASSERT(sd_discover(sd, NULL, buf, 4, &found) != 0,
           "sd_discover null name should fail");
    ASSERT(sd_discover(sd, "s", NULL, 4, &found) != 0,
           "sd_discover null instances should fail");
    ASSERT(sd_discover(sd, "s", buf, 4, NULL) != 0,
           "sd_discover null found_count should fail");

    sd_destroy(sd);
    PASS();
}

int main(void)
{
    printf("\n=== Service Discovery Module Unit Tests ===\n\n");

    test_sd_create_default_config();
    test_sd_create_null_config();
    test_sd_create_with_config();
    test_sd_destroy_null();
    test_sd_lifecycle();
    test_sd_start_idempotent();
    test_sd_stop_null();
    test_sd_register_normal();
    test_sd_register_null_params();
    test_sd_deregister_normal();
    test_sd_deregister_nonexistent();
    test_sd_deregister_all();
    test_sd_deregister_all_nonexistent();
    test_sd_discover_normal();
    test_sd_discover_nonexistent();
    test_sd_discover_max_count();
    test_sd_discover_only_healthy();
    test_sd_discover_null_params();
    test_sd_discover_by_type();
    test_sd_discover_by_tags();
    test_sd_select_instance_round_robin();
    test_sd_select_instance_random();
    test_sd_select_instance_weighted();
    test_sd_select_instance_least_connection();
    test_sd_select_instance_least_load();
    test_sd_select_instance_nonexistent();
    test_sd_heartbeat_normal();
    test_sd_heartbeat_nonexistent();
    test_sd_update_health();
    test_sd_callback_health_changes();
    test_sd_update_connections();
    test_sd_get_dependencies();
    test_sd_check_dependencies();
    test_sd_register_event_callback();
    test_sd_multiple_callbacks();
    test_sd_get_stats();
    test_sd_service_count();
    test_sd_lb_strategy_to_string();
    test_sd_multiple_services_stress();
    test_sd_null_parameter_safety();

    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}