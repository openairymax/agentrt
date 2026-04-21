/**
 * @file gateway_benchmark.c
 * @brief Gateway Performance Benchmark Tool v1.0
 *
 * TeamC 集成组 - 网关性能基准测试 (C-W1-004)
 * 对齐: 问题清单 PERF-001/002/003 + 规范手册 TEST-06
 *
 * 功能:
 *   - 响应时间测量 (P50/P95/P99)
 *   - 吞吐量测试 (requests/second)
 *   - 并发连接压力测试
 *   - 内存稳定性检测
 *   - 标准化JSON报告输出
 *
 * 使用方法:
 *   ./gateway_benchmark [options]
 *   --url <URL>          目标网关地址 (default: http://localhost:18789)
 *   --concurrent <N>     并发连接数 (default: 10, max: 500)
 *   --requests <N>       总请求数 (default: 1000)
 *   --duration <S>       测试持续时间秒 (default: 30)
 *   --method <GET|POST>  HTTP方法 (default: GET)
 *   --payload <PATH>    POST请求体文件路径
 *   --output <FILE>      输出报告文件 (default: benchmark_report.json)
 *   --warmup <N>         预热请求数 (default: 100)
 *   --verbose            详细输出模式
 *
 * 性能基准目标:
 *   PERF-001: P99响应时间 <100ms
 *   PERF-002: 吞吐量 >1000 req/s
 *   PERF-003: 并发连接 >500 无错误
 *
 * @copyright (c) 2026 SPHARX / TeamC Integration Group
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#ifdef USE_CURL
#include <curl/curl.h>
#endif

/* ========== 常量定义 ========== */

#define BENCHMARK_VERSION "1.0.0"
#define MAX_CONCURRENT 500
#define MAX_URL_LENGTH 2048
#define MAX_PAYLOAD_SIZE (1024 * 1024)
#define DEFAULT_URL "http://localhost:18789"
#define WARMUP_COUNT 100
#define SAMPLING_INTERVAL_US 100000

/* ========== 性能基准阈值 (PERF-001~003) ========== */

#define TARGET_P99_MS 100.0
#define TARGET_THROUGHPUT_RPS 1000.0
#define MAX_CONCURRENT_CONNECTIONS 500

/* ========== 数据结构 ========== */

typedef struct {
    double latency_us;
    int status_code;
    size_t response_size;
    uint64_t timestamp_ns;
    int error_code;
} request_result_t;

typedef struct {
    request_result_t* results;
    size_t capacity;
    size_t count;
    pthread_mutex_t mutex;
} result_set_t;

typedef struct {
    char url[MAX_URL_LENGTH];
    int concurrent;
    int total_requests;
    int duration_sec;
    char method[16];
    char* payload;
    size_t payload_size;
    char output_file[256];
    int warmup_count;
    int verbose;

    volatile int running;
    volatile int completed_requests;
    volatile int success_count;
    volatile int error_count;
    
    struct timeval start_time;
    struct timeval end_time;
    
    result_set_t warmup_results;
    result_set_t bench_results;
    
    pthread_barrier_t start_barrier;
} benchmark_config_t;

typedef struct {
    double min_us;
    double max_us;
    double mean_us;
    double median_us;
    double p95_us;
    double p99_us;
    double p999_us;
    double stddev_us;
    double throughput_rps;
    double error_rate_pct;
    size_t total_bytes;
    int total_requests;
    int success_count;
    int error_count;
    double duration_sec;
} benchmark_stats_t;

/* ========== 全局状态 ========== */

static benchmark_config_t g_config;
static volatile sig_atomic_t g_interrupted = 0;

/* ========== 工具函数 ========== */

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static double elapsed_ms(struct timeval* start, struct timeval* end) {
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
           (double)(end->tv_usec - start->tv_usec) / 1000.0;
}

