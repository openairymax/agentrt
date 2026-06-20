/**
 * @file config_loader.c
 * @brief 配置加载器实?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "config_loader.h"

#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <errno.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"
#include "agentos_quality.h"

#define AGENTOS_MAX_CONFIG_SIZE (4 * 1024 * 1024)

/**
 * @brief 从文件加载配置内容
 * @param path 文件路径
 * @param out_json 输出文件内容字符串（需调用者释放）
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_config_load(const char *path, char **out_json)
{
    if (!path || !out_json)
        return AGENTOS_EINVAL;

    FILE *file = fopen(path, "rb");
    if (!file) {
        AGENTOS_LOG_ERROR("Failed to open manager file: %s", path);
        return AGENTOS_ENOENT;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0 || size > AGENTOS_MAX_CONFIG_SIZE) {
        fclose(file);
        return (size < 0) ? AGENTOS_EIO : AGENTOS_EINVAL;
    }

    char *buffer = (char *)AGENTOS_MALLOC((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return AGENTOS_ENOMEM;
    }

    size_t read = fread(buffer, 1, (size_t)size, file);
    if (read != (size_t)size) {
        AGENTOS_FREE(buffer);
        fclose(file);
        return AGENTOS_EIO;
    }
    buffer[read] = '\0';
    fclose(file);

    *out_json = buffer;
    return AGENTOS_SUCCESS;
}

/* ================================================================
 * C-L01: Manager → CoreLoopThree 连接线实现（P1.1）
 * ================================================================ */

#include "yaml_loader.h"
#include "memory_compat.h"

#include <sys/stat.h>

#define AGENTOS_DEFAULT_CONFIG_PATH  "./configs/agentos.yaml"
#define AGENTOS_CONFIG_WATCH_INTERVAL_MS 1000
#define AGENTOS_MAX_RELOAD_CALLBACKS 32

/* ── 全局配置实例 ── */
static agentos_yaml_config_t g_global_config;
static bool g_config_loaded = false;
static char g_config_path[256] = AGENTOS_DEFAULT_CONFIG_PATH;

/* ── 热重载回调 ── */
static agentos_config_reload_cb_t g_reload_callbacks[AGENTOS_MAX_RELOAD_CALLBACKS];
static void *g_reload_userdata[AGENTOS_MAX_RELOAD_CALLBACKS];
static size_t g_reload_cb_count = 0;

/* ── 监听线程 ── */
static pthread_t g_watch_thread;
static volatile bool g_watch_running = false;
static int g_inotify_fd = -1;     /* P3.15: inotify 文件描述符 */

int agentos_config_load_yaml(const char *yaml_path,
                             agentos_yaml_config_t *config)
{
    if (!config) return -1;

    const char *path = yaml_path ? yaml_path : AGENTOS_DEFAULT_CONFIG_PATH;

    AGENTOS_LOG_INFO("ConfigLoader: loading YAML config from %s", path);

    /* 保存路径用于热重载 */
    safe_strcpy(g_config_path, sizeof(g_config_path), path);

    int ret = agentos_yaml_load(path, config);
    if (ret != 0) {
        /* 加载失败，使用默认配置 */
        AGENTOS_LOG_WARN("ConfigLoader: YAML load failed for %s, using defaults", path);
        agentos_yaml_config_defaults(config);
        agentos_yaml_resolve_platform_paths(config);
        return 0; /* 返回成功但使用默认值 */
    }

    AGENTOS_LOG_INFO("ConfigLoader: YAML loaded successfully from %s", path);

    /* 平台路径映射（P1.12.4） */
    agentos_yaml_resolve_platform_paths(config);

    /* 验证配置 */
    if (agentos_yaml_validate(config) != 0) {
        agentos_yaml_config_defaults(config);
        agentos_yaml_resolve_platform_paths(config);
    }

    return 0;
}

const agentos_yaml_config_t *agentos_config_get_global(void)
{
    if (!g_config_loaded) {
        return NULL;
    }
    return &g_global_config;
}

int agentos_config_on_reload(agentos_config_reload_cb_t callback,
                             void *user_data)
{
    if (!callback) return -1;
    if (g_reload_cb_count >= AGENTOS_MAX_RELOAD_CALLBACKS) return -1;

    g_reload_callbacks[g_reload_cb_count] = callback;
    g_reload_userdata[g_reload_cb_count] = user_data;
    g_reload_cb_count++;
    return 0;
}

