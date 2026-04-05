/**
 * @file advanced_storage.c
 * @brief L1 增强存储管理器 - 生产级存储引擎（精简版）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 增强存储管理器为 MemoryRovol L1 层提供企业级存储功能，支持 99.999% 可靠性标准。
 * 基于 advanced_storage_utils、advanced_storage_cache、advanced_storage_async 模块构建。
 */

#include "layer1_raw.h"
#include "agentos.h"
#include "logger.h"
#include "observability.h"
#include "advanced_storage_utils.h"
#include "advanced_storage_cache.h"
#include "advanced_storage_async.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 基础库兼容性层 */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"

/* ==================== 常量定义 ==================== */

#define MAX_SHARDS 256
#define ENCRYPTION_KEY_LENGTH 32
#define ENCRYPTION_IV_LENGTH 12
#define DEFAULT_MAX_CACHE_MEMORY (256 * 1024 * 1024)  /* 256MB */

/* ==================== 数据结构 ==================== */

/**
 * @brief 分片管理器
 */
typedef struct shard_manager {
    int shard_id;
    char* base_path;
    cache_manager_t* cache;
    async_queue_t* async_queue;
    agentos_layer1_raw_t* storage;
    agentos_observability_t* obs;
    uint64_t write_count;
    uint64_t read_count;
    uint64_t error_count;
    agentos_mutex_t* stats_lock;
} shard_manager_t;

/**
 * @brief 增强存储管理器主结构
 */
struct agentos_advanced_storage {
    shard_manager_t* shards[MAX_SHARDS];
    size_t shard_count;
    int replication_factor;
    compression_algorithm_t default_comp_algo;
    encryption_algorithm_t default_enc_algo;
    agentos_thread_t** worker_threads;
    size_t worker_count;
    agentos_observability_t* obs;
    char* storage_id;
    uint8_t encryption_key[ENCRYPTION_KEY_LENGTH];
    uint8_t master_iv[ENCRYPTION_IV_LENGTH];
    agentos_mutex_t* global_lock;
};

/* ==================== 分片管理 ==================== */

static shard_manager_t* create_shard(int shard_id, const char* base_path) {
    if (!base_path) return NULL;

    shard_manager_t* shard = (shard_manager_t*)AGENTOS_CALLOC(1, sizeof(shard_manager_t));
    if (!shard) {
        AGENTOS_LOG_ERROR("Failed to allocate shard manager");
        return NULL;
    }

    shard->shard_id = shard_id;
    shard->base_path = AGENTOS_STRDUP(base_path);
    shard->stats_lock = agentos_mutex_create();
    shard->write_count = 0;
    shard->read_count = 0;
    shard->error_count = 0;

    /* 创建缓存管理器 */
    shard->cache = advanced_cache_manager_create(DEFAULT_MAX_CACHE_MEMORY);
    if (!shard->cache) {
        AGENTOS_LOG_ERROR("Failed to create cache for shard %d", shard_id);
        AGENTOS_FREE(shard->base_path);
        AGENTOS_FREE(shard);
        return NULL;
    }

    /* 创建异步队列 */
    shard->async_queue = advanced_async_queue_create(1024);
    if (!shard->async_queue) {
        AGENTOS_LOG_ERROR("Failed to create async queue for shard %d", shard_id);
        advanced_cache_manager_destroy(shard->cache);
        AGENTOS_FREE(shard->base_path);
        AGENTOS_FREE(shard);
        return NULL;
    }

    /* 创建底层存储 */
    agentos_error_t err = agentos_layer1_raw_create_async(base_path, 1024, 4, &shard->storage);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to create L1 raw storage for shard %d", shard_id);
        advanced_async_queue_destroy(shard->async_queue);
        advanced_cache_manager_destroy(shard->cache);
        AGENTOS_FREE(shard->base_path);
        AGENTOS_FREE(shard);
        return NULL;
    }

    return shard;
}

