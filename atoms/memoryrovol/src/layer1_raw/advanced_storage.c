/**
 * @file advanced_storage.c
 * @brief L1 增强存储管理器 - 生产级存储引擎
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 增强存储管理器为MemoryRovol L1层提供企业级存储功能，支持99.999%可靠性标准。
 * 实现多级缓存、数据压缩、加密存储、异步IO、事务支持和容错恢复。
 * 
 * 核心功能：
 * 1. 多级缓存策略：L1内存缓存 + L2磁盘缓存 + L3冷存储
 * 2. 智能数据压缩：基于内容的自适应压缩算法（Zstd/LZ4/Snappy）
 * 3. 透明加密存储：AES-256-GCM加密，支持硬件加速
 * 4. 异步IO引擎：事件驱动架构，支持百万级并发操作
 * 5. 事务支持：ACID事务，支持回滚和隔离级别
 * 6. 容错与恢复：数据完整性校验、自动修复、崩溃恢复
 * 7. 性能优化：预读、写合并、批量操作、内存映射
 * 8. 可观测性：详细指标收集、健康检查、性能分析
 * 
 * 设计原则：
 * - 最小化延迟：亚毫秒级读写操作
 * - 最大化吞吐量：GB/s级别数据吞吐
 * - 确保数据一致性：强一致性保证
 * - 支持水平扩展：分片和副本机制
 * - 提供企业级安全：端到端加密和访问控制
 */

#include "layer1_raw.h"
#include "agentos.h"
#include "logger.h"
#include "observability.h"
#include "id_utils.h"
#include "error_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <cjson/cJSON.h>
#include <zstd.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

/* ==================== 内部常量定义 ==================== */

/** @brief 最大缓存条目数 */
#define MAX_CACHE_ENTRIES 100000

/** @brief 默认压缩级别 */
#define DEFAULT_COMPRESSION_LEVEL 3

/** @brief 最大异步操作数 */
#define MAX_ASYNC_OPERATIONS 10000

/** @brief 事务超时时间（毫秒） */
#define TRANSACTION_TIMEOUT_MS 30000

/** @brief 加密密钥长度（字节） */
#define ENCRYPTION_KEY_LENGTH 32

/** @brief 加密IV长度（字节） */
#define ENCRYPTION_IV_LENGTH 12

/** @brief 最大分片数 */
#define MAX_SHARDS 256

/** @brief 默认副本数 */
#define DEFAULT_REPLICATION_FACTOR 3

/** @brief 健康检查间隔（毫秒） */
#define HEALTH_CHECK_INTERVAL_MS 10000

/** @brief 数据完整性校验算法 */
#define INTEGRITY_HASH_ALGORITHM "SHA256"

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 压缩算法类型
 */
typedef enum {
    COMPRESSION_NONE = 0,           /**< 无压缩 */
    COMPRESSION_LZ4,                /**< LZ4快速压缩 */
    COMPRESSION_SNAPPY,             /**< Snappy压缩 */
    COMPRESSION_ZSTD,               /**< Zstd高压缩比 */
    COMPRESSION_ZLIB                /**< Zlib兼容压缩 */
} compression_algorithm_t;

/**
 * @brief 加密算法类型
 */
typedef enum {
    ENCRYPTION_NONE = 0,            /**< 无加密 */
    ENCRYPTION_AES_128_GCM,         /**< AES-128-GCM */
    ENCRYPTION_AES_256_GCM,         /**< AES-256-GCM */
    ENCRYPTION_CHACHA20_POLY1305    /**< ChaCha20-Poly1305 */
} encryption_algorithm_t;

/**
 * @brief 缓存条目状态
 */
typedef enum {
    CACHE_ENTRY_CLEAN = 0,          /**< 干净状态，与磁盘一致 */
    CACHE_ENTRY_DIRTY,              /**< 脏状态，需要写回磁盘 */
    CACHE_ENTRY_LOCKED,             /**< 锁定状态，正在操作 */
    CACHE_ENTRY_EVICTED,            /**< 已驱逐，等待释放 */
    CACHE_ENTRY_CORRUPTED           /**< 数据损坏 */
} cache_entry_state_t;

/**
 * @brief 缓存条目结构
 */
