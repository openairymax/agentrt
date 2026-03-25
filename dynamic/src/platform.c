/**
 * @file platform.c
 * @brief 跨平台兼容层实现
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "platform.h"

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

/* ========== 时间工具函数 ========== */

/**
 * @brief 获取当前时间（纳秒）
 * @return 当前时间纳秒数
 */
uint64_t time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000000ULL / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

#endif /* _WIN32 */
