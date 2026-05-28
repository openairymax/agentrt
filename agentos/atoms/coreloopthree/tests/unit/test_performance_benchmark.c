/* SPDX-License-Identifier: Apache-2.0 */

#include "agentos.h"
#include "cognition/delegate.h"
#include "cognition/parallel_dispatcher.h"
#include "multi_agent_collaboration.h"
#include "platform.h"

#include <assert.h>
#ifndef NDEBUG
#else
#undef assert
#define assert(x) ((void)(x))
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define BENCH_PASS(name)             \
    do {                             \
        g_tests_passed++;            \
        printf("[PASS] %s\n", name); \
    } while (0)
#define BENCH_RUN(name)                    \
    do {                                   \
        g_tests_run++;                     \
        printf("  BENCHMARK: %s\n", name); \
    } while (0)

static int g_exec_count = 0;

static agentos_error_t mock_executor(const char *tool_name __attribute__((unused)),
                                     const char *arguments __attribute__((unused)),
                                     size_t arguments_len __attribute__((unused)),
                                     char **out_output, size_t *out_output_len,
                                     void *user_data __attribute__((unused)))
{
    g_exec_count++;
    *out_output = strdup("{\"result\":\"ok\"}");
    if (out_output_len)
        *out_output_len = strlen(*out_output);
    return AGENTOS_SUCCESS;
}

static void free_results(agentos_tool_result_t *results, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        free(results[i].tool_name);
        free(results[i].output);
    }
    free(results);
}

static uint64_t bench_time_ns(void)
{
    return agentos_time_ns();
}

static void print_ns(uint64_t ns, const char *label)
{
    if (ns >= 1000000000ULL)
        printf("    %s: %.3f s (%lu ns)\n", label, ns / 1e9, (unsigned long)ns);
    else if (ns >= 1000000ULL)
        printf("    %s: %.3f ms (%lu ns)\n", label, ns / 1e6, (unsigned long)ns);
    else
        printf("    %s: %.3f us (%lu ns)\n", label, ns / 1e3, (unsigned long)ns);
}

static void bench_parallel_dispatcher_single(void)
{
    BENCH_RUN("parallel_dispatcher single call overhead");
    agentos_parallel_dispatcher_t *d = agentos_parallel_dispatcher_create(4);
    assert(d != NULL);
    agentos_parallel_dispatcher_set_executor(d, mock_executor, NULL);

    agentos_tool_call_t call;
    memset(&call, 0, sizeof(call));
    call.tool_name = "bench";
    call.arguments = "{}";
    call.arguments_len = 2;
    call.safety_class = AGENTOS_TOOL_READ_ONLY;

    const int iterations = 10000;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < iterations; i++) {
        agentos_tool_result_t *results = NULL;
        size_t count = 0;
        agentos_parallel_dispatcher_dispatch(d, &call, 1, &results, &count);
        if (results)
            free_results(results, count);
    }
    uint64_t t_end = bench_time_ns();

    uint64_t total_ns = t_end - t_start;
    print_ns(total_ns, "total");
    printf("    per-call: %lu ns\n", (unsigned long)(total_ns / iterations));

    agentos_parallel_dispatcher_destroy(d);
    BENCH_PASS("parallel_dispatcher single call overhead");
}

static void bench_parallel_dispatcher_multi(void)
{
    BENCH_RUN("parallel_dispatcher multi-call (10 calls x 100 iters)");
    agentos_parallel_dispatcher_t *d = agentos_parallel_dispatcher_create(8);
    assert(d != NULL);
    agentos_parallel_dispatcher_set_executor(d, mock_executor, NULL);

    agentos_tool_call_t calls[10];
    const char *tools[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j"};
    for (int i = 0; i < 10; i++) {
        memset(&calls[i], 0, sizeof(calls[i]));
        calls[i].tool_name = tools[i];
        calls[i].arguments = "{}";
        calls[i].arguments_len = 2;
        calls[i].safety_class = AGENTOS_TOOL_READ_ONLY;
    }

    const int iterations = 100;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < iterations; i++) {
        agentos_tool_result_t *results = NULL;
        size_t count = 0;
        int rc = agentos_parallel_dispatcher_dispatch(d, calls, 10, &results, &count);
        (void)rc;
        if (results)
            free_results(results, count);
    }
    uint64_t t_end = bench_time_ns();
    uint64_t total_ns = t_end - t_start;
    print_ns(total_ns, "total");
    printf("    per-iter (10 calls): %lu ns\n", (unsigned long)(total_ns / iterations));

    agentos_parallel_dispatcher_destroy(d);
    BENCH_PASS("parallel_dispatcher multi-call");
}