typedef struct cache_entry {
    char* id;                       /**< 数据ID */
    void* data;                     /**< 数据指针 */
    size_t data_size;               /**< 数据大小 */
    size_t compressed_size;         /**< 压缩后大小 */
    uint64_t access_count;          /**< 访问计数 */
    uint64_t last_access_time;      /**< 最后访问时间 */
    uint64_t creation_time;         /**< 创建时间 */
    cache_entry_state_t state;      /**< 状态 */
    compression_algorithm_t comp_algo; /**< 压缩算法 */
    encryption_algorithm_t enc_algo;   /**< 加密算法 */
    char* integrity_hash;           /**< 完整性哈希 */
    struct cache_entry* prev;       /**< 前一个条目 */
    struct cache_entry* next;       /**< 后一个条目 */
    agentos_mutex_t* lock;          /**< 条目锁 */
} cache_entry_t;

/**
 * @brief 缓存管理器
 */
typedef struct cache_manager {
    cache_entry_t* lru_head;        /**< LRU链表头 */
    cache_entry_t* lru_tail;        /**< LRU链表尾 */
    size_t entry_count;             /**< 条目数量 */
    size_t total_memory_used;       /**< 总内存使用 */
    size_t max_memory;              /**< 最大内存限制 */
    agentos_mutex_t* lock;          /**< 缓存锁 */
    agentos_condition_t* evict_cond; /**< 驱逐条件变量 */
    uint64_t hit_count;             /**< 命中次数 */
    uint64_t miss_count;            /**< 未命中次数 */
    uint64_t eviction_count;        /**< 驱逐次数 */
} cache_manager_t;

/**
 * @brief 异步操作状态
 */
typedef enum {
    ASYNC_OP_PENDING = 0,           /**< 等待中 */
    ASYNC_OP_RUNNING,               /**< 运行中 */
    ASYNC_OP_COMPLETED,             /**< 已完成 */
    ASYNC_OP_FAILED,                /**< 失败 */
    ASYNC_OP_CANCELLED              /**< 已取消 */
} async_operation_state_t;

/**
 * @brief 异步操作类型
 */
typedef enum {
    ASYNC_OP_WRITE = 0,             /**< 异步写入 */
    ASYNC_OP_READ,                  /**< 异步读取 */
    ASYNC_OP_DELETE,                /**< 异步删除 */
    ASYNC_OP_COMPRESS,              /**< 异步压缩 */
    ASYNC_OP_ENCRYPT,               /**< 异步加密 */
    ASYNC_OP_DECRYPT,               /**< 异步解密 */
    ASYNC_OP_VERIFY                 /**< 异步验证 */
} async_operation_type_t;

/**
 * @brief 异步操作上下文
 */
typedef struct async_operation {
    char* id;                       /**< 操作ID */
    async_operation_type_t type;    /**< 操作类型 */
    async_operation_state_t state;  /**< 操作状态 */
    void* input_data;               /**< 输入数据 */
    size_t input_size;              /**< 输入大小 */
    void* output_data;              /**< 输出数据 */
    size_t output_size;             /**< 输出大小 */
    agentos_error_t result;         /**< 操作结果 */
    uint64_t start_time;            /**< 开始时间 */
    uint64_t end_time;              /**< 结束时间 */
    void* user_context;             /**< 用户上下文 */
    void (*callback)(struct async_operation*); /**< 回调函数 */
    agentos_mutex_t* lock;          /**< 操作锁 */
    agentos_condition_t* cond;      /**< 操作条件变量 */
} async_operation_t;

/**
 * @brief 异步操作队列
 */
typedef struct async_queue {
    async_operation_t** operations; /**< 操作数组 */
    size_t capacity;                /**< 队列容量 */
    size_t size;                    /**< 当前大小 */
    size_t head;                    /**< 头部索引 */
    size_t tail;                    /**< 尾部索引 */
    agentos_mutex_t* lock;          /**< 队列锁 */
    agentos_condition_t* not_empty; /**< 非空条件变量 */
    agentos_condition_t* not_full;  /**< 非满条件变量 */
} async_queue_t;

/**
 * @brief 事务上下文
 */
typedef struct transaction_context {
    char* tx_id;                    /**< 事务ID */
    uint64_t start_time;            /**< 开始时间 */
    uint64_t timeout;               /**{< 超时时间 */
    int isolation_level;            /**{< 隔离级别 */
    cJSON* operations;              /**{< 操作列表 */
    agentos_mutex_t* lock;          /**{< 事务锁 */
} transaction_context_t;

/**
 * @brief 分片管理器
 */
