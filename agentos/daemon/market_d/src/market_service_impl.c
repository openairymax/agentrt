#include "memory_compat.h"
#include "error.h"
/**
 * @file market_service_impl.c
 * @brief 市场服务核心实现
 * @details 定义 struct market_service 并实现 market_service.h 中的所有公共API
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "daemon_errors.h"
#include "market_service.h"
#include "svc_logger.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifndef _WIN32
#include <ftw.h>       /* nftw 递归删除目录树（仅 POSIX） */
#include <unistd.h>    /* rmdir 等 POSIX 文件操作 */
#include <sys/wait.h>  /* fork/execlp/waitpid/WIFEXITED 仅 POSIX 可用 */
#else
#include <windows.h>   /* CreateProcess / FindFirstFile / RemoveDirectory 等 Windows API */
#endif

#define MAX_AGENTS 256
#define MAX_SKILLS 256

#ifdef _WIN32
/**
 * @brief Windows 等价的同步外部命令执行（替代 POSIX fork/execlp/waitpid）。
 *
 * 语义对齐原 curl/tar 调用：
 *   - 不经过 shell：lpApplicationName=NULL，命令行首 token 为可执行文件名，
 *     CreateProcess 按 PATH 搜索（等价 execlp），无命令注入风险；
 *   - 输出继承父进程控制台句柄（对齐 POSIX 版本未重定向的行为）；
 *   - 阻塞等待子进程退出并返回退出码；CreateProcess 失败返回 -1
 *     （等价 POSIX fork 失败 / execlp 失败 _exit(127) 的非零语义）。
 *
 * 局限：Windows 命令行经 CreateProcess 重新解析为 argv，含空格的参数已加
 * 双引号转义；本上下文参数为受控的 URL/路径/flag，可安全处理。
 */
static int win_run_command(const char *prog, const char *const args[])
{
    char cmdline[2048];
    size_t off = 0;
    int n = snprintf(cmdline, sizeof(cmdline), "\"%s\"", prog);
    if (n < 0) return -1;
    off = (size_t)n;
    for (size_t i = 0; args && args[i] && off < sizeof(cmdline) - 1; i++) {
        const char *a = args[i];
        int quote = (strpbrk(a, " \t\"") != NULL);
        n = quote
                ? snprintf(cmdline + off, sizeof(cmdline) - off, " \"%s\"", a)
                : snprintf(cmdline + off, sizeof(cmdline) - off, " %s", a);
        if (n < 0) break;
        off += (size_t)n;
    }
    cmdline[sizeof(cmdline) - 1] = '\0';

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}
#endif /* _WIN32 */

#ifndef _WIN32
static int nftw_remove_cb(const char *fpath, const struct stat *sb, int typeflag,
                          struct FTW *ftwbuf)
{
    (void)sb;
    (void)ftwbuf;
    if (typeflag == FTW_DP || typeflag == FTW_D) {
        return rmdir(fpath);
    }
    return remove(fpath);
}
#endif

/**
 * @brief 递归删除目录树（跨平台）
 *
 * POSIX: 使用 nftw 深度优先遍历删除。
 * Windows: 使用 FindFirstFile/FindNextFile 递归下降删除，
 *          清除只读属性后删除文件，最后 RemoveDirectory 删除空目录。
 *          行为对齐 POSIX 版本，无 shell 调用，无命令注入风险。
 */
