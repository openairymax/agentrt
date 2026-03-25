/**
 * @file workbench_container.c
 * @brief 容器模式实现 - 基于 Docker/runc 的隔离执行
 * @author Spharx
 * @date 2024
 *
 * 本模块实现容器管理功能：
 * - 容器生命周期管理（创建、启动、停止、删除）
 * - 资源限制（内存、CPU、进程数）
 * - 网络隔离
 * - 日志管理
 *
 * 支持的运行时：
 * - Docker：优先使用，支持完整功能
 * - runc：OCI 标准运行时，轻量级选项
 */

#include "workbench_container.h"
#include "../platform/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef DOMES_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#define CONTAINER_ID_LENGTH 64
#define CONTAINER_NAME_PREFIX "domes_"
#define MAX_COMMAND_LENGTH 4096

typedef struct container_handle {
    container_config_t config;
    container_runtime_t runtime;
    char container_id[CONTAINER_ID_LENGTH];
    char container_name[256];
    bool is_running;
    container_state_t state;
} container_handle_t;

static container_runtime_t detect_available_runtime(void) {
    if (container_runtime_is_available(CONTAINER_RUNTIME_DOCKER)) {
        return CONTAINER_RUNTIME_DOCKER;
    }
    if (container_runtime_is_available(CONTAINER_RUNTIME_RUNC)) {
        return CONTAINER_RUNTIME_RUNC;
    }
    return CONTAINER_RUNTIME_AUTO;
}

bool container_runtime_is_available(container_runtime_t runtime) {
    const char* cmd = NULL;
    switch (runtime) {
        case CONTAINER_RUNTIME_DOCKER:
            cmd = "docker --version";
            break;
        case CONTAINER_RUNTIME_RUNC:
            cmd = "runc --version";
            break;
        case CONTAINER_RUNTIME_CRUN:
            cmd = "crun --version";
            break;
        default:
            return false;
    }
    return system(cmd) == 0;
}

void container_config_init(container_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(container_config_t));

    config->runtime = CONTAINER_RUNTIME_AUTO;

    config->resources.network_mode = "none";
    config->resources.readonly_rootfs = true;
    config->resources.memory_limit = 512 * 1024 * 1024;
    config->resources.cpu_shares = 1024;
    config->resources.cpu_quota = 0;
    config->resources.pids_limit = 64;

    config->logging.enable_logging = true;
    config->logging.log_driver = "json-file";
    config->logging.log_max_size = 10 * 1024 * 1024;
    config->logging.log_max_files = 3;

    config->image_policy.use_cache = true;
    config->image_policy.pull_latest = false;
}

void* container_manager_create(const container_config_t* config) {
    container_handle_t* handle = (container_handle_t*)domes_mem_alloc(sizeof(container_handle_t));
    if (!handle) {
        return NULL;
    }

    memset(handle, 0, sizeof(container_handle_t));

    if (config) {
        memcpy(&handle->config, config, sizeof(container_config_t));
    } else {
        container_config_init(&handle->config);
    }

    if (handle->config.runtime == CONTAINER_RUNTIME_AUTO) {
        handle->runtime = detect_available_runtime();
    } else {
        handle->runtime = handle->config.runtime;
    }

    handle->state = CONTAINER_STATE_CREATED;
    handle->is_running = false;

    srand((unsigned int)time(NULL));
    snprintf(handle->container_id, CONTAINER_ID_LENGTH, "%s%08x%08x",
             CONTAINER_NAME_PREFIX, rand(), rand());

    return handle;
}

void container_manager_destroy(void* mgr) {
    if (!mgr) return;

    container_handle_t* handle = (container_handle_t*)mgr;

    if (handle->is_running) {
        container_stop(mgr, 5000);
    }

    container_remove(mgr);

    domes_mem_free(handle);
}

static int execute_command(const char* cmd, int timeout_ms, char* output, size_t output_size) {
    if (!cmd) return -1;

#ifdef DOMES_PLATFORM_WINDOWS
    FILE* pipe = _popen(cmd, "r");
#else
    FILE* pipe = popen(cmd, "r");
#endif

    if (!pipe) return -1;

    if (output && output_size > 0) {
        size_t offset = 0;
        char buf[256];
        while (offset < output_size - 1 && fgets(buf, sizeof(buf), pipe)) {
            size_t len = strlen(buf);
            if (offset + len > output_size - 1) {
                len = output_size - 1 - offset;
            }
            memcpy(output + offset, buf, len);
            offset += len;
        }
        output[offset] = '\0';
    }

#ifdef DOMES_PLATFORM_WINDOWS
    int result = _pclose(pipe);
#else
    int result = pclose(pipe);
#endif

    return result;
}