static void config_reload_notify(const agentos_yaml_config_t *old_config,
                                 const agentos_yaml_config_t *new_config)
{
    for (size_t i = 0; i < g_reload_cb_count; i++) {
        if (g_reload_callbacks[i]) {
            g_reload_callbacks[i](old_config, new_config,
                                  g_reload_userdata[i]);
        }
    }
}

/**
 * @brief P3.15: inotify 文件监听线程
 *
 * 使用 Linux inotify API 监听配置文件变更，
 * 替代 Phase 1 的轮询模式，实现零延迟热重载。
 *
 * 工作流程：
 *   1. 创建 inotify 实例，添加 IN_MODIFY | IN_CLOSE_WRITE 监听
 *   2. 阻塞等待 inotify 事件
 *   3. 检测到文件变更 → 触发配置重载 + 通知回调
 *   4. 循环直到 g_watch_running 为 false
 */
static void *config_watch_thread(void *arg)
{
    (void)arg;

    g_inotify_fd = inotify_init1(IN_NONBLOCK);
    if (g_inotify_fd < 0) {
        AGENTOS_LOG_ERROR("C-L01: ConfigLoader: WATCH-FAIL — inotify_init1 failed "
                          "errno=%d (%s) "
                          "STACK: inotify_init1(IN_NONBLOCK) → config_watch_thread() → "
                          "pthread_create()",
                          errno, strerror(errno));
        g_watch_running = false;
        return NULL;
    }

    /* 添加文件监听：IN_MODIFY (写入) + IN_CLOSE_WRITE (原子写入完成) */
    int wd = inotify_add_watch(g_inotify_fd, g_config_path,
                                IN_MODIFY | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF);
    if (wd < 0) {
        AGENTOS_LOG_ERROR("C-L01: ConfigLoader: WATCH-FAIL — inotify_add_watch failed "
                          "path=%s errno=%d (%s) inotify_fd=%d "
                          "STACK: inotify_add_watch() → config_watch_thread() → "
                          "g_watch_running=%d",
                          g_config_path, errno, strerror(errno), g_inotify_fd,
                          (int)g_watch_running);
        AGENTOS_LOG_ERROR("C-L01: ConfigLoader: WATCH-FAIL — check if file exists "
                          "and is readable: %s", g_config_path);
        close(g_inotify_fd);
        g_inotify_fd = -1;
        g_watch_running = false;
        return NULL;
    }

    AGENTOS_LOG_INFO("C-L01: ConfigLoader: WATCH-STARTED path=%s inotify_fd=%d wd=%d "
                     "(P3.15: inotify-based, zero-polling)",
                     g_config_path, g_inotify_fd, wd);

    /* 事件缓冲区：足够容纳多个 inotify_event */
    char event_buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

    while (g_watch_running) {
        /* 阻塞等待事件（1 秒超时，用于检查 g_watch_running） */
        ssize_t len = read(g_inotify_fd, event_buf, sizeof(event_buf));

        if (len < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                /* 非阻塞模式下的无数据或信号中断，继续循环 */
                usleep(100000); /* 100ms 避免忙等 */
                continue;
            }
            AGENTOS_LOG_WARN("C-L01: ConfigLoader: WATCH — read error (errno=%d)", errno);
            break;
        }

        if (len == 0) {
            /* EOF — 不应在 inotify 上发生 */
            AGENTOS_LOG_WARN("C-L01: ConfigLoader: WATCH — unexpected EOF");
            break;
        }

        /* 解析 inotify 事件 */
        ssize_t offset = 0;
        bool reload_triggered = false;

        while (offset < len) {
            struct inotify_event *event = (struct inotify_event *)(event_buf + offset);

            if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
                AGENTOS_LOG_INFO("C-L01: ConfigLoader: WATCH — file modified "
                                 "path=%s mask=0x%x", g_config_path, event->mask);
                reload_triggered = true;
            } else if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
                const char *event_type = (event->mask & IN_DELETE_SELF) ? "DELETE_SELF" : "MOVE_SELF";
                AGENTOS_LOG_WARN("C-L01: ConfigLoader: WATCH — file deleted/moved "
                                 "path=%s mask=0x%x event_type=%s cookie=%u name=%s "
                                 "inotify_fd=%d old_wd=%d "
                                 "TRACEBACK: [1] inotify event %s received "
                                 "[2] attempting to re-add watch on same path "
                                 "(re-adding watch)",
                                 g_config_path, event->mask, event_type, event->cookie,
                                 event->len > 0 ? event->name : "(none)",
                                 g_inotify_fd, wd,
                                 event_type);

                /* 重新添加监听 */
                int old_wd = wd;
                int rm_rc = inotify_rm_watch(g_inotify_fd, wd);
                AGENTOS_LOG_INFO("C-L01: ConfigLoader: WATCH — removed old watch "
                                 "old_wd=%d inotify_fd=%d rm_rc=%d "
                                 "TRACEBACK: inotify_rm_watch(old_wd=%d) → %s handler",
                                 old_wd, g_inotify_fd, rm_rc,
                                 old_wd, event_type);

                /* 获取文件状态用于诊断 */
                struct stat file_stat;
                int stat_rc = stat(g_config_path, &file_stat);
                const char *stat_err = (stat_rc != 0) ? strerror(errno) : "OK";

                wd = inotify_add_watch(g_inotify_fd, g_config_path,
                                        IN_MODIFY | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF);
                if (wd < 0) {
                    const char *err_str = strerror(errno);
                    AGENTOS_LOG_ERROR("C-L01: ConfigLoader: WATCH-RE-ADD-FAIL "
                                      "path=%s errno=%d (%s) old_wd=%d inotify_fd=%d "
                                      "mask=IN_MODIFY|IN_CLOSE_WRITE|IN_DELETE_SELF|IN_MOVE_SELF "
                                      "event_type=%s "
                                      "file_stat=(rc=%d, err=%s, size=%ld, mode=%o, uid=%d, gid=%d) "
                                      "STACK: inotify_add_watch() → config_watch_thread() → "
                                      "%s handler → "
                                      "g_watch_running=%d "
                                      "TRACEBACK: [1] inotify event %s (cookie=%u) "
                                      "[2] inotify_rm_watch(old_wd=%d) returned rc=%d "
                                      "[3] stat(%s) returned rc=%d (%s) "
                                      "[4] inotify_add_watch() failed errno=%d (%s) "
                                      "[5] FATAL: watch re-add failed, thread exiting",
                                      g_config_path, errno, err_str,
                                      old_wd, g_inotify_fd,
                                      event_type,
                                      stat_rc, stat_err,
                                      (stat_rc == 0) ? (long)file_stat.st_size : -1L,
                                      (stat_rc == 0) ? (unsigned int)file_stat.st_mode : 0U,
                                      (stat_rc == 0) ? (int)file_stat.st_uid : -1,
                                      (stat_rc == 0) ? (int)file_stat.st_gid : -1,
                                      event_type,
                                      (int)g_watch_running,
                                      event_type, event->cookie,
                                      old_wd, rm_rc,
                                      g_config_path, stat_rc, stat_err,
                                      errno, err_str);
                    AGENTOS_LOG_ERROR("C-L01: ConfigLoader: WATCH-RE-ADD-FAIL — "
                                      "file may have been permanently removed or permissions "
                                      "changed. Watch thread will exit. "
                                      "DIAGNOSIS: errno=%d (%s) → %s. "
                                      "Manual intervention required: restart daemon or "
                                      "restore config file at %s",
                                      errno, err_str,
                                      (errno == ENOENT) ? "file does not exist" :
                                      (errno == EACCES) ? "permission denied" :
                                      (errno == ENOSPC) ? "inotify watch limit reached" :
                                      (errno == ENOTDIR) ? "parent is not a directory" :
                                      "unknown error",
                                      g_config_path);
                    g_watch_running = false;
                    break;
                }

                AGENTOS_LOG_INFO("C-L01: ConfigLoader: WATCH — watch re-added successfully "
                                 "old_wd=%d new_wd=%d path=%s "
                                 "file_stat=(size=%ld, mode=%o) "
                                 "TRACEBACK: [1] inotify %s event "
                                 "[2] inotify_rm_watch(%d) OK "
                                 "[3] stat(%s) OK "
                                 "[4] inotify_add_watch() returned new_wd=%d",
                                 old_wd, wd, g_config_path,
                                 (stat_rc == 0) ? (long)file_stat.st_size : -1L,
                                 (stat_rc == 0) ? (unsigned int)file_stat.st_mode : 0U,
                                 event_type, old_wd,
                                 g_config_path, wd);
                reload_triggered = true;
            }

            offset += sizeof(struct inotify_event) + event->len;
        }

        /* 触发配置重载 */
        if (reload_triggered) {
            /* 短暂延迟：等待文件写入完成 */
            usleep(50000); /* 50ms */

            agentos_yaml_config_t new_config;
            memset(&new_config, 0, sizeof(new_config));

            /* 分步执行以捕获详细错误码 */
            int load_rc = agentos_yaml_load(g_config_path, &new_config);
            if (load_rc != 0) {
                /* 获取文件状态信息用于诊断 */
                struct stat file_stat;
                int stat_rc = stat(g_config_path, &file_stat);
                const char *stat_err = (stat_rc != 0) ? strerror(errno) : "OK";

                AGENTOS_LOG_ERROR("C-L01: ConfigLoader: WATCH-RELOAD-FAIL — "
                                  "YAML load failed "
                                  "path=%s rc=%d "
                                  "errno=%d (%s) "
                                  "file_stat=(rc=%d, err=%s, size=%ld, mode=%o, uid=%d, gid=%d) "
                                  "STACK: agentos_yaml_load() → config_watch_thread() → "
                                  "IN_MODIFY/IN_CLOSE_WRITE handler → "
                                  "g_watch_running=%d inotify_fd=%d wd=%d "
                                  "TRACEBACK: [1] inotify event received (mask=IN_MODIFY|IN_CLOSE_WRITE) "
                                  "[2] usleep(50000) for write stabilization "
                                  "[3] agentos_yaml_load() returned rc=%d "
                                  "[4] ERROR: config NOT reloaded, keeping previous config",
                                  g_config_path, load_rc,
                                  errno, strerror(errno),
                                  stat_rc, stat_err,
                                  (stat_rc == 0) ? (long)file_stat.st_size : -1L,
                                  (stat_rc == 0) ? (unsigned int)file_stat.st_mode : 0U,
                                  (stat_rc == 0) ? (int)file_stat.st_uid : -1,
                                  (stat_rc == 0) ? (int)file_stat.st_gid : -1,
                                  (int)g_watch_running, g_inotify_fd, wd,
                                  load_rc);
                AGENTOS_LOG_WARN("C-L01: ConfigLoader: WATCH-RELOAD-FAIL — "
                                 "keeping current config, file may be corrupted or "
                                 "have syntax errors. "
                                 "DIAGNOSIS: rc=%d → %s. "
                                 "Check file: %s",
                                 load_rc,
                                 (load_rc == -1) ? "YAML parse error (check syntax)" :
                                 (load_rc == -2) ? "file not found (race condition?)" :
                                 "unknown error",
                                 g_config_path);
                continue;
            }

            int val_rc = agentos_yaml_validate(&new_config);
            if (val_rc != 0) {
                /* 记录验证失败详情，同时记录文件状态 */
                struct stat file_stat;
                int stat_rc = stat(g_config_path, &file_stat);
                const char *stat_err = (stat_rc != 0) ? strerror(errno) : "OK";

                AGENTOS_LOG_ERROR("C-L01: ConfigLoader: WATCH-RELOAD-FAIL — "
                                  "YAML validation failed "
                                  "path=%s val_rc=%d "
                                  "errno=%d (%s) "
                                  "file_stat=(rc=%d, err=%s, size=%ld, mode=%o, uid=%d, gid=%d) "
                                  "STACK: agentos_yaml_validate() → config_watch_thread() → "
                                  "IN_MODIFY/IN_CLOSE_WRITE handler → "
                                  "g_watch_running=%d inotify_fd=%d wd=%d "
                                  "TRACEBACK: [1] inotify event received (mask=IN_MODIFY|IN_CLOSE_WRITE) "
                                  "[2] agentos_yaml_load() succeeded "
                                  "[3] agentos_yaml_validate() returned rc=%d "
                                  "[4] ERROR: config NOT reloaded, keeping previous config",
                                  g_config_path, val_rc,
                                  errno, strerror(errno),
                                  stat_rc, stat_err,
                                  (stat_rc == 0) ? (long)file_stat.st_size : -1L,
                                  (stat_rc == 0) ? (unsigned int)file_stat.st_mode : 0U,
                                  (stat_rc == 0) ? (int)file_stat.st_uid : -1,
                                  (stat_rc == 0) ? (int)file_stat.st_gid : -1,
                                  (int)g_watch_running, g_inotify_fd, wd,
                                  val_rc);
                AGENTOS_LOG_WARN("C-L01: ConfigLoader: WATCH-RELOAD-FAIL — "
                                 "validation failed (rc=%d), keeping current config. "
                                 "DIAGNOSIS: Common causes — missing required fields, "
                                 "invalid values, schema mismatch. "
                                 "Check agentos.yaml schema at %s",
                                 val_rc, g_config_path);
                continue;
            }

            agentos_yaml_env_override(&new_config);

            agentos_yaml_config_t old_config = g_global_config;
            g_global_config = new_config;

            config_reload_notify(&old_config, &new_config);

            AGENTOS_LOG_INFO("C-L01: ConfigLoader: WATCH-RELOAD — config reloaded "
                             "successfully from %s version=%s",
                             g_config_path,
                             new_config.version[0] ? new_config.version : "unknown");
            }
        }

    /* 清理 */
    if (wd >= 0) {
        inotify_rm_watch(g_inotify_fd, wd);
    }
    close(g_inotify_fd);
    g_inotify_fd = -1;

    AGENTOS_LOG_INFO("C-L01: ConfigLoader: WATCH-STOPPED path=%s", g_config_path);
    return NULL;
}