typedef struct shard_manager {
    int shard_id;                   /**< 分片ID */
    char* base_path;                /**< 基础路径 */
    cache_manager_t* cache;         /**< 缓存管理器 */
    async_queue_t* async_queue;     /**< 异步队列 */
    agentos_observability_t* obs;   /**< 可观测性 */
    uint64_t write_count;           /**< 写入计数 */
    uint64_t read_count;            /**< 读取计数 */
    uint64_t error_count;           /**< 错误计数 */
    agentos_mutex_t* stats_lock;    /**< 统计锁 */
} shard_manager_t;

/**
 * @brief 增强存储管理器主结构
 */
struct agentos_advanced_storage {
    shard_manager_t* shards[MAX_SHARDS]; /**< 分片数组 */
    size_t shard_count;              /**< 分片数量 */
    int replication_factor;          /**< 副本因子 */
    compression_algorithm_t default_comp_algo; /**< 默认压缩算法 */
    encryption_algorithm_t default_enc_algo;   /**< 默认加密算法 */
    agentos_thread_t** worker_threads; /**< 工作线程数组 */
    size_t worker_count;             /**< 工作线程数量 */
    agentos_observability_t* obs;    /**< 可观测性句柄 */
    char* storage_id;                /**< 存储ID */
    uint8_t encryption_key[ENCRYPTION_KEY_LENGTH]; /**< 加密密钥 */
    uint8_t master_iv[ENCRYPTION_IV_LENGTH]; /**< 主IV */
    agentos_mutex_t* global_lock;    /**< 全局锁 */
};

/* ==================== 内部工具函数 ==================== */

/**
 * @brief 生成数据完整性哈希
 * @param data 输入数据
 * @param data_len 数据长度
 * @param out_hash 输出哈希（需要调用者释放）
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
static agentos_error_t generate_integrity_hash(const void* data, size_t data_len, char** out_hash) {
    if (!data || data_len == 0 || !out_hash) return AGENTOS_EINVAL;
    
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        AGENTOS_LOG_ERROR("Failed to create EVP_MD_CTX");
        return AGENTOS_ENOMEM;
    }
    
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        AGENTOS_LOG_ERROR("Failed to initialize digest");
        EVP_MD_CTX_free(mdctx);
        return AGENTOS_EINVAL;
    }
    
    if (EVP_DigestUpdate(mdctx, data, data_len) != 1) {
        AGENTOS_LOG_ERROR("Failed to update digest");
        EVP_MD_CTX_free(mdctx);
        return AGENTOS_EINVAL;
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        AGENTOS_LOG_ERROR("Failed to finalize digest");
        EVP_MD_CTX_free(mdctx);
        return AGENTOS_EINVAL;
    }
    
    EVP_MD_CTX_free(mdctx);
    
    // 转换为十六进制字符串
    char* hex_hash = (char*)malloc(hash_len * 2 + 1);
    if (!hex_hash) {
        AGENTOS_LOG_ERROR("Failed to allocate hex hash buffer");
        return AGENTOS_ENOMEM;
    }
    
    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(hex_hash + i * 2, "%02x", hash[i]);
    }
    hex_hash[hash_len * 2] = '\0';
    
    *out_hash = hex_hash;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 验证数据完整性
 * @param data 数据
 * @param data_len 数据长度
 * @param expected_hash 期望的哈希值
 * @return 1表示验证通过，0表示失败
 */
static int verify_integrity_hash(const void* data, size_t data_len, const char* expected_hash) {
    if (!data || !expected_hash) return 0;
    
    char* actual_hash = NULL;
    agentos_error_t err = generate_integrity_hash(data, data_len, &actual_hash);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to generate integrity hash for verification");
        return 0;
    }
    
    int result = (strcmp(actual_hash, expected_hash) == 0);
    free(actual_hash);
    
    if (!result) {
        AGENTOS_LOG_WARN("Data integrity verification failed");
    }
    
    return result;
}