static int recursive_remove(const char *path)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return -1; /* 路径不存在 */
    }
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        /* 文件：清除只读属性后直接删除 */
        SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
        return DeleteFileA(path) ? 0 : -1;
    }
    /* 目录：递归删除内容后删除目录本身 */
    char pattern[MAX_PATH];
    WIN32_FIND_DATAA fd;
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return RemoveDirectoryA(path) ? 0 : -1;
    }
    do {
        if (fd.cFileName[0] == '.') continue;
        char child[MAX_PATH];
        snprintf(child, sizeof(child), "%s\\%s", path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            recursive_remove(child);
        } else {
            SetFileAttributesA(child, FILE_ATTRIBUTE_NORMAL);
            DeleteFileA(child);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return RemoveDirectoryA(path) ? 0 : -1;
#else
    return nftw(path, nftw_remove_cb, 64, FTW_DEPTH | FTW_PHYS);
#endif
}

static int is_safe_for_shell(const char *str)
{
    if (!str)
        return 0;
    const char *dangerous = ";|&$`'\"\\(){}[]!#~<>\n\r";
    for (size_t i = 0; i < strlen(dangerous); i++) {
        if (strchr(str, dangerous[i]))
            return 0;
    }
    return 1;
}

static int is_valid_url(const char *url)
{
    if (!url || strlen(url) == 0)
        return 0;
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
        return 0;
    return is_safe_for_shell(url);
}

static int is_safe_path_component(const char *str)
{
    if (!str || strlen(str) == 0)
        return 0;
    if (strchr(str, '/') || strchr(str, '\\'))
        return 0;
    if (strstr(str, ".."))
        return 0;
    return 1;
}

struct market_service {
    market_config_t config;
    agent_info_t *agents[MAX_AGENTS];
    size_t agent_count;
    skill_info_t *skills[MAX_SKILLS];
    size_t skill_count;
    int initialized;
};

int market_service_create(const market_config_t *config, market_service_t **service)
{
    market_config_t default_cfg;
    if (!service) {
        SVC_LOG_ERROR("market_service_create: NULL service output parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (!config) {
        __builtin_memset(&default_cfg, 0, sizeof(default_cfg));
        default_cfg.cache_ttl_ms = 3600000;
        default_cfg.sync_interval_ms = 30000;
        config = &default_cfg;
    }

    market_service_t *svc = (market_service_t *)AGENTOS_CALLOC(1, sizeof(market_service_t));
    if (!svc) {
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate service struct");
    }

    __builtin_memcpy(&svc->config, config, sizeof(market_config_t));
    if (config->registry_url)
        svc->config.registry_url = AGENTOS_STRDUP(config->registry_url);
    if (config->storage_path)
        svc->config.storage_path = AGENTOS_STRDUP(config->storage_path);

    svc->initialized = 1;
    *service = svc;
    return 0;
}

int market_service_destroy(market_service_t *service)
{
    if (!service) {
        SVC_LOG_ERROR("market_service_destroy: NULL service parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    for (size_t i = 0; i < service->agent_count; i++) {
        if (service->agents[i]) {
            AGENTOS_FREE(service->agents[i]->agent_id);
            AGENTOS_FREE(service->agents[i]->name);
            AGENTOS_FREE(service->agents[i]->version);
            AGENTOS_FREE(service->agents[i]->description);
            AGENTOS_FREE(service->agents[i]->author);
            AGENTOS_FREE(service->agents[i]->repository);
            AGENTOS_FREE(service->agents[i]->dependencies);
            AGENTOS_FREE(service->agents[i]);
        }
    }

    for (size_t i = 0; i < service->skill_count; i++) {
        if (service->skills[i]) {
            AGENTOS_FREE(service->skills[i]->skill_id);
            AGENTOS_FREE(service->skills[i]->name);
            AGENTOS_FREE(service->skills[i]->version);
            AGENTOS_FREE(service->skills[i]->description);
            AGENTOS_FREE(service->skills[i]->author);
            AGENTOS_FREE(service->skills[i]->repository);
            AGENTOS_FREE(service->skills[i]->dependencies);
            AGENTOS_FREE(service->skills[i]);
        }
    }

    AGENTOS_FREE((void *)service->config.registry_url);
    AGENTOS_FREE((void *)service->config.storage_path);
    AGENTOS_FREE(service);
    return 0;
}

int market_service_register_agent(market_service_t *service, const agent_info_t *agent_info)
{
    if (!service || !agent_info || !service->initialized) {
        SVC_LOG_ERROR("market_service_register_agent: NULL parameter or not initialized (service=%p, agent_info=%p, initialized=%d)", (const void *)service, (const void *)agent_info, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (service->agent_count >= MAX_AGENTS) {
        SVC_LOG_ERROR("market_service_register_agent: max agents exceeded (count=%zu, max=%d)", service->agent_count, MAX_AGENTS);
        AGENTOS_ERROR(AGENTOS_ERR_OVERFLOW, "max agents exceeded");
    }

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_info->agent_id) == 0) {
            AGENTOS_FREE(service->agents[i]->name);
            service->agents[i]->name = NULL;
            AGENTOS_FREE(service->agents[i]->version);
            service->agents[i]->version = NULL;
            AGENTOS_FREE(service->agents[i]->description);
            service->agents[i]->description = NULL;
            AGENTOS_FREE(service->agents[i]->author);
            service->agents[i]->author = NULL;
            AGENTOS_FREE(service->agents[i]->repository);
            service->agents[i]->repository = NULL;
            AGENTOS_FREE(service->agents[i]->dependencies);
            service->agents[i]->dependencies = NULL;

            service->agents[i]->name = agent_info->name ? AGENTOS_STRDUP(agent_info->name) : NULL;
            service->agents[i]->version =
                agent_info->version ? AGENTOS_STRDUP(agent_info->version) : NULL;
            service->agents[i]->description =
                agent_info->description ? AGENTOS_STRDUP(agent_info->description) : NULL;
            service->agents[i]->type = agent_info->type;
            service->agents[i]->status = agent_info->status;
            service->agents[i]->author =
                agent_info->author ? AGENTOS_STRDUP(agent_info->author) : NULL;
            service->agents[i]->repository =
                agent_info->repository ? AGENTOS_STRDUP(agent_info->repository) : NULL;
            service->agents[i]->dependencies =
                agent_info->dependencies ? AGENTOS_STRDUP(agent_info->dependencies) : NULL;
            service->agents[i]->rating = agent_info->rating;
            service->agents[i]->download_count = agent_info->download_count;
            service->agents[i]->last_updated = (uint64_t)time(NULL);
            return 0;
        }
    }

    agent_info_t *new_agent = (agent_info_t *)AGENTOS_CALLOC(1, sizeof(agent_info_t));
    if (!new_agent) {
        SVC_LOG_ERROR("market_service_register_agent: calloc failed for new agent entry");
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate agent entry");
    }

    new_agent->agent_id = agent_info->agent_id ? AGENTOS_STRDUP(agent_info->agent_id) : NULL;
    new_agent->name = agent_info->name ? AGENTOS_STRDUP(agent_info->name) : NULL;
    new_agent->version = agent_info->version ? AGENTOS_STRDUP(agent_info->version) : NULL;
    new_agent->description =
        agent_info->description ? AGENTOS_STRDUP(agent_info->description) : NULL;
    new_agent->type = agent_info->type;
    new_agent->status = agent_info->status;
    new_agent->author = agent_info->author ? AGENTOS_STRDUP(agent_info->author) : NULL;
    new_agent->repository = agent_info->repository ? AGENTOS_STRDUP(agent_info->repository) : NULL;
    new_agent->dependencies =
        agent_info->dependencies ? AGENTOS_STRDUP(agent_info->dependencies) : NULL;
    if (!new_agent->agent_id || !new_agent->name || !new_agent->version) {
        SVC_LOG_ERROR("market_service_register_agent: strdup failed for required agent fields (agent_id=%p, name=%p, version=%p)", (const void *)new_agent->agent_id, (const void *)new_agent->name, (const void *)new_agent->version);
        AGENTOS_FREE(new_agent->agent_id);
        AGENTOS_FREE(new_agent->name);
        AGENTOS_FREE(new_agent->version);
        AGENTOS_FREE(new_agent->description);
        AGENTOS_FREE(new_agent->author);
        AGENTOS_FREE(new_agent->repository);
        AGENTOS_FREE(new_agent->dependencies);
        AGENTOS_FREE(new_agent);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to duplicate agent required fields");
    }
    new_agent->rating = agent_info->rating;
    new_agent->download_count = agent_info->download_count;
    new_agent->last_updated = (uint64_t)time(NULL);

    service->agents[service->agent_count++] = new_agent;
    return 0;
}

int market_service_register_skill(market_service_t *service, const skill_info_t *skill_info)
{
    if (!service || !skill_info || !service->initialized) {
        SVC_LOG_ERROR("market_service_register_skill: NULL parameter or not initialized (service=%p, skill_info=%p, initialized=%d)", (const void *)service, (const void *)skill_info, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (service->skill_count >= MAX_SKILLS) {
        SVC_LOG_ERROR("market_service_register_skill: max skills exceeded (count=%zu, max=%d)", service->skill_count, MAX_SKILLS);
        AGENTOS_ERROR(AGENTOS_ERR_OVERFLOW, "max skills exceeded");
    }

    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, skill_info->skill_id) == 0) {
            AGENTOS_FREE(service->skills[i]->name);
            service->skills[i]->name = NULL;
            AGENTOS_FREE(service->skills[i]->version);
            service->skills[i]->version = NULL;
            AGENTOS_FREE(service->skills[i]->description);
            service->skills[i]->description = NULL;
            AGENTOS_FREE(service->skills[i]->author);
            service->skills[i]->author = NULL;
            AGENTOS_FREE(service->skills[i]->repository);
            service->skills[i]->repository = NULL;
            AGENTOS_FREE(service->skills[i]->dependencies);
            service->skills[i]->dependencies = NULL;

            service->skills[i]->name = skill_info->name ? AGENTOS_STRDUP(skill_info->name) : NULL;
            service->skills[i]->version =
                skill_info->version ? AGENTOS_STRDUP(skill_info->version) : NULL;
            service->skills[i]->description =
                skill_info->description ? AGENTOS_STRDUP(skill_info->description) : NULL;
            service->skills[i]->type = skill_info->type;
            service->skills[i]->author =
                skill_info->author ? AGENTOS_STRDUP(skill_info->author) : NULL;
            service->skills[i]->repository =
                skill_info->repository ? AGENTOS_STRDUP(skill_info->repository) : NULL;
            service->skills[i]->dependencies =
                skill_info->dependencies ? AGENTOS_STRDUP(skill_info->dependencies) : NULL;
            service->skills[i]->rating = skill_info->rating;
            service->skills[i]->download_count = skill_info->download_count;
            service->skills[i]->last_updated = (uint64_t)time(NULL);
            return 0;
        }
    }

    skill_info_t *new_skill = (skill_info_t *)AGENTOS_CALLOC(1, sizeof(skill_info_t));
    if (!new_skill) {
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate skill entry");
    }

    new_skill->skill_id = skill_info->skill_id ? AGENTOS_STRDUP(skill_info->skill_id) : NULL;
    new_skill->name = skill_info->name ? AGENTOS_STRDUP(skill_info->name) : NULL;
    new_skill->version = skill_info->version ? AGENTOS_STRDUP(skill_info->version) : NULL;
    new_skill->description =
        skill_info->description ? AGENTOS_STRDUP(skill_info->description) : NULL;
    new_skill->type = skill_info->type;
    new_skill->author = skill_info->author ? AGENTOS_STRDUP(skill_info->author) : NULL;
    new_skill->repository = skill_info->repository ? AGENTOS_STRDUP(skill_info->repository) : NULL;
    new_skill->dependencies =
        skill_info->dependencies ? AGENTOS_STRDUP(skill_info->dependencies) : NULL;
    if (!new_skill->skill_id || !new_skill->name || !new_skill->version) {
        AGENTOS_FREE(new_skill->skill_id);
        AGENTOS_FREE(new_skill->name);
        AGENTOS_FREE(new_skill->version);
        AGENTOS_FREE(new_skill->description);
        AGENTOS_FREE(new_skill->author);
        AGENTOS_FREE(new_skill->repository);
        AGENTOS_FREE(new_skill->dependencies);
        AGENTOS_FREE(new_skill);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to duplicate skill required fields");
    }
    new_skill->rating = skill_info->rating;
    new_skill->download_count = skill_info->download_count;
    new_skill->last_updated = (uint64_t)time(NULL);

    service->skills[service->skill_count++] = new_skill;
    return 0;
}

int market_service_search_agents(market_service_t *service, const search_params_t *params,
                                 agent_info_t ***agents, size_t *count)
{
    if (!service || !params || !agents || !count || !service->initialized) {
        SVC_LOG_ERROR("market_service_search_agents: NULL parameter or not initialized (service=%p, params=%p, agents=%p, count=%p, initialized=%d)", (const void *)service, (const void *)params, (const void *)agents, (const void *)count, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    size_t results_size = 16;
    agent_info_t **results = (agent_info_t **)AGENTOS_MALLOC(sizeof(agent_info_t *) * results_size);
    if (!results) {
        SVC_LOG_ERROR("market_service_search_agents: malloc failed for search results");
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate search results");
    }

    size_t found = 0;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (params->query && strlen(params->query) > 0) {
            if (!strstr(service->agents[i]->agent_id, params->query) &&
                !strstr(service->agents[i]->name, params->query) &&
                !(service->agents[i]->description &&
                  strstr(service->agents[i]->description, params->query))) {
                continue;
            }
        }

        if (found >= results_size) {
            results_size *= 2;
            agent_info_t **tmp =
                (agent_info_t **)AGENTOS_REALLOC(results, sizeof(agent_info_t *) * results_size);
            if (!tmp) {
                SVC_LOG_ERROR("market_service_search_agents: realloc failed for search results (results_size=%zu)", results_size);
                AGENTOS_FREE(results);
                AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to resize search results");
            }
            results = tmp;
        }

        results[found++] = service->agents[i];
        if (params->limit > 0 && found >= params->limit)
            break;
    }

    *agents = results;
    *count = found;
    return 0;
}

int market_service_search_skills(market_service_t *service, const search_params_t *params,
                                 skill_info_t ***skills, size_t *count)
{
    if (!service || !params || !skills || !count || !service->initialized)
        return AGENTOS_ERR_INVALID_PARAM;

    size_t results_size = 16;
    skill_info_t **results = (skill_info_t **)AGENTOS_MALLOC(sizeof(skill_info_t *) * results_size);
    if (!results) {
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate skill search results");
    }

    size_t found = 0;
    for (size_t i = 0; i < service->skill_count; i++) {
        if (params->query && strlen(params->query) > 0) {
            if (!strstr(service->skills[i]->skill_id, params->query) &&
                !strstr(service->skills[i]->name, params->query) &&
                !(service->skills[i]->description &&
                  strstr(service->skills[i]->description, params->query))) {
                continue;
            }
        }

        if (found >= results_size) {
            results_size *= 2;
            skill_info_t **tmp =
                (skill_info_t **)AGENTOS_REALLOC(results, sizeof(skill_info_t *) * results_size);
            if (!tmp) {
                AGENTOS_FREE(results);
                AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to resize skill search results");
            }
            results = tmp;
        }

        results[found++] = service->skills[i];
        if (params->limit > 0 && found >= params->limit)
            break;
    }

    *skills = results;
    *count = found;
    return 0;
}

int market_service_install_agent(market_service_t *service, const install_request_t *request,
                                 install_result_t **result)
{
    if (!service || !request || !result || !service->initialized) {
        SVC_LOG_ERROR("market_service_install_agent: NULL parameter or not initialized (service=%p, request=%p, result=%p, initialized=%d)", (const void *)service, (const void *)request, (const void *)result, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (!is_safe_path_component(request->id)) {
        SVC_LOG_ERROR("market_service_install_agent: unsafe path component in install request id (id=%s)", request->id ? request->id : "NULL");
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "install request id is unsafe");
    }

    install_result_t *res = (install_result_t *)AGENTOS_CALLOC(1, sizeof(install_result_t));
    if (!res) {
        SVC_LOG_ERROR("market_service_install_agent: calloc failed for install result");
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate install result");
    }

    agent_info_t *target = NULL;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, request->id) == 0) {
            target = service->agents[i];
            break;
        }
    }

    if (!target) {
        res->success = false;
        res->message = AGENTOS_STRDUP("Agent not found");
        res->error_code = -3;
        *result = res;
        return 0;
    }

    const char *base_path =
        request->install_path
            ? request->install_path
            : (service->config.storage_path ? service->config.storage_path : "./agents");
    char install_dir[1024];
    snprintf(install_dir, sizeof(install_dir), "%s/%s", base_path, request->id);

    {
        int mkret = mkdir(install_dir, 0755);
        if (mkret != 0 && errno != EEXIST) {
            SVC_LOG_ERROR("market_service_install_agent: mkdir failed for install directory (path=%s, errno=%d)", install_dir, errno);
            res->success = false;
            res->message = AGENTOS_STRDUP("Failed to create install directory");
            res->error_code = -4;
            *result = res;
            return 0;
        }
    }

    char meta_path[1024];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(meta_path, sizeof(meta_path), "%s/agent.json", install_dir);
#pragma GCC diagnostic pop
    FILE *meta_fp = fopen(meta_path, "w");
    if (meta_fp) {
        char _mi_buf[1024];
        fputs("{\n", meta_fp);
        snprintf(_mi_buf, sizeof(_mi_buf), "  \"agent_id\": \"%s\",\n", target->agent_id);
        fputs(_mi_buf, meta_fp);
        snprintf(_mi_buf, sizeof(_mi_buf), "  \"name\": \"%s\",\n",
                 target->name ? target->name : "");
        fputs(_mi_buf, meta_fp);
        snprintf(_mi_buf, sizeof(_mi_buf), "  \"version\": \"%s\",\n",
                 request->version ? request->version
                                  : (target->version ? target->version : "0.0.1"));
        fputs(_mi_buf, meta_fp);
        snprintf(_mi_buf, sizeof(_mi_buf), "  \"author\": \"%s\",\n",
                 target->author ? target->author : "");
        fputs(_mi_buf, meta_fp);
        fputs("  \"status\": \"installed\",\n", meta_fp);
        snprintf(_mi_buf, sizeof(_mi_buf), "  \"installed_at\": %lld\n", (long long)time(NULL));
        fputs(_mi_buf, meta_fp);
        fputs("}\n", meta_fp);
        fclose(meta_fp);
    }

    if (target->repository && strlen(target->repository) > 0 && is_valid_url(target->repository)) {
        char download_path[1024];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(download_path, sizeof(download_path), "%s/package.tar.gz", install_dir);
#pragma GCC diagnostic pop

#ifdef _WIN32
        /* Windows 等价：用 win_run_command（CreateProcess）替代 fork/execlp/waitpid。
         * 行为对齐：curl 失败则仅元数据安装；成功则解压 tar 并清理下载文件。 */
        {
            const char *const curl_args[] = {"-sfL", "-o", download_path,
                                             target->repository, NULL};
            int curl_ret = win_run_command("curl", curl_args);
            if (curl_ret != 0) {
                SVC_LOG_WARN(
                    "Download failed for agent %s from %s (curl_ret=%d), metadata only install",
                    request->id, target->repository, curl_ret);
            } else {
                const char *const tar_args[] = {"-xzf", download_path, "-C", install_dir, NULL};
                win_run_command("tar", tar_args);
                remove(download_path);
            }
        }
#else
        pid_t curl_pid = fork();
        if (curl_pid == 0) {
            execlp("curl", "curl", "-sfL", "-o", download_path, target->repository, (char *)NULL);
            _exit(127);
        } else if (curl_pid > 0) {
            int curl_status = 0;
            waitpid(curl_pid, &curl_status, 0);
            int curl_ret = WIFEXITED(curl_status) ? WEXITSTATUS(curl_status) : -1;
            if (curl_ret != 0) {
                SVC_LOG_WARN(
                    "Download failed for agent %s from %s (curl_ret=%d), metadata only install",
                    request->id, target->repository, curl_ret);
            } else {
                pid_t tar_pid = fork();
                if (tar_pid == 0) {
                    execlp("tar", "tar", "-xzf", download_path, "-C", install_dir, (char *)NULL);
                    _exit(127);
                } else if (tar_pid > 0) {
                    int tar_status = 0;
                    waitpid(tar_pid, &tar_status, 0);
                }
                remove(download_path);
            }
        } else {
            SVC_LOG_WARN("fork failed for agent %s download: %s", request->id, strerror(errno));
        }
#endif
    }

    target->status = AGENT_STATUS_AVAILABLE;
    target->download_count++;

    res->success = true;
    res->message = AGENTOS_STRDUP("Agent installed successfully");
    res->installed_version = AGENTOS_STRDUP(request->version ? request->version : target->version);
    res->install_path = AGENTOS_STRDUP(install_dir);
    res->error_code = 0;

    *result = res;
    return 0;
}

