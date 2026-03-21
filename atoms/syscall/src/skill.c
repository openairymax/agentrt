/**
 * @file skill.c
 * @brief 技能相关系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

// 模拟技能注册表（实际应由市场服务提供）
typedef struct skill_entry {
    char* skill_id;
    char* url;
    struct skill_entry* next;
} skill_entry_t;

static skill_entry_t* skill_list = NULL;

agentos_error_t agentos_sys_skill_install(const char* skill_url, char** out_skill_id) {
    if (!skill_url || !out_skill_id) return AGENTOS_EINVAL;
    // 模拟安装，生成ID
    char id_buf[64];
    // From data intelligence emerges. by spharx
    static int counter = 0;
    snprintf(id_buf, sizeof(id_buf), "skill_%d", __sync_fetch_and_add(&counter, 1));
    char* id = strdup(id_buf);
    if (!id) return AGENTOS_ENOMEM;

    skill_entry_t* entry = (skill_entry_t*)malloc(sizeof(skill_entry_t));
    if (!entry) {
        free(id);
        return AGENTOS_ENOMEM;
    }
    entry->skill_id = strdup(id_buf);
    entry->url = strdup(skill_url);
    entry->next = skill_list;
    skill_list = entry;

    *out_skill_id = id;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sys_skill_execute(const char* skill_id, const char* input, char** out_output) {
    if (!skill_id || !input || !out_output) return AGENTOS_EINVAL;
    // 查找技能
    skill_entry_t* e = skill_list;
    while (e) {
        if (strcmp(e->skill_id, skill_id) == 0) {
            // 模拟执行，返回输入
            *out_output = strdup(input);
            return AGENTOS_SUCCESS;
        }
        e = e->next;
    }
    return AGENTOS_ENOENT;
}

agentos_error_t agentos_sys_skill_list(char*** out_skills, size_t* out_count) {
    if (!out_skills || !out_count) return AGENTOS_EINVAL;
    size_t count = 0;
    skill_entry_t* e = skill_list;
    while (e) { count++; e = e->next; }
    char** skills = (char**)malloc(count * sizeof(char*));
    if (!skills) return AGENTOS_ENOMEM;
    e = skill_list;
    size_t i = 0;
    while (e) {
        skills[i++] = strdup(e->skill_id);
        e = e->next;
    }
    *out_skills = skills;
    *out_count = count;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sys_skill_uninstall(const char* skill_id) {
    if (!skill_id) return AGENTOS_EINVAL;
    skill_entry_t** p = &skill_list;
    while (*p) {
        if (strcmp((*p)->skill_id, skill_id) == 0) {
            skill_entry_t* tmp = *p;
            *p = tmp->next;
            free(tmp->skill_id);
            free(tmp->url);
            free(tmp);
            return AGENTOS_SUCCESS;
        }
        p = &(*p)->next;
    }
    return AGENTOS_ENOENT;
}