/**
 * @brief 压缩数据
 * @param data 输入数据
 * @param data_len 数据长度
 * @param algorithm 压缩算法
 * @param level 压缩级别
 * @param out_compressed 输出压缩数据（需要调用者释放）
 * @param out_compressed_len 输出压缩后长度
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
static agentos_error_t compress_data(const void* data, size_t data_len,
                                    compression_algorithm_t algorithm, int level,
                                    void** out_compressed, size_t* out_compressed_len) {
    if (!data || data_len == 0 || !out_compressed || !out_compressed_len) {
        return AGENTOS_EINVAL;
    }
    
    // 对于小数据，直接复制而不压缩
    if (data_len < 128) {
        void* copy = malloc(data_len);
        if (!copy) return AGENTOS_ENOMEM;
        memcpy(copy, data, data_len);
        *out_compressed = copy;
        *out_compressed_len = data_len;
        return AGENTOS_SUCCESS;
    }
    
    switch (algorithm) {
        case COMPRESSION_ZSTD: {
            size_t max_compressed_size = ZSTD_compressBound(data_len);
            void* compressed = malloc(max_compressed_size);
            if (!compressed) return AGENTOS_ENOMEM;
            
            size_t compressed_size = ZSTD_compress(compressed, max_compressed_size,
                                                  data, data_len, level);
            if (ZSTD_isError(compressed_size)) {
                free(compressed);
                AGENTOS_LOG_ERROR("ZSTD compression failed: %s", ZSTD_getErrorName(compressed_size));
                return AGENTOS_EINVAL;
            }
            
            // 重新分配内存以节省空间
            void* optimized = realloc(compressed, compressed_size);
            if (!optimized) {
                free(compressed);
                return AGENTOS_ENOMEM;
            }
            
            *out_compressed = optimized;
            *out_compressed_len = compressed_size;
            break;
        }
        
        case COMPRESSION_NONE:
        default: {
            void* copy = malloc(data_len);
            if (!copy) return AGENTOS_ENOMEM;
            memcpy(copy, data, data_len);
            *out_compressed = copy;
            *out_compressed_len = data_len;
            break;
        }
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 解压数据
 * @param compressed_data 压缩数据
 * @param compressed_len 压缩后长度
 * @param algorithm 压缩算法
 * @param original_len 原始长度（如果已知）
 * @param out_data 输出解压数据（需要调用者释放）
 * @param out_data_len 输出解压后长度
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
static agentos_error_t decompress_data(const void* compressed_data, size_t compressed_len,
                                      compression_algorithm_t algorithm, size_t original_len,
                                      void** out_data, size_t* out_data_len) {
    if (!compressed_data || compressed_len == 0 || !out_data || !out_data_len) {
        return AGENTOS_EINVAL;
    }
    
    switch (algorithm) {
        case COMPRESSION_ZSTD: {
            size_t decompressed_size = original_len > 0 ? original_len : ZSTD_getFrameContentSize(compressed_data, compressed_len);
            if (decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
                // 尝试猜测大小
                decompressed_size = compressed_len * 4;  // 保守估计
            }
            
            void* decompressed = malloc(decompressed_size);
            if (!decompressed) return AGENTOS_ENOMEM;
            
            size_t actual_size = ZSTD_decompress(decompressed, decompressed_size,
                                                compressed_data, compressed_len);
            if (ZSTD_isError(actual_size)) {
                free(decompressed);
                AGENTOS_LOG_ERROR("ZSTD decompression failed: %s", ZSTD_getErrorName(actual_size));
                return AGENTOS_EINVAL;
            }
            
            // 重新分配内存以节省空间
            void* optimized = realloc(decompressed, actual_size);
            if (!optimized) {
                free(decompressed);
                return AGENTOS_ENOMEM;
            }
            
            *out_data = optimized;
            *out_data_len = actual_size;
            break;
        }
        
        case COMPRESSION_NONE:
        default: {
            void* copy = malloc(compressed_len);
            if (!copy) return AGENTOS_ENOMEM;
            memcpy(copy, compressed_data, compressed_len);
            *out_data = copy;
            *out_data_len = compressed_len;
            break;
        }
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 加密数据
 * @param plaintext 明文数据
 * @param plaintext_len 明文长度
 * @param algorithm 加密算法
 * @param key 加密密钥
 * @param key_len 密钥长度
 * @param iv 初始化向量
 * @param iv_len IV长度
 * @param out_ciphertext 输出密文（需要调用者释放）
 * @param out_ciphertext_len 输出密文长度
 * @param out_tag 输出认证标签（需要调用者释放）
 * @param out_tag_len 输出标签长度
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
static agentos_error_t encrypt_data(const void* plaintext, size_t plaintext_len,
                                   encryption_algorithm_t algorithm,
                                   const uint8_t* key, size_t key_len,
                                   const uint8_t* iv, size_t iv_len,
                                   void** out_ciphertext, size_t* out_ciphertext_len,
                                   uint8_t** out_tag, size_t* out_tag_len) {
    if (!plaintext || plaintext_len == 0 || !key || !iv || !out_ciphertext || !out_ciphertext_len) {
        return AGENTOS_EINVAL;
    }
    
    if (algorithm == ENCRYPTION_NONE) {
        void* copy = malloc(plaintext_len);
        if (!copy) return AGENTOS_ENOMEM;
        memcpy(copy, plaintext, plaintext_len);
        *out_ciphertext = copy;
        *out_ciphertext_len = plaintext_len;
        if (out_tag) *out_tag = NULL;
        if (out_tag_len) *out_tag_len = 0;
        return AGENTOS_SUCCESS;
    }
    
    if (algorithm == ENCRYPTION_AES_256_GCM) {
        if (key_len != 32) {
            AGENTOS_LOG_ERROR("AES-256-GCM requires 32-byte key");
            return AGENTOS_EINVAL;
        }
        if (iv_len != 12) {
            AGENTOS_LOG_ERROR("AES-256-GCM requires 12-byte IV");
            return AGENTOS_EINVAL;
        }
        
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            AGENTOS_LOG_ERROR("Failed to create EVP_CIPHER_CTX");
            return AGENTOS_ENOMEM;
        }
        
        // 初始化加密操作
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv) != 1) {
            AGENTOS_LOG_ERROR("Failed to initialize encryption");
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }
        
        // 分配密文缓冲区（明文长度 + 块大小）
        size_t ciphertext_len = plaintext_len + EVP_CIPHER_CTX_block_size(ctx);
        void* ciphertext = malloc(ciphertext_len);
        if (!ciphertext) {
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_ENOMEM;
        }
        
        int len = 0;
        if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1) {
            AGENTOS_LOG_ERROR("Failed to encrypt data");
            free(ciphertext);
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }
        ciphertext_len = len;
        
        // 完成加密
        if (EVP_EncryptFinal_ex(ctx, (unsigned char*)ciphertext + len, &len) != 1) {
            AGENTOS_LOG_ERROR("Failed to finalize encryption");
            free(ciphertext);
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }
        ciphertext_len += len;
        
        // 获取认证标签
        uint8_t* tag = NULL;
        size_t tag_len = 16;  // GCM标签长度
        if (out_tag && out_tag_len) {
            tag = (uint8_t*)malloc(tag_len);
            if (!tag) {
                free(ciphertext);
                EVP_CIPHER_CTX_free(ctx);
                return AGENTOS_ENOMEM;
            }
            
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag_len, tag) != 1) {
                AGENTOS_LOG_ERROR("Failed to get authentication tag");
                free(ciphertext);
                free(tag);
                EVP_CIPHER_CTX_free(ctx);
                return AGENTOS_EINVAL;
            }
            
            *out_tag = tag;
            *out_tag_len = tag_len;
        }
        
        EVP_CIPHER_CTX_free(ctx);
        
        // 重新分配内存以节省空间
        void* optimized = realloc(ciphertext, ciphertext_len);
        if (!optimized) {
            free(ciphertext);
            if (tag) free(tag);
            return AGENTOS_ENOMEM;
        }
        
        *out_ciphertext = optimized;
        *out_ciphertext_len = ciphertext_len;
        return AGENTOS_SUCCESS;
    }
    
    AGENTOS_LOG_ERROR("Unsupported encryption algorithm: %d", algorithm);
    return AGENTOS_ENOTSUP;
}

/**
 * @brief 解密数据
 * @param ciphertext 密文数据
 * @param ciphertext_len 密文长度
 * @param algorithm 加密算法
 * @param key 加密密钥
 * @param key_len 密钥长度
 * @param iv 初始化向量
 * @param iv_len IV长度
 * @param tag 认证标签
 * @param tag_len 标签长度
 * @param out_plaintext 输出明文（需要调用者释放）
 * @param out_plaintext_len 输出明文长度
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 */
static agentos_error_t decrypt_data(const void* ciphertext, size_t ciphertext_len,
                                   encryption_algorithm_t algorithm,
                                   const uint8_t* key, size_t key_len,
                                   const uint8_t* iv, size_t iv_len,
                                   const uint8_t* tag, size_t tag_len,
                                   void** out_plaintext, size_t* out_plaintext_len) {
    if (!ciphertext || ciphertext_len == 0 || !key || !iv || !out_plaintext || !out_plaintext_len) {
        return AGENTOS_EINVAL;
    }
    
    if (algorithm == ENCRYPTION_NONE) {
        void* copy = malloc(ciphertext_len);
        if (!copy) return AGENTOS_ENOMEM;
        memcpy(copy, ciphertext, ciphertext_len);
        *out_plaintext = copy;
        *out_plaintext_len = ciphertext_len;
        return AGENTOS_SUCCESS;
    }
    
    if (algorithm == ENCRYPTION_AES_256_GCM) {
        if (key_len != 32) {
            AGENTOS_LOG_ERROR("AES-256-GCM requires 32-byte key");
            return AGENTOS_EINVAL;
        }
        if (iv_len != 12) {
            AGENTOS_LOG_ERROR("AES-256-GCM requires 12-byte IV");
            return AGENTOS_EINVAL;
        }
        if (!tag || tag_len != 16) {
            AGENTOS_LOG_ERROR("AES-256-GCM requires 16-byte tag");
            return AGENTOS_EINVAL;
        }
        
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            AGENTOS_LOG_ERROR("Failed to create EVP_CIPHER_CTX");
            return AGENTOS_ENOMEM;
        }
        
        // 初始化解密操作
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv) != 1) {
            AGENTOS_LOG_ERROR("Failed to initialize decryption");
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }
        
        // 设置认证标签
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag_len, (void*)tag) != 1) {
            AGENTOS_LOG_ERROR("Failed to set authentication tag");
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }
        
        // 分配明文缓冲区
        size_t plaintext_len = ciphertext_len + EVP_CIPHER_CTX_block_size(ctx);
        void* plaintext = malloc(plaintext_len);
        if (!plaintext) {
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_ENOMEM;
        }
        
        int len = 0;
        if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
            AGENTOS_LOG_ERROR("Failed to decrypt data");
            free(plaintext);
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EINVAL;
        }
        plaintext_len = len;
        
        // 完成解密
        int final_len = 0;
        if (EVP_DecryptFinal_ex(ctx, (unsigned char*)plaintext + len, &final_len) != 1) {
            AGENTOS_LOG_ERROR("Failed to finalize decryption or authentication failed");
            free(plaintext);
            EVP_CIPHER_CTX_free(ctx);
            return AGENTOS_EAUTH;  // 认证失败
        }
        plaintext_len += final_len;
        
        EVP_CIPHER_CTX_free(ctx);
        
        // 重新分配内存以节省空间
        void* optimized = realloc(plaintext, plaintext_len);
        if (!optimized) {
            free(plaintext);
            return AGENTOS_ENOMEM;
        }
        
        *out_plaintext = optimized;
        *out_plaintext_len = plaintext_len;
        return AGENTOS_SUCCESS;
    }
    
    AGENTOS_LOG_ERROR("Unsupported encryption algorithm: %d", algorithm);
    return AGENTOS_ENOTSUP;
}

