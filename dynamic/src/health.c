/**
 * @file health.c
 * @brief 健康检查器实现
 */

#include "health.h"
#include "logger.h"
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

typedef struct health_component {
    char*                 name;
    health_check_func_t   func;
    struct health_component* next;
} health_component_t;

struct health_checker {
    uint32_t            interval_sec;
    pthread_t           thread;
    volatile int        running;
    pthread_mutex_t     lock;
    health_component_t* components;
};

static void* health_thread_func(void* arg) {
    health_checker_t* hc = (health_checker_t*)arg;
    while (hc->running) {
        sleep(hc->interval_sec);
        pthread_mutex_lock(&hc->lock);
        health_component_t* comp = hc->components;
        while (comp) {
            char* status = NULL;
            if (comp->func(&status) == 0 && status) {
                AGENTOS_LOG_DEBUG("Health %s: %s", comp->name, status);
                free(status);
            } else {
                AGENTOS_LOG_ERROR("Health check %s failed", comp->name);
            }
            comp = comp->next;
        }
        pthread_mutex_unlock(&hc->lock);
    }
    return NULL;
}

health_checker_t* health_checker_create(uint32_t interval_sec) {
    health_checker_t* hc = (health_checker_t*)calloc(1, sizeof(health_checker_t));
    if (!hc) return NULL;
    hc->interval_sec = interval_sec;
    pthread_mutex_init(&hc->lock, NULL);
    hc->running = 1;
    if (pthread_create(&hc->thread, NULL, health_thread_func, hc) != 0) {
        pthread_mutex_destroy(&hc->lock);
        free(hc);
        return NULL;
    }
    return hc;
}

void health_checker_destroy(health_checker_t* hc) {
    if (!hc) return;
    hc->running = 0;
    pthread_join(hc->thread, NULL);
    pthread_mutex_lock(&hc->lock);
    health_component_t* c = hc->components;
    while (c) {
        health_component_t* next = c->next;
        free(c->name);
        free(c);
        c = next;
    }
    pthread_mutex_unlock(&hc->lock);
    pthread_mutex_destroy(&hc->lock);
    free(hc);
}

int health_checker_register(health_checker_t* hc, const char* name, health_check_func_t func) {
    if (!hc || !name || !func) return -1;
    health_component_t* comp = (health_component_t*)malloc(sizeof(health_component_t));
    if (!comp) return -1;
    comp->name = strdup(name);
    if (!comp->name) {
        free(comp);
        return -1;
    }
    comp->func = func;
    pthread_mutex_lock(&hc->lock);
    comp->next = hc->components;
    hc->components = comp;
    pthread_mutex_unlock(&hc->lock);
    return 0;
}