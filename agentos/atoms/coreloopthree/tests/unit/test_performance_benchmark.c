/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cognition/parallel_dispatcher.h"
#include "cognition/delegate.h"
#include "multi_agent_collaboration.h"
#include "agentos.h"
#include "platform.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define BENCH_PASS(name) do { g_tests_passed++; printf("[PASS] %s\n", name); } while(0)
#define BENCH_RUN(name) do { g_tests_run++; printf("  BENCHMARK: %s\n", name); } while(0)

static int g_exec_count = 0;

static agentos_error_t mock_executor(const char* tool_name,
                                      const char* arguments,
                                      size_t arguments_len,
                                      char** out_output,
                                      size_t* out_output_len,
                                      void* user_data) {
    (void)user_data; (void)tool_name; (void)arguments; (void)arguments_len;
    g_exec_count++;
    *out_output = strdup("{\"result\":\"ok\"}");
    if (out_output_len) *out_output_len = strlen(*out_output);
    return AGENTOS_SUCCESS;
}

static void free_results(agentos_tool_result_t* results, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(results[i].tool_name);
        free(results[i].output);
    }
    free(results);
}

static uint64_t bench_time_ns(void) {
    return agentos_time_ns();
}

static void print_ns(uint64_t ns, const char* label) {
    if (ns >= 1000000000ULL)
        printf("    %s: %.3f s (%lu ns)\n", label, ns / 1e9, (unsigned long)ns);
    else if (ns >= 1000000ULL)
        printf("    %s: %.3f ms (%lu ns)\n", label, ns / 1e6, (unsigned long)ns);
    else
        printf("    %s: %.3f us (%lu ns)\n", label, ns / 1e3, (unsigned long)ns);
}

static void bench_parallel_dispatcher_single(void) {
    BENCH_RUN("parallel_dispatcher single call overhead");
    agentos_parallel_dispatcher_t* d = agentos_parallel_dispatcher_create(4);
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
        agentos_tool_result_t* results = NULL;
        size_t count = 0;
        agentos_parallel_dispatcher_dispatch(d, &call, 1, &results, &count);
        if (results) free_results(results, count);
    }
    uint64_t t_end = bench_time_ns();

    uint64_t total_ns = t_end - t_start;
    print_ns(total_ns, "total");
    printf("    per-call: %lu ns\n", (unsigned long)(total_ns / iterations));

    agentos_parallel_dispatcher_destroy(d);
    BENCH_PASS("parallel_dispatcher single call overhead");
}

static void bench_parallel_dispatcher_multi(void) {
    BENCH_RUN("parallel_dispatcher multi-call (10 calls x 1000 iters)");
    agentos_parallel_dispatcher_t* d = agentos_parallel_dispatcher_create(8);
    assert(d != NULL);
    agentos_parallel_dispatcher_set_executor(d, mock_executor, NULL);

    agentos_tool_call_t calls[10];
    const char* tools[] = {"a","b","c","d","e","f","g","h","i","j"};
    for (int i = 0; i < 10; i++) {
        memset(&calls[i], 0, sizeof(calls[i]));
        calls[i].tool_name = tools[i];
        calls[i].arguments = "{}";
        calls[i].arguments_len = 2;
        calls[i].safety_class = AGENTOS_TOOL_READ_ONLY;
    }

    const int iterations = 1000;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < iterations; i++) {
        agentos_tool_result_t* results = NULL;
        size_t count = 0;
        agentos_parallel_dispatcher_dispatch(d, calls, 10, &results, &count);
        if (results) free_results(results, count);
    }
    uint64_t t_end = bench_time_ns();
    uint64_t total_ns = t_end - t_start;
    print_ns(total_ns, "total");
    printf("    per-iter (10 calls): %lu ns\n", (unsigned long)(total_ns / iterations));

    agentos_parallel_dispatcher_destroy(d);
    BENCH_PASS("parallel_dispatcher multi-call");
}

static void bench_delegate_create_destroy(void) {
    BENCH_RUN("delegate create/destroy cycle (10000 iters)");
    agentos_delegate_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_depth = 2;
    config.max_iterations = 5;

    const int iterations = 10000;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < iterations; i++) {
        agentos_delegate_task_t* task =
            agentos_delegate_create("benchmark task", &config);
        if (task) agentos_delegate_destroy(task);
    }
    uint64_t t_end = bench_time_ns();
    print_ns(t_end - t_start, "total");
    printf("    per-cycle: %lu ns\n", (unsigned long)((t_end - t_start) / iterations));

    BENCH_PASS("delegate create/destroy cycle");
}

static void bench_delegate_assign_collect(void) {
    BENCH_RUN("delegate assign+collect (5000 iters)");
    agentos_delegate_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_depth = 2;
    config.max_iterations = 5;

    const int iterations = 5000;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < iterations; i++) {
        agentos_delegate_task_t* task =
            agentos_delegate_create("bench task", &config);
        if (!task) continue;
        agentos_error_t err = agentos_delegate_assign(task, mock_executor, NULL);
        (void)err;
        char* result = NULL;
        size_t rlen = 0;
        agentos_delegate_collect(task, &result, &rlen);
        if (result) free(result);
        agentos_delegate_destroy(task);
    }
    uint64_t t_end = bench_time_ns();
    print_ns(t_end - t_start, "total");
    printf("    per-cycle: %lu ns\n", (unsigned long)((t_end - t_start) / iterations));

    BENCH_PASS("delegate assign+collect");
}

static void bench_mac_register_1000(void) {
    BENCH_RUN("MultiAgent register 1024 agents");
    mac_framework_t* fw = mac_framework_create(MAC_MODE_COLLABORATIVE);
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
    BENCH_PASS("MultiAgent register 1024 agents");
}

static void bench_mac_consensus_100_votes(void) {
    BENCH_RUN("MultiAgent consensus resolve with 100 votes");
    mac_framework_t* fw = mac_framework_create(MAC_MODE_CONSENSUS);
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

    char* cid = NULL;
    mac_framework_start_consensus(fw, NULL, "{\"action\":\"deploy\"}",
                                   MAC_CONSENSUS_MAJORITY, &cid);

    const int n_votes = 100;
    uint64_t t_start = bench_time_ns();
    for (int i = 0; i < n_votes; i++) {
        char vote[64];
        snprintf(vote, sizeof(vote), "{\"agent_id\":\"voter_%03d\",\"vote\":\"approve\"}", i);
        mac_framework_vote(fw, cid, vote, vote);
    }

    char* result = NULL;
    mac_framework_resolve_consensus(fw, cid, &result);
    uint64_t t_end = bench_time_ns();

    print_ns(t_end - t_start, "total 100 votes + resolve");
    printf("    result: %s\n", result ? result : "(null)");

    free(cid); free(result);
    mac_framework_destroy(fw);
    BENCH_PASS("MultiAgent consensus 100 votes");
}

static void bench_mutex_lock_unlock(void) {
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

int main(void) {
    printf("\n========================================\n");
    printf("  AgentOS Performance Benchmark Suite\n");
    printf("========================================\n\n");

    bench_mutex_lock_unlock();
    bench_parallel_dispatcher_single();
    bench_parallel_dispatcher_multi();
    bench_delegate_create_destroy();
    bench_delegate_assign_collect();
    bench_mac_register_1000();
    bench_mac_consensus_100_votes();

    printf("\n========================================\n");
    printf("  Benchmark Results: %d run, %d passed\n",
           g_tests_run, g_tests_passed);
    printf("========================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
