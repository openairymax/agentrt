// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_ime.c
 * @brief 轻量内置拼音输入法词典实现（全拼前缀查询，本地只读）。
 *
 * 数据布局（全部小端，见 tools/gen_ime_dict.py）：
 *   Header(24B) | Entries[N*12B] | 候选区 | 字符串池
 * 加载时一次性读入内存并做 magic/version/CRC 校验，查询零拷贝、
 * 端序无关（显式小端读取），跨 x86/ARM/RISC-V（含大端）一致。
 */

#include "airy_ime.h"

#include "airy_memory.h"

#include <stdio.h>
#include <string.h>

#define AIRY_IME_MAGIC "AIRYIME1"
#define AIRY_IME_VERSION 1u
#define AIRY_IME_HEADER_SIZE 24
#define AIRY_IME_ENTRY_SIZE 12
#define AIRY_IME_CAND_SIZE 8
/* 单次查询收集候选上限：防极端短前缀（如 "a"）扫出过量条目 */
#define AIRY_IME_QUERY_CAP 4096

/* 词典内存视图（全部相对文件头偏移，只读） */
struct airy_ime {
    const uint8_t *buf;    /* 文件内容（OWNER） */
    size_t size;
    uint32_t entry_count;
    uint32_t pool_off;
    const uint8_t *entries; /* buf + 24 */
    const uint8_t *pool;    /* buf + pool_off */
};

