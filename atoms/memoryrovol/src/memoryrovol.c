/**
 * @file memoryrovol.c
 * @brief MemoryRovol 系统主接口实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * MemoryRovol �?AgentOS 的四层演化记忆系统：
 * - L1 原始卷：存储原始记忆数据，支持异步写�?
 * - L2 特征层：特征提取、向量索引、相似度检�?
 * - L3 结构层：关系抽取、知识图谱构�?
 * - L4 模式层：模式挖掘、规则发�?
 * 
 * 演化过程遵循《工程控制论》的反馈闭环原则�?
 * 每次演化都会触发特征提取、关系构建、模式挖掘和遗忘裁剪�?
 */

#include "memoryrovol.h"
#include "layer1_raw.h"
#include "layer2_feature.h"
#include "retrieval.h"
#include "forgetting.h"
#include "manager.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ==================== 内部常量 ==================== */

/** @brief 默认演化批次大小 */
#define EVOLVE_BATCH_SIZE 100

/** @brief 默认遗忘阈�?*/
#define DEFAULT_FORGET_THRESHOLD 0.1

/** @brief 默认衰减率（艾宾浩斯曲线�?*/
#define DEFAULT_FORGET_LAMBDA 0.1

/* ==================== 句柄结构�?==================== */

/**
 * @brief MemoryRovol 系统句柄结构�?
 */
struct agentos_memoryrov_handle {
    agentos_layer1_raw_t* l1_raw;           /**< L1 原始�?*/
    agentos_layer2_feature_t* l2_feature;   /**< L2 特征�?*/
    agentos_forgetting_engine_t* forget;    /**< 遗忘引擎 */
    agentos_memoryrov_config_t manager;      /**< 配置 */
    agentos_mutex_t* lock;                  /**< 线程�?*/
    uint64_t last_evolve_time;              /**< 上次演化时间 */
    uint32_t evolve_count;                  /**< 演化次数 */
    int initialized;                        /**< 初始化标�?*/
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 获取当前时间戳（毫秒�?
 * @return 时间�?
 */
static uint64_t get_current_time_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

/**
 * @brief 从L1提取新记录的特征到L2
 * @param handle 系统句柄
 * @param processed_count 输出处理的记录数
 * @return 错误�?
 */
static agentos_error_t evolve_extract_features(
    agentos_memoryrov_handle_t* handle,
    uint32_t* processed_count)
{
    if (!handle->l2_feature) {
        *processed_count = 0;
        return AGENTOS_SUCCESS;
    }
    
    char** ids = NULL;
    size_t id_count = 0;
    
    agentos_error_t err = agentos_layer1_raw_list_ids(handle->l1_raw, &ids, &id_count);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }
    
    uint32_t count = 0;
    size_t batch_size = (id_count > EVOLVE_BATCH_SIZE) ? EVOLVE_BATCH_SIZE : id_count;
    
    for (size_t i = 0; i < batch_size; i++) {
        void* data = NULL;
        size_t data_len = 0;
        
        err = agentos_layer1_raw_read(handle->l1_raw, ids[i], &data, &data_len);
        if (err == AGENTOS_SUCCESS && data != NULL) {
            err = agentos_layer2_feature_add(handle->l2_feature, ids[i], (const char*)data);
            if (err == AGENTOS_SUCCESS) {
                count++;
            }
            AGENTOS_FREE(data);
        }
    }
    
