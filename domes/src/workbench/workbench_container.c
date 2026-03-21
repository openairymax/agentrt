/**
 * @file workbench_container.c
 * @brief 基于容器的虚拟工位实现（使用 runc 二进制调用）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "workbench.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

/* 容器模式私有数据结构 */
typedef struct container_workbench {
    char*   id;
    char*   agent_id;
    char*   bundle_dir;       // OCI bundle 目录
    char*   container_id;     // runc 容器 ID
    pid_t   init_pid;         // 容器内 init 进程 PID（可选）
    struct container_workbench* next;
} container_workbench_t;

/* 容器模式后端上下文 */
typedef struct container_backend {
    container_workbench_t* workbenches;
    pthread_mutex_t        lock;
    uint64_t               memory_bytes;
    float                  cpu_quota;
    int                    network;
    char*                  rootfs;
} container_backend_t;

/* 声明操作表 */
extern const workbench_ops_t workbench_ops_container;

/* 生成 OCI bundle 目录路径 */
static char* make_bundle_path(const char* base, const char* id) {
    char* path = malloc(strlen(base) + strlen(id) + 2);
    if (!path) return NULL;
    sprintf(path, "%s/%s", base, id);
    return path;
}

/* 创建 OCI bundle 配置 */
static int create_bundle(const char* bundle_dir, const char* rootfs,
                         uint64_t memory_bytes, float cpu_quota, int network) {
    // 创建 bundle 目录
    if (mkdir(bundle_dir, 0755) != 0 && errno != EEXIST) {
        AGENTOS_LOG_ERROR("mkdir bundle failed: %s", strerror(errno));
        return -1;
    }
    // 创建 rootfs 链接
    char rootfs_path[1024];
    snprintf(rootfs_path, sizeof(rootfs_path), "%s/rootfs", bundle_dir);
    if (symlink(rootfs, rootfs_path) != 0 && errno != EEXIST) {
        AGENTOS_LOG_ERROR("symlink rootfs failed: %s", strerror(errno));
        return -1;
    }
    // 生成 config.json
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/config.json", bundle_dir);
    FILE* f = fopen(config_path, "w");
    if (!f) {
        AGENTOS_LOG_ERROR("fopen config.json failed: %s", strerror(errno));
        return -1;
    }
    fprintf(f, "{\n");
    fprintf(f, "  \"ociVersion\": \"1.0.2\",\n");
    fprintf(f, "  \"process\": {\n");
    fprintf(f, "    \"terminal\": false,\n");
    fprintf(f, "    \"user\": {\"uid\": 0, \"gid\": 0},\n");
    fprintf(f, "    \"args\": [\"/bin/sh\"],\n");
    fprintf(f, "    \"env\": [\"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\"]\n");
    fprintf(f, "  },\n");
    fprintf(f, "  \"root\": {\"path\": \"rootfs\"},\n");
    fprintf(f, "  \"hostname\": \"agentos-workbench\",\n");
    fprintf(f, "  \"linux\": {\n");
    fprintf(f, "    \"resources\": {\n");
    fprintf(f, "      \"memory\": {\"limit\": %llu},\n", (unsigned long long)memory_bytes);
    fprintf(f, "      \"cpu\": {\"quota\": %ld, \"period\": 100000}\n", (long)(cpu_quota * 100000));
    fprintf(f, "    },\n");
    fprintf(f, "    \"namespaces\": [\n");
    fprintf(f, "      {\"type\": \"pid\"},\n");
    fprintf(f, "      {\"type\": \"ipc\"},\n");
    fprintf(f, "      {\"type\": \"uts\"},\n");
    fprintf(f, "      {\"type\": \"mount\"}\n");
    if (!network) {
        fprintf(f, "      ,{\"type\": \"network\"}\n");
    }
    fprintf(f, "    ]\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

/* 执行 shell 命令并获取输出（简化版） */
static int run_cmd(const char* cmd, char* out_buf, size_t buf_size) {
    FILE* fp = popen(cmd, "r");
    if (!fp) return -1;
    if (fgets(out_buf, buf_size, fp) == NULL) {
        pclose(fp);
        return -1;
    }
    int status = pclose(fp);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
    return -1;
}

/* 创建后端上下文 */
static void* container_create_ctx(workbench_manager_t* mgr) {
    container_backend_t* ctx = (container_backend_t*)calloc(1, sizeof(container_backend_t));
    if (!ctx) return NULL;
    pthread_mutex_init(&ctx->lock, NULL);
    ctx->memory_bytes = mgr->memory_bytes;
    ctx->cpu_quota = mgr->cpu_quota;
    ctx->network = mgr->network;
    ctx->rootfs = mgr->rootfs ? strdup(mgr->rootfs) : NULL;
    return ctx;
}

/* 销毁后端上下文 */
static void container_cleanup_ctx(void* ctx) {
    container_backend_t* bctx = (container_backend_t*)ctx;
    if (!bctx) return;
    pthread_mutex_lock(&bctx->lock);
    container_workbench_t* w = bctx->workbenches;
    while (w) {
        container_workbench_t* next = w->next;
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "runc kill %s KILL", w->container_id);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "runc delete %s", w->container_id);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "rm -rf %s", w->bundle_dir);
        system(cmd);
        free(w->id);
        free(w->agent_id);
        free(w->bundle_dir);
        free(w->container_id);
        free(w);
        w = next;
    }
    pthread_mutex_unlock(&bctx->lock);
    free(bctx->rootfs);
    pthread_mutex_destroy(&bctx->lock);
    free(bctx);
}

