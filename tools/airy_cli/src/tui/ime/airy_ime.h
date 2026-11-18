// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_ime.h
 * @brief 轻量内置拼音输入法词典（全拼，本地只读，零外部依赖）。
 *
 * 面向无系统输入法（IME）的设备（端侧/服务器/树莓派），在 TUI/CLI
 * 内提供中文输入：全拼前缀查询 + 候选频次排序。纯本地、无网络、
 * 词典只读（数据区 CRC32 校验），不修改系统输入法。
 *
 * 词典文件（airy_ime.dat）由 tools/gen_ime_dict.py 从 MIT 授权的开源
 * 数据生成（pinyin_data 汉字拼音 + jieba 词频），支持跨端序加载。
 */

#ifndef AIRY_IME_H
#define AIRY_IME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明句柄：加载后的词典（内存映射视图，只读） */
typedef struct airy_ime airy_ime_t;

/* 候选条目：UTF-8 文本 + 频次（降序） */
typedef struct {
    const char *text;
    uint32_t freq;
} airy_ime_cand_t;

/**
 * 加载词典文件。
 * @param path airy_ime.dat 路径（NULL 返回 NULL）
 * @return 句柄；文件缺失/损坏（magic/version/CRC 校验失败）返回 NULL
 */
airy_ime_t *airy_ime_load(const char *path);

/** 释放词典句柄（NULL 安全）。 */
void airy_ime_destroy(airy_ime_t *ime);

/**
 * 全拼前缀查询：输入 "zhong" → 所有以 zhong 开头的拼音条目（zhong、
 * zhongguo …）候选合并，按频次降序输出到 out。
 * @param pinyin 仅接受小写字母 [a-z]（ü 以 v 表示）；空/含非法字符返回 0
 * @param out_cap out 容量；候选多于 out_cap 时截断
 * @return 输出候选数（0 = 无匹配或非法输入）
 */
int airy_ime_query(const airy_ime_t *ime, const char *pinyin,
                   airy_ime_cand_t *out, int out_cap);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_IME_H */
