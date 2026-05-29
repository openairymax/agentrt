/* SPDX-License-Identifier: Apache-2.0 */

#include "multi_agent_collaboration.h"
#include "platform.h"
#include "error.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_PASS(name)              \
    do {                             \
        g_tests_passed++;            \
        printf("[PASS] %s\n", name); \
    } while (0)
#define TEST_FAIL(name, msg)                  \
    do {                                      \
        g_tests_failed++;                     \
        printf("[FAIL] %s: %s\n", name, msg); \
    } while (0)
#define RUN_TEST(func) \
    do {               \
        g_tests_run++; \
        func();        \
    } while (0)

static void register_test_agents(mac_framework_t *fw, int count, const char *prefix)
{
    mac_agent_info_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.performance_score = 0.9;
    agent.reliability_score = 0.95;
    agent.max_concurrent_tasks = 8;
    agent.available = true;
    agent.capabilities_json = NULL;
    for (int i = 0; i < count; i++) {
        snprintf(agent.id, sizeof(agent.id), "%s_%03d", prefix, i);
        snprintf(agent.name, sizeof(agent.name), "%s_%03d", prefix, i);
        mac_framework_register_agent(fw, &agent);
    }
}

static void test_mac_create_destroy(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    assert(fw != NULL);
    mac_framework_destroy(fw);
    TEST_PASS("mac create/destroy");
}

static void test_mac_register_unregister(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_COLLABORATIVE);
    assert(fw != NULL);

    mac_agent_info_t agent;
    memset(&agent, 0, sizeof(agent));
    snprintf(agent.id, sizeof(agent.id), "test_agent_001");
    snprintf(agent.name, sizeof(agent.name), "TestAgent");
    agent.performance_score = 0.8;
    agent.reliability_score = 0.9;
    agent.max_concurrent_tasks = 4;
    agent.available = true;

    int __attribute__((unused)) rc = mac_framework_register_agent(fw, &agent);
    assert(rc == 0);
    assert(mac_framework_get_agent_count(fw) == 1);

    int rc2 __attribute__((unused)) = mac_framework_unregister_agent(fw, "test_agent_001");
    assert(rc2 == 0);
    assert(mac_framework_get_agent_count(fw) == 0);

    mac_framework_destroy(fw);
    TEST_PASS("mac register/unregister agent");
}

static void test_mac_group_create_disband(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    register_test_agents(fw, 5, "grp_agent");

    const char *ids[5];
    char id_bufs[5][32];
    for (int i = 0; i < 5; i++) {
        snprintf(id_bufs[i], sizeof(id_bufs[i]), "grp_agent_%03d", i);
        ids[i] = id_bufs[i];
    }

    char *gid = NULL;
    int __attribute__((unused)) rc =
        mac_framework_create_group(fw, "test_group", MAC_MODE_CONSENSUS, ids, 5, &gid);
    assert(rc == 0);
    assert(gid != NULL);
    assert(mac_framework_get_group_count(fw) == 1);

    mac_framework_disband_group(fw, gid);
    assert(mac_framework_get_group_count(fw) == 0);

    free(gid);
    mac_framework_destroy(fw);
    TEST_PASS("mac group create/disband");
}

static void test_consensus_majority_approve(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    register_test_agents(fw, 10, "maj_agent");

    char *cid = NULL;
    mac_framework_start_consensus(fw, NULL, "{\"action\":\"deploy\"}", MAC_CONSENSUS_MAJORITY,
                                  &cid);
    assert(cid != NULL);

    for (int i = 0; i < 7; i++) {
        char agent_id[32], vote[64];
        snprintf(agent_id, sizeof(agent_id), "maj_agent_%03d", i);
        snprintf(vote, sizeof(vote), "{\"agent_id\":\"maj_agent_%03d\",\"vote\":\"approve\"}", i);
        mac_framework_vote(fw, cid, agent_id, vote);
    }
    for (int i = 7; i < 10; i++) {
        char agent_id[32], vote[64];
        snprintf(agent_id, sizeof(agent_id), "maj_agent_%03d", i);
        snprintf(vote, sizeof(vote), "{\"agent_id\":\"maj_agent_%03d\",\"vote\":\"reject\"}", i);
        mac_framework_vote(fw, cid, agent_id, vote);
    }

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    assert(result != NULL);
    assert(strstr(result, "rejected") == NULL);

    free(cid);
    free(result);
    mac_framework_destroy(fw);
    TEST_PASS("consensus majority approve (7/10)");
}

