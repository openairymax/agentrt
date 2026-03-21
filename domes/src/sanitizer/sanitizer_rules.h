/**
 * @file sanitizer_rules.h
 * @brief 净化规则管理器内部接口
 */
#ifndef DOMAIN_SANITIZER_RULES_H
#define DOMAIN_SANITIZER_RULES_H

#include <regex.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 规则类型 */
typedef enum {
    RULE_TYPE_BLOCK,   // 阻塞级风险
    RULE_TYPE_WARN     // 警告级风险
} rule_type_t;

/* 规则结构 */
typedef struct rule {
    rule_type_t type;
    regex_t     pattern;
    char*       replacement;   // 替换字符串，若为 NULL 则删除匹配部分
    int         compiled;
    struct rule* next;
} rule_t;

/* 规则集管理器 */
typedef struct rule_set {
    rule_t*         rules;
    pthread_rwlock_t rwlock;
    time_t          last_load_time;
    char*           path;
} rule_set_t;

/**
 * @brief 从 JSON 文件加载规则集
 * @param path 规则文件路径
 * @return 规则集句柄，失败返回 NULL
 */
rule_set_t* rule_set_load(const char* path);

/**
 * @brief 重新加载规则集（如果文件已更新）
 * @param rs 规则集
 * @return 0 成功，-1 失败
 */
int rule_set_reload(rule_set_t* rs);

/**
 * @brief 应用规则到字符串，可能修改字符串内容
 * @param rs 规则集
 * @param str 输入字符串（会被修改）
 * @param out_new 输出新字符串（如果发生修改，会重新分配）
 * @return 风险等级（0-3）
 */
int rule_set_apply(rule_set_t* rs, char* str, char** out_new);

/**
 * @brief 销毁规则集
 */
void rule_set_destroy(rule_set_t* rs);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_SANITIZER_RULES_H */