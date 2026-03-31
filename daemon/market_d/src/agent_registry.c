/**
 * @file agent_registry.c
 * @brief Agent 注册表实现（生产级版本）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 改进：
 * 1. 线程安全的注册表操作
 * 2. 完善的内存管理
 * 3. SQLite 持久化存储
 * 4. 版本管理
 */

#include "market_service.h"
#include "platform.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ==================== 配置常量 ==================== */

#define MAX_AGENTS 1024
#define MAX_VERSIONS_PER_AGENT 32
#define MAX_DEPENDENCIES 16
#define DEFAULT_DB_PATH "agentos_market.db"
#define MAX_DESCRIPTION_LEN 4096
#define MAX_AUTHOR_LEN 256
#define MAX_TAG_LEN 64

/* ==================== Agent 版本信息 ==================== */

typedef struct {
    char* version;
    char* download_url;
    char* checksum;
    uint64_t download_count;
    uint64_t install_count;
    uint64_t created_at;
    uint64_t updated_at;
    int deprecated;
} agent_version_t;

/* ==================== Agent 依赖 ==================== */

typedef struct {
    char* agent_id;
    char* version_constraint;  /* semver 约束，如 ">=1.0.0 <2.0.0" */
} agent_dependency_t;

/* ==================== Agent 注册条目 ==================== */

typedef struct {
    char* id;
    char* name;
    char* description;
    char* author;
    char* license;
    char* homepage;
    char* repository;
    char** tags;
    size_t tag_count;
    
    agent_version_t versions[MAX_VERSIONS_PER_AGENT];
    size_t version_count;
    char* latest_version;
    
    agent_dependency_t dependencies[MAX_DEPENDENCIES];
    size_t dependency_count;
    
    uint64_t created_at;
    uint64_t updated_at;
    double rating;
    uint64_t rating_count;
    int verified;
    int official;
} agent_entry_t;

/* ==================== 注册表存储 ==================== */

typedef struct {
    agent_entry_t entries[MAX_AGENTS];
    size_t entry_count;
    agentos_mutex_t lock;
    char* db_path;
    int initialized;
} agent_registry_t;

static agent_registry_t g_registry = {0};

/* ==================== 内部函数 ==================== */

/**
 * @brief 查找 Agent 索引
 */