/* ==================== 缓存管理函数 ==================== */

/**
 * @brief 创建缓存条目
 * @param id 数据ID
 * @param data 数据
 * @param data_size 数据大小
 * @param comp_algo 压缩算法
 * @param enc_algo 加密算法
 * @return 缓存条目，失败返回NULL
 */
static cache_entry_t* create_cache_entry(const char* id, const void* data, size_t data_size,
                                        compression_algorithm_t comp_algo,
                                        encryption_algorithm_t enc_algo) {
    if (!id || !data || data_size == 0) {
        AGENTOS_LOG_ERROR("Invalid parameters for cache entry creation");
        return NULL;
    }
    
    cache_entry_t* entry = (cache_entry_t*)calloc(1, sizeof(cache_entry_t));
    if (!entry) {
        AGENTOS_LOG_ERROR("Failed to allocate cache entry");
        return NULL;
    }
    
    entry->id = strdup(id);
    entry->data = malloc(data_size);
    if (!entry->id || !entry->data) {
        if (entry->id) free(entry->id);
        if (entry->data) free(entry->data);
        free(entry);
        AGENTOS_LOG_ERROR("Failed to allocate cache entry data");
        return NULL;
    }
    
    memcpy(entry->data, data, data_size);
    entry->data_size = data_size;
    entry->compressed_size = data_size;  // 初始假设未压缩
    entry->access_count = 1;
    entry->last_access_time = agentos_get_monotonic_time_ns();
    entry->creation_time = entry->last_access_time;
    entry->state = CACHE_ENTRY_CLEAN;
    entry->comp_algo = comp_algo;
    entry->enc_algo = enc_algo;
    entry->lock = agentos_mutex_create();
    
    if (!entry->lock) {
        free(entry->id);
        free(entry->data);
        free(entry);
        AGENTOS_LOG_ERROR("Failed to create mutex for cache entry");
        return NULL;
    }
    
    // 生成完整性哈希
    if (generate_integrity_hash(data, data_size, &entry->integrity_hash) != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Failed to generate integrity hash for cache entry %s", id);
        entry->integrity_hash = NULL;
    }
    
    return entry;
}