int market_service_install_skill(market_service_t *service, const install_request_t *request,
                                 install_result_t **result)
{
    if (!service || !request || !result || !service->initialized) {
        SVC_LOG_ERROR("market_service_install_skill: NULL parameter or not initialized (service=%p, request=%p, result=%p, initialized=%d)", (const void *)service, (const void *)request, (const void *)result, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    install_result_t *res = (install_result_t *)AGENTOS_CALLOC(1, sizeof(install_result_t));
    if (!res) {
        SVC_LOG_ERROR("market_service_install_skill: calloc failed for skill install result");
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate skill install result");
    }

    skill_info_t *target = NULL;
    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, request->id) == 0) {
            target = service->skills[i];
            break;
        }
    }

    if (!target) {
        res->success = false;
        res->message = AGENTOS_STRDUP("Skill not found");
        res->error_code = -3;
        *result = res;
        return 0;
    }

    target->download_count++;

    res->success = true;
    res->message = AGENTOS_STRDUP("Skill installed successfully");
    res->installed_version = AGENTOS_STRDUP(request->version ? request->version : target->version);
    res->install_path = AGENTOS_STRDUP(request->install_path ? request->install_path : "./skills");
    res->error_code = 0;

    *result = res;
    return 0;
}

int market_service_uninstall_agent(market_service_t *service, const char *agent_id)
{
    if (!service || !agent_id || !service->initialized) {
        SVC_LOG_ERROR("market_service_uninstall_agent: NULL parameter or not initialized (service=%p, agent_id=%p, initialized=%d)", (const void *)service, (const void *)agent_id, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (!is_safe_path_component(agent_id)) {
        SVC_LOG_ERROR("market_service_uninstall_agent: unsafe path component in agent_id (agent_id=%s)", agent_id);
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "agent_id is unsafe path component");
    }

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_id) == 0) {
            const char *storage =
                service->config.storage_path ? service->config.storage_path : "./agents";
            char install_dir[1024];
            snprintf(install_dir, sizeof(install_dir), "%s/%s", storage, agent_id);

            int rm_ret = recursive_remove(install_dir);
            if (rm_ret != 0) {
                SVC_LOG_WARN("Failed to remove install directory: %s (ret=%d)", install_dir,
                             rm_ret);
            }

            service->agents[i]->status = AGENT_STATUS_DISABLED;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_NOT_FOUND, "agent not found for uninstall");
}

