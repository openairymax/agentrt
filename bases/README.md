# AgentOS ��������??(bases Utils)

**�汾**: v1.0.0.6  
**����??*: 2026-03-25  
**���??*: Apache License 2.0

---

## ?? ģ�鶨λ

`bases/` �ṩ���� AgentOS ��Ŀ����??*ͨ�ù��ߺͻ�����ʩ**�����������򻮷�Ϊ�����ģ��??

- **platform/**: ƽ̨����㣨��ƽ̨֧�֣�
- **utils/**: ͨ�ù��߼���15 ������ģ�飩
  - **manager/**: ���ù�������ء���֤���ȸ���??
  - **config_unified/**: ͳһ���ýӿ�
  - **core/**: �����������ͺͻ�������
  - **cost/**: ���ܷ����ͺ�ʱͳ��
  - **error/**: ͳһ������
  - **io/**: IO ������װ
  - **logging/**: ��־ϵͳ���ṹ��������??
  - **memory/**: �ڴ�������
  - **observability/**: �ɹ۲��ԣ�ָ�ꡢ׷�٣�
  - **resource/**: ��Դ����
  - **security/**: ��ȫ���ߣ����ܡ���֤��
  - **string/**: �ַ�����??
  - **sync/**: ͬ��ԭ��
  - **token/**: Token �������??
  - **types/**: ͨ�����Ͷ���

**��Ҫ**: ��ģ���� C ����ʵ�֣������ϲ�ģ�飨atoms, daemon, cupolas �ȣ��������ڴ�??

---

## ?? Ŀ¼�ṹ

```
bases/
������ platform/               # ƽ̨����??
??  ������ os.h                # ����ϵͳ��??
??  ������ compiler.h          # ��������??
������ utils/                  # ͨ�ù��߼���15 ��ģ�飩
??  ������ manager/             # ���ù���
??  ??  ������ config_loader.h # YAML ���ü���
??  ??  ������ config_loader.c
??  ������ config_unified/     # ͳһ���ýӿ�
??  ������ core/               # ���Ĺ���
??  ??  ������ macros.h        # �궨??
??  ??  ������ utils.h         # ͨ�ú���
??  ������ cost/               # ���ܷ���
??  ??  ������ profiler.h      # ���ܷ���??
??  ??  ������ timer.h         # �߾��ȼ�ʱ��
??  ������ error/              # ������
??  ??  ������ error_code.h    # �����붨??
??  ??  ������ error_handler.h # ������??
??  ������ io/                 # IO ��װ
??  ??  ������ file_utils.h    # �ļ�����
??  ??  ������ socket_utils.h  # Socket ����
??  ������ logging/            # ��־ϵͳ
??  ??  ������ logger.h        # ��־����
??  ??  ������ log_backend.h   # ��˽ӿ�
??  ??  ������ log_format.h    # ��ʽ����
??  ������ memory/             # �ڴ����
??  ??  ������ mem_pool.h      # �ڴ�??
??  ??  ������ smart_ptr.h     # ����ָ��
??  ������ observability/      # �ɹ۲�??
??  ??  ������ metrics.h       # ָ��ɼ�
??  ??  ������ tracer.h        # �ֲ�ʽ׷??
??  ������ resource/           # ��Դ����
??  ??  ������ handle.h        # �������
??  ??  ������ pool.h          # ��Դ??
??  ������ security/           # ��ȫ����
??  ??  ������ crypto.h        # ���ܽ���
??  ??  ������ auth.h          # ��֤��Ȩ
??  ������ string/             # �ַ�����??
??  ??  ������ str_utils.h     # �ַ�����??
??  ??  ������ json_parser.h   # JSON ����
??  ������ sync/               # ͬ��ԭ��
??  ??  ������ mutex.h         # ����??
??  ??  ������ rwlock.h        # ��д??
??  ������ token/              # Token ����
??  ??  ������ token_bucket.h  # ����Ͱ��??
??  ??  ������ rate_limiter.h  # ����??
??  ������ types/              # ���Ͷ���
??      ������ basic_types.h   # ��������
������ tests/                  # ��Ԫ����
������ CMakeLists.txt          # ��������
������ test_unified_modules.c  # ���ɲ���
```

---

## ?? ���Ĺ������

### 1. ƽ̨����??(`platform/`)

�ṩ��ƽ̨֧�֣��Զ�������ϵͳ�ͱ���������??

```c
#include <platform/os.h>
#include <platform/compiler.h>

// ����ϵͳ��??
#if defined(AGENTOS_OS_LINUX)
    // Linux �ض�����
#elif defined(AGENTOS_OS_MACOS)
    // macOS �ض�����
#elif defined(AGENTOS_OS_WINDOWS)
    // Windows �ض�����
#endif

// ��������??
AGENTOS_MAYBE_UNUSED  // [[maybe_unused]] ��??
AGENTOS_NORETURN      // [[noreturn]] ��??
AGENTOS_INLINE        // ǿ������
```

### 2. ���ù��� (`utils/manager/` & `utils/config_unified/`)

֧�� YAML ��ʽ���õļ��ء���֤���ȸ���??

#### ʹ��ʾ��

```c
#include <config_loader.h>

// ���������ļ�
config_t* cfg = config_load("./manager/kernel.yaml");
if (!cfg) {
    FATAL("Failed to load manager");
}

// ��ȡ��������
const char* log_level = config_get_string(cfg, "logging.level");
int timeout = config_get_int(cfg, "network.timeout", 30);  // Ĭ��??30
bool debug = config_get_bool(cfg, "debug.enabled", false);

// ��ȡ����
config_array_t* servers = config_get_array(cfg, "cluster.servers");
for (size_t i = 0; i < servers->size; i++) {
    printf("Server %zu: %s\n", i, servers->items[i]);
}

// �����ȸ��¼�??
config_watch(cfg, "llm.model", on_model_changed, NULL);

// �ͷ�����
config_free(cfg);
```

#### �����ļ�ʾ��

```yaml
# manager/kernel.yaml
logging:
  level: INFO
  format: json
  
network:
  timeout: 30
  retry_count: 3
  
cluster:
  servers:
    - server1:8080
    - server2:8080
    - server3:8080
    
llm:
  model: gpt-4
  temperature: 0.7
  max_tokens: 2048
```

### 3. ��־ϵͳ (`utils/logging/`)

��λһ��Ŀɹ۲��ԣ���־ + ָ�� + ׷��??

#### ��־ϵͳ����

```c
#include <logging/logger.h>

// ������־����
logger_set_level(LOG_LEVEL_INFO);

// ��¼��־���Զ�����ʱ������ļ������кţ�
LOG_DEBUG("Debug info: %d", value);
LOG_INFO("Service started on port %d", port);
LOG_WARNING("High memory usage: %.2f%%", memory_percent);
LOG_ERROR("Failed to connect: %s", error_msg);
LOG_FATAL("Critical error, exiting...");

// �ṹ����־��JSON ��ʽ??
LOG_JSON(INFO, 
    "{\"event\": \"request\", \"method\": \"%s\", \"latency_ms\": %d}",
    "GET", 150
);

// ??Trace ID ����־�����ڷֲ�ʽ׷�٣�
log_set_trace_id("trace-abc123");
LOG_INFO("Processing request");  // �Զ����� trace_id
```

### 4. �ɹ۲�??(`utils/observability/`)

#### ָ��ɼ���׷??

```c
#include <observability/metrics.h>
#include <observability/tracer.h>

// === ָ��ɼ� ===
// ��������??
metrics_counter_t* requests = metrics_counter_create(
    "http_requests_total",
    "Total HTTP requests"
);

// ���Ӽ���
metrics_counter_inc(requests, 1);

// ����ֱ��??
metrics_histogram_t* latency = metrics_histogram_create(
    "http_request_duration_seconds",
    "Request duration in seconds",
    (double[]){0.01, 0.05, 0.1, 0.5, 1.0, 5.0},  // buckets
    6
);

// �۲�??
metrics_histogram_observe(latency, 0.234);

// ����ָ�꣨Prometheus ��ʽ??
char* output = metrics_export_all();
printf("%s\n", output);

// === �ֲ�ʽ׷??===
// ��ʼһ??Span
span_t* span = tracer_start_span("process_request");
tracer_set_attribute(span, "user_id", "user-123");
tracer_set_attribute(span, "method", "POST");

// ����??Span��Ƕ�׵��ã�
span_t* child = tracer_start_child_span(span, "db_query");
// ... ���ݿ��??...
tracer_end_span(child);

// ���� Span
tracer_end_span(span);

// ��ȡ Trace ID ������־����
const char* trace_id = tracer_get_trace_id(span);
```

### 5. �ڴ���� (`utils/memory/`)

�ṩ RAII �����ڴ������ڴ��??

```c
#include <memory/smart_ptr.h>
#include <memory/mem_pool.h>

// === RAII ����ָ�� ===
core_mem_ptr_t ptr = core_mem_alloc(size);
if (!ptr) {
    return AGENTOS_ERROR_NO_MEMORY;
}
// �뿪������ʱ�Զ��ͷţ���й©����

// === �ڴ�??===
mem_pool_t* pool = mem_pool_create(1024 * 1024);  // 1MB ??

void* obj1 = mem_pool_alloc(pool, 256);
void* obj2 = mem_pool_alloc(pool, 512);

// �����ͷţ�������� free??
mem_pool_reset(pool);

// ���ٳ�
mem_pool_destroy(pool);
```

### 6. ������ (`utils/error/`)

ͳһ�Ĵ�������ϵ�ʹ��������??

```c
#include <error_handler.h>

// �����붨�壨??error_code.h �У�
typedef enum {
    OK = 0,
    ERR_INVALID_PARAM = -1,
    ERR_OUT_OF_MEMORY = -2,
    ERR_TIMEOUT = -3,
    ERR_NETWORK = -4,
    // ... �������??
} status_t;

// ���ش���
status_t func() {
    if (invalid_param) {
        return ERR_INVALID_PARAM;
    }
    return OK;
}

// ������??
status_t result = func();
if (result != OK) {
    LOG_ERROR("Function failed: %s", error_string(result));
    return result;
}

// ���ԣ�����ģʽ��Ч��
ASSERT(ptr != NULL);
ASSERT(value > 0);

// ȷ���������??defer??
DEFER(free(ptr));
void* ptr = malloc(1024);
```

### 7. ���ܷ��� (`utils/cost/`)

�߾��ȵ����ܷ����ͺ�ʱͳ��??

```c
#include <profiler.h>

// ���������ܷ���
PROFILE_FUNC();  // �Զ���¼����ִ��ʱ��

void slow_function() {
    PROFILE_FUNC();
    // ... ��ʱ���� ...
}

// ��������ܷ���
{
    PROFILE_BLOCK("database_query");
    // ... ���ݿ��??...
}

// �ֶ���ʱ
profiler_t* prof = profiler_create("custom_operation");
profiler_start(prof);

// ... ִ�в��� ...

profiler_stop(prof);
printf("Elapsed: %.3f ms\n", profiler_elapsed_ms(prof));

// ͳ����Ϣ
profiler_stats_t stats;
profiler_get_stats(prof, &stats);
printf("Avg: %.3f ms, P99: %.3f ms\n", 
    stats.avg_ms, stats.p99_ms);
```

### 8. Token ���� (`utils/token/`)

����Ͱ�㷨ʵ�ֵ�������??

```c
#include <token_bucket.h>

// ��������??
token_bucket_t* bucket = token_bucket_create(
    100,    // Ͱ���������������??
    10      // ������ʣ�ÿ??10 �����ƣ�
);

// �������ƣ�����ʽ??
token_bucket_acquire(bucket, 5);  // ��ȡ 5 ����??

// �������󣨷�����??
if (token_bucket_try_acquire(bucket, 1)) {
    // ���㹻���ƣ�ִ�в���
    process_request();
} else {
    // �������ܾ���??
    LOG_WARNING("Rate limit exceeded");
}

// ��������Ͱ
token_bucket_destroy(bucket);
```

### 9. ��ȫ���� (`utils/security/`)

���õļ��ܺ���֤����??

```c
#include <crypto.h>

// === ��ϣ ===
// SHA256
uint8_t hash[SHA256_DIGEST_LENGTH];
sha256("hello world", 11, hash);

// HMAC-SHA256
hmac_sha256("key", 3, "data", 4, hash);

// Base64 ���??
char* encoded = base64_encode(data, data_len);
uint8_t* decoded = base64_decode(encoded, &decoded_len);

// === AES ���� ===
aes_ctx_t* ctx = aes_init(key, key_len);
uint8_t* ciphertext = aes_encrypt(ctx, plaintext, plaintext_len, &ciphertext_len);
uint8_t* plaintext = aes_decrypt(ctx, ciphertext, ciphertext_len, &plaintext_len);
aes_free(ctx);

// === JWT ===
#include <auth.h>

// ���� JWT
char* token = jwt_generate(
    "{\"sub\": \"user123\", \"role\": \"admin\"}",
    secret_key
);

// ��֤ JWT
jwt_payload_t* payload = jwt_verify(token, secret_key);
if (payload) {
    printf("User: %s, Role: %s\n", 
        payload->sub, payload->role);
}
```

### 10. �ַ�����??(`utils/string/`)

�ṩ���õ��ַ�������??JSON ����??

```c
#include <string/str_utils.h>
#include <string/json_parser.h>

// === �ַ�����??===
// �ַ�����??
char** tokens = str_split("a,b,c", ",", &count);

// �ַ�����??
char* joined = str_join(tokens, count, "-");

// �޼�հ�
char* trimmed = str_trim("  hello  ");  // "hello"

// === JSON ���� ===
json_t* json = json_parse("{\"name\": \"Alice\", \"age\": 30}");

const char* name = json_get_string(json, "name");
int age = json_get_int(json, "age");

json_free(json);
```

### 11. ͬ��ԭ�� (`utils/sync/`)

�ṩ�߳�ͬ������??

```c
#include <sync/mutex.h>
#include <sync/rwlock.h>

// === ����??===
agentos_mutex_t* mutex = agentos_mutex_create();
agentos_mutex_lock(mutex);
// ... �ٽ�??...
agentos_mutex_unlock(mutex);
agentos_mutex_destroy(mutex);

// === ��д??===
rwlock_t* rwlock = rwlock_create();

// ������������??
rwlock_rdlock(rwlock);
// ... ��ȡ���� ...
rwlock_unlock(rwlock);

// д������ռ��
rwlock_wrlock(rwlock);
// ... �޸����� ...
rwlock_unlock(rwlock);

rwlock_destroy(rwlock);
```

---

## ?? ���ٿ�??

### ����

```bash
cd AgentOS/bases
mkdir build && cd build

# ��׼����
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# ���ø����ʲ�??
cmake .. -DENABLE_COVERAGE=ON
make -j$(nproc)

# ֻ�����ض�ģ??
make utils-logging      # ֻ������־ģ??
make utils-observability # ֻ����ɹ۲���ģ??
make utils-manager        # ֻ��������ģ??
```

### ����

```bash
cd build

# �������в�??
ctest --output-on-failure

# �����ض�ģ�����
ctest -R logger         # ֻ����־ģ��
ctest -R manager         # ֻ������ģ��
ctest -R memory         # ֻ���ڴ�ģ��

# ���ɸ����ʱ�??
make coverage
# �鿴 html/coverage/index.html
```

### ���ɵ������??

```cmake
# CMakeLists.txt
add_subdirectory(../bases/utils utils_build)
target_link_libraries(your_app PRIVATE utils)
```

---

## ?? ����ָ��

| ģ�� | ���� | �ӳ� | ����??|
|------|------|------|--------|
| ���ü��� | �������� | < 10ms | - |
| ��־ϵͳ | ����д�� | < 1��s | 100,000+/s |
| ָ��ɼ� | ���μ�¼ | < 100ns | 1,000,000+/s |
| ���ܷ��� | ���/ֹͣ | < 50ns | - |
| Token ���� | ��ȡ���� | < 1��s | - |
| SHA256 | 1KB ���� | < 5��s | - |
| AES ���� | 1KB ���� | < 10��s | - |
| JSON ���� | 1KB ���� | < 100��s | - |
| �ڴ�ط�??| ���η��� | < 100ns | 10,000,000+/s |

---

## ????���ʵ??

### 1. ��־ʹ�ù淶

```c
// ??�Ƽ���ʹ�ú��¼��־
LOG_INFO("User %s logged in", username);

// ??���Ƽ���ֱ�ӵ��õײ㺯��
logger_log(LOG_LEVEL_INFO, "User %s logged in", username);

// ??�Ƽ����ؼ�·��ʹ�ýṹ����־
LOG_JSON(INFO, 
    "{\"event\": \"login\", \"user_id\": \"%s\"}",
    user_id
);
```

### 2. ������淶

```c
// ??�Ƽ���������з���??
status_t ret = some_function();
if (ret != OK) {
    LOG_ERROR("Failed: %s", error_string(ret));
    return ret;
}

// ??���Ƽ������Է���??
some_function();  // ����ʧ��??
```

### 3. ���ܷ����淶

```c
// ??�Ƽ����ڹؼ�����������ܷ���
void critical_function() {
    PROFILE_FUNC();
    // ...
}

// ??�Ƽ������ȵ�������з�??
for (int i = 0; i < count; i++) {
    PROFILE_BLOCK("loop_iteration");
    // ...
}
```

---

## ?? ����ĵ�

- [����ģ����ϸָ��](../manager/README.md)
- [��ط���](../daemon/monit_d/README.md)
- [cupolas ���ģ��](../cupolas/README.md)
- [��Ŀ��Ŀ¼](../README.md)
- [�ܹ����ԭ��](../paper/architecture/folder/architectural_design_principles.md)

---

## ?? ����ָ��

��ӭ�ύ Issue ??Pull Request??

## ?? ��ϵ��ʽ

- **ά��??*: AgentOS �ܹ�ίԱ??
- **����֧??*: lidecheng@spharx.cn
- **���ⷴ��**: https://github.com/SpharxTeam/AgentOS/issues
- **�ٷ��ֿ�**: https://gitee.com/spharx/agentos

---

? 2026 SPHARX Ltd. All Rights Reserved.

*"Utilities for everything. ����֮����??*