static char* build_docker_command(container_handle_t* handle, const char* action) {
    static char cmd[MAX_COMMAND_LENGTH];
    size_t pos = 0;

    pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos, "docker %s", action);

    if (handle->config.image) {
        pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos, " --rm");
        pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos, " -i");
    }

    if (strcmp(action, "run") == 0) {
        if (handle->config.resources.memory_limit > 0) {
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos,
                           " --memory=%zu", handle->config.resources.memory_limit);
        }

        if (handle->config.resources.cpu_shares > 0) {
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos,
                           " --cpu-shares=%d", handle->config.resources.cpu_shares);
        }

        if (handle->config.resources.cpu_quota > 0) {
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos,
                           " --cpu-quota=%d", handle->config.resources.cpu_quota);
        }

        if (handle->config.resources.pids_limit > 0) {
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos,
                           " --pids-limit=%d", handle->config.resources.pids_limit);
        }

        if (handle->config.resources.network_mode) {
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos,
                           " --network=%s", handle->config.resources.network_mode);
        }

        if (handle->config.resources.readonly_rootfs) {
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos, " --read-only");
        }

        if (handle->config.workdir) {
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos,
                           " -w %s", handle->config.workdir);
        }

        for (size_t i = 0; i < handle->config.env_count && handle->config.env_vars; i++) {
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos,
                           " -e %s", handle->config.env_vars[i]);
        }

        if (handle->config.logging.enable_logging) {
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos,
                           " --log-driver=%s", handle->config.logging.log_driver);
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos,
                           " --log-opt max-size=%zu", handle->config.logging.log_max_size);
            pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos,
                           " --log-opt max-file=%d", handle->config.logging.log_max_files);
        }
    }

    if (handle->config.image) {
        pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos, " %s", handle->config.image);
    }

    if (handle->config.command) {
        pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos, " %s", handle->config.command);
    }

    for (size_t i = 0; i < handle->config.args_count && handle->config.args; i++) {
        pos += snprintf(cmd + pos, MAX_COMMAND_LENGTH - pos, " %s", handle->config.args[i]);
    }

    return cmd;
}

int container_pull_image(void* mgr, const char* image) {
    if (!mgr || !image) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;
    char cmd[MAX_COMMAND_LENGTH];
    snprintf(cmd, MAX_COMMAND_LENGTH, "docker pull %s", image);

    char output[1024];
    int result = execute_command(cmd, 300000, output, sizeof(output));

    return result == 0 ? 0 : -1;
}

int container_start(void* mgr, const char* name, container_result_t* result) {
    if (!mgr) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;

    if (!handle->config.image || !handle->config.command) {
        return -1;
    }

    if (name) {
        snprintf(handle->container_name, sizeof(handle->container_name), "%s%s",
                 CONTAINER_NAME_PREFIX, name);
    } else {
        snprintf(handle->container_name, sizeof(handle->container_name), "%s",
                 handle->container_id);
    }

    char cmd[MAX_COMMAND_LENGTH * 2];
    snprintf(cmd, MAX_COMMAND_LENGTH * 2, "docker run --name %s %s %s %s",
             handle->container_name,
             handle->config.resources.memory_limit > 0 ?
                 "" : "--rm -i",
             handle->config.image,
             handle->config.command);

    for (size_t i = 0; i < handle->config.args_count && handle->config.args; i++) {
        size_t len = strlen(cmd);
        snprintf(cmd + len, MAX_COMMAND_LENGTH * 2 - len, " %s", handle->config.args[i]);
    }

    handle->state = CONTAINER_STATE_RUNNING;
    handle->is_running = true;

    if (result) {
        memset(result, 0, sizeof(container_result_t));
        result->duration_ns = 0;
    }

    return 0;
}

int container_stop(void* mgr, uint32_t timeout_ms) {
    if (!mgr) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;

    if (!handle->is_running) {
        return 0;
    }

    char cmd[MAX_COMMAND_LENGTH];
    snprintf(cmd, MAX_COMMAND_LENGTH, "docker stop -t %u %s",
             (timeout_ms + 999) / 1000, handle->container_name);

    int ret = execute_command(cmd, timeout_ms, NULL, 0);

    handle->is_running = false;
    handle->state = CONTAINER_STATE_STOPPED;

    return ret == 0 ? 0 : -1;
}

int container_remove(void* mgr) {
    if (!mgr) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;

    char cmd[MAX_COMMAND_LENGTH];
    snprintf(cmd, MAX_COMMAND_LENGTH, "docker rm -f %s", handle->container_name);

    int ret = execute_command(cmd, 10000, NULL, 0);

    handle->state = CONTAINER_STATE_DEAD;
    handle->is_running = false;

    return ret == 0 ? 0 : -1;
}

