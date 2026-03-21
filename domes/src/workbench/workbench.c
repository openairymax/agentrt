/**
 * @file workbench.c
 * @brief 虚拟工位管理器通用实现（工厂模式）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "workbench.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

/* 操作表定义 */
typedef struct workbench_ops {
    int  (*create)(void* ctx, const char* agent_id, char** out_id);
    int  (*exec)(void* ctx, const char* workbench_id,
                 const char* const* argv, uint32_t timeout_ms,
                 char** out_stdout, char** out_stderr,
                 int* out_exit_code, char** out_error);
    void (*destroy)(void* ctx, const char* workbench_id);
    void (*list)(void* ctx, char*** out_ids, size_t* out_count);
    void (*cleanup)(void* ctx);  // 销毁所有工位，释放后端私有资源
} workbench_ops_t;

/* 工位条目（用于统一管理） */
typedef struct workbench_entry {
    char*   id;
    void*   backend_ctx;   // 后端私有数据
    struct workbench_entry* next;
} workbench_entry_t;

/* 管理器结构 */
struct workbench_manager {
    const workbench_ops_t* ops;
    void*                  backend_data;    // 后端私有数据（如进程管理器的entries）
    workbench_entry_t*     entries;         // 统一条目链表（可选）
    pthread_mutex_t        lock;
    uint64_t               memory_bytes;
    float                  cpu_quota;
    int                    network;
    char*                  rootfs;
};

/* 外部声明各后端的操作表 */
extern const workbench_ops_t workbench_ops_process;
extern const workbench_ops_t workbench_ops_container;

/* 生成唯一 ID（全局递增） */
static void generate_workbench_id(char* buf, size_t len) {
    static uint64_t counter = 0;
    snprintf(buf, len, "wb_%llu", (unsigned long long)__sync_fetch_and_add(&counter, 1));
}

/* 工厂函数 */
workbench_manager_t* workbench_manager_create(const char* type,
                                              uint64_t memory_bytes,
                                              float cpu_quota,
                                              int network,
                                              const char* rootfs) {
    if (!type) {
        AGENTOS_LOG_ERROR("workbench_manager_create: type is NULL");
        return NULL;
    }

    workbench_manager_t* mgr = (workbench_manager_t*)calloc(1, sizeof(workbench_manager_t));
    if (!mgr) {
        AGENTOS_LOG_ERROR("workbench_manager_create: calloc failed");
        return NULL;
    }

    pthread_mutex_init(&mgr->lock, NULL);
    mgr->memory_bytes = memory_bytes;
    mgr->cpu_quota = cpu_quota;
    mgr->network = network;
    if (rootfs) mgr->rootfs = strdup(rootfs);

    if (strcmp(type, "process") == 0) {
        mgr->ops = &workbench_ops_process;
        mgr->backend_data = workbench_ops_process.create_ctx(mgr);  // 调用后端初始化
        if (!mgr->backend_data) {
            AGENTOS_LOG_ERROR("workbench_manager_create: failed to initialize process backend");
            goto fail;
        }
    } else if (strcmp(type, "container") == 0) {
        mgr->ops = &workbench_ops_container;
        mgr->backend_data = workbench_ops_container.create_ctx(mgr);
        if (!mgr->backend_data) {
            AGENTOS_LOG_ERROR("workbench_manager_create: failed to initialize container backend");
            goto fail;
        }
    } else {
        AGENTOS_LOG_ERROR("workbench_manager_create: unknown type '%s'", type);
        goto fail;
    }

    return mgr;

fail:
    free(mgr->rootfs);
    pthread_mutex_destroy(&mgr->lock);
    free(mgr);
    return NULL;
}

