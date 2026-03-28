/**
 * @file skill.c
 * @brief 技能相关系统调用实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../bases/utils/memory/include/memory_compat.h"
#include "../../../bases/utils/string/include/string_compat.h"
#include <string.h>

typedef struct skill_entry {
    char* skill_id;
    char* url;
    struct skill_entry* next;
} skill_entry_t;

static skill_entry_t* skill_list = NULL;
static agentos_mutex_t* skill_lock = NULL;

/**
 * @brief 线程安全地确保技能锁已初始化
 */
static void ensure_skill_lock(void) {
    /* 使用内存顺序 acquire 读取当前�?*/
    agentos_mutex_t* current = __atomic_load_n(&skill_lock, __ATOMIC_ACQUIRE);
    if (!current) {
        agentos_mutex_t* new_lock = agentos_mutex_create();
        if (!new_lock) return;
        
        /* 原子比较交换，使�?acquire-release 内存顺序 */
        agentos_mutex_t* expected = NULL;
        if (!__atomic_compare_exchange_n(&skill_lock, &expected, new_lock,
                                          false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            /* 其他线程已经设置了锁，销毁我们创建的�?*/
            agentos_mutex_destroy(new_lock);
        }
    }
}

/**
 * @brief 安装技�?
 */
agentos_error_t agentos_sys_skill_install(const char* skill_url, char** out_skill_id) {
    if (!skill_url || !out_skill_id) return AGENTOS_EINVAL;
    ensure_skill_lock();

    char id_buf[64];
    static int counter = 0;
    snprintf(id_buf, sizeof(id_buf), "skill_%d", __sync_fetch_and_add(&counter, 1));

    skill_entry_t* entry = (skill_entry_t*)AGENTOS_CALLOC(1, sizeof(skill_entry_t));
    if (!entry) return AGENTOS_ENOMEM;

    entry->skill_id = AGENTOS_STRDUP(id_buf);
    entry->url = AGENTOS_STRDUP(skill_url);
    if (!entry->skill_id || !entry->url) {
        if (entry->skill_id) AGENTOS_FREE(entry->skill_id);
        if (entry->url) AGENTOS_FREE(entry->url);
        AGENTOS_FREE(entry);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(__atomic_load_n(&skill_lock, __ATOMIC_ACQUIRE));
    entry->next = skill_list;
    skill_list = entry;
    agentos_mutex_unlock(skill_lock);

    *out_skill_id = AGENTOS_STRDUP(entry->skill_id);
    if (!*out_skill_id) {
        agentos_mutex_lock(__atomic_load_n(&skill_lock, __ATOMIC_ACQUIRE));
        skill_entry_t** pp = &skill_list;
        while (*pp) {
            if (*pp == entry) { *pp = entry->next; break; }
            pp = &(*pp)->next;
        }
        agentos_mutex_unlock(__atomic_load_n(&skill_lock, __ATOMIC_ACQUIRE));
        AGENTOS_FREE(entry->skill_id);
        AGENTOS_FREE(entry->url);
        AGENTOS_FREE(entry);
        return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 执行技�?
 */
agentos_error_t agentos_sys_skill_execute(const char* skill_id, const char* input, char** out_output) {
    if (!skill_id || !input || !out_output) return AGENTOS_EINVAL;
    ensure_skill_lock();
    agentos_mutex_lock(skill_lock);
    skill_entry_t* e = skill_list;
    while (e) {
        if (strcmp(e->skill_id, skill_id) == 0) {
            char* result = AGENTOS_STRDUP(input);
            agentos_mutex_unlock(skill_lock);
            if (!result) return AGENTOS_ENOMEM;
            *out_output = result;
            return AGENTOS_SUCCESS;
        }
        e = e->next;
    }
    agentos_mutex_unlock(skill_lock);
    return AGENTOS_ENOENT;
}

/**
 * @brief 列出所有已安装技�?
 */
agentos_error_t agentos_sys_skill_list(char*** out_skills, size_t* out_count) {
    if (!out_skills || !out_count) return AGENTOS_EINVAL;
    ensure_skill_lock();
    agentos_mutex_lock(skill_lock);
    size_t count = 0;
    skill_entry_t* e = skill_list;
    while (e) { count++; e = e->next; }
    char** skills = (char**)AGENTOS_CALLOC(count, sizeof(char*));
    if (!skills) {
        agentos_mutex_unlock(skill_lock);
        return AGENTOS_ENOMEM;
    }
    e = skill_list;
    size_t i = 0;
    while (e) {
        skills[i] = AGENTOS_STRDUP(e->skill_id);
        if (!skills[i]) {
            for (size_t j = 0; j < i; j++) AGENTOS_FREE(skills[j]);
            AGENTOS_FREE(skills);
            agentos_mutex_unlock(skill_lock);
            return AGENTOS_ENOMEM;
        }
        i++;
        e = e->next;
    }
    agentos_mutex_unlock(skill_lock);
    *out_skills = skills;
    *out_count = count;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 卸载技�?
 */
agentos_error_t agentos_sys_skill_uninstall(const char* skill_id) {
    if (!skill_id) return AGENTOS_EINVAL;
    ensure_skill_lock();
    agentos_mutex_lock(skill_lock);
    skill_entry_t** p = &skill_list;
    while (*p) {
        if (strcmp((*p)->skill_id, skill_id) == 0) {
            skill_entry_t* tmp = *p;
            *p = tmp->next;
            AGENTOS_FREE(tmp->skill_id);
            AGENTOS_FREE(tmp->url);
            AGENTOS_FREE(tmp);
            agentos_mutex_unlock(skill_lock);
            return AGENTOS_SUCCESS;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(skill_lock);
    return AGENTOS_ENOENT;
}