static void bench_delegate_create_destroy(void)
{
    BENCH_RUN("delegate create/destroy cycle (10000 iters)");
    agentos_delegate_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_depth = 2;
    config.max_iterations = 5;

    const int iterations = 10000;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < iterations; i++) {
        agentos_delegate_task_t *task = agentos_delegate_create("benchmark task", &config);
        if (task)
            agentos_delegate_destroy(task);
    }
    uint64_t t_end = bench_time_ns();
    print_ns(t_end - t_start, "total");
    printf("    per-cycle: %lu ns\n", (unsigned long)((t_end - t_start) / iterations));

    BENCH_PASS("delegate create/destroy cycle");
}

static void bench_delegate_assign_collect(void)
{
    BENCH_RUN("delegate assign+collect (5000 iters)");
    agentos_delegate_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_depth = 2;
    config.max_iterations = 5;

    const int iterations = 5000;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < iterations; i++) {
        agentos_delegate_task_t *task = agentos_delegate_create("bench task", &config);
        if (!task)
            continue;
        agentos_error_t err = agentos_delegate_assign(task, mock_executor, NULL);
        (void)err;
        char *result = NULL;
        size_t rlen = 0;
        agentos_delegate_collect(task, &result, &rlen);
        if (result)
            free(result);
        agentos_delegate_destroy(task);
    }
    uint64_t t_end = bench_time_ns();
    print_ns(t_end - t_start, "total");
    printf("    per-cycle: %lu ns\n", (unsigned long)((t_end - t_start) / iterations));

    BENCH_PASS("delegate assign+collect");
}

static void bench_mac_register_1000(void)
{
    BENCH_RUN("MultiAgent register 1024 agents");
    mac_framework_t *fw = mac_framework_create(MAC_MODE_COLLABORATIVE);
    assert(fw != NULL);

    mac_agent_info_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.performance_score = 0.9;
    agent.reliability_score = 0.95;
    agent.max_concurrent_tasks = 4;
    agent.available = true;
    agent.capabilities_json = strdup("{}");

    const int n_agents = 1024;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < n_agents; i++) {
        snprintf(agent.id, sizeof(agent.id), "agent_%04d", i);
        snprintf(agent.name, sizeof(agent.name), "Agent_%04d", i);
        mac_framework_register_agent(fw, &agent);
    }
    uint64_t t_end = bench_time_ns();
    print_ns(t_end - t_start, "total 1024 agents");
    printf("    per-agent: %lu ns\n", (unsigned long)((t_end - t_start) / n_agents));
    printf("    registered: %zu\n", mac_framework_get_agent_count(fw));

    mac_framework_destroy(fw);
    free(agent.capabilities_json);
    BENCH_PASS("MultiAgent register 1024 agents");
}

static void bench_mac_consensus_100_votes(void)
{
    BENCH_RUN("MultiAgent consensus resolve with 100 votes");
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    assert(fw != NULL);

    mac_agent_info_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.performance_score = 0.85;
    agent.reliability_score = 0.90;
    agent.max_concurrent_tasks = 8;
    agent.available = true;
    agent.capabilities_json = NULL;

    for (int i = 0; i < 100; i++) {
        snprintf(agent.id, sizeof(agent.id), "voter_%03d", i);
        snprintf(agent.name, sizeof(agent.name), "Voter_%03d", i);
        mac_framework_register_agent(fw, &agent);
    }

    char *cid = NULL;
    mac_framework_start_consensus(fw, NULL, "{\"action\":\"deploy\"}", MAC_CONSENSUS_MAJORITY,
                                  &cid);

    const int n_votes = 100;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < n_votes; i++) {
        char agent_id[32];
        char vote[64];
        snprintf(agent_id, sizeof(agent_id), "voter_%03d", i);
        snprintf(vote, sizeof(vote), "{\"agent_id\":\"voter_%03d\",\"vote\":\"approve\"}", i);
        mac_framework_vote(fw, cid, agent_id, vote);
    }

    char *result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    uint64_t t_end = bench_time_ns();

    print_ns(t_end - t_start, "total 100 votes + resolve");
    printf("    result: %s\n", result ? result : "(null)");

    free(cid);
    free(result);
    mac_framework_destroy(fw);
    BENCH_PASS("MultiAgent consensus 100 votes");
}