void workbench_manager_destroy(workbench_manager_t* mgr) {
    if (!mgr) return;
    pthread_mutex_lock(&mgr->lock);
    if (mgr->ops && mgr->ops->cleanup) {
        mgr->ops->cleanup(mgr->backend_data);
    }
    workbench_entry_t* e = mgr->entries;
    while (e) {
        workbench_entry_t* next = e->next;
        free(e->id);
        free(e);
        e = next;
    }
    pthread_mutex_unlock(&mgr->lock);
    pthread_mutex_destroy(&mgr->lock);
    free(mgr->rootfs);
    free(mgr);
}

/* 公共接口包装 */
int workbench_manager_create_workbench(workbench_manager_t* mgr,
                                       const char* agent_id,
                                       char** out_workbench_id) {
    if (!mgr || !agent_id || !out_workbench_id) return -1;
    pthread_mutex_lock(&mgr->lock);
    char id_buf[32];
    generate_workbench_id(id_buf, sizeof(id_buf));
    char* id = strdup(id_buf);
    if (!id) {
        pthread_mutex_unlock(&mgr->lock);
        return -1;
    }

    // 调用后端创建
    int ret = mgr->ops->create(mgr->backend_data, agent_id, &id);
    if (ret != 0) {
        free(id);
        pthread_mutex_unlock(&mgr->lock);
        return ret;
    }

    // 保存到统一链表
    workbench_entry_t* e = (workbench_entry_t*)malloc(sizeof(workbench_entry_t));
    if (!e) {
        mgr->ops->destroy(mgr->backend_data, id);
        free(id);
        pthread_mutex_unlock(&mgr->lock);
        return -1;
    }
    e->id = id;
    e->backend_ctx = NULL; // 后端已自己管理，这里仅用于列表
    e->next = mgr->entries;
    mgr->entries = e;

    pthread_mutex_unlock(&mgr->lock);
    *out_workbench_id = strdup(id);
    return 0;
}

int workbench_manager_exec(workbench_manager_t* mgr,
                           const char* workbench_id,
                           const char* const* argv,
                           uint32_t timeout_ms,
                           char** out_stdout,
                           char** out_stderr,
                           int* out_exit_code,
                           char** out_error) {
    if (!mgr || !workbench_id || !argv || !out_stdout || !out_stderr || !out_exit_code || !out_error)
        return -1;
    pthread_mutex_lock(&mgr->lock);
    int ret = mgr->ops->exec(mgr->backend_data, workbench_id, argv, timeout_ms,
                             out_stdout, out_stderr, out_exit_code, out_error);
    pthread_mutex_unlock(&mgr->lock);
    return ret;
}

void workbench_manager_destroy_workbench(workbench_manager_t* mgr,
                                         const char* workbench_id) {
    if (!mgr || !workbench_id) return;
    pthread_mutex_lock(&mgr->lock);
    // 从后端移除
    mgr->ops->destroy(mgr->backend_data, workbench_id);
    // 从统一链表中移除
    workbench_entry_t** p = &mgr->entries;
    while (*p) {
        if (strcmp((*p)->id, workbench_id) == 0) {
            workbench_entry_t* tmp = *p;
            *p = tmp->next;
            free(tmp->id);
            free(tmp);
            break;
        }
        p = &(*p)->next;
    }
    pthread_mutex_unlock(&mgr->lock);
}

int workbench_manager_list(workbench_manager_t* mgr,
                           char*** out_ids,
                           size_t* out_count) {
    if (!mgr || !out_ids || !out_count) return -1;
    pthread_mutex_lock(&mgr->lock);
    size_t cnt = 0;
    workbench_entry_t* e = mgr->entries;
    while (e) { cnt++; e = e->next; }
    char** arr = (char**)malloc(cnt * sizeof(char*));
    if (!arr) {
        pthread_mutex_unlock(&mgr->lock);
        return -1;
    }
    size_t i = 0;
    e = mgr->entries;
    while (e) {
        arr[i++] = strdup(e->id);
        e = e->next;
    }
    pthread_mutex_unlock(&mgr->lock);
    *out_ids = arr;
    *out_count = cnt;
    return 0;
}