static void test_consensus_majority_reject(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    register_test_agents(fw, 10, "majr_agent");

    char *cid = NULL;
    mac_framework_start_consensus(fw, NULL, "{\"action\":\"deploy\"}", MAC_CONSENSUS_MAJORITY,
                                  &cid);

    for (int i = 0; i < 3; i++) {
        char agent_id[32], vote[64];
        snprintf(agent_id, sizeof(agent_id), "majr_agent_%03d", i);
        snprintf(vote, sizeof(vote), "{\"agent_id\":\"majr_agent_%03d\",\"vote\":\"approve\"}", i);
        mac_framework_vote(fw, cid, agent_id, vote);
    }
    for (int i = 3; i < 10; i++) {
        char agent_id[32], vote[64];
        snprintf(agent_id, sizeof(agent_id), "majr_agent_%03d", i);
        snprintf(vote, sizeof(vote), "{\"agent_id\":\"majr_agent_%03d\",\"vote\":\"reject\"}", i);
        mac_framework_vote(fw, cid, agent_id, vote);
    }

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    assert(result != NULL);
    assert(strstr(result, "rejected") != NULL);

    free(cid);
    free(result);
    mac_framework_destroy(fw);
    TEST_PASS("consensus majority reject (3/10)");
}

static void test_consensus_unanimous_approve(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    register_test_agents(fw, 5, "unan_agent");

    char *cid = NULL;
    mac_framework_start_consensus(fw, NULL, "{\"action\":\"deploy\"}", MAC_CONSENSUS_UNANIMOUS,
                                  &cid);

    for (int i = 0; i < 5; i++) {
        char agent_id[32], vote[64];
        snprintf(agent_id, sizeof(agent_id), "unan_agent_%03d", i);
        snprintf(vote, sizeof(vote), "{\"agent_id\":\"unan_agent_%03d\",\"vote\":\"approve\"}", i);
        mac_framework_vote(fw, cid, agent_id, vote);
    }

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    assert(result != NULL);
    assert(strstr(result, "rejected") == NULL);

    free(cid);
    free(result);
    mac_framework_destroy(fw);
    TEST_PASS("consensus unanimous approve (5/5)");
}

static void test_consensus_unanimous_reject(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    register_test_agents(fw, 5, "unanr_agent");

    char *cid = NULL;
    mac_framework_start_consensus(fw, NULL, "{\"action\":\"deploy\"}", MAC_CONSENSUS_UNANIMOUS,
                                  &cid);

    for (int i = 0; i < 4; i++) {
        char agent_id[32], vote[64];
        snprintf(agent_id, sizeof(agent_id), "unanr_agent_%03d", i);
        snprintf(vote, sizeof(vote), "{\"agent_id\":\"unanr_agent_%03d\",\"vote\":\"approve\"}", i);
        mac_framework_vote(fw, cid, agent_id, vote);
    }
    char agent_id[32], vote[64];
    snprintf(agent_id, sizeof(agent_id), "unanr_agent_004");
    snprintf(vote, sizeof(vote), "{\"agent_id\":\"unanr_agent_004\",\"vote\":\"reject\"}");
    mac_framework_vote(fw, cid, agent_id, vote);

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    assert(result != NULL);
    assert(strstr(result, "rejected") != NULL);

    free(cid);
    free(result);
    mac_framework_destroy(fw);
    TEST_PASS("consensus unanimous reject (1 veto)");
}