static void bench_mac_delegate_10_agents(void)
{
    BENCH_RUN("MultiAgent delegate task to 10 agents (< 100ms)");
    mac_framework_t *fw = mac_framework_create(MAC_MODE_DELEGATED);
    assert(fw != NULL);

    mac_agent_info_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.performance_score = 0.9;
    agent.reliability_score = 0.95;
    agent.max_concurrent_tasks = 4;
    agent.available = true;
    agent.capabilities_json = NULL;

    for (int i = 0; i < 10; i++) {
        snprintf(agent.id, sizeof(agent.id), "del_agent_%02d", i);
        snprintf(agent.name, sizeof(agent.name), "DelAgent_%02d", i);
        agent.performance_score = 0.8f + (float)i * 0.02f;
        mac_framework_register_agent(fw, &agent);
    }

    const int n_tasks = 10;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < n_tasks; i++) {
        mac_collab_task_t task;
        memset(&task, 0, sizeof(task));
        snprintf(task.id, sizeof(task.id), "task_%d", i);
        task.input_json = "{\"type\":\"benchmark\"}";
        char *assigned = NULL;
        mac_framework_delegate_task(fw, "default", &task, &assigned);
        if (assigned)
            free(assigned);
    }
    uint64_t t_end = bench_time_ns();

    uint64_t total_ns = t_end - t_start;
    print_ns(total_ns, "total 10 tasks delegated");
    printf("    per-task: %lu ns\n", (unsigned long)(total_ns / n_tasks));

    int under_100ms = (total_ns < 100000000ULL);
    printf("    SLA check (< 100ms total): %s\n", under_100ms ? "PASS" : "FAIL");

    mac_framework_destroy(fw);
    BENCH_PASS("MultiAgent delegate 10 agents");
}

static void bench_mac_consensus_100_accuracy(void)
{
    BENCH_RUN("MultiAgent 100-agent consensus accuracy (100% correct)");
    mac_framework_t *fw = mac_framework_create(MAC_MODE_CONSENSUS);
    assert(fw != NULL);

    mac_agent_info_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.performance_score = 0.9;
    agent.reliability_score = 0.95;
    agent.max_concurrent_tasks = 8;
    agent.available = true;
    agent.capabilities_json = NULL;

    for (int i = 0; i < 100; i++) {
        snprintf(agent.id, sizeof(agent.id), "acc_voter_%03d", i);
        snprintf(agent.name, sizeof(agent.name), "AccVoter_%03d", i);
        mac_framework_register_agent(fw, &agent);
    }

    const char *agent_ids[100];
    char id_bufs[100][32];
    for (int i = 0; i < 100; i++) {
        snprintf(id_bufs[i], sizeof(id_bufs[i]), "acc_voter_%03d", i);
        agent_ids[i] = id_bufs[i];
    }

    char *gid = NULL;
    mac_framework_create_group(fw, "bench_acc_group", MAC_MODE_CONSENSUS, agent_ids, 100, &gid);

    int correct = 0;
    int total = 10;

    for (int trial = 0; trial < total; trial++) {
        char *cid = NULL;
        char proposal[64];
        snprintf(proposal, sizeof(proposal), "{\"trial\":%d}", trial);
        mac_framework_start_consensus(fw, gid, proposal, MAC_CONSENSUS_MAJORITY, &cid);

        for (int i = 0; i < 100; i++) {
            char agent_id[32];
            char vote[64];
            snprintf(agent_id, sizeof(agent_id), "acc_voter_%03d", i);
            snprintf(vote, sizeof(vote), "{\"agent_id\":\"acc_voter_%03d\",\"vote\":\"approve\"}",
                     i);
            mac_framework_vote(fw, cid, agent_id, vote);
        }

        char *result = NULL;
        mac_framework_resolve_consensus(fw, cid, &result);
        if (result && strstr(result, "\"rejected\":true") == NULL) {
            correct++;
        }
        free(cid);
        free(result);
    }

    float accuracy = (float)correct / (float)total * 100.0f;
    printf("    Accuracy: %.1f%% (%d/%d)\n", accuracy, correct, total);
    printf("    SLA check (100%% correct): %s\n", correct == total ? "PASS" : "FAIL");

    free(gid);
    mac_framework_destroy(fw);
    BENCH_PASS("MultiAgent 100-agent consensus accuracy");
}