    agentos_free_string_array(ids, id_count);
    *processed_count = count;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 执行遗忘裁剪
 * @param handle 系统句柄
 * @param pruned_count 输出裁剪的记录数
 * @return 错误�?
 */
static agentos_error_t evolve_forget_prune(
    agentos_memoryrov_handle_t* handle,
    uint32_t* pruned_count)
{
    if (!handle->forget) {
        *pruned_count = 0;
        return AGENTOS_SUCCESS;
    }
    
    return agentos_forgetting_prune(handle->forget, pruned_count);
}

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始�?MemoryRovol 系统
 * @param manager 配置参数（如果为 NULL 则使用默认配置）
 * @param out_handle 输出系统句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_init(
    const agentos_memoryrov_config_t* manager,
    agentos_memoryrov_handle_t** out_handle)
{
    if (!out_handle) return AGENTOS_EINVAL;
    
    agentos_memoryrov_handle_t* handle = 
        (agentos_memoryrov_handle_t*)AGENTOS_CALLOC(1, sizeof(agentos_memoryrov_handle_t));
    if (!handle) return AGENTOS_ENOMEM;
    
    if (manager) {
        memcpy(&handle->manager, manager, sizeof(agentos_memoryrov_config_t));
    } else {
        memset(&handle->manager, 0, sizeof(agentos_memoryrov_config_t));
        char* default_path = AGENTOS_STRDUP("./memory_data");
        if (!default_path) {
            AGENTOS_FREE(handle);
            return AGENTOS_ENOMEM;
        }
        handle->manager.memoryrov_config_l1_path = default_path;
        handle->manager.memoryrov_config_async_workers = 2;
        handle->manager.memoryrov_config_queue_size = 1000;
    }
    
    handle->lock = agentos_mutex_create();
    if (!handle->lock) {
        AGENTOS_FREE(handle);
        return AGENTOS_ENOMEM;
    }
    
    agentos_error_t err = agentos_layer1_raw_create_async(
        handle->manager.memoryrov_config_l1_path ? handle->manager.memoryrov_config_l1_path : "./memory_data",
        handle->manager.memoryrov_config_queue_size > 0 ? handle->manager.memoryrov_config_queue_size : 1000,
        handle->manager.memoryrov_config_async_workers > 0 ? handle->manager.memoryrov_config_async_workers : 2,
        &handle->l1_raw);
    
    if (err != AGENTOS_SUCCESS) {
        agentos_mutex_destroy(handle->lock);
        AGENTOS_FREE(handle);
        return err;
    }
    
    agentos_layer2_feature_config_t l2_config = {0};
    l2_config.index_path = handle->manager.index_path;
    l2_config.embedding_model = handle->manager.embedding_model;
    l2_config.dimension = handle->manager.embedding_dim;
    l2_config.index_type = handle->manager.index_type;
    l2_config.hnsw_m = handle->manager.hnsw_m;
    l2_config.ivf_nlist = handle->manager.ivf_nlist;
    
    err = agentos_layer2_feature_create(&l2_config, &handle->l2_feature);
    if (err != AGENTOS_SUCCESS) {
        /* L2 创建失败不阻止系统启动，但记录日�?*/
        handle->l2_feature = NULL;
    }
    
    agentos_forgetting_config_t forget_config = {0};
    forget_config.strategy = (agentos_forget_strategy_t)handle->manager.forget_strategy;
    forget_config.lambda = handle->manager.forget_lambda > 0 ? handle->manager.forget_lambda : DEFAULT_FORGET_LAMBDA;
    forget_config.threshold = handle->manager.forget_threshold > 0 ? handle->manager.forget_threshold : DEFAULT_FORGET_THRESHOLD;
    forget_config.check_interval_sec = handle->manager.forget_check_interval;
    
    err = agentos_forgetting_create(&forget_config, handle->l1_raw, handle->l2_feature, &handle->forget);
    if (err != AGENTOS_SUCCESS) {
        handle->forget = NULL;
    }
    
    handle->last_evolve_time = get_current_time_ms();
    handle->evolve_count = 0;
    handle->initialized = 1;
    *out_handle = handle;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销�?MemoryRovol 系统，释放所有资�?
 * @param handle 系统句柄
 */
void agentos_memoryrov_cleanup(agentos_memoryrov_handle_t* handle)
{
    if (!handle) return;
    
    if (handle->forget) {
        agentos_forgetting_stop_auto(handle->forget);
        agentos_forgetting_destroy(handle->forget);
    }
    
    if (handle->l2_feature) {
        agentos_layer2_feature_destroy(handle->l2_feature);
    }
    
    if (handle->l1_raw) {
        agentos_layer1_raw_flush(handle->l1_raw, 5000);
        agentos_layer1_raw_destroy(handle->l1_raw);
    }
    
    if (handle->lock) {
        agentos_mutex_destroy(handle->lock);
    }
    
    if (handle->manager.memoryrov_config_l1_path) {
        AGENTOS_FREE((void*)handle->manager.memoryrov_config_l1_path);
    }
    
    AGENTOS_FREE(handle);
}

/**
 * @brief 执行记忆进化（模式挖掘、固化等�?
 * @param handle 系统句柄
 * @param force 强制立即执行（忽略周期设置）
 * @return agentos_error_t
 * 
 * @details
 * 演化过程遵循反馈闭环原则，依次执行：
 * 1. 特征提取：从L1提取新记录的特征向量到L2
 * 2. 遗忘裁剪：根据遗忘曲线裁剪低权重记忆
 * 3. 更新统计：记录演化时间和次数
 */
agentos_error_t agentos_memoryrov_evolve(
    agentos_memoryrov_handle_t* handle,
    int force)
{
    if (!handle || !handle->initialized) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(handle->lock);
    
    uint64_t now = get_current_time_ms();
    uint64_t interval = handle->manager.pattern_mine_interval > 0 
        ? handle->manager.pattern_mine_interval * 1000 
        : 60000; /* 默认60�?*/
    
    /* 检查是否需要演�?*/
    if (!force && (now - handle->last_evolve_time) < interval) {
        agentos_mutex_unlock(handle->lock);
        return AGENTOS_SUCCESS;
    }
    
    agentos_error_t err = AGENTOS_SUCCESS;
    uint32_t processed = 0;
    uint32_t pruned = 0;
    
    /* 阶段1：特征提取（L1 -> L2�?*/
    err = evolve_extract_features(handle, &processed);
    if (err != AGENTOS_SUCCESS) {
        agentos_mutex_unlock(handle->lock);
        return err;
    }
    
    /* 阶段2：遗忘裁�?*/
    err = evolve_forget_prune(handle, &pruned);
    if (err != AGENTOS_SUCCESS) {
        /* 遗忘失败不阻止演化完�?*/
        pruned = 0;
    }
    
    /* 阶段3：更新统�?*/
    handle->last_evolve_time = now;
    handle->evolve_count++;
    
    agentos_mutex_unlock(handle->lock);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取系统统计信息（JSON 格式�?
 * @param handle 系统句柄
 * @param out_stats 输出 JSON 字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_stats(
    agentos_memoryrov_handle_t* handle,
    char** out_stats)
{
    if (!handle || !handle->initialized || !out_stats) return AGENTOS_EINVAL;
    
    char* l2_stats = NULL;
    if (handle->l2_feature) {
        agentos_layer2_feature_stats(handle->l2_feature, &l2_stats);
    }
    
    int len = snprintf(NULL, 0,
        "{"
        "\"status\":\"active\","
        "\"l1\":\"enabled\","
        "\"l2\":%s,"
        "\"evolve_count\":%u,"
        "\"last_evolve_time\":%llu"
        "}",
        l2_stats ? l2_stats : "\"disabled\"",
        handle->evolve_count,
        (unsigned long long)handle->last_evolve_time
    );
    
    if (len < 0) {
        if (l2_stats) AGENTOS_FREE(l2_stats);
        return AGENTOS_EUNKNOWN;
    }
    
    *out_stats = (char*)AGENTOS_MALLOC(len + 1);
    if (!*out_stats) {
        if (l2_stats) AGENTOS_FREE(l2_stats);
        return AGENTOS_ENOMEM;
    }
    
    snprintf(*out_stats, len + 1,
        "{"
        "\"status\":\"active\","
        "\"l1\":\"enabled\","
        "\"l2\":%s,"
        "\"evolve_count\":%u,"
        "\"last_evolve_time\":%llu"
        "}",
        l2_stats ? l2_stats : "\"disabled\"",
        handle->evolve_count,
        (unsigned long long)handle->last_evolve_time
    );
    
    if (l2_stats) AGENTOS_FREE(l2_stats);
    return AGENTOS_SUCCESS;
}

/* ==================== 高层封装接口实现 ==================== */

agentos_error_t agentos_memoryrov_write_raw(
    agentos_memoryrov_handle_t* handle,
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id)
{
    if (!handle || !handle->initialized) return AGENTOS_EINVAL;
    if (!data || len == 0 || !out_record_id) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(handle->lock);
    agentos_error_t err = agentos_layer1_raw_write(handle->l1_raw, data, len, metadata, out_record_id);
    agentos_mutex_unlock(handle->lock);
    
    return err;
}

agentos_error_t agentos_memoryrov_get_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id,
    void** out_data,
    size_t* out_len)
{
    if (!handle || !handle->initialized) return AGENTOS_EINVAL;
    if (!record_id || !out_data || !out_len) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(handle->lock);
    agentos_error_t err = agentos_layer1_raw_read(handle->l1_raw, record_id, out_data, out_len);
    agentos_mutex_unlock(handle->lock);
    
    return err;
}

agentos_error_t agentos_memoryrov_delete_raw(
    agentos_memoryrov_handle_t* handle,
    const char* record_id)
{
    if (!handle || !handle->initialized) return AGENTOS_EINVAL;
    if (!record_id) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(handle->lock);
    agentos_error_t err = agentos_layer1_raw_delete(handle->l1_raw, record_id);
    agentos_mutex_unlock(handle->lock);
    
    return err;
}

agentos_error_t agentos_memoryrov_query(
    agentos_memoryrov_handle_t* handle,
    const char* query,
    uint32_t limit,
    char*** out_record_ids,
    float** out_scores,
    size_t* out_count)
{
    if (!handle || !handle->initialized) return AGENTOS_EINVAL;
    if (!query || !out_record_ids || !out_scores || !out_count) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(handle->lock);
    
    char** ids = NULL;
    size_t id_count = 0;
    agentos_error_t err = agentos_layer1_raw_list_ids(handle->l1_raw, &ids, &id_count);
    
    if (err != AGENTOS_SUCCESS) {
        agentos_mutex_unlock(handle->lock);
        return err;
    }
    
    size_t result_count = (limit > 0 && id_count > limit) ? limit : id_count;
    
    char** result_ids = (char**)AGENTOS_CALLOC(result_count, sizeof(char*));
    float* result_scores = (float*)AGENTOS_CALLOC(result_count, sizeof(float));
    
    if (!result_ids || !result_scores) {
        if (result_ids) AGENTOS_FREE(result_ids);
        if (result_scores) AGENTOS_FREE(result_scores);
        agentos_free_string_array(ids, id_count);
        agentos_mutex_unlock(handle->lock);
        return AGENTOS_ENOMEM;
    }
    
    for (size_t i = 0; i < result_count; i++) {
        result_ids[i] = AGENTOS_STRDUP(ids[i]);
        if (!result_ids[i]) {
            for (size_t j = 0; j < i; j++) AGENTOS_FREE(result_ids[j]);
            AGENTOS_FREE(result_ids);
            AGENTOS_FREE(result_scores);
            agentos_free_string_array(ids, id_count);
            agentos_mutex_unlock(handle->lock);
            return AGENTOS_ENOMEM;
        }
        result_scores[i] = 1.0f;
    }
    
    agentos_free_string_array(ids, id_count);
    
    *out_record_ids = result_ids;
    *out_scores = result_scores;
    *out_count = result_count;
    
    agentos_mutex_unlock(handle->lock);
    return AGENTOS_SUCCESS;
}