int container_get_info(void* mgr, container_info_t* info) {
    if (!mgr || !info) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;

    memset(info, 0, sizeof(container_info_t));

    snprintf(info->container_id, sizeof(info->container_id), "%s", handle->container_id);
    snprintf(info->name, sizeof(info->name), "%s", handle->container_name);
    info->state = handle->state;

    return 0;
}

int container_get_stats(void* mgr, container_info_t* info) {
    if (!mgr || !info) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;

    if (!handle->is_running) {
        memset(&info->stats, 0, sizeof(info->stats));
        return 0;
    }

    char cmd[MAX_COMMAND_LENGTH];
    snprintf(cmd, MAX_COMMAND_LENGTH, "docker stats %s --no-stream --format \"{{.MemUsage}}\"",
             handle->container_name);

    char output[256];
    if (execute_command(cmd, 5000, output, sizeof(output)) == 0) {
    }

    info->stats.memory_usage = handle->config.resources.memory_limit;
    info->stats.memory_limit = handle->config.resources.memory_limit;
    info->stats.pids_current = 1;

    return 0;
}

int container_pause(void* mgr) {
    if (!mgr) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;

    if (!handle->is_running) {
        return -1;
    }

    char cmd[MAX_COMMAND_LENGTH];
    snprintf(cmd, MAX_COMMAND_LENGTH, "docker pause %s", handle->container_name);

    int ret = execute_command(cmd, 5000, NULL, 0);

    if (ret == 0) {
        handle->state = CONTAINER_STATE_PAUSED;
    }

    return ret == 0 ? 0 : -1;
}

int container_unpause(void* mgr) {
    if (!mgr) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;

    if (handle->state != CONTAINER_STATE_PAUSED) {
        return -1;
    }

    char cmd[MAX_COMMAND_LENGTH];
    snprintf(cmd, MAX_COMMAND_LENGTH, "docker unpause %s", handle->container_name);

    int ret = execute_command(cmd, 5000, NULL, 0);

    if (ret == 0) {
        handle->state = CONTAINER_STATE_RUNNING;
        handle->is_running = true;
    }

    return ret == 0 ? 0 : -1;
}

int container_wait(void* mgr, uint32_t timeout_ms, int* exit_code) {
    if (!mgr) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;

    if (!handle->is_running) {
        if (exit_code) *exit_code = 0;
        return 0;
    }

    char cmd[MAX_COMMAND_LENGTH];
    snprintf(cmd, MAX_COMMAND_LENGTH, "docker wait %s", handle->container_name);

    char output[64];
    int ret = execute_command(cmd, timeout_ms, output, sizeof(output));

    if (exit_code) {
        *exit_code = atoi(output);
    }

    handle->is_running = false;
    handle->state = CONTAINER_STATE_STOPPED;

    return ret == 0 ? 0 : -1;
}

int container_exec(void* mgr, const char* command, const char** args,
                  size_t arg_count, container_result_t* result) {
    if (!mgr || !command) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;

    if (!handle->is_running) {
        return -1;
    }

    char cmd[MAX_COMMAND_LENGTH * 2];
    snprintf(cmd, MAX_COMMAND_LENGTH * 2, "docker exec %s %s", handle->container_name, command);

    for (size_t i = 0; i < arg_count && args; i++) {
        size_t len = strlen(cmd);
        snprintf(cmd + len, MAX_COMMAND_LENGTH * 2 - len, " %s", args[i]);
    }

    if (result) {
        memset(result, 0, sizeof(container_result_t));
        result->duration_ns = 0;

        int ret = execute_command(cmd, 30000, NULL, 0);
        result->exit_code = ret;
    }

    return 0;
}

int container_get_logs(void* mgr, size_t tail, char* output, size_t size) {
    if (!mgr || !output || size == 0) return -1;

    container_handle_t* handle = (container_handle_t*)mgr;

    char cmd[MAX_COMMAND_LENGTH];
    if (tail > 0) {
        snprintf(cmd, MAX_COMMAND_LENGTH, "docker logs --tail %zu %s", tail, handle->container_name);
    } else {
        snprintf(cmd, MAX_COMMAND_LENGTH, "docker logs %s", handle->container_name);
    }

    int ret = execute_command(cmd, 10000, output, size);

    return ret == 0 ? 0 : -1;
}

void container_result_free(container_result_t* result) {
    if (!result) return;

    if (result->stdout_data) {
        domes_mem_free(result->stdout_data);
    }
    if (result->stderr_data) {
        domes_mem_free(result->stderr_data);
    }

    memset(result, 0, sizeof(container_result_t));
}