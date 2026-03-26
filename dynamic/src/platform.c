/**
 * @file platform.c
 * @brief 跨平台兼容层实现
 *
 * 本文件提供 dynamic 模块特定的平台实现：
 * - Windows getopt_long 实现
 * - time_ns() 包装函数（映射到 common 模块的 agentos_time_ns）
 *
 * 其他平台功能由 common/platform/platform.c 提供
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "platform.h"

#include <time.h>
#include <errno.h>

#ifdef _WIN32

/* Windows getopt_long 实现 */

char* optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

static int sp = 1;

int getopt_long(int argc, char* const argv[], const char* optstring,
    const struct option* longopts, int* longindex) {

    char* p;

    if (sp == 1) {
        if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
            return -1;
        }

        if (strcmp(argv[optind], "--") == 0) {
            optind++;
            return -1;
        }

        /* 检查长选项 */
        if (argv[optind][1] == '-') {
            char* name = argv[optind] + 2;
            char* eq = strchr(name, '=');
            size_t name_len = eq ? (size_t)(eq - name) : strlen(name);

            const struct option* opt;
            for (opt = longopts; opt->name; opt++) {
                if (strncmp(name, opt->name, name_len) == 0 &&
                    opt->name[name_len] == '\0') {

                    if (longindex) *longindex = (int)(opt - longopts);

                    if (opt->has_arg == required_argument) {
                        if (eq) {
                            optarg = eq + 1;
                        } else if (++optind < argc) {
                            optarg = argv[optind];
                        } else {
                            if (opterr) {
                                fprintf(stderr, "%s: option '--%s' requires an argument\n",
                                    argv[0], opt->name);
                            }
                            return '?';
                        }
                    } else if (opt->has_arg == optional_argument) {
                        optarg = eq ? eq + 1 : NULL;
                    } else {
                        optarg = NULL;
                    }

                    optind++;
                    if (opt->flag) {
                        *opt->flag = opt->val;
                        return 0;
                    }
                    return opt->val;
                }
            }

            if (opterr) {
                fprintf(stderr, "%s: unrecognized option '--%.*s'\n",
                    argv[0], (int)name_len, name);
            }
            optind++;
            return '?';
        }
    }

    /* 短选项 */
    char c = argv[optind][sp];
    p = strchr(optstring, c);

    if (p == NULL || c == ':') {
        if (opterr) {
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], c);
        }
        optopt = c;
        if (argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return '?';
    }

    if (p[1] == ':') {
        if (argv[optind][sp + 1] != '\0') {
            optarg = argv[optind] + sp + 1;
            optind++;
            sp = 1;
        } else if (++optind >= argc) {
            if (opterr) {
                fprintf(stderr, "%s: option requires an argument -- '%c'\n",
                    argv[0], c);
            }
            optopt = c;
            sp = 1;
            return '?';
        } else {
            optarg = argv[optind++];
            sp = 1;
        }
    } else {
        if (argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        optarg = NULL;
    }

    return c;
}

#endif /* _WIN32 */

/* ========== time_ns 包装函数 ========== */

/**
 * @brief 获取当前时间（纳秒）- 包装函数
 *
 * 此函数映射到 common 模块的 agentos_time_ns()
 * 使用 CLOCK_MONOTONIC 保证单调性
 *
 * @return 当前时间纳秒数
 */
uint64_t time_ns(void) {
    return agentos_time_ns();
}

/* ========== 时间函数扩展（dynamic 特定） ========== */

/**
 * @brief 获取当前时间（纳秒）- 墙钟时间
 *
 * 使用 CLOCK_REALTIME，返回自 Unix 纪元以来的时间
 *
 * @return 当前时间纳秒数
 */
uint64_t agentos_time_current_ns(void) {
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t ns = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    ns -= 11644473600000000000ULL;
    return ns * 100;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

/**
 * @brief 纳秒级睡眠
 *
 * @param ns 睡眠时间（纳秒）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t agentos_time_nanosleep(uint64_t ns) {
#if defined(_WIN32)
    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (!timer) {
        return AGENTOS_ERROR;
    }
    LARGE_INTEGER li;
    li.QuadPart = -(LONGLONG)(ns / 100);
    SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    return AGENTOS_SUCCESS;
#else
    struct timespec req = {
        .tv_sec = ns / 1000000000ULL,
        .tv_nsec = ns % 1000000000ULL
    };
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
    }
    return AGENTOS_SUCCESS;
#endif
}