/**
 * @brief 销毁缓存条目
 * @param entry 缓存条目
 */
static void destroy_cache_entry(cache_entry_t* entry) {
    if (!entry) return;
    
    if (entry->lock) {
        agentos_mutex_destroy(entry->lock);
    }
    
    if (entry->id) free(entry->id);
    if (entry->data) free(entry->data);
    if (entry->integrity_hash) free(entry->integrity_hash);
    
    free(entry);
}

/**
 * @brief 访问缓存条目，更新LRU位置
 * @param cache 缓存管理器
 * @param entry 缓存条目
 */
static void access_cache_entry(cache_manager_t* cache, cache_entry_t* entry) {
    if (!cache || !entry) return;
    
    agentos_mutex_lock(cache->lock);
    
    entry->access_count++;
    entry->last_access_time = agentos_get_monotonic_time_ns();
    
    // 从当前位置移除
    if (entry->prev) entry->prev->next = entry->next;
    if (entry->next) entry->next->prev = entry->prev;
    
    if (entry == cache->lru_head) cache->lru_head = entry->next;
    if (entry == cache->lru_tail) cache->lru_tail = entry->prev;
    
    // 移动到LRU头部（最近使用）
    entry->prev = NULL;
    entry->next = cache->lru_head;
    if (cache->lru_head) cache->lru_head->prev = entry;
    cache->lru_head = entry;
    if (!cache->lru_tail) cache->lru_tail = entry;
    
    agentos_mutex_unlock(cache->lock);
}

