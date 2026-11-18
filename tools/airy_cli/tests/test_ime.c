// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_ime.c
 * @brief airy_ime 词典加载/查询单测（无框架，断言失败即退出非 0）。
 *
 * 用法: test_ime <airy_ime.dat 路径>
 */

#include "airy_ime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#define IME_GETPID() ((int)_getpid())
#else
#include <unistd.h>
#define IME_GETPID() ((int)getpid())
#endif

static int g_fail = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);    \
            g_fail = 1;                                                        \
        }                                                                      \
    } while (0)

static int has_cand(const airy_ime_cand_t *c, int n, const char *text)
{
    for (int i = 0; i < n; i++)
        if (strcmp(c[i].text, text) == 0)
            return 1;
    return 0;
}

int main(int argc, char *argv[])
{
    /* 词典路径：命令行参数优先；未传时用编译期默认（CMake 注入
     * AIRY_IME_TEST_DICT，指向源码树 data/airy_ime.dat）。 */
    const char *dict = (argc >= 2) ? argv[1] : NULL;
#ifdef AIRY_IME_TEST_DICT
    if (!dict || !dict[0])
        dict = AIRY_IME_TEST_DICT;
#endif
    if (!dict || !dict[0]) {
        fprintf(stderr, "用法: test_ime <airy_ime.dat>\n");
        return 2;
    }

    /* 1. 加载成功 */
    airy_ime_t *ime = airy_ime_load(dict);
    CHECK(ime != NULL, "load success");

    if (!ime)
        return 1;

    /* 2. 全拼前缀：zhong → 中 / 中国 */
    airy_ime_cand_t cand[16];
    int n = airy_ime_query(ime, "zhong", cand, 16);
    CHECK(n > 0, "zhong has candidates");
    if (n > 0) {
        CHECK(has_cand(cand, n, "中"), "zhong contains 中");
        CHECK(has_cand(cand, n, "中国"), "zhong prefix hits 中国 (zhongguo)");
        /* 候选按频次降序 */
        for (int i = 1; i < n; i++)
            CHECK(cand[i - 1].freq >= cand[i].freq, "candidates sorted by freq desc");
    }

    /* 3. 常见字 */
    n = airy_ime_query(ime, "wo", cand, 16);
    CHECK(n > 0 && has_cand(cand, n, "我"), "wo contains 我");

    n = airy_ime_query(ime, "ni", cand, 16);
    CHECK(n > 0 && has_cand(cand, n, "你"), "ni contains 你");

    /* 4. 词组全拼精确前缀 */
    n = airy_ime_query(ime, "zhongguo", cand, 16);
    CHECK(n > 0 && has_cand(cand, n, "中国"), "zhongguo contains 中国");

    n = airy_ime_query(ime, "nihao", cand, 16);
    CHECK(n > 0 && has_cand(cand, n, "你好"), "nihao contains 你好");

    /* 5. 非法/空/无匹配输入 */
    CHECK(airy_ime_query(ime, "", cand, 16) == 0, "empty input -> 0");
    CHECK(airy_ime_query(ime, "Zhong", cand, 16) == 0, "uppercase rejected");
    CHECK(airy_ime_query(ime, "z1", cand, 16) == 0, "digit rejected");
    CHECK(airy_ime_query(ime, "zzzzzzzz", cand, 16) == 0, "no match -> 0");

    /* 6. out_cap 截断 */
    n = airy_ime_query(ime, "shi", cand, 2);
    CHECK(n == 2, "cap truncation");

    /* 7. ü 以 v 表示（lü -> lv） */
    n = airy_ime_query(ime, "lv", cand, 16);
    CHECK(n > 0 && has_cand(cand, n, "绿"), "lv contains 绿 (lü)");

    airy_ime_destroy(ime);

    /* 8. 损坏文件 fail-closed */
    {
        /* CI-2：路径带 PID，避免并行 ctest 下多个实例写同一文件互相踩踏。 */
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "/tmp/airy_ime_corrupt_%d.dat", IME_GETPID());
        FILE *f = fopen(tmp, "wb");
        if (f) {
            const char *bad = "AIRYIME1\x01\x00\x00\x00\xff\xff\xff\xff"
                              "xxxxxxxxxxxxxxxxxxxxxxxx";
            fwrite(bad, 1, strlen(bad) + 1, f);
            fclose(f);
        }
        CHECK(airy_ime_load(tmp) == NULL, "corrupt file rejected (crc)");
        CHECK(airy_ime_load(NULL) == NULL, "NULL path -> NULL");
        CHECK(airy_ime_load("/nonexistent/airy_ime.dat") == NULL,
              "missing file -> NULL");
    }

    if (g_fail) {
        fprintf(stderr, "test_ime: FAILED\n");
        return 1;
    }
    printf("test_ime: ALL PASS\n");
    return 0;
}