int market_service_uninstall_skill(market_service_t *service, const char *skill_id)
{
    if (!service || !skill_id || !service->initialized)
        return AGENTOS_ERR_INVALID_PARAM;

    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, skill_id) == 0) {
            AGENTOS_FREE(service->skills[i]->skill_id);
            AGENTOS_FREE(service->skills[i]->name);
            AGENTOS_FREE(service->skills[i]->version);
            AGENTOS_FREE(service->skills[i]->description);
            AGENTOS_FREE(service->skills[i]->author);
            AGENTOS_FREE(service->skills[i]->repository);
            AGENTOS_FREE(service->skills[i]->dependencies);
            AGENTOS_FREE(service->skills[i]);

            for (size_t j = i; j < service->skill_count - 1; j++) {
                service->skills[j] = service->skills[j + 1];
            }
            service->skill_count--;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_NOT_FOUND, "skill not found for uninstall");
}

int market_service_get_installed_agents(market_service_t *service, agent_info_t ***agents,
                                        size_t *count)
{
    if (!service || !agents || !count || !service->initialized)
        return AGENTOS_ERR_INVALID_PARAM;

    size_t results_size = 16;
    agent_info_t **results = (agent_info_t **)AGENTOS_MALLOC(sizeof(agent_info_t *) * results_size);
    if (!results) {
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate installed agents list");
    }

    size_t found = 0;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (service->agents[i]->status == AGENT_STATUS_AVAILABLE ||
            service->agents[i]->status == AGENT_STATUS_ERROR) {

            if (found >= results_size) {
                results_size *= 2;
                agent_info_t **tmp = (agent_info_t **)AGENTOS_REALLOC(
                    results, sizeof(agent_info_t *) * results_size);
                if (!tmp) {
                    AGENTOS_FREE(results);
                    AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY,
                                  "failed to resize installed agents list");
                }
                results = tmp;
            }

            results[found++] = service->agents[i];
        }
    }

    *agents = results;
    *count = found;
    return 0;
}