static void test_consensus_weighted_approve(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);

    mac_agent_info_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.max_concurrent_tasks = 8;
    agent.available = true;
    agent.capabilities_json = NULL;

    snprintf(agent.id, sizeof(agent.id), "whi_000");
    snprintf(agent.name, sizeof(agent.name), "HighPerf");
    agent.performance_score = 0.95;
    agent.reliability_score = 0.98;
    mac_framework_register_agent(fw, &agent);

    snprintf(agent.id, sizeof(agent.id), "wlo_001");
    snprintf(agent.name, sizeof(agent.name), "LowPerf");
    agent.performance_score = 0.3;
    agent.reliability_score = 0.4;
    mac_framework_register_agent(fw, &agent);

    char *cid = NULL;
    mac_framework_start_consensus(fw, NULL, "{\"action\":\"deploy\"}", MAC_CONSENSUS_WEIGHTED,
                                  &cid);

    char vote1[64], vote2[64];
    snprintf(vote1, sizeof(vote1), "{\"agent_id\":\"whi_000\",\"vote\":\"approve\"}");
    mac_framework_vote(fw, cid, "whi_000", vote1);
    snprintf(vote2, sizeof(vote2), "{\"agent_id\":\"wlo_001\",\"vote\":\"reject\"}");
    mac_framework_vote(fw, cid, "wlo_001", vote2);

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    assert(result != NULL);
    assert(strstr(result, "rejected") == NULL);

    free(cid);
    free(result);
    mac_framework_destroy(fw);
    TEST_PASS("consensus weighted approve (high-weight approve)");
}

static void test_consensus_leader_approve(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    register_test_agents(fw, 5, "ldr_agent");

    const char *ids[5];
    char id_bufs[5][32];
    for (int i = 0; i < 5; i++) {
        snprintf(id_bufs[i], sizeof(id_bufs[i]), "ldr_agent_%03d", i);
        ids[i] = id_bufs[i];
    }

    char *gid = NULL;
    mac_framework_create_group(fw, "leader_group", MAC_MODE_CONSENSUS, ids, 5, &gid);

    char *cid = NULL;
    mac_framework_start_consensus(fw, gid, "{\"action\":\"deploy\"}", MAC_CONSENSUS_LEADER, &cid);

    char vote[64];
    snprintf(vote, sizeof(vote), "{\"agent_id\":\"ldr_agent_000\",\"vote\":\"approve\"}");
    mac_framework_vote(fw, cid, "ldr_agent_000", vote);

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    assert(result != NULL);
    assert(strstr(result, "rejected") == NULL);

    free(cid);
    free(result);
    free(gid);
    mac_framework_destroy(fw);
    TEST_PASS("consensus leader approve (leader approves)");
}

static void test_consensus_leader_reject(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    register_test_agents(fw, 5, "ldrr_agent");

    const char *ids[5];
    char id_bufs[5][32];
    for (int i = 0; i < 5; i++) {
        snprintf(id_bufs[i], sizeof(id_bufs[i]), "ldrr_agent_%03d", i);
        ids[i] = id_bufs[i];
    }

    char *gid = NULL;
    mac_framework_create_group(fw, "leader_reject_group", MAC_MODE_CONSENSUS, ids, 5, &gid);

    char *cid = NULL;
    mac_framework_start_consensus(fw, gid, "{\"action\":\"deploy\"}", MAC_CONSENSUS_LEADER, &cid);

    for (int i = 1; i < 5; i++) {
        char agent_id[32], vote[64];
        snprintf(agent_id, sizeof(agent_id), "ldrr_agent_%03d", i);
        snprintf(vote, sizeof(vote), "{\"agent_id\":\"ldrr_agent_%03d\",\"vote\":\"approve\"}", i);
        mac_framework_vote(fw, cid, agent_id, vote);
    }
    char vote[64];
    snprintf(vote, sizeof(vote), "{\"agent_id\":\"ldrr_agent_000\",\"vote\":\"reject\"}");
    mac_framework_vote(fw, cid, "ldrr_agent_000", vote);

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    assert(result != NULL);
    assert(strstr(result, "rejected") != NULL);

    free(cid);
    free(result);
    free(gid);
    mac_framework_destroy(fw);
    TEST_PASS("consensus leader reject (leader rejects despite majority approve)");
}