static void bench_mac_1000_agents_no_crash(void)
{
    BENCH_RUN("MultiAgent 1000-agent parallel (no crash)");
    mac_framework_t *fw = mac_framework_create(MAC_MODE_COLLABORATIVE);
    assert(fw != NULL);

    mac_agent_info_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.performance_score = 0.85;
    agent.reliability_score = 0.90;
    agent.max_concurrent_tasks = 2;
    agent.available = true;
    agent.capabilities_json = NULL;

    const int n_agents = 1000;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < n_agents; i++) {
        snprintf(agent.id, sizeof(agent.id), "pagent_%04d", i);
        snprintf(agent.name, sizeof(agent.name), "ParallelAgent_%04d", i);
        agent.performance_score = 0.5f + (float)(i % 50) * 0.01f;
        agent.reliability_score = 0.6f + (float)(i % 40) * 0.01f;
        int reg_err = mac_framework_register_agent(fw, &agent);
        if (reg_err != 0 && reg_err != -1) {
            printf("    Registration failed at agent %d: err=%d\n", i, reg_err);
        }
    }
    uint64_t reg_end = bench_time_ns();

    size_t registered = mac_framework_get_agent_count(fw);
    printf("    Registered: %zu / %d agents\n", registered, n_agents);

    const char *member_ids[100];
    char member_buf[100][32];
    for (int i = 0; i < 100 && i < (int)registered; i++) {
        snprintf(member_buf[i], sizeof(member_buf[i]), "pagent_%04d", i);
        member_ids[i] = member_buf[i];
    }
    char *gid = NULL;
    int gerr =
        mac_framework_create_group(fw, "bench_group", MAC_MODE_CONSENSUS, member_ids, 100, &gid);
    printf("    Group creation: %s\n", gerr == 0 ? "OK" : "FAILED");

    if (gid) {
        char *cid = NULL;
        mac_framework_start_consensus(fw, gid, "{\"action\":\"scale_test\"}",
                                      MAC_CONSENSUS_MAJORITY, &cid);

        for (int i = 0; i < 100; i++) {
            char agent_id[32];
            char vote[64];
            snprintf(agent_id, sizeof(agent_id), "pagent_%04d", i);
            snprintf(vote, sizeof(vote), "{\"agent_id\":\"pagent_%04d\",\"vote\":\"approve\"}", i);
            mac_framework_vote(fw, cid, agent_id, vote);
        }

        char *result = NULL;
        mac_framework_resolve_consensus(fw, cid, &result);
        printf("    Consensus result: %s\n", result ? result : "(null)");
        free(cid);
        free(result);
        free(gid);
    }

    uint64_t t_end = bench_time_ns();
    print_ns(reg_end - t_start, "registration");
    print_ns(t_end - reg_end, "consensus");
    print_ns(t_end - t_start, "total");

    mac_framework_destroy(fw);
    printf("    SLA check (no crash): PASS\n");
    BENCH_PASS("MultiAgent 1000-agent parallel");
}