int market_service_get_installed_skills(market_service_t *service, skill_info_t ***skills,
                                        size_t *count)
{
    if (!service || !skills || !count || !service->initialized)
        return AGENTOS_ERR_INVALID_PARAM;

    size_t results_size = 16;
    skill_info_t **results = (skill_info_t **)AGENTOS_MALLOC(sizeof(skill_info_t *) * results_size);
    if (!results) {
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate installed skills list");
    }

    size_t found = 0;
    for (size_t i = 0; i < service->skill_count; i++) {
        if (found >= results_size) {
            results_size *= 2;
            skill_info_t **tmp =
                (skill_info_t **)AGENTOS_REALLOC(results, sizeof(skill_info_t *) * results_size);
            if (!tmp) {
                AGENTOS_FREE(results);
                AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to resize installed skills list");
            }
            results = tmp;
        }

        results[found++] = service->skills[i];
    }

    *skills = results;
    *count = found;
    return 0;
}

int market_service_check_update(market_service_t *service, const char *id, bool *has_update,
                                char **latest_version)
{
    if (!service || !id || !has_update || !latest_version || !service->initialized)
        return AGENTOS_ERR_INVALID_PARAM;

    *has_update = false;

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, id) == 0) {
            *latest_version = AGENTOS_STRDUP(service->agents[i]->version);
            return 0;
        }
    }

    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, id) == 0) {
            *latest_version = AGENTOS_STRDUP(service->skills[i]->version);
            return 0;
        }
    }

    *latest_version = NULL;
    AGENTOS_ERROR(AGENTOS_ERR_NOT_FOUND, "update check: id not found");
}