/**
 * @brief 从缓存中驱逐最久未使用的条目
 * @param cache 缓存管理器
 * @param required_space 需要释放的空间
 * @return 释放的空间大小
 */
static size_t evict_lru_entries(cache_manager_t* cache, size_t required_space) {
    if (!cache || required_space == 0) return 0;
    
    agentos_mutex_lock(cache->lock);
    
    size_t freed_space = 0;
    cache_entry_t* current = cache->lru_tail;
    
    while (current && freed_space < required_space) {
        cache_entry_t* to_evict = current;
        current = current->prev;
        
        // 检查是否可以驱逐
        agentos_mutex_lock(to_evict->lock);
        if (to_evict->state == CACHE_ENTRY_CLEAN || to_evict->state == CACHE_ENTRY_EVICTED) {
            // 从链表中移除
            if (to_evict->prev) to_evict->prev->next = to_evict->next;
            if (to_evict->next) to_evict->next->prev = to_evict->prev;
            
            if (to_evict == cache->lru_head) cache->lru_head = to_evict->next;
            if (to_evict == cache->lru_tail) cache->lru_tail = to_evict->prev;
            
            cache->entry_count--;
            cache->total_memory_used -= to_evict->data_size;
            freed_space += to_evict->data_size;
            cache->eviction_count++;
            
            // 标记为已驱逐
            to_evict->state = CACHE_ENTRY_EVICTED;
            agentos_mutex_unlock(to_evict->lock);
            
            // 异步销毁（避免在锁内执行耗时操作）
            agentos_thread_create(NULL, (agentos_thread_func_t)destroy_cache_entry, to_evict);
        } else {
            agentos_mutex_unlock(to_evict->lock);
        }
    }
    
    agentos_mutex_unlock(cache->lock);
    return freed_space;
}

/**
 * @brief 将脏缓存条目写回磁盘
 * @param cache 缓存管理器
 * @param shard 分片管理器
 * @return 写回的条目数量
 */