static void test_consensus_zero_votes(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    register_test_agents(fw, 5, "zero_agent");

    char *cid = NULL;
    mac_framework_start_consensus(fw, NULL, "{\"action\":\"deploy\"}", MAC_CONSENSUS_MAJORITY,
                                  &cid);

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    assert(result != NULL);
    assert(strstr(result, "rejected") != NULL);

    free(cid);
    free(result);
    mac_framework_destroy(fw);
    TEST_PASS("consensus zero votes rejected");
}

static void test_consensus_vote_after_resolve(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    register_test_agents(fw, 5, "resolved_agent");

    char *cid = NULL;
    mac_framework_start_consensus(fw, NULL, "{\"action\":\"deploy\"}", MAC_CONSENSUS_MAJORITY,
                                  &cid);

    char vote[64];
    snprintf(vote, sizeof(vote), "{\"agent_id\":\"resolved_agent_000\",\"vote\":\"approve\"}");
    mac_framework_vote(fw, cid, "resolved_agent_000", vote);

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    free(result);

    char vote2[64];
    snprintf(vote2, sizeof(vote2), "{\"agent_id\":\"resolved_agent_001\",\"vote\":\"approve\"}");
    int __attribute__((unused)) rc = mac_framework_vote(fw, cid, "resolved_agent_001", vote2);
    assert(rc == AGENTOS_ERR_NOT_FOUND);

    free(cid);
    mac_framework_destroy(fw);
    TEST_PASS("consensus vote after resolve rejected (-2)");
}

static void test_consensus_pure_text_vote(void)
{
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    register_test_agents(fw, 5, "text_agent");

    char *cid = NULL;
    mac_framework_start_consensus(fw, NULL, "{\"action\":\"deploy\"}", MAC_CONSENSUS_MAJORITY,
                                  &cid);

    for (int i = 0; i < 4; i++) {
        char agent_id[32];
        snprintf(agent_id, sizeof(agent_id), "text_agent_%03d", i);
        mac_framework_vote(fw, cid, agent_id, "approve");
    }

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    assert(result != NULL);
    assert(strstr(result, "rejected") == NULL);

    free(cid);
    free(result);
    mac_framework_destroy(fw);
    TEST_PASS("consensus pure text vote (approve)");
}

int main(void)
{
    printf("\n========================================\n");
    printf("  MultiAgent Collaboration Unit Tests\n");
    printf("========================================\n\n");

    RUN_TEST(test_mac_create_destroy);
    RUN_TEST(test_mac_register_unregister);
    RUN_TEST(test_mac_group_create_disband);
    RUN_TEST(test_consensus_majority_approve);
    RUN_TEST(test_consensus_majority_reject);
    RUN_TEST(test_consensus_unanimous_approve);
    RUN_TEST(test_consensus_unanimous_reject);
    RUN_TEST(test_consensus_weighted_approve);
    RUN_TEST(test_consensus_leader_approve);
    RUN_TEST(test_consensus_leader_reject);
    RUN_TEST(test_consensus_zero_votes);
    RUN_TEST(test_consensus_vote_after_resolve);
    RUN_TEST(test_consensus_pure_text_vote);

    printf("\n========================================\n");
    printf("  Results: %d run, %d passed, %d failed\n", g_tests_run, g_tests_passed,
           g_tests_failed);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