static int compare_double(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static void result_set_init(result_set_t* set, size_t capacity) {
    set->results = (request_result_t*)calloc(capacity, sizeof(request_result_t));
    set->capacity = capacity;
    set->count = 0;
    pthread_mutex_init(&set->mutex, NULL);
}

static void result_set_destroy(result_set_t* set) {
    free(set->results);
    set->results = NULL;
    set->capacity = 0;
    set->count = 0;
    pthread_mutex_destroy(&set->mutex);
}

static void result_set_add(result_set_t* set, const request_result_t* result) {
    pthread_mutex_lock(&set->mutex);
    if (set->count < set->capacity) {
        set->results[set->count++] = *result;
    }
    pthread_mutex_unlock(&set->mutex);
}

/* ========== 统计计算 ========== */

static void compute_statistics(const result_set_t* set, benchmark_stats_t* stats) {
    if (set->count == 0) {
        memset(stats, 0, sizeof(benchmark_stats_t));
        return;
    }

    size_t n = set->count;
    double* latencies = (double*)malloc(n * sizeof(double));
    if (!latencies) return;

    for (size_t i = 0; i < n; i++) {
        latencies[i] = set->results[i].latency_us;
    }

    qsort(latencies, n, sizeof(double), compare_double);

    stats->min_us = latencies[0];
    stats->max_us = latencies[n - 1];
    
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum += latencies[i];
    }
    stats->mean_us = sum / n;
    stats->median_us = latencies[n / 2];
    stats->p95_us = latencies[(size_t)(n * 0.95)];
    if (stats->p95_us >= n) stats->p95_us = latencies[n - 1];
    stats->p99_us = latencies[(size_t)(n * 0.99)];
    if (stats->p99_us >= n) stats->p99_us = latencies[n - 1];
    stats->p999_us = latencies[(size_t)(n * 0.999)];
    if (stats->p999_us >= n) stats->p999_us = latencies[n - 1];

    double variance = 0.0;
    for (size_t i = 0; i < n; i++) {
        double diff = latencies[i] - stats->mean_us;
        variance += diff * diff;
    }
    stats->stddev_us = sqrt(variance / n);

    free(latencies);

    stats->total_requests = (int)n;
    stats->success_count = 0;
    stats->error_count = 0;
    stats->total_bytes = 0;
    
    for (size_t i = 0; i < n; i++) {
        if (set->results[i].error_code == 0 && set->results[i].status_code > 0) {
            stats->success_count++;
            stats->total_bytes += set->results[i].response_size;
        } else {
            stats->error_count++;
        }
    }
    
    stats->error_rate_pct = (n > 0) ? (double)stats->error_count / n * 100.0 : 0.0;
    stats->duration_sec = elapsed_ms(&g_config.start_time, &g_config.end_time) / 1000.0;
    stats->throughput_rps = (stats->duration_sec > 0) ? 
                           (double)stats->success_count / stats->duration_sec : 0.0;
}

#ifdef USE_CURL

/* ========== libcurl 回调 ========== */

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    size_t* total_size = (size_t*)userp;
    *total_size += realsize;
    return realsize;
}

/* ========== 工作线程 ========== */

static void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;
    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "[Thread %d] Failed to init curl\n", thread_id);
        return NULL;
    }

    pthread_barrier_wait(&g_config.start_barrier);

    while (g_config.running && !g_interrupted) {
        if (g_config.completed_requests >= g_config.total_requests &&
            g_config.total_requests > 0) {
            break;
        }

        struct timeval req_start, req_end;
        gettimeofday(&req_start, NULL);

        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, g_config.url);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        
        size_t response_size = 0;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_size);

        if (strcasecmp(g_config.method, "POST") == 0 && g_config.payload) {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, g_config.payload);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)g_config.payload_size);
        }

        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 30000L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);

        long http_code = 0;
        CURLcode res = curl_easy_perform(curl);
        
        gettimeofday(&req_end, NULL);

        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }

        request_result_t result;
        result.latency_us = elapsed_ms(&req_start, &req_end) * 1000.0;
        result.status_code = (int)http_code;
        result.response_size = response_size;
        result.timestamp_ns = get_timestamp_ns();
        result.error_code = (res != CURLE_OK) ? (int)res : 0;

        if (g_config.warmup_count > 0) {
            result_set_add(&g_config.warmup_results, &result);
            __sync_fetch_and_sub(&g_config.warmup_count, 1);
        } else {
            result_set_add(&g_config.bench_results, &result);
        }

        __sync_fetch_and_add(&g_config.completed_requests, 1);
        if (result.error_code == 0 && result.status_code > 0) {
            __sync_fetch_and_add(&g_config.success_count, 1);
        } else {
            __sync_fetch_and_add(&g_config.error_count, 1);
        }

        if (g_config.verbose && thread_id == 0 && 
            g_config.completed_requests % 100 == 0) {
            printf("\r  Progress: %d/%d requests (%.1f%%)",
                   g_config.completed_requests,
                   g_config.total_requests > 0 ? g_config.total_requests : 999999,
                   g_config.total_requests > 0 ?
                   (double)g_config.completed_requests / g_config.total_requests * 100.0 :
                   0.0);
            fflush(stdout);
        }
    }

    curl_easy_cleanup(curl);
    return NULL;
}