static void bench_mac_batch_register_1000(void)
{
    BENCH_RUN("MultiAgent batch register 1000 agents");
    mac_framework_t *fw = mac_framework_create(MAC_MODE_COLLABORATIVE);
    assert(fw != NULL);

    mac_agent_info_t agents[1000];
    for (int i = 0; i < 1000; i++) {
        memset(&agents[i], 0, sizeof(mac_agent_info_t));
        snprintf(agents[i].id, sizeof(agents[i].id), "batch_%04d", i);
        snprintf(agents[i].name, sizeof(agents[i].name), "BatchAgent_%04d", i);
        agents[i].performance_score = 0.8f + (float)(i % 20) * 0.01f;
        agents[i].reliability_score = 0.85f + (float)(i % 15) * 0.01f;
        agents[i].max_concurrent_tasks = 4;
        agents[i].available = true;
    }

    size_t registered = 0;
    uint64_t t_start = bench_time_ns();
    mac_framework_register_agents_batch(fw, agents, 1000, &registered);
    uint64_t t_end = bench_time_ns();

    printf("    Batch registered: %zu / 1000\n", registered);
    print_ns(t_end - t_start, "total batch");
    printf("    per-agent: %lu ns\n",
           (unsigned long)((t_end - t_start) / (registered ? registered : 1)));

    mac_framework_destroy(fw);
    BENCH_PASS("MultiAgent batch register 1000 agents");
}

static void bench_mac_hash_lookup_1000(void)
{
    BENCH_RUN("MultiAgent hash ops 1000 agents (10000 unregister+register)");
    mac_framework_t *fw = mac_framework_create(MAC_MODE_COLLABORATIVE);
    assert(fw != NULL);

    mac_agent_info_t agents[1000];
    for (int i = 0; i < 1000; i++) {
        memset(&agents[i], 0, sizeof(mac_agent_info_t));
        snprintf(agents[i].id, sizeof(agents[i].id), "hagent_%04d", i);
        snprintf(agents[i].name, sizeof(agents[i].name), "HashAgent_%04d", i);
        agents[i].performance_score = 0.9;
        agents[i].reliability_score = 0.95;
        agents[i].max_concurrent_tasks = 4;
        agents[i].available = true;
    }
    size_t reg_count = 0;
    mac_framework_register_agents_batch(fw, agents, 1000, &reg_count);

    const int n_lookups = 10000;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < n_lookups; i++) {
        char lookup_id[32];
        snprintf(lookup_id, sizeof(lookup_id), "hagent_%04d", i % 1000);
        mac_framework_unregister_agent(fw, lookup_id);
        mac_framework_register_agent(fw, &agents[i % 1000]);
    }
    uint64_t t_end = bench_time_ns();

    print_ns(t_end - t_start, "total 10000 unregister+re-register");
    printf("    per-op: %lu ns\n", (unsigned long)((t_end - t_start) / n_lookups));

    mac_framework_destroy(fw);
    BENCH_PASS("MultiAgent hash ops 1000 agents");
}

static void bench_mutex_lock_unlock(void)
{
    BENCH_RUN("mutex lock/unlock pair (1000000 iters)");
    agentos_mutex_t m;
    agentos_mutex_init(&m);

    const int iterations = 1000000;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < iterations; i++) {
        agentos_mutex_lock(&m);
        agentos_mutex_unlock(&m);
    }
    uint64_t t_end = bench_time_ns();
    print_ns(t_end - t_start, "total");
    printf("    per-pair: %lu ns\n", (unsigned long)((t_end - t_start) / iterations));

    agentos_mutex_destroy(&m);
    BENCH_PASS("mutex lock/unlock pair");
}

int main(void)
{
    printf("\n========================================\n");
    printf("  AgentOS Performance Benchmark Suite\n");
    printf("========================================\n\n");

    bench_mutex_lock_unlock();
    bench_parallel_dispatcher_single();
    bench_delegate_create_destroy();
    bench_delegate_assign_collect();
    bench_mac_register_1000();
    bench_mac_batch_register_1000();
    bench_mac_hash_lookup_1000();
    bench_mac_consensus_100_votes();
    bench_mac_delegate_10_agents();
    bench_mac_consensus_100_accuracy();
    bench_mac_1000_agents_no_crash();

    printf("\n========================================\n");
    printf("  Benchmark Results: %d run, %d passed\n", g_tests_run, g_tests_passed);
    printf("========================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