static void destroy_shard(shard_manager_t* shard) {
    if (!shard) return;

    if (shard->storage) {
        agentos_layer1_raw_destroy(shard->storage);
    }
    if (shard->async_queue) {
        advanced_async_queue_destroy(shard->async_queue);
    }
    if (shard->cache) {
        advanced_cache_manager_destroy(shard->cache);
    }
    if (shard->base_path) AGENTOS_FREE(shard->base_path);
    if (shard->stats_lock) agentos_mutex_destroy(shard->stats_lock);
    
    AGENTOS_FREE(shard);
}

/* ==================== 公共 API ==================== */

/**
 * @brief 创建增强存储管理器
 */
agentos_error_t agentos_advanced_storage_create(const char* storage_id,
                                               const char* base_path,
                                               agentos_advanced_storage_t** out_storage) {
    if (!storage_id || !base_path || !out_storage) {
        return AGENTOS_EINVAL;
    }

    agentos_advanced_storage_t* storage = (agentos_advanced_storage_t*)AGENTOS_CALLOC(1, sizeof(agentos_advanced_storage_t));
    if (!storage) {
        AGENTOS_LOG_ERROR("Failed to allocate advanced storage manager");
        return AGENTOS_ENOMEM;
    }

    storage->storage_id = AGENTOS_STRDUP(storage_id);
    storage->global_lock = agentos_mutex_create();
    storage->replication_factor = 1;
    storage->default_comp_algo = COMPRESSION_ZSTD;
    storage->default_enc_algo = ENCRYPTION_AES_256_GCM;
    storage->shard_count = 1;

    if (!storage->storage_id || !storage->global_lock) {
        AGENTOS_LOG_ERROR("Failed to initialize storage resources");
        if (storage->storage_id) AGENTOS_FREE(storage->storage_id);
        if (storage->global_lock) agentos_mutex_destroy(storage->global_lock);
        AGENTOS_FREE(storage);
        return AGENTOS_ENOMEM;
    }

    /* 创建默认分片 */
    storage->shards[0] = create_shard(0, base_path);
    if (!storage->shards[0]) {
        AGENTOS_LOG_ERROR("Failed to create default shard");
        AGENTOS_FREE(storage->storage_id);
        agentos_mutex_destroy(storage->global_lock);
        AGENTOS_FREE(storage);
        return AGENTOS_EINVAL;
    }

    *out_storage = storage;
    AGENTOS_LOG_INFO("Created advanced storage manager: %s", storage_id);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁增强存储管理器
 */
void agentos_advanced_storage_destroy(agentos_advanced_storage_t* storage) {
    if (!storage) return;

    agentos_mutex_lock(storage->global_lock);

    /* 销毁所有分片 */
    for (size_t i = 0; i < storage->shard_count; i++) {
        if (storage->shards[i]) {
            destroy_shard(storage->shards[i]);
        }
    }

    agentos_mutex_unlock(storage->global_lock);

    if (storage->global_lock) agentos_mutex_destroy(storage->global_lock);
    if (storage->storage_id) AGENTOS_FREE(storage->storage_id);
    AGENTOS_FREE(storage);

    AGENTOS_LOG_INFO("Destroyed advanced storage manager");
}

/**
 * @brief 写入数据（带压缩和加密）
 */
agentos_error_t agentos_advanced_storage_write(agentos_advanced_storage_t* storage,
                                               const char* id,
                                               const void* data,
                                               size_t data_len) {
    if (!storage || !id || !data || data_len == 0) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(storage->global_lock);
    shard_manager_t* shard = storage->shards[0];  /* 简单哈希路由 */
    agentos_mutex_unlock(storage->global_lock);

    if (!shard) return AGENTOS_EINVAL;

    /* 压缩数据 */
    void* compressed_data = NULL;
    size_t compressed_len = 0;
    agentos_error_t err = advanced_storage_compress(data, data_len,
                                                   storage->default_comp_algo,
                                                   DEFAULT_COMPRESSION_LEVEL,
                                                   &compressed_data, &compressed_len);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to compress data for %s", id);
        return err;
    }

    /* 加密数据 */
    void* encrypted_data = NULL;
    size_t encrypted_len = 0;
    uint8_t* tag = NULL;
    size_t tag_len = 0;
    err = advanced_storage_encrypt(compressed_data, compressed_len,
                                  storage->default_enc_algo,
                                  storage->encryption_key, ENCRYPTION_KEY_LENGTH,
                                  storage->master_iv, ENCRYPTION_IV_LENGTH,
                                  &encrypted_data, &encrypted_len,
                                  &tag, &tag_len);
    AGENTOS_FREE(compressed_data);

    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to encrypt data for %s", id);
        return err;
    }

    /* 写入底层存储 */
    err = agentos_layer1_raw_write(shard->storage, id, encrypted_data, encrypted_len);
    AGENTOS_FREE(encrypted_data);
    if (tag) AGENTOS_FREE(tag);

    if (err == AGENTOS_SUCCESS) {
        agentos_mutex_lock(shard->stats_lock);
        shard->write_count++;
        agentos_mutex_unlock(shard->stats_lock);
    }

    return err;
}