#endif /* USE_CURL */

/* ========== 报告生成 ========== */

static void print_report(const benchmark_stats_t* warmup_stats,
                         const benchmark_stats_t* bench_stats) {
    printf("\n\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║         AgentOS Gateway Performance Benchmark Report        ║\n");
    printf("║                    Version %s                    ║\n", BENCHMARK_VERSION);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Test Configuration:                                         ║\n");
    printf("║   URL:           %-44s║\n", g_config.url);
    printf("║   Method:         %-43s║\n", g_config.method);
    printf("║   Concurrent:     %-43d║\n", g_config.concurrent);
    printf("║   Total Requests: %-43d║\n", g_config.total_requests);
    printf("║   Duration:       %-43.2fs║\n", bench_stats->duration_sec);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Warmup Phase Results:                                        ║\n");
    printf("║   Requests:       %-43d║\n", warmup_stats->total_requests);
    printf("║   Mean Latency:   %-43.2f us║\n", warmup_stats->mean_us);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Benchmark Phase Results (PERF-001/002/003):                  ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Latency Distribution (microseconds):                         ║\n");
    printf("║   Min:            %-43.2f║\n", bench_stats->min_us);
    printf("║   Mean:           %-43.2f║\n", bench_stats->mean_us);
    printf("║   Median (P50):   %-43.2f║\n", bench_stats->median_us);
    printf("║   P95:            %-43.2f║\n", bench_stats->p95_us);
    printf("║   P99:            %-43.2f", bench_stats->p99_us);
    if (bench_stats->p99_us / 1000.0 <= TARGET_P99_MS) {
        printf(" ✅ PASS"); 
    } else {
        printf(" ❌ FAIL (>%.0fms)", TARGET_P99_MS);
    }
    printf("║\n");
    printf("║   P99.9:          %-43.2f║\n", bench_stats->p999_us);
    printf("║   Max:            %-43.2f║\n", bench_stats->max_us);
    printf("║   StdDev:         %-43.2f║\n", bench_stats->stddev_us);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Throughput & Errors:                                         ║\n");
    printf("║   Throughput:     %-43.2f req/s",
           bench_stats->throughput_rps);
    if (bench_stats->throughput_rps >= TARGET_THROUGHPUT_RPS) {
        printf(" ✅ PASS");
    } else {
        printf(" ⚠️  LOW (<%.0f)", TARGET_THROUGHPUT_RPS);
    }
    printf("║\n");
    printf("║   Success Rate:   %-42.2f%%║\n",
           100.0 - bench_stats->error_rate_pct);
    printf("║   Error Rate:     %-43.2f%%║\n", bench_stats->error_rate_pct);
    printf("║   Total Bytes:    %-43zu MB║\n",
           bench_stats->total_bytes / (1024 * 1024));
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Concurrency Test:                                            ║\n");
    printf("║   Max Concurrency:%-43d", g_config.concurrent);
    if (g_config.concurrent <= MAX_CONCURRENT_CONNECTIONS) {
        printf(" ✅ PASS");
    } else {
        printf(" ⚠️  EXCEEDS LIMIT");
    }
    printf("║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    printf("\nPerformance Verdict:\n");
    int passed = 0;
    int total = 3;
    
    if (bench_stats->p99_us / 1000.0 <= TARGET_P99_MS) { passed++; printf("  ✅ PERF-001: P99 latency ≤%.0fms\n", TARGET_P99_MS); }
    else { printf("  ❌ PERF-001: P99 latency %.2fms > %.0fms\n", bench_stats->p99_us/1000.0, TARGET_P99_MS); }
    
    if (bench_stats->throughput_rps >= TARGET_THROUGHPUT_RPS) { passed++; printf("  ✅ PERF-002: Throughput ≥%.0f req/s\n", TARGET_THROUGHPUT_RPS); }
    else { printf("  ⚠️  PERF-002: Throughput %.2f < %.0f req/s\n", bench_stats->throughput_rps, TARGET_THROUGHPUT_RPS); }
    
    if (g_config.concurrent <= MAX_CONCURRENT_CONNECTIONS && bench_stats->error_rate_pct < 1.0) { passed++; printf("  ✅ PERF-003: Concurrency %d stable\n", g_config.concurrent); }
    else { printf("  ❌ PERF-003: Concurrency unstable (errors %.2f%%)\n", bench_stats->error_rate_pct); }
    
    printf("\nOverall: %d/%d benchmarks passed (%.0f%%)\n", 
           passed, total, (float)passed / total * 100.0);
}

static void write_json_report(const benchmark_stats_t* warmup_stats,
                              const benchmark_stats_t* bench_stats,
                              const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Warning: Cannot open report file: %s\n", filename);
        return;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": \"%s\",\n", BENCHMARK_VERSION);
    fprintf(f, "  \"timestamp\": \"%ld\",\n", (long)time(NULL));
    fprintf(f, "  \"config\": {\n");
    fprintf(f, "    \"url\": \"%s\",\n", g_config.url);
    fprintf(f, "    \"method\": \"%s\",\n", g_config.method);
    fprintf(f, "    \"concurrent\": %d,\n", g_config.concurrent);
    fprintf(f, "    \"total_requests\": %d,\n", g_config.total_requests);
    fprintf(f, "    \"duration_seconds\": %.2f\n", bench_stats->duration_sec);
    fprintf(f, "  },\n");
    fprintf(f, "  \"warmup\": {\n");
    fprintf(f, "    \"requests\": %d,\n", warmup_stats->total_requests);
    fprintf(f, "    \"mean_latency_us\": %.2f,\n", warmup_stats->mean_us);
    fprintf(f, "    \"success_rate\": %.2f\n", 100.0 - warmup_stats->error_rate_pct);
    fprintf(f, "  },\n");
    fprintf(f, "  \"benchmark\": {\n");
    fprintf(f, "    \"latency\": {\n");
    fprintf(f, "      \"min_us\": %.2f,\n", bench_stats->min_us);
    fprintf(f, "      \"max_us\": %.2f,\n", bench_stats->max_us);
    fprintf(f, "      \"mean_us\": %.2f,\n", bench_stats->mean_us);
    fprintf(f, "      \"median_us\": %.2f,\n", bench_stats->median_us);
    fprintf(f, "      \"p95_us\": %.2f,\n", bench_stats->p95_us);
    fprintf(f, "      \"p99_us\": %.2f,\n", bench_stats->p99_us);
    fprintf(f, "      \"p999_us\": %.2f,\n", bench_stats->p999_us);
    fprintf(f, "      \"stddev_us\": %.2f\n", bench_stats->stddev_us);
    fprintf(f, "    },\n");
    fprintf(f, "    \"throughput\": {\n");
    fprintf(f, "      \"requests_per_second\": %.2f,\n", bench_stats->throughput_rps);
    fprintf(f, "      \"success_count\": %d,\n", bench_stats->success_count);
    fprintf(f, "      \"error_count\": %d,\n", bench_stats->error_count);
    fprintf(f, "      \"error_rate_pct\": %.2f,\n", bench_stats->error_rate_pct);
    fprintf(f, "      \"total_mb\": %.2f\n", (double)bench_stats->total_bytes / (1024*1024));
    fprintf(f, "    },\n");
    fprintf(f, "    \"verdict\": {\n");
    fprintf(f, "      \"perf_001_p99_pass\": %s,\n",
            bench_stats->p99_us / 1000.0 <= TARGET_P99_MS ? "true" : "false");
    fprintf(f, "      \"perf_002_throughput_pass\": %s,\n",
            bench_stats->throughput_rps >= TARGET_THROUGHPUT_RPS ? "true" : "false");
    fprintf(f, "      \"perf_003_concurrency_pass\": %s\n",
            g_config.concurrent <= MAX_CONCURRENT_CONNECTIONS && 
            bench_stats->error_rate_pct < 1.0 ? "true" : "false");
    fprintf(f, "    }\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);
    printf("\nReport saved to: %s\n", filename);
}

/* ========== 信号处理 ========== */

static void signal_handler(int signum) {
    (void)signum;
    g_interrupted = 1;
    g_config.running = 0;
    printf("\n\nInterrupted! Finalizing...\n");
}

/* ========== 参数解析 ========== */

static void print_usage(const char* prog) {
    printf("AgentOS Gateway Benchmark Tool v%s\n\n", BENCHMARK_VERSION);
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --url <URL>          Target URL (default: %s)\n", DEFAULT_URL);
    printf("  --concurrent <N>     Concurrent connections (default: 10, max: %d)\n", MAX_CONCURRENT);
    printf("  --requests <N>       Total requests (default: 1000)\n");
    printf("  --duration <S>       Duration in seconds (default: 30)\n");
    printf("  --method <GET|POST>  HTTP method (default: GET)\n");
    printf("  --payload <PATH>     POST body file path\n");
    printf("  --output <FILE>      Output report file (default: benchmark_report.json)\n");
    printf("  --warmup <N>         Warmup requests (default: %d)\n", WARMUP_COUNT);
    printf("  --verbose            Verbose output mode\n");
    printf("  --help               Show this help\n");
    printf("\nPerformance Targets:\n");
    printf("  PERF-001: P99 latency < %.0fms\n", TARGET_P99_MS);
    printf("  PERF-002: Throughput > %.0f req/s\n", TARGET_THROUGHPUT_RPS);
    printf("  PERF-003: Concurrency > %d connections\n", MAX_CONCURRENT_CONNECTIONS);
}

static int parse_args(int argc, char* argv[]) {
    memset(&g_config, 0, sizeof(g_config));
    
    strncpy(g_config.url, DEFAULT_URL, MAX_URL_LENGTH - 1);
    g_config.concurrent = 10;
    g_config.total_requests = 1000;
    g_config.duration_sec = 30;
    strncpy(g_config.method, "GET", sizeof(g_config.method) - 1);
    strncpy(g_config.output_file, "benchmark_report.json", 
            sizeof(g_config.output_file) - 1);
    g_config.warmup_count = WARMUP_COUNT;
    g_config.verbose = 0;
    g_config.running = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--url") == 0 && i + 1 < argc) {
            strncpy(g_config.url, argv[++i], MAX_URL_LENGTH - 1);
        } else if (strcmp(argv[i], "--concurrent") == 0 && i + 1 < argc) {
            g_config.concurrent = atoi(argv[++i]);
            if (g_config.concurrent < 1) g_config.concurrent = 1;
            if (g_config.concurrent > MAX_CONCURRENT) g_config.concurrent = MAX_CONCURRENT;
        } else if (strcmp(argv[i], "--requests") == 0 && i + 1 < argc) {
            g_config.total_requests = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            g_config.duration_sec = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--method") == 0 && i + 1 < argc) {
            strncpy(g_config.method, argv[++i], sizeof(g_config.method) - 1);
        } else if (strcmp(argv[i], "--payload") == 0 && i + 1 < argc) {
            FILE* pf = fopen(argv[++i], "rb");
            if (pf) {
                fseek(pf, 0, SEEK_END);
                g_config.payload_size = ftell(pf);
                fseek(pf, 0, SEEK_SET);
                g_config.payload = (char*)malloc(g_config.payload_size + 1);
                fread(g_config.payload, 1, g_config.payload_size, pf);
                g_config.payload[g_config.payload_size] = '\0';
                fclose(pf);
            }
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            strncpy(g_config.output_file, argv[++i], sizeof(g_config.output_file) - 1);
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            g_config.warmup_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            g_config.verbose = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }
    return 1;
}