/* 创建工位（容器模式） */
static int container_create_workbench(void* ctx, const char* agent_id, char** out_id) {
    container_backend_t* bctx = (container_backend_t*)ctx;
    if (!bctx->rootfs) {
        AGENTOS_LOG_ERROR("container_create_workbench: rootfs not set");
        return -1;
    }

    char* id = *out_id;
    // 创建 bundle 目录
    char* bundle_dir = make_bundle_path("/var/lib/agentos/containers", id);
    if (!bundle_dir) return -1;
    if (create_bundle(bundle_dir, bctx->rootfs, bctx->memory_bytes, bctx->cpu_quota, bctx->network) != 0) {
        free(bundle_dir);
        return -1;
    }

    // 调用 runc create
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "runc create --bundle %s %s", bundle_dir, id);
    int ret = system(cmd);
    if (ret != 0) {
        AGENTOS_LOG_ERROR("runc create failed: %d", ret);
        free(bundle_dir);
        return -1;
    }

    // 获取容器 PID（可选）
    char state_cmd[1024];
    snprintf(state_cmd, sizeof(state_cmd), "runc state %s | grep -o '\"pid\": [0-9]*' | cut -d' ' -f2", id);
    char pid_buf[32];
    pid_t pid = -1;
    if (run_cmd(state_cmd, pid_buf, sizeof(pid_buf)) == 0) {
        pid = atoi(pid_buf);
    }

    container_workbench_t* wb = (container_workbench_t*)calloc(1, sizeof(container_workbench_t));
    if (!wb) {
        snprintf(cmd, sizeof(cmd), "runc delete %s", id);
        system(cmd);
        free(bundle_dir);
        return -1;
    }
    wb->id = strdup(id);
    wb->agent_id = strdup(agent_id);
    wb->bundle_dir = bundle_dir;
    wb->container_id = strdup(id);
    wb->init_pid = pid;

    pthread_mutex_lock(&bctx->lock);
    wb->next = bctx->workbenches;
    bctx->workbenches = wb;
    pthread_mutex_unlock(&bctx->lock);

    return 0;
}

/* 执行命令（容器模式） */
static int container_exec_workbench(void* ctx, const char* workbench_id,
                                     const char* const* argv,
                                     uint32_t timeout_ms,
                                     char** out_stdout,
                                     char** out_stderr,
                                     int* out_exit_code,
                                     char** out_error) {
    container_backend_t* bctx = (container_backend_t*)ctx;
    pthread_mutex_lock(&bctx->lock);
    container_workbench_t* wb = bctx->workbenches;
    while (wb) {
        if (strcmp(wb->id, workbench_id) == 0) break;
        wb = wb->next;
    }
    pthread_mutex_unlock(&bctx->lock);
    if (!wb) return -1;

    // 构建 runc exec 命令
    char cmd[8192] = "runc exec ";
    if (timeout_ms > 0) {
        char timeout_buf[64];
        snprintf(timeout_buf, sizeof(timeout_buf), "timeout %gs ", timeout_ms / 1000.0);
        strcat(cmd, timeout_buf);
    }
    strcat(cmd, wb->container_id);
    for (int i = 0; argv[i] != NULL; i++) {
        strcat(cmd, " ");
        strcat(cmd, argv[i]);
    }
    strcat(cmd, " 2>&1"); // 将 stderr 合并到 stdout

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        *out_error = strdup("popen failed");
        return -1;
    }
    char buf[8192] = {0};
    size_t total = 0;
    size_t n;
    while ((n = fread(buf + total, 1, sizeof(buf)-total-1, fp)) > 0) {
        total += n;
        if (total >= sizeof(buf)-1) break;
    }
    int status = pclose(fp);
    *out_stdout = strdup(buf);
    *out_stderr = strdup(""); // 已合并
    *out_exit_code = (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    *out_error = NULL;
    return 0;
}

/* 销毁工位（容器模式） */
static void container_destroy_workbench(void* ctx, const char* workbench_id) {
    container_backend_t* bctx = (container_backend_t*)ctx;
    pthread_mutex_lock(&bctx->lock);
    container_workbench_t** p = &bctx->workbenches;
    while (*p) {
        if (strcmp((*p)->id, workbench_id) == 0) {
            container_workbench_t* victim = *p;
            *p = victim->next;
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "runc kill %s KILL", victim->container_id);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "runc delete %s", victim->container_id);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "rm -rf %s", victim->bundle_dir);
            system(cmd);
            free(victim->id);
            free(victim->agent_id);
            free(victim->bundle_dir);
            free(victim->container_id);
            free(victim);
            break;
        }
        p = &(*p)->next;
    }
    pthread_mutex_unlock(&bctx->lock);
}

/* 列表（容器模式，不需要额外操作） */
static void container_list_workbenches(void* ctx, char*** out_ids, size_t* out_count) {
    // 由通用层处理
}

/* 操作表定义 */
const workbench_ops_t workbench_ops_container = {
    .create_ctx = container_create_ctx,
    .create = container_create_workbench,
    .exec = container_exec_workbench,
    .destroy = container_destroy_workbench,
    .list = container_list_workbenches,
    .cleanup = container_cleanup_ctx,
};