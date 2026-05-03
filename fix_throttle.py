import sys

new_code = r"""
/* ==================== 日志节流（Throttling）内部数据结构 ==================== */

/** 节流哈希桶数量 */
#define THROTTLE_BUCKET_COUNT 256

/** 节流哈希桶 */
typedef struct {
    uint64_t hash_key;
    uint64_t last_second;
    uint32_t count;
} throttle_bucket_t;

static throttle_bucket_t g_throttle_buckets[THROTTLE_BUCKET_COUNT];
static volatile uint32_t g_throttle_enabled = 0;
static volatile uint32_t g_throttle_max_per_sec = 100;
static agentos_mutex_t g_throttle_mutex;
static bool g_throttle_mutex_init = false;

/* ==================== 日志采样（Sampling）内部数据结构 ==================== */

static volatile uint32_t g_sample_counter_debug = 0;
static volatile uint32_t g_sample_counter_info = 0;
static volatile uint32_t g_sample_counter_warn = 0;

/**
 * @brief 计算消息哈希（用于节流去重）
 */
static uint64_t throttle_hash(const char* module, int line, const char* message)
{
    uint64_t h = 14695981039346656037ULL;
    const char* p;

    if (module) {
        for (p = module; *p; p++) {
            h ^= (uint64_t)(unsigned char)*p;
            h *= 1099511628211ULL;
        }
    }

    h ^= (uint64_t)line;
    h *= 1099511628211ULL;

    if (message) {
        for (p = message; *p; p++) {
            h ^= (uint64_t)(unsigned char)*p;
            h *= 1099511628211ULL;
        }
    }

    return h;
}

/**
 * @brief 检查日志是否应被节流抑制
 *
 * @param module 模块名
 * @param line 行号
 * @param message 日志消息
 * @param now_sec 当前时间（秒）
 * @return true 应抑制（跳过），false 应输出
 */
static bool throttle_should_suppress(const char* module, int line,
                                     const char* message, uint64_t now_sec)
{
    if (!g_throttle_enabled) return false;
    if (!g_throttle_mutex_init) return false;

    uint64_t h = throttle_hash(module, line, message);
    uint32_t bucket_idx = (uint32_t)(h % THROTTLE_BUCKET_COUNT);

    agentos_mutex_lock(&g_throttle_mutex);

    throttle_bucket_t* bucket = &g_throttle_buckets[bucket_idx];

    if (bucket->last_second != now_sec) {
        if (bucket->last_second > 0 && bucket->count > g_throttle_max_per_sec) {
            agentos_mutex_unlock(&g_throttle_mutex);
            return false;
        }
        bucket->last_second = now_sec;
        bucket->hash_key = h;
        bucket->count = 1;
        agentos_mutex_unlock(&g_throttle_mutex);
        return false;
    }

    if (bucket->hash_key == h) {
        if (bucket->count >= g_throttle_max_per_sec) {
            bucket->count++;
            uint32_t suppressed = bucket->count - g_throttle_max_per_sec;
            agentos_mutex_unlock(&g_throttle_mutex);

            if (suppressed == 1) {
                fprintf(stderr, "[THROTTLE] Suppressing further identical messages: %s:%d\n",
                        module ? module : "?", line);
            }
            return true;
        }
        bucket->count++;
        agentos_mutex_unlock(&g_throttle_mutex);
        return false;
    }

    if (bucket->count > g_throttle_max_per_sec) {
        uint32_t old_suppressed = bucket->count - g_throttle_max_per_sec;
        if (old_suppressed > 0) {
            agentos_mutex_unlock(&g_throttle_mutex);
            fprintf(stderr, "[THROTTLE] Previous bucket flushed: %u messages suppressed\n",
                    old_suppressed);
            agentos_mutex_lock(&g_throttle_mutex);
        }
    }
    bucket->hash_key = h;
    bucket->count = 1;
    agentos_mutex_unlock(&g_throttle_mutex);
    return false;
}

/** 日志系统全局状态实?*/
static logging_state_t g_logging_state = {
    .initialized = false,
    .module_level_count = 0
};
"""

content = open(sys.argv[1], "r", encoding="utf-8", errors="replace").read()

# find the marker AFTER which we insert
marker = "static const size_t MAX_MESSAGE_LEN = 4096;"
old_marker = "/** 日志系统全局状态实?*/\nstatic logging_state_t g_logging_state = {\n    .initialized = false,\n    .module_level_count = 0\n};"

idx = content.find(marker)
if idx == -1:
    print("ERROR: MAX_MESSAGE_LEN not found")
    sys.exit(1)

new_content = content[:idx + len(marker)] + "\n" + new_code + "\n" + content[idx + len(marker):]

# remove the old g_logging_state block since we included it in new_code
# find it after the insertion point
old_idx = new_content.find(old_marker, idx + len(marker) + len(new_code) + 1)
if old_idx != -1:
    new_content = new_content[:old_idx] + new_content[old_idx + len(old_marker):]

open(sys.argv[1], "w", encoding="utf-8", errors="replace").write(new_content)
print("DONE")