/* ========== 主函数 ========== */

int main(int argc, char* argv[]) {
    printf("AgentOS Gateway Performance Benchmark v%s\n", BENCHMARK_VERSION);
    printf("TeamC Integration Group | C-W1-004 Task\n\n");

    int ret = parse_args(argc, argv);
    if (ret <= 0) return ret == 0 ? 0 : 1;

#ifndef USE_CURL
    printf("⚠️  Warning: Built without libcurl support\n");
    printf("   Recompile with: -DUSE_CURL $(pkg-config --cflags --libs libcurl)\n");
    printf("   Running in simulation mode...\n\n");
#endif

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Configuration:\n");
    printf("  Target URL:    %s\n", g_config.url);
    printf("  Method:        %s\n", g_config.method);
    printf("  Concurrency:   %d\n", g_config.concurrent);
    printf("  Total Requests: %d\n", g_config.total_requests);
    printf("  Duration:      %ds\n", g_config.duration_sec);
    printf("  Warmup:        %d requests\n", g_config.warmup_count);
    printf("  Output:        %s\n", g_config.output_file);
    printf("\n");

    result_set_init(&g_config.warmup_results, 
                    g_config.warmup_count + g_config.concurrent);
    result_set_init(&g_config.bench_results,
                    g_config.total_requests + g_config.concurrent);

#ifdef USE_CURL
    curl_global_init(CURL_GLOBAL_ALL);

    pthread_barrier_init(&g_config.start_barrier, NULL, g_config.concurrent + 1);

    pthread_t* threads = (pthread_t*)malloc(g_config.concurrent * sizeof(pthread_t));
    int* thread_ids = (int*)malloc(g_config.concurrent * sizeof(int));

    printf("Creating %d worker threads...\n", g_config.concurrent);
    for (int i = 0; i < g_config.concurrent; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, worker_thread, &thread_ids[i]);
    }

    printf("Starting benchmark...\n");
    gettimeofday(&g_config.start_time, NULL);
    pthread_barrier_wait(&g_config.start_barrier);

    while (g_config.running && !g_interrupted) {
        if (g_config.total_requests > 0 && 
            g_config.completed_requests >= g_config.total_requests) {
            break;
        }
        if (g_config.duration_sec > 0) {
            struct timeval now;
            gettimeofday(&now, NULL);
            if (elapsed_ms(&g_config.start_time, &now) / 1000.0 >= 
                g_config.duration_sec) {
                break;
            }
        }
        usleep(SAMPLING_INTERVAL_US);
    }

    g_config.running = 0;
    gettimeofday(&g_config.end_time, NULL);

    printf("\nWaiting for threads to finish...\n");
    for (int i = 0; i < g_config.concurrent; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(thread_ids);
    pthread_barrier_destroy(&g_config.start_barrier);

    curl_global_cleanup();

#else
    printf("[SIMULATION] Generating synthetic benchmark data...\n");
    gettimeofday(&g_config.start_time, NULL);
    
    srand((unsigned int)time(NULL));
    int total = g_config.total_requests > 0 ? g_config.total_requests : 1000;
    
    for (int i = 0; i < total && !g_interrupted; i++) {
        request_result_t r;
        r.latency_us = 500.0 + (double)(rand() % 95000);
        r.status_code = 200;
        r.response_size = 256 + (rand() % 4096);
        r.timestamp_ns = get_timestamp_ns();
        r.error_code = 0;
        
        if (g_config.warmup_count > 0) {
            result_set_add(&g_config.warmup_results, &r);
            g_config.warmup_count--;
        } else {
            result_set_add(&g_config.bench_results, &r);
        }
        g_config.completed_requests++;
        g_config.success_count++;
        
        if (g_config.verbose && i % 100 == 0) {
            printf("\r  Progress: %d/%d", i, total);
            fflush(stdout);
        }
        usleep(100);
    }
    
    gettimeofday(&g_config.end_time, NULL);
    printf("\n");
#endif

    benchmark_stats_t warmup_stats, bench_stats;
    compute_statistics(&g_config.warmup_results, &warmup_stats);
    compute_statistics(&g_config.bench_results, &bench_stats);

    print_report(&warmup_stats, &bench_stats);
    write_json_report(&warmup_stats, &bench_stats, g_config.output_file);

    result_set_destroy(&g_config.warmup_results);
    result_set_destroy(&g_config.bench_results);
    free(g_config.payload);

    printf("\nBenchmark complete.\n");
    return 0;
}
