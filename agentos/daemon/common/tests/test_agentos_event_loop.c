/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 * test_agentos_event_loop.c - AgentRT Event Loop Module Unit Tests
 */

#include "../include/agentos_event_loop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-50s", name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); return; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)

static int timer_fired_count = 0;

static void timer_cb(agentos_event_loop_t *loop, uint64_t timer_id, void *user_data)
{
    (void)loop;
    (void)timer_id;
    (void)user_data;
    timer_fired_count++;
}

static int fd_cb(int fd, uint32_t events, void *user_data)
{
    (void)fd;
    (void)events;
    (void)user_data;
    return 0;
}

static void test_create_default(void)
{
    TEST("Create with default max_events");
    agentos_event_loop_t *loop = agentos_event_loop_create(AGENTOS_EVENT_LOOP_MAX_EVENTS);
    ASSERT(loop != NULL, "create should succeed");
    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_create_zero_events(void)
{
    TEST("Create with zero max_events");
    agentos_event_loop_t *loop = agentos_event_loop_create(0);
    ASSERT(loop != NULL || loop == NULL, "create with 0 should not crash");
    if (loop) agentos_event_loop_destroy(loop);
    PASS();
}

static void test_destroy_null(void)
{
    TEST("Destroy NULL loop is safe");
    agentos_event_loop_destroy(NULL);
    PASS();
}

static void test_add_fd(void)
{
    TEST("Add an fd to watch");
    agentos_event_loop_t *loop = agentos_event_loop_create(512);
    ASSERT(loop != NULL, "create");

    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT(efd >= 0, "create eventfd");

    int ret = agentos_event_loop_add_fd(loop, efd, AGENTOS_EVENT_TYPE_READ, fd_cb, NULL);
    ASSERT(ret == 0, "add fd should succeed");

    int count = agentos_event_loop_get_fd_count(loop);
    ASSERT(count >= 1, "fd count should be at least 1 after add");

    agentos_event_loop_remove_fd(loop, efd);
    close(efd);
    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_add_fd_with_callback(void)
{
    TEST("Add fd with callback");
    agentos_event_loop_t *loop = agentos_event_loop_create(512);
    ASSERT(loop != NULL, "create");

    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT(efd >= 0, "create eventfd");

    int ret = agentos_event_loop_add_fd(loop, efd, AGENTOS_EVENT_TYPE_READ, fd_cb, NULL);
    ASSERT(ret == 0, "add fd with callback should succeed");

    agentos_event_loop_remove_fd(loop, efd);
    close(efd);
    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_add_fd_invalid(void)
{
    TEST("Add invalid fd rejected");
    agentos_event_loop_t *loop = agentos_event_loop_create(512);
    ASSERT(loop != NULL, "create");

    int ret = agentos_event_loop_add_fd(loop, -1, AGENTOS_EVENT_TYPE_READ, fd_cb, NULL);
    ASSERT(ret != 0, "invalid fd should be rejected");

    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_add_fd_null_loop(void)
{
    TEST("Add fd to NULL loop rejected");
    int efd = eventfd(0, EFD_NONBLOCK);
    int ret = agentos_event_loop_add_fd(NULL, efd, AGENTOS_EVENT_TYPE_READ, fd_cb, NULL);
    ASSERT(ret != 0, "NULL loop should be rejected");
    close(efd);
    PASS();
}

static void test_add_fd_null_callback(void)
{
    TEST("Add fd with NULL callback rejected");
    agentos_event_loop_t *loop = agentos_event_loop_create(512);
    ASSERT(loop != NULL, "create");

    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT(efd >= 0, "create eventfd");

    int ret = agentos_event_loop_add_fd(loop, efd, AGENTOS_EVENT_TYPE_READ, NULL, NULL);
    ASSERT(ret != 0, "NULL callback should be rejected");

    close(efd);
    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_remove_fd(void)
{
    TEST("Remove fd from watch");
    agentos_event_loop_t *loop = agentos_event_loop_create(512);
    ASSERT(loop != NULL, "create");

    int efd = eventfd(0, EFD_NONBLOCK);
    ASSERT(efd >= 0, "create eventfd");

    agentos_event_loop_add_fd(loop, efd, AGENTOS_EVENT_TYPE_READ, fd_cb, NULL);
    int pre_count = agentos_event_loop_get_fd_count(loop);

    agentos_event_loop_remove_fd(loop, efd);

    int post_count = agentos_event_loop_get_fd_count(loop);
    ASSERT(post_count < pre_count, "fd count should decrease after remove");

    close(efd);
    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_remove_fd_null_loop(void)
{
    TEST("Remove fd from NULL loop is safe");
    agentos_event_loop_remove_fd(NULL, 0);
    PASS();
}

static void test_get_fd_count(void)
{
    TEST("Get fd count tracking");
    agentos_event_loop_t *loop = agentos_event_loop_create(512);
    ASSERT(loop != NULL, "create");

    int efd1 = eventfd(0, EFD_NONBLOCK);
    int efd2 = eventfd(0, EFD_NONBLOCK);
    ASSERT(efd1 >= 0 && efd2 >= 0, "create eventfds");

    int ret1 = agentos_event_loop_add_fd(loop, efd1, AGENTOS_EVENT_TYPE_READ, fd_cb, NULL);
    int ret2 = agentos_event_loop_add_fd(loop, efd2, AGENTOS_EVENT_TYPE_READ, fd_cb, NULL);
    ASSERT(ret1 == 0 && ret2 == 0, "add fds");

    int count = agentos_event_loop_get_fd_count(loop);
    ASSERT(count == 2, "fd count should be 2 after adding 2 fds");

    agentos_event_loop_remove_fd(loop, efd1);
    count = agentos_event_loop_get_fd_count(loop);
    ASSERT(count == 1, "fd count should be 1 after removing 1 fd");

    agentos_event_loop_remove_fd(loop, efd2);
    count = agentos_event_loop_get_fd_count(loop);
    ASSERT(count == 0, "fd count should be 0 after removing all fds");

    close(efd1);
    close(efd2);
    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_add_timer(void)
{
    TEST("Add timer");
    timer_fired_count = 0;
    agentos_event_loop_t *loop = agentos_event_loop_create(512);
    ASSERT(loop != NULL, "create");

    uint64_t timer_id = agentos_event_loop_add_timer(loop, 100, timer_cb, NULL);
    ASSERT(timer_id > 0, "add timer should return valid id");

    agentos_event_loop_cancel_timer(loop, timer_id);
    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_add_timer_null_callback(void)
{
    TEST("Add timer with NULL callback rejected");
    agentos_event_loop_t *loop = agentos_event_loop_create(512);
    ASSERT(loop != NULL, "create");

    uint64_t timer_id = agentos_event_loop_add_timer(loop, 100, NULL, NULL);
    ASSERT(timer_id == 0, "NULL callback should be rejected");

    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_add_timer_null_loop(void)
{
    TEST("Add timer to NULL loop rejected");
    uint64_t timer_id = agentos_event_loop_add_timer(NULL, 100, timer_cb, NULL);
    ASSERT(timer_id == 0, "NULL loop should be rejected");
    PASS();
}

static void test_cancel_timer(void)
{
    TEST("Cancel timer by ID");
    agentos_event_loop_t *loop = agentos_event_loop_create(512);
    ASSERT(loop != NULL, "create");

    uint64_t timer_id = agentos_event_loop_add_timer(loop, 100, timer_cb, NULL);
    ASSERT(timer_id > 0, "add timer");

    int ret = agentos_event_loop_cancel_timer(loop, timer_id);
    ASSERT(ret == 0, "cancel timer should succeed");

    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_cancel_timer_null_loop(void)
{
    TEST("Cancel timer on NULL loop rejected");
    int ret = agentos_event_loop_cancel_timer(NULL, 1);
    ASSERT(ret != 0, "NULL loop should be rejected");
    PASS();
}

static void test_multiple_fds_remove(void)
{
    TEST("Multiple fds add then remove all");
    agentos_event_loop_t *loop = agentos_event_loop_create(512);
    ASSERT(loop != NULL, "create");

    int fds[5];
    for (int i = 0; i < 5; i++) {
        fds[i] = eventfd(0, EFD_NONBLOCK);
        ASSERT(fds[i] >= 0, "create eventfd");
        int ret = agentos_event_loop_add_fd(loop, fds[i], AGENTOS_EVENT_TYPE_READ, fd_cb, NULL);
        ASSERT(ret == 0, "add fd");
    }

    int count = agentos_event_loop_get_fd_count(loop);
    ASSERT(count == 5, "fd count should be 5");

    for (int i = 0; i < 5; i++) {
        agentos_event_loop_remove_fd(loop, fds[i]);
        close(fds[i]);
    }

    count = agentos_event_loop_get_fd_count(loop);
    ASSERT(count == 0, "fd count should be 0");

    agentos_event_loop_destroy(loop);
    PASS();
}

static void test_multiple_cycles(void)
{
    TEST("Multiple create/destroy cycles");
    for (int i = 0; i < 5; i++) {
        agentos_event_loop_t *loop = agentos_event_loop_create(64);
        ASSERT(loop != NULL, "repeat create");

        int efd = eventfd(0, EFD_NONBLOCK);
        agentos_event_loop_add_fd(loop, efd, AGENTOS_EVENT_TYPE_READ, fd_cb, NULL);
        uint64_t tid = agentos_event_loop_add_timer(loop, 10, timer_cb, NULL);

        agentos_event_loop_remove_fd(loop, efd);
        close(efd);
        if (tid > 0) {
            agentos_event_loop_cancel_timer(loop, tid);
        }

        agentos_event_loop_destroy(loop);
    }
    PASS();
}

int main(void)
{
    printf("\n=== AgentRT Event Loop Module Unit Tests ===\n\n");

    test_create_default();
    test_create_zero_events();
    test_destroy_null();
    test_add_fd();
    test_add_fd_with_callback();
    test_add_fd_invalid();
    test_add_fd_null_loop();
    test_add_fd_null_callback();
    test_remove_fd();
    test_remove_fd_null_loop();
    test_get_fd_count();
    test_add_timer();
    test_add_timer_null_callback();
    test_add_timer_null_loop();
    test_cancel_timer();
    test_cancel_timer_null_loop();
    test_multiple_fds_remove();
    test_multiple_cycles();

    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}