int agentos_config_watch_start(const char *yaml_path, uint32_t interval_ms)
{
    if (g_watch_running) {
        AGENTOS_LOG_WARN("C-L01: ConfigLoader: WATCH-START — already running");
        return -1;
    }

    const char *path = yaml_path ? yaml_path : AGENTOS_DEFAULT_CONFIG_PATH;
    safe_strcpy(g_config_path, sizeof(g_config_path), path);

    g_watch_running = true;

    /* P3.15: 使用 inotify 替代轮询，interval_ms 参数保留用于兼容性 */
    AGENTOS_LOG_INFO("C-L01: ConfigLoader: WATCH-START path=%s interval_ms=%u "
                     "(P3.15: inotify-based, interval_ms ignored)",
                     g_config_path, interval_ms);

    if (pthread_create(&g_watch_thread, NULL, config_watch_thread, NULL) != 0) {
        AGENTOS_LOG_ERROR("C-L01: ConfigLoader: WATCH-FAIL — pthread_create failed");
        g_watch_running = false;
        return -1;
    }

    return 0;
}

void agentos_config_watch_stop(void)
{
    if (!g_watch_running) {
        AGENTOS_LOG_DEBUG("C-L01: ConfigLoader: WATCH-STOP — not running");
        return;
    }

    AGENTOS_LOG_INFO("C-L01: ConfigLoader: WATCH-STOP — stopping watch thread");

    g_watch_running = false;

    /* P3.15: 关闭 inotify fd 以唤醒阻塞的 read() */
    if (g_inotify_fd >= 0) {
        close(g_inotify_fd);
        g_inotify_fd = -1;
    }

    /* P3.15: 等待监听线程退出 */
    int join_rc = pthread_join(g_watch_thread, NULL);
    if (join_rc != 0) {
        AGENTOS_LOG_WARN("C-L01: ConfigLoader: WATCH-STOP — pthread_join failed (errno=%d)",
                         join_rc);
    } else {
        AGENTOS_LOG_INFO("C-L01: ConfigLoader: WATCH-STOPPED — thread joined successfully");
    }
}