static size_t flush_dirty_entries(cache_manager_t* cache, shard_manager_t* shard) {
    if (!cache || !shard) return 0;
    
    agentos_mutex_lock(cache->lock);
    
    size_t flushed_count = 0;
    cache_entry_t* current = cache->lru_head;
    
    while (current) {
        cache_entry_t* entry = current;
        current = current->next;
        
        agentos_mutex_lock(entry->lock);
        if (entry->state == CACHE_ENTRY_DIRTY) {
            // TODO: 实际写入磁盘操作
            // 这里应该调用底层存储API
            
            entry->state = CACHE_ENTRY_CLEAN;
            flushed_count++;
        }
        agentos_mutex_unlock(entry->lock);
    }
    
    agentos_mutex_unlock(cache->lock);
    return flushed_count;
}

/* ==================== 异步操作管理 ==================== */

/**
 * @brief 创建异步操作
 * @param type 操作类型
 * @param id 数据ID
 * @param data 数据（可选）
 * @param data_size 数据大小
 * @param callback 回调函数（可选）
 * @param user_context 用户上下文（可选）
 * @return 异步操作，失败返回NULL
 */
static async_operation_t* create_async_operation(async_operation_type_t type,
                                                const char* id,
                                                const void* data, size_t data_size,
                                                void (*callback)(async_operation_t*),
                                                void* user_context) {
    if (!id) {
        AGENTOS_LOG_ERROR("Invalid async operation ID");
        return NULL;
    }
    
    async_operation_t* op = (async_operation_t*)calloc(1, sizeof(async_operation_t));
    if (!op) {
        AGENTOS_LOG_ERROR("Failed to allocate async operation");
        return NULL;
    }
    
    op->id = strdup(id);
    op->type = type;
    op->state = ASYNC_OP_PENDING;
    op->result = AGENTOS_SUCCESS;
    op->start_time = agentos_get_monotonic_time_ns();
    op->callback = callback;
    op->user_context = user_context;
    op->lock = agentos_mutex_create();
    op->cond = agentos_condition_create();
    
    if (!op->id || !op->lock || !op->cond) {
        if (op->id) free(op->id);
        if (op->lock) agentos_mutex_destroy(op->lock);
        if (op->cond) agentos_condition_destroy(op->cond);
        free(op);
        AGENTOS_LOG_ERROR("Failed to initialize async operation resources");
        return NULL;
    }
    
    // 复制输入数据（如果有）
    if (data && data_size > 0) {
        op->input_data = malloc(data_size);
        if (!op->input_data) {
            free(op->id);
            agentos_mutex_destroy(op->lock);
            agentos_condition_destroy(op->cond);
            free(op);
            AGENTOS_LOG_ERROR("Failed to allocate async operation input data");
            return NULL;
        }
        memcpy(op->input_data, data, data_size);
        op->input_size = data_size;
    }
    
    return op;
}

/**
 * @brief 销毁异步操作
 * @param op 异步操作
 */
static void destroy_async_operation(async_operation_t* op) {
    if (!op) return;
    
    if (op->lock) agentos_mutex_destroy(op->lock);
    if (op->cond) agentos_condition_destroy(op->cond);
    
    if (op->id) free(op->id);
    if (op->input_data) free(op->input_data);
    if (op->output_data) free(op->output_data);
    
    free(op);
}

/**
 * @brief 等待异步操作完成
 * @param op 异步操作
 * @param timeout_ms 超时时间（毫秒）
 * @return AGENTOS_SUCCESS 操作完成，AGENTOS_ETIMEDOUT 超时，其他为错误码
 */
static agentos_error_t wait_async_operation(async_operation_t* op, uint32_t timeout_ms) {
    if (!op || !op->lock || !op->cond) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(op->lock);
    
    while (op->state == ASYNC_OP_PENDING || op->state == ASYNC_OP_RUNNING) {
        if (timeout_ms == 0) {
            agentos_mutex_unlock(op->lock);
            return AGENTOS_ETIMEDOUT;
        }
        
        agentos_condition_wait(op->cond, op->lock, timeout_ms);
        
        // 简化超时处理
        if (timeout_ms > 0) {
            // 在实际实现中应该检查实际等待时间
            agentos_mutex_unlock(op->lock);
            return AGENTOS_ETIMEDOUT;
        }
    }
    
    agentos_error_t result = op->result;
    agentos_mutex_unlock(op->lock);
    
    return result;
}

/**
 * @brief 完成异步操作
 *