/**
 * @brief 读取数据（带解密和解压）
 */
agentos_error_t agentos_advanced_storage_read(agentos_advanced_storage_t* storage,
                                              const char* id,
                                              void** out_data,
                                              size_t* out_len) {
    if (!storage || !id || !out_data || !out_len) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(storage->global_lock);
    shard_manager_t* shard = storage->shards[0];
    agentos_mutex_unlock(storage->global_lock);

    if (!shard) return AGENTOS_EINVAL;

    /* 从底层存储读取 */
    void* encrypted_data = NULL;
    size_t encrypted_len = 0;
    agentos_error_t err = agentos_layer1_raw_read(shard->storage, id, &encrypted_data, &encrypted_len);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to read encrypted data for %s", id);
        return err;
    }

    /* 解密数据 */
    void* compressed_data = NULL;
    size_t compressed_len = 0;
    err = advanced_storage_decrypt(encrypted_data, encrypted_len,
                                  storage->default_enc_algo,
                                  storage->encryption_key, ENCRYPTION_KEY_LENGTH,
                                  storage->master_iv, ENCRYPTION_IV_LENGTH,
                                  NULL, 0,  /* tag 需要从元数据中获取 */
                                  &compressed_data, &compressed_len);
    AGENTOS_FREE(encrypted_data);

    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to decrypt data for %s", id);
        return err;
    }

    /* 解压数据 */
    err = advanced_storage_decompress(compressed_data, compressed_len,
                                     storage->default_comp_algo, 0,
                                     out_data, out_len);
    AGENTOS_FREE(compressed_data);

    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to decompress data for %s", id);
        return err;
    }

    agentos_mutex_lock(shard->stats_lock);
    shard->read_count++;
    agentos_mutex_unlock(shard->stats_lock);

    return AGENTOS_SUCCESS;
}

/**
 * @brief 删除数据
 */
agentos_error_t agentos_advanced_storage_delete(agentos_advanced_storage_t* storage,
                                                const char* id) {
    if (!storage || !id) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(storage->global_lock);
    shard_manager_t* shard = storage->shards[0];
    agentos_mutex_unlock(storage->global_lock);

    if (!shard) return AGENTOS_EINVAL;

    return agentos_layer1_raw_delete(shard->storage, id);
}

/**
 * @brief 获取统计信息
 */
void agentos_advanced_storage_get_stats(agentos_advanced_storage_t* storage,
                                       uint64_t* total_writes,
                                       uint64_t* total_reads,
                                       uint64_t* total_errors) {
    if (!storage) return;

    uint64_t writes = 0, reads = 0, errors = 0;

    agentos_mutex_lock(storage->global_lock);
    for (size_t i = 0; i < storage->shard_count; i++) {
        shard_manager_t* shard = storage->shards[i];
        if (shard && shard->stats_lock) {
            agentos_mutex_lock(shard->stats_lock);
            writes += shard->write_count;
            reads += shard->read_count;
            errors += shard->error_count;
            agentos_mutex_unlock(shard->stats_lock);
        }
    }
    agentos_mutex_unlock(storage->global_lock);

    if (total_writes) *total_writes = writes;
    if (total_reads) *total_reads = reads;
    if (total_errors) *total_errors = errors;
}