int agentos_config_reload(const char *yaml_path)
{
    const char *path = yaml_path ? yaml_path : g_config_path;

    AGENTOS_LOG_INFO("C-L01: ConfigLoader: RELOAD-TRIGGERED path=%s g_config_loaded=%d",
                     path, (int)g_config_loaded);

    agentos_yaml_config_t new_config;
    memset(&new_config, 0, sizeof(new_config));

    int load_rc = agentos_yaml_load(path, &new_config);
    if (load_rc != 0) {
        struct stat file_stat;
        int stat_rc = stat(path, &file_stat);
        const char *stat_err = (stat_rc != 0) ? strerror(errno) : "OK";

        AGENTOS_LOG_ERROR("C-L01: ConfigLoader: RELOAD-FAIL — YAML load failed "
                          "path=%s rc=%d errno=%d (%s) "
                          "file_stat=(rc=%d, err=%s, size=%ld, mode=%o, uid=%d, gid=%d) "
                          "STACK: agentos_yaml_load() → agentos_config_reload() → caller "
                          "TRACEBACK: [1] agentos_config_reload(%s) called "
                          "[2] agentos_yaml_load() returned rc=%d "
                          "[3] ERROR: keeping current config",
                          path, load_rc, errno, strerror(errno),
                          stat_rc, stat_err,
                          (stat_rc == 0) ? (long)file_stat.st_size : -1L,
                          (stat_rc == 0) ? (unsigned int)file_stat.st_mode : 0U,
                          (stat_rc == 0) ? (int)file_stat.st_uid : -1,
                          (stat_rc == 0) ? (int)file_stat.st_gid : -1,
                          path, load_rc);
        return -1;
    }

    int val_rc = agentos_yaml_validate(&new_config);
    if (val_rc != 0) {
        struct stat file_stat;
        int stat_rc = stat(path, &file_stat);
        const char *stat_err = (stat_rc != 0) ? strerror(errno) : "OK";

        AGENTOS_LOG_ERROR("C-L01: ConfigLoader: RELOAD-FAIL — validation failed "
                          "path=%s val_rc=%d errno=%d (%s) "
                          "file_stat=(rc=%d, err=%s, size=%ld, mode=%o, uid=%d, gid=%d) "
                          "STACK: agentos_yaml_validate() → agentos_config_reload() → caller "
                          "TRACEBACK: [1] agentos_config_reload(%s) called "
                          "[2] agentos_yaml_load() succeeded "
                          "[3] agentos_yaml_validate() returned rc=%d "
                          "[4] ERROR: keeping current config",
                          path, val_rc, errno, strerror(errno),
                          stat_rc, stat_err,
                          (stat_rc == 0) ? (long)file_stat.st_size : -1L,
                          (stat_rc == 0) ? (unsigned int)file_stat.st_mode : 0U,
                          (stat_rc == 0) ? (int)file_stat.st_uid : -1,
                          (stat_rc == 0) ? (int)file_stat.st_gid : -1,
                          path, val_rc);
        return -1;
    }

    /* P1.1.3: 应用环境变量覆盖（C-L01） */
    agentos_yaml_env_override(&new_config);

    agentos_yaml_config_t old_config = g_global_config;
    g_global_config = new_config;
    g_config_loaded = true;

    config_reload_notify(&old_config, &new_config);
    AGENTOS_LOG_INFO("C-L01: ConfigLoader: RELOAD-OK path=%s version=%s",
                     path, new_config.version[0] ? new_config.version : "unknown");
    return 0;
}

/* ── 模块初始化 ── */

/**
 * @brief CoreLoopThree 配置模块初始化（在 agentos_init() 之后调用）
 * @return 0 成功，非0失败
 */
int agentos_config_init(const char *yaml_path)
{
    const char *path = yaml_path ? yaml_path : AGENTOS_DEFAULT_CONFIG_PATH;

    AGENTOS_LOG_INFO("ConfigLoader: init START (path=%s)", path);

    agentos_yaml_config_defaults(&g_global_config);

    if (agentos_yaml_load(path, &g_global_config) == 0) {
        g_config_loaded = true;
        AGENTOS_LOG_INFO("ConfigLoader: init OK (config loaded from %s)", path);
    } else {
        /* 文件不存在时使用默认配置 */
        g_config_loaded = true;
        AGENTOS_LOG_INFO("ConfigLoader: init OK (using defaults, %s not found)", path);
    }

    /* P1.1.3: 应用环境变量覆盖（C-L01） */
    agentos_yaml_env_override(&g_global_config);

    /* 平台路径映射（P1.12.4） */
    agentos_yaml_resolve_platform_paths(&g_global_config);

    return 0;
}