/* ---- 端序无关小端读取 ---- */
static uint32_t airy_ime_rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t airy_ime_rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* ---- CRC32（IEEE 802.3，标准查表法） ---- */
static uint32_t airy_ime_crc32(const uint8_t *data, size_t len)
{
    static uint32_t table[256];
    static int init = 0;
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;

    if (!init) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        init = 1;
    }
    for (i = 0; i < len; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

/* 读取第 idx 条 entry 的拼音串（pool 内 '\0' 结尾） */
static const char *airy_ime_entry_py(const airy_ime_t *ime, size_t idx)
{
    const uint8_t *e = ime->entries + idx * AIRY_IME_ENTRY_SIZE;
    return (const char *)(ime->pool + airy_ime_rd_u32(e));
}

/* 二分下界：第一条 pinyin >= key（字典序） */
static size_t airy_ime_lower_bound(const airy_ime_t *ime, const char *key)
{
    size_t lo = 0, hi = ime->entry_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (strcmp(airy_ime_entry_py(ime, mid), key) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

airy_ime_t *airy_ime_load(const char *path)
{
    if (!path || !path[0])
        return NULL;

    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz <= 0 || (unsigned long)sz > (size_t)-1 - AIRY_IME_HEADER_SIZE) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    airy_ime_t *ime = (airy_ime_t *)AIRY_CALLOC(1, sizeof(*ime));
    if (!ime) {
        fclose(f);
        return NULL;
    }
    ime->buf = (const uint8_t *)AIRY_MALLOC((size_t)sz);
    if (!ime->buf) {
        AIRY_FREE(ime);
        fclose(f);
        return NULL;
    }
    if (fread((void *)ime->buf, 1, (size_t)sz, f) != (size_t)sz) {
        /* 读不满 = 文件异常，视为损坏 */
        AIRY_FREE((void *)ime->buf);
        AIRY_FREE(ime);
        fclose(f);
        return NULL;
    }
    fclose(f);
    ime->size = (size_t)sz;

    /* magic/version/CRC 校验（fail-closed：损坏即拒绝加载） */
    if (ime->size < AIRY_IME_HEADER_SIZE ||
        memcmp(ime->buf, AIRY_IME_MAGIC, 8) != 0) {
        goto corrupt;
    }
    uint32_t ver = airy_ime_rd_u32(ime->buf + 8);
    uint32_t count = airy_ime_rd_u32(ime->buf + 12);
    uint32_t crc = airy_ime_rd_u32(ime->buf + 16);
    uint32_t pool_off = airy_ime_rd_u32(ime->buf + 20);
    if (ver != AIRY_IME_VERSION)
        goto corrupt;
    if (count > (ime->size - AIRY_IME_HEADER_SIZE) / AIRY_IME_ENTRY_SIZE)
        goto corrupt;
    if (pool_off < AIRY_IME_HEADER_SIZE + count * AIRY_IME_ENTRY_SIZE ||
        pool_off > ime->size)
        goto corrupt;
    if (airy_ime_crc32(ime->buf + AIRY_IME_HEADER_SIZE,
                       ime->size - AIRY_IME_HEADER_SIZE) != crc)
        goto corrupt;

    ime->entry_count = count;
    ime->pool_off = pool_off;
    ime->entries = ime->buf + AIRY_IME_HEADER_SIZE;
    ime->pool = ime->buf + pool_off;
    return ime;

corrupt:
    AIRY_FREE((void *)ime->buf);
    AIRY_FREE(ime);
    return NULL;
}

void airy_ime_destroy(airy_ime_t *ime)
{
    if (!ime)
        return;
    AIRY_FREE((void *)ime->buf);
    AIRY_FREE(ime);
}

int airy_ime_query(const airy_ime_t *ime, const char *pinyin,
                   airy_ime_cand_t *out, int out_cap)
{
    if (!ime || !pinyin || !out || out_cap <= 0)
        return 0;

    /* 输入校验：仅小写字母 [a-z]（ü 约定以 v 表示） */
    size_t qlen = strlen(pinyin);
    if (qlen == 0)
        return 0;
    for (size_t i = 0; i < qlen; i++) {
        if (pinyin[i] < 'a' || pinyin[i] > 'z')
            return 0;
    }

    /* 候选收集缓冲（堆上，前缀命中条目候选合并） */
    airy_ime_cand_t *cands =
        (airy_ime_cand_t *)AIRY_MALLOC(AIRY_IME_QUERY_CAP * sizeof(*cands));
    if (!cands)
        return 0;
    int n = 0;

    /* 二分定位首个 pinyin >= query，顺序扫描前缀命中条目 */
    size_t idx = airy_ime_lower_bound(ime, pinyin);
    for (; idx < ime->entry_count; idx++) {
        const char *py = airy_ime_entry_py(ime, idx);
        if (strncmp(py, pinyin, qlen) != 0)
            break; /* 字典序：前缀区间结束 */
        const uint8_t *e = ime->entries + idx * AIRY_IME_ENTRY_SIZE;
        uint32_t cand_off = airy_ime_rd_u32(e + 4);
        uint16_t cand_count = airy_ime_rd_u16(e + 8);
        /* 候选区绝对位置：entries 表尾 + cand_off（相对候选区起点） */
        const uint8_t *cb =
            ime->entries + ime->entry_count * AIRY_IME_ENTRY_SIZE + cand_off;
        for (uint16_t k = 0; k < cand_count && n < AIRY_IME_QUERY_CAP; k++) {
            uint32_t text_off = airy_ime_rd_u32(cb + (size_t)k * AIRY_IME_CAND_SIZE);
            uint32_t freq = airy_ime_rd_u32(cb + (size_t)k * AIRY_IME_CAND_SIZE + 4);
            cands[n].text = (const char *)(ime->pool + text_off);
            cands[n].freq = freq;
            n++;
        }
    }
    if (n == 0) {
        AIRY_FREE(cands);
        return 0;
    }

    /* 按频次降序稳定排序（同频保候选原始顺序） */
    for (int i = 1; i < n; i++) {
        airy_ime_cand_t tmp = cands[i];
        int j = i - 1;
        while (j >= 0 &&
               (cands[j].freq < tmp.freq ||
                (cands[j].freq == tmp.freq && strcmp(cands[j].text, tmp.text) > 0))) {
            cands[j + 1] = cands[j];
            j--;
        }
        cands[j + 1] = tmp;
    }

    int out_n = n < out_cap ? n : out_cap;
    for (int i = 0; i < out_n; i++)
        out[i] = cands[i];
    AIRY_FREE(cands);
    return out_n;
}