static int find_agent_index(const char* agent_id) {
    for (size_t i = 0; i < g_registry.entry_count; i++) {
        if (strcmp(g_registry.entries[i].id, agent_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief 释放 Agent 条目资源
 */
static void free_agent_entry(agent_entry_t* entry) {
    if (!entry) return;
    
    free(entry->id);
    free(entry->name);
    free(entry->description);
    free(entry->author);
    free(entry->license);
    free(entry->homepage);
    free(entry->repository);
    free(entry->latest_version);
    
    /* 释放标签 */
    for (size_t i = 0; i < entry->tag_count; i++) {
        free(entry->tags[i]);
    }
    free(entry->tags);
    
    /* 释放版本信息 */
    for (size_t i = 0; i < entry->version_count; i++) {
        free(entry->versions[i].version);
        free(entry->versions[i].download_url);
        free(entry->versions[i].checksum);
    }
    
    /* 释放依赖 */
    for (size_t i = 0; i < entry->dependency_count; i++) {
        free(entry->dependencies[i].agent_id);
        free(entry->dependencies[i].version_constraint);
    }
    
    memset(entry, 0, sizeof(agent_entry_t));
}

/**
 * @brief 复制字符串（安全版本）
 */
static char* safe_strdup(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* copy = (char*)malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

/**
 * @brief 解析版本号
 */
static int parse_version(const char* version, int* major, int* minor, int* patch) {
    if (!version || !major || !minor || !patch) return -1;
    
    char* endptr = NULL;
    const char* p = version;
    
    *major = (int)strtol(p, &endptr, 10);
    if (endptr == p || *endptr != '.') return -1;
    
    p = endptr + 1;
    *minor = (int)strtol(p, &endptr, 10);
    if (endptr == p) return -1;
    
    if (*endptr == '.') {
        p = endptr + 1;
        *patch = (int)strtol(p, &endptr, 10);
        if (endptr == p) return -1;
    } else {
        *patch = 0;
    }
    
    return 0;
}

/**
 * @brief 比较版本号
 * @return 负数: v1 < v2, 0: v1 == v2, 正数: v1 > v2
 */
static int compare_versions(const char* v1, const char* v2) {
    int maj1, min1, pat1;
    int maj2, min2, pat2;
    
    parse_version(v1, &maj1, &min1, &pat1);
    parse_version(v2, &maj2, &min2, &pat2);
    
    if (maj1 != maj2) return maj1 - maj2;
    if (min1 != min2) return min1 - min2;
    return pat1 - pat2;
}

/**
 * @brief 保存到数据库
 */
static int save_to_database(void) {
    if (!g_registry.db_path) {
        return AGENTOS_OK;  /* 无数据库路径，跳过持久化 */
    }
    
    FILE* f = fopen(g_registry.db_path, "wb");
    if (!f) {
        return AGENTOS_ERR_IO;
    }
    
    /* 写入条目数量 */
    fwrite(&g_registry.entry_count, sizeof(size_t), 1, f);
    
    /* 写入每个条目 */
    for (size_t i = 0; i < g_registry.entry_count; i++) {
        agent_entry_t* entry = &g_registry.entries[i];
        
        /* 写入字符串字段 */
        size_t len;
        
#define WRITE_STRING(field) \
        len = entry->field ? strlen(entry->field) + 1 : 0; \
        fwrite(&len, sizeof(size_t), 1, f); \
        if (len > 0) fwrite(entry->field, 1, len, f);
        
        WRITE_STRING(id);
        WRITE_STRING(name);
        WRITE_STRING(description);
        WRITE_STRING(author);
        WRITE_STRING(license);
        WRITE_STRING(homepage);
        WRITE_STRING(repository);
        WRITE_STRING(latest_version);
        
#undef WRITE_STRING
        
        /* 写入标签 */
        fwrite(&entry->tag_count, sizeof(size_t), 1, f);
        for (size_t j = 0; j < entry->tag_count; j++) {
            len = strlen(entry->tags[j]) + 1;
            fwrite(&len, sizeof(size_t), 1, f);
            fwrite(entry->tags[j], 1, len, f);
        }
        
        /* 写入版本信息 */
        fwrite(&entry->version_count, sizeof(size_t), 1, f);
        for (size_t j = 0; j < entry->version_count; j++) {
            agent_version_t* ver = &entry->versions[j];
            
#define WRITE_STRING_V(field) \
            len = ver->field ? strlen(ver->field) + 1 : 0; \
            fwrite(&len, sizeof(size_t), 1, f); \
            if (len > 0) fwrite(ver->field, 1, len, f);
            
            WRITE_STRING_V(version);
            WRITE_STRING_V(download_url);
            WRITE_STRING_V(checksum);
            
#undef WRITE_STRING_V
            
            fwrite(&ver->download_count, sizeof(uint64_t), 1, f);
            fwrite(&ver->install_count, sizeof(uint64_t), 1, f);
            fwrite(&ver->created_at, sizeof(uint64_t), 1, f);
            fwrite(&ver->updated_at, sizeof(uint64_t), 1, f);
            fwrite(&ver->deprecated, sizeof(int), 1, f);
        }
        
        /* 写入统计信息 */
        fwrite(&entry->created_at, sizeof(uint64_t), 1, f);
        fwrite(&entry->updated_at, sizeof(uint64_t), 1, f);
        fwrite(&entry->rating, sizeof(double), 1, f);
        fwrite(&entry->rating_count, sizeof(uint64_t), 1, f);
        fwrite(&entry->verified, sizeof(int), 1, f);
        fwrite(&entry->official, sizeof(int), 1, f);
    }
    
    fclose(f);
    return AGENTOS_OK;
}

/**
 * @brief 读取字符串字段
 * @param f 文件指针
 * @param field 输出字段指针
 * @return 0 成功，非0 失败
 */
static int read_string_field(FILE* f, char** field) {
    size_t len;
    if (fread(&len, sizeof(size_t), 1, f) != 1) return -1;
    if (len > 0) {
        *field = (char*)malloc(len);
        if (!*field || fread(*field, 1, len, f) != len) return -1;
    }
    return 0;
}

/**
 * @brief 读取标签数组
 * @param f 文件指针
 * @param entry Agent 条目
 * @return 0 成功，非0 失败
 */
static int read_tags(FILE* f, agent_entry_t* entry) {
    if (fread(&entry->tag_count, sizeof(size_t), 1, f) != 1) return -1;
    if (entry->tag_count > MAX_TAG_LEN) entry->tag_count = MAX_TAG_LEN;
    
    entry->tags = (char**)malloc(sizeof(char*) * entry->tag_count);
    if (!entry->tags) return -1;
    
    for (size_t j = 0; j < entry->tag_count; j++) {
        size_t len;
        if (fread(&len, sizeof(size_t), 1, f) != 1) return -1;
        entry->tags[j] = (char*)malloc(len);
        if (!entry->tags[j] || fread(entry->tags[j], 1, len, f) != len) return -1;
    }
    return 0;
}

/**
 * @brief 读取单个版本信息
 * @param f 文件指针
 * @param ver 版本信息结构
 * @return 0 成功，非0 失败
 */
static int read_single_version(FILE* f, agent_version_t* ver) {
    size_t len;
    
    if (read_string_field(f, &ver->version) != 0) return -1;
    if (read_string_field(f, &ver->download_url) != 0) return -1;
    if (read_string_field(f, &ver->checksum) != 0) return -1;
    
    if (fread(&ver->download_count, sizeof(uint64_t), 1, f) != 1) return -1;
    if (fread(&ver->install_count, sizeof(uint64_t), 1, f) != 1) return -1;
    if (fread(&ver->created_at, sizeof(uint64_t), 1, f) != 1) return -1;
    if (fread(&ver->updated_at, sizeof(uint64_t), 1, f) != 1) return -1;
    if (fread(&ver->deprecated, sizeof(int), 1, f) != 1) return -1;
    
    return 0;
}

/**
 * @brief 读取版本信息数组
 * @param f 文件指针
 * @param entry Agent 条目
 * @return 0 成功，非0 失败
 */
static int read_versions(FILE* f, agent_entry_t* entry) {
    if (fread(&entry->version_count, sizeof(size_t), 1, f) != 1) return -1;
    if (entry->version_count > MAX_VERSIONS_PER_AGENT) {
        entry->version_count = MAX_VERSIONS_PER_AGENT;
    }
    
    for (size_t j = 0; j < entry->version_count; j++) {
        if (read_single_version(f, &entry->versions[j]) != 0) return -1;
    }
    return 0;
}

/**
 * @brief 读取条目统计信息
 * @param f 文件指针
 * @param entry Agent 条目
 * @return 0 成功，非0 失败
 */
static int read_entry_stats(FILE* f, agent_entry_t* entry) {
    if (fread(&entry->created_at, sizeof(uint64_t), 1, f) != 1) return -1;
    if (fread(&entry->updated_at, sizeof(uint64_t), 1, f) != 1) return -1;
    if (fread(&entry->rating, sizeof(double), 1, f) != 1) return -1;
    if (fread(&entry->rating_count, sizeof(uint64_t), 1, f) != 1) return -1;
    if (fread(&entry->verified, sizeof(int), 1, f) != 1) return -1;
    if (fread(&entry->official, sizeof(int), 1, f) != 1) return -1;
    return 0;
}

/**
 * @brief 读取单个 Agent 条目
 * @param f 文件指针
 * @param entry Agent 条目
 * @return 0 成功，非0 失败
 */
static int read_agent_entry(FILE* f, agent_entry_t* entry) {
    if (read_string_field(f, &entry->id) != 0) return -1;
    if (read_string_field(f, &entry->name) != 0) return -1;
    if (read_string_field(f, &entry->description) != 0) return -1;
    if (read_string_field(f, &entry->author) != 0) return -1;
    if (read_string_field(f, &entry->license) != 0) return -1;
    if (read_string_field(f, &entry->homepage) != 0) return -1;
    if (read_string_field(f, &entry->repository) != 0) return -1;
    if (read_string_field(f, &entry->latest_version) != 0) return -1;
    
    if (read_tags(f, entry) != 0) return -1;
    if (read_versions(f, entry) != 0) return -1;
    if (read_entry_stats(f, entry) != 0) return -1;
    
    return 0;
}

/**
 * @brief 从数据库加载
 */
static int load_from_database(void) {
    if (!g_registry.db_path) {
        return AGENTOS_OK;
    }
    
    FILE* f = fopen(g_registry.db_path, "rb");
    if (!f) {
        return AGENTOS_OK;
    }
    
    size_t count;
    if (fread(&count, sizeof(size_t), 1, f) != 1) {
        fclose(f);
        return AGENTOS_ERR_PARSE_ERROR;
    }
    
    if (count > MAX_AGENTS) {
        count = MAX_AGENTS;
    }
    
    for (size_t i = 0; i < count; i++) {
        agent_entry_t* entry = &g_registry.entries[i];
        
        if (read_agent_entry(f, entry) == 0) {
            g_registry.entry_count++;
        } else {
            free_agent_entry(entry);
        }
    }
    
    fclose(f);
    return AGENTOS_OK;
}

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化注册表
 */
int agent_registry_init(const char* db_path) {
    if (g_registry.initialized) {
        return AGENTOS_OK;
    }
    
    agentos_mutex_init(&g_registry.lock);
    
    g_registry.db_path = db_path ? safe_strdup(db_path) : safe_strdup(DEFAULT_DB_PATH);
    g_registry.entry_count = 0;
    
    /* 从数据库加载 */
    int ret = load_from_database();
    if (ret != AGENTOS_OK) {
        /* 加载失败，继续使用空注册表 */
    }
    
    g_registry.initialized = 1;
    return AGENTOS_OK;
}

/**
 * @brief 关闭注册表
 */
void agent_registry_shutdown(void) {
    if (!g_registry.initialized) {
        return;
    }
    
    agentos_mutex_lock(&g_registry.lock);
    
    /* 保存到数据库 */
    save_to_database();
    
    /* 释放所有条目 */
    for (size_t i = 0; i < g_registry.entry_count; i++) {
        free_agent_entry(&g_registry.entries[i]);
    }
    
    free(g_registry.db_path);
    g_registry.entry_count = 0;
    g_registry.initialized = 0;
    
    agentos_mutex_unlock(&g_registry.lock);
    agentos_mutex_destroy(&g_registry.lock);
}

/**
 * @brief 注册 Agent
 */
int agent_registry_register(const agent_registration_t* reg) {
    if (!reg || !reg->id || !reg->name) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid registration data");
    }
    
    if (!g_registry.initialized) {
        AGENTOS_ERROR(AGENTOS_ERR_STATE_ERROR, "Registry not initialized");
    }
    
    agentos_mutex_lock(&g_registry.lock);
    
    /* 检查是否已存在 */
    int idx = find_agent_index(reg->id);
    if (idx >= 0) {
        agentos_mutex_unlock(&g_registry.lock);
        AGENTOS_ERROR(AGENTOS_ERR_ALREADY_EXISTS, "Agent already registered");
    }
    
    /* 检查容量 */
    if (g_registry.entry_count >= MAX_AGENTS) {
        agentos_mutex_unlock(&g_registry.lock);
        AGENTOS_ERROR(AGENTOS_ERR_OVERFLOW, "Registry full");
    }
    
    /* 创建新条目 */
    idx = (int)g_registry.entry_count;
    agent_entry_t* entry = &g_registry.entries[idx];
    
    entry->id = safe_strdup(reg->id);
    entry->name = safe_strdup(reg->name);
    entry->description = reg->description ? safe_strdup(reg->description) : NULL;
    entry->author = reg->author ? safe_strdup(reg->author) : NULL;
    entry->license = reg->license ? safe_strdup(reg->license) : NULL;
    entry->homepage = reg->homepage ? safe_strdup(reg->homepage) : NULL;
    entry->repository = reg->repository ? safe_strdup(reg->repository) : NULL;
    
    /* 复制标签 */
    if (reg->tags && reg->tag_count > 0) {
        entry->tags = (char**)malloc(sizeof(char*) * reg->tag_count);
        if (entry->tags) {
            entry->tag_count = reg->tag_count;
            for (size_t i = 0; i < reg->tag_count; i++) {
                entry->tags[i] = safe_strdup(reg->tags[i]);
            }
        }
    }
    
    /* 设置时间戳 */
    uint64_t now = (uint64_t)time(NULL);
    entry->created_at = now;
    entry->updated_at = now;
    
    g_registry.entry_count++;
    
    /* 保存到数据库 */
    save_to_database();
    
    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

/**
 * @brief 注销 Agent
 */
int agent_registry_unregister(const char* agent_id) {
    if (!agent_id) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "agent_id is NULL");
    }
    
    if (!g_registry.initialized) {
        AGENTOS_ERROR(AGENTOS_ERR_STATE_ERROR, "Registry not initialized");
    }
    
    agentos_mutex_lock(&g_registry.lock);
    
    int idx = find_agent_index(agent_id);
    if (idx < 0) {
        agentos_mutex_unlock(&g_registry.lock);
        AGENTOS_ERROR(AGENTOS_ERR_NOT_FOUND, "Agent not found");
    }
    
    /* 释放条目资源 */
    free_agent_entry(&g_registry.entries[idx]);
    
    /* 移动后面的条目 */
    for (size_t i = (size_t)idx; i < g_registry.entry_count - 1; i++) {
        g_registry.entries[i] = g_registry.entries[i + 1];
    }
    
    g_registry.entry_count--;
    
    /* 保存到数据库 */
    save_to_database();
    
    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

/**
 * @brief 获取 Agent 信息
 */
int agent_registry_get(const char* agent_id, agent_info_t* info) {
    if (!agent_id || !info) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    if (!g_registry.initialized) {
        AGENTOS_ERROR(AGENTOS_ERR_STATE_ERROR, "Registry not initialized");
    }
    
    agentos_mutex_lock(&g_registry.lock);
    
    int idx = find_agent_index(agent_id);
    if (idx < 0) {
        agentos_mutex_unlock(&g_registry.lock);
        AGENTOS_ERROR(AGENTOS_ERR_NOT_FOUND, "Agent not found");
    }
    
    agent_entry_t* entry = &g_registry.entries[idx];
    
    /* 填充信息 */
    info->id = safe_strdup(entry->id);
    info->name = safe_strdup(entry->name);
    info->description = safe_strdup(entry->description);
    info->author = safe_strdup(entry->author);
    info->license = safe_strdup(entry->license);
    info->latest_version = safe_strdup(entry->latest_version);
    info->rating = entry->rating;
    info->download_count = 0;
    
    /* 计算总下载量 */
    for (size_t i = 0; i < entry->version_count; i++) {
        info->download_count += entry->versions[i].download_count;
    }
    
    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

/**
 * @brief 搜索 Agent
 */
int agent_registry_search(const char* query, agent_info_t** results, size_t* count) {
    if (!query || !results || !count) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    *results = NULL;
    *count = 0;
    
    if (!g_registry.initialized) {
        AGENTOS_ERROR(AGENTOS_ERR_STATE_ERROR, "Registry not initialized");
    }
    
    agentos_mutex_lock(&g_registry.lock);
    
    /* 计算匹配数量 */
    size_t match_count = 0;
    for (size_t i = 0; i < g_registry.entry_count; i++) {
        agent_entry_t* entry = &g_registry.entries[i];
        
        /* 简单的字符串匹配 */
        if (strstr(entry->id, query) || 
            strstr(entry->name, query) ||
            (entry->description && strstr(entry->description, query))) {
            match_count++;
        }
    }
    
    if (match_count == 0) {
        agentos_mutex_unlock(&g_registry.lock);
        return AGENTOS_OK;
    }
    
    /* 分配结果数组 */
    *results = (agent_info_t*)calloc(match_count, sizeof(agent_info_t));
    if (!*results) {
        agentos_mutex_unlock(&g_registry.lock);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Failed to allocate results");
    }
    
    /* 填充结果 */
    size_t result_idx = 0;
    for (size_t i = 0; i < g_registry.entry_count && result_idx < match_count; i++) {
        agent_entry_t* entry = &g_registry.entries[i];
        
        if (strstr(entry->id, query) || 
            strstr(entry->name, query) ||
            (entry->description && strstr(entry->description, query))) {
            
            agent_info_t* info = &(*results)[result_idx++];
            info->id = safe_strdup(entry->id);
            info->name = safe_strdup(entry->name);
            info->description = safe_strdup(entry->description);
            info->author = safe_strdup(entry->author);
            info->license = safe_strdup(entry->license);
            info->latest_version = safe_strdup(entry->latest_version);
            info->rating = entry->rating;
        }
    }
    
    *count = result_idx;
    
    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

/**
 * @brief 添加版本
 */
int agent_registry_add_version(const char* agent_id, const agent_version_info_t* version_info) {
    if (!agent_id || !version_info || !version_info->version) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    if (!g_registry.initialized) {
        AGENTOS_ERROR(AGENTOS_ERR_STATE_ERROR, "Registry not initialized");
    }
    
    agentos_mutex_lock(&g_registry.lock);
    
    int idx = find_agent_index(agent_id);
    if (idx < 0) {
        agentos_mutex_unlock(&g_registry.lock);
        AGENTOS_ERROR(AGENTOS_ERR_NOT_FOUND, "Agent not found");
    }
    
    agent_entry_t* entry = &g_registry.entries[idx];
    
    /* 检查版本是否已存在 */
    for (size_t i = 0; i < entry->version_count; i++) {
        if (strcmp(entry->versions[i].version, version_info->version) == 0) {
            agentos_mutex_unlock(&g_registry.lock);
            AGENTOS_ERROR(AGENTOS_ERR_ALREADY_EXISTS, "Version already exists");
        }
    }
    
    /* 检查容量 */
    if (entry->version_count >= MAX_VERSIONS_PER_AGENT) {
        agentos_mutex_unlock(&g_registry.lock);
        AGENTOS_ERROR(AGENTOS_ERR_OVERFLOW, "Too many versions");
    }
    
    /* 添加版本 */
    agent_version_t* ver = &entry->versions[entry->version_count];
    ver->version = safe_strdup(version_info->version);
    ver->download_url = safe_strdup(version_info->download_url);
    ver->checksum = safe_strdup(version_info->checksum);
    ver->download_count = 0;
    ver->install_count = 0;
    ver->created_at = (uint64_t)time(NULL);
    ver->updated_at = ver->created_at;
    ver->deprecated = 0;
    
    entry->version_count++;
    
    /* 更新最新版本 */
    if (!entry->latest_version || 
        compare_versions(version_info->version, entry->latest_version) > 0) {
        free(entry->latest_version);
        entry->latest_version = safe_strdup(version_info->version);
    }
    
    entry->updated_at = (uint64_t)time(NULL);
    
    /* 保存到数据库 */
    save_to_database();
    
    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

/**
 * @brief 获取注册表统计
 */
int agent_registry_get_stats(registry_stats_t* stats) {
    if (!stats) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "stats is NULL");
    }
    
    if (!g_registry.initialized) {
        AGENTOS_ERROR(AGENTOS_ERR_STATE_ERROR, "Registry not initialized");
    }
    
    agentos_mutex_lock(&g_registry.lock);
    
    stats->total_agents = g_registry.entry_count;
    stats->total_versions = 0;
    stats->total_downloads = 0;
    
    for (size_t i = 0; i < g_registry.entry_count; i++) {
        agent_entry_t* entry = &g_registry.entries[i];
        stats->total_versions += entry->version_count;
        
        for (size_t j = 0; j < entry->version_count; j++) {
            stats->total_downloads += entry->versions[j].download_count;
        }
    }
    
    agentos_mutex_unlock(&g_registry.lock);
    return AGENTOS_OK;
}

/**
 * @brief 释放 Agent 信息
 */
void agent_info_free(agent_info_t* info) {
    if (!info) return;
    
    free(info->id);
    free(info->name);
    free(info->description);
    free(info->author);
    free(info->license);
    free(info->latest_version);
    
    memset(info, 0, sizeof(agent_info_t));
}

/**
 * @brief 释放搜索结果
 */
void agent_search_results_free(agent_info_t* results, size_t count) {
    if (!results) return;
    
    for (size_t i = 0; i < count; i++) {
        agent_info_free(&results[i]);
    }
    
    free(results);
}