int market_service_reload_config(market_service_t *service, const market_config_t *config)
{
    if (!service || !config || !service->initialized)
        return AGENTOS_ERR_INVALID_PARAM;

    // Save old owned pointers
    char *old_url = (char *)service->config.registry_url;
    char *old_path = (char *)service->config.storage_path;

    // Copy non-pointer fields, preserving old owned pointers temporarily
    {
        market_config_t tmp = *config;
        tmp.registry_url = old_url;
        tmp.storage_path = old_path;
        service->config = tmp;
    }

    // Replace pointer fields with our own copies
    char *new_url = config->registry_url ? AGENTOS_STRDUP(config->registry_url) : NULL;
    char *new_path = config->storage_path ? AGENTOS_STRDUP(config->storage_path) : NULL;

    if (config->registry_url && !new_url) {
        AGENTOS_FREE(new_path);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to duplicate registry_url");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    if (config->storage_path && !new_path) {
        AGENTOS_FREE(new_url);
        service->config.registry_url = new_url;
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to duplicate storage_path");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    service->config.registry_url = new_url;
    service->config.storage_path = new_path;
    AGENTOS_FREE(old_url);
    AGENTOS_FREE(old_path);

    return 0;
}

int market_service_sync_registry(market_service_t *service)
{
    if (!service || !service->initialized)
        return AGENTOS_ERR_INVALID_PARAM;

    if (!service->config.enable_remote_registry) {
        return 0;
    }

    if (!service->config.registry_url || strlen(service->config.registry_url) == 0) {
        SVC_LOG_WARN("Sync registry: no registry_url configured");
        return 0;
    }

    const char *storage =
        service->config.storage_path ? service->config.storage_path : AGENTOS_CACHE_DIR;

    {
        size_t pos = 0;
        char tmp[1024];
AGENTOS_STRNCPY_TERM(tmp, storage, sizeof(tmp));
        (tmp)[sizeof(tmp) - 1] = '\0';
        tmp[sizeof(tmp) - 1] = '\0';
        while (tmp[pos]) {
            if (tmp[pos] == '/' && pos > 0) {
                tmp[pos] = '\0';
                mkdir(tmp, 0755);
                tmp[pos] = '/';
            }
            pos++;
        }
        mkdir(tmp, 0755);
    }

    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/registry_index.json", storage);

    if (!is_safe_for_shell(service->config.registry_url)) {
        SVC_LOG_WARN("Sync registry: invalid registry_url, possible injection detected");
        return 0;
    }

    char url[2048];
    if (strncmp(service->config.registry_url, "http", 4) == 0) {
        snprintf(url, sizeof(url), "%s/index.json", service->config.registry_url);
    } else {
        snprintf(url, sizeof(url), "https://%s/index.json", service->config.registry_url);
    }

#ifdef _WIN32
    /* Windows 等价：用 win_run_command（CreateProcess）替代 fork/execlp/waitpid。
     * 行为对齐：curl 失败（含 CreateProcess 找不到 curl）则告警并返回 0。 */
    {
        const char *const curl_args[] = {"-sfL", "-o", index_path, url,
                                         "--connect-timeout", "10", "--max-time", "60", NULL};
        int curl_ret = win_run_command("curl", curl_args);
        if (curl_ret != 0) {
            SVC_LOG_WARN("Sync registry: download failed from %s (curl_ret=%d)", url, curl_ret);
            return 0;
        }
    }
#else
    pid_t curl_pid = fork();
    if (curl_pid == 0) {
        execlp("curl", "curl", "-sfL", "-o", index_path, url, "--connect-timeout", "10",
               "--max-time", "60", (char *)NULL);
        _exit(127);
    } else if (curl_pid > 0) {
        int curl_status = 0;
        waitpid(curl_pid, &curl_status, 0);
        int curl_ret = WIFEXITED(curl_status) ? WEXITSTATUS(curl_status) : -1;
        if (curl_ret != 0) {
            SVC_LOG_WARN("Sync registry: download failed from %s (curl_ret=%d)", url, curl_ret);
            return 0;
        }
    } else {
        SVC_LOG_WARN("Sync registry: fork failed: %s", strerror(errno));
        AGENTOS_ERROR(AGENTOS_ERR_IO, "fork failed during sync");
    }
#endif

    FILE *idx_fp = fopen(index_path, "r");
    if (!idx_fp) {
        SVC_LOG_WARN("Sync registry: cannot open downloaded index %s", index_path);
        return 0;
    }

    fseek(idx_fp, 0, SEEK_END);
    long fsize = ftell(idx_fp);
    fseek(idx_fp, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 10 * 1024 * 1024) {
        fclose(idx_fp);
        SVC_LOG_WARN("Sync registry: invalid index size %ld", fsize);
        return 0;
    }

    char *idx_data = (char *)AGENTOS_MALLOC((size_t)fsize + 1);
    if (!idx_data) {
        fclose(idx_fp);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "failed to allocate index buffer");
    }
    size_t nread = fread(idx_data, 1, (size_t)fsize, idx_fp);
    if (nread != (size_t)fsize) {
        AGENTOS_FREE(idx_data);
        idx_data = NULL;
        fclose(idx_fp);
        AGENTOS_ERROR(AGENTOS_ERR_IO, "fread index file failed");
    }
    idx_data[nread] = '\0';
    fclose(idx_fp);

    char *entry = strstr(idx_data, "\"agent_id\"");
    int synced = 0;
    while (entry && synced < 256) {
        char *id_start = strchr(entry, ':');
        if (!id_start)
            break;
        id_start++;
        while (*id_start && (*id_start == ' ' || *id_start == '\t' || *id_start == '"'))
            id_start++;
        char *id_end = id_start;
        while (*id_end && *id_end != '"' && *id_end != ',' && *id_end != '}')
            id_end++;

        size_t id_len = (size_t)(id_end - id_start);
        if (id_len > 0 && id_len < 128) {
            char found_id[128];
            __builtin_memcpy(found_id, id_start, id_len);
            found_id[id_len] = '\0';

            int already_exists = 0;
            for (size_t i = 0; i < service->agent_count; i++) {
                if (strcmp(service->agents[i]->agent_id, found_id) == 0) {
                    already_exists = 1;
                    break;
                }
            }

            if (!already_exists && service->agent_count < MAX_AGENTS) {
                agent_info_t *new_agent = (agent_info_t *)AGENTOS_CALLOC(1, sizeof(agent_info_t));
                if (new_agent) {
                    new_agent->agent_id = AGENTOS_STRDUP(found_id);
                    new_agent->name = AGENTOS_STRDUP(found_id);
                    new_agent->version = AGENTOS_STRDUP("latest");
                    new_agent->status = AGENT_STATUS_AVAILABLE;
                    service->agents[service->agent_count++] = new_agent;
                    synced++;
                }
            }
        }

        entry = strstr(id_end + 1, "\"agent_id\"");
    }

    AGENTOS_FREE(idx_data);
    idx_data = NULL;
    SVC_LOG_INFO("Sync registry: synced %d new agents from %s", synced, url);
    return 0;
}
