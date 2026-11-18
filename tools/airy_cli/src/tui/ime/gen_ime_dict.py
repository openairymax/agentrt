#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
"""
gen_ime_dict.py — 生成 airy_ime 内置输入法词典（airy_ime.dat）。

输入（均为 MIT 授权的开源数据，运行时零依赖、纯本地）：
  --pinyin-json  xiaohk/pinyin_data 的 pinyin.json
                 （41216 个汉字 → 拼音列表，首选读音在首位）
  --jieba-dict   jieba 的 dict.txt（词 频次 [词性]，用于词组与词频排序）

输出：
  --out  airy_ime.dat（紧凑二进制，头部 + 拼音条目表 + 候选区 + 字符串池，
         数据区带 CRC32 校验；C 端 airy_ime_load 一次性读入，只读查找）

拼音约定：
  - 声调字母去掉（zhōng -> zhong）；ü -> v（lü -> lv，输入 v 亦可）。
  - 多音字取 pinyin.json 首位（kMandarin 最常用读音）。
  - 词组拼音 = 逐字首选音拼接（如 中国 -> zhongguo）。

二进制布局（全部小端，跨端序由 C 端转换）：
  Header (24B): magic[8]="AIRYIME1" u32 version u32 entry_count u32 crc32
                u32 pool_off（字符串池起点相对文件头）
  Entries (entry_count*12B): u32 pinyin_off u32 cand_off u16 cand_count u16 pad
  候选区（每候选 8B）: u32 text_off u32 freq
  字符串池: 所有拼音串与候选文本的 UTF-8 连续存储（'\\0' 结尾）

用法：
  python3 gen_ime_dict.py \
      --pinyin-json /path/pinyin.json --jieba-dict /path/dict.txt \
      --out data/airy_ime.dat [--max-words 50000] [--max-word-len 6]
"""

import argparse
import json
import struct
import sys
import zlib

MAGIC = b"AIRYIME1"
VERSION = 1
HEADER_SIZE = 24

# 声调字母 -> 无调（ü 系映射到 v，输入法约定）
_TONE_MAP = {
    "ā": "a", "á": "a", "ǎ": "a", "à": "a",
    "ē": "e", "é": "e", "ě": "e", "è": "e",
    "ī": "i", "í": "i", "ǐ": "i", "ì": "i",
    "ō": "o", "ó": "o", "ǒ": "o", "ò": "o",
    "ū": "u", "ú": "u", "ǔ": "u", "ù": "u",
    "ǖ": "v", "ǘ": "v", "ǚ": "v", "ǜ": "v", "ü": "v",
}

_CJK_LO = 0x4E00
_CJK_HI = 0x9FFF


def strip_tone(s: str) -> str:
    return "".join(_TONE_MAP.get(c, c) for c in s)


def is_cjk_char(ch: str) -> bool:
    cp = ord(ch)
    return _CJK_LO <= cp <= _CJK_HI


def parse_pinyin_json(path: str) -> dict:
    """字 -> 首选无调拼音。"""
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    out = {}
    for ch, pinyins in data.items():
        if not pinyins:
            continue
        p = strip_tone(pinyins[0]).strip()
        if p:
            out[ch] = p
    return out


def parse_jieba_dict(path: str) -> dict:
    """词 -> 频次。"""
    out = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            word, freq = parts[0], parts[1]
            try:
                fq = int(freq)
            except ValueError:
                continue
            out[word] = fq
    return out


def word_pinyin(word: str, char_py: dict) -> str:
    parts = []
    for ch in word:
        p = char_py.get(ch)
        if not p:
            return None  # 词中含无拼音字符（非汉字）→ 跳过
        parts.append(p)
    return "".join(parts)


def build_entries(char_py: dict, jieba: dict, max_words: int, max_word_len: int):
    """pinyin -> [(text, freq)]，候选按频次降序。"""
    bucket = {}

    def add(text: str, freq: int):
        """同文本去重合并（jieba 单字词与全字表单字可能重复）：保留高值。"""
        p = word_pinyin(text, char_py)
        if not p:
            return
        cands = bucket.setdefault(p, [])
        for i, (t, f) in enumerate(cands):
            if t == text:
                cands[i] = (text, max(f, freq))
                return
        cands.append((text, freq))

    # 1) 词组：按频次取 top max_words，长度 1..max_word_len
    words = sorted(jieba.items(), key=lambda kv: kv[1], reverse=True)
    picked = 0
    for word, freq in words:
        n = sum(1 for ch in word if is_cjk_char(ch))
        if n == 0 or n > max_word_len:
            continue
        add(word, freq)
        picked += 1
        if picked >= max_words:
            break

    # 2) 全部汉字作为单字候选：字频 = 自身频次 + 该字在 top 词表中的
    #    累计贡献（"国"常作词素出现于"中国"等高频词，单字频次偏低——
    #    按标准字频统计（词内字累加）加权后排序符合输入法直觉）。
    char_freq = {ch: jieba.get(ch, 1) for ch in char_py if is_cjk_char(ch)}
    for word, freq in words:
        n = sum(1 for ch in word if is_cjk_char(ch))
        if n == 0 or n > max_word_len:
            continue
        for ch in word:
            if ch in char_freq:
                char_freq[ch] += freq
    for ch, freq in char_freq.items():
        add(ch, freq)

    # 3) 每条拼音的候选按频次降序、同频按文本稳定排序
    entries = {}
    for p, cands in bucket.items():
        cands.sort(key=lambda kv: (-kv[1], kv[0]))
        entries[p] = cands
    return entries


def serialize(entries: dict) -> bytes:
    # 拼音条目按字典序排序（C 端二分）
    sorted_py = sorted(entries.keys())

    # 字符串池：串去重 + 偏移记录，UTF-8 连续存储（'\0' 结尾）
    pool = bytearray()
    str_off = {}

    def _intern(s: str) -> int:
        b = s.encode("utf-8")
        if b in str_off:
            return str_off[b]
        off = len(pool)
        pool.extend(b)
        pool.append(0)
        str_off[b] = off
        return off

    entry_table = bytearray()
    cand_blob = bytearray()
    for p in sorted_py:
        cands = entries[p]
        p_off = _intern(p)
        c_off = len(cand_blob)
        for text, freq in cands:
            cand_blob += struct.pack("<II", _intern(text), freq)
        entry_table += struct.pack("<IIHH", p_off, c_off, len(cands), 0)

    pool_off = HEADER_SIZE + len(entry_table) + len(cand_blob)
    data = bytes(entry_table) + bytes(cand_blob) + bytes(pool)
    crc = zlib.crc32(data) & 0xFFFFFFFF
    header = MAGIC + struct.pack("<IIII", VERSION, len(sorted_py), crc, pool_off)
    return header + data


def main():
    ap = argparse.ArgumentParser(description="Generate airy_ime.dat dictionary")
    ap.add_argument("--pinyin-json", required=True, help="pinyin_data pinyin.json")
    ap.add_argument("--jieba-dict", required=True, help="jieba dict.txt")
    ap.add_argument("--out", required=True, help="output airy_ime.dat")
    ap.add_argument("--max-words", type=int, default=50000)
    ap.add_argument("--max-word-len", type=int, default=6)
    args = ap.parse_args()

    char_py = parse_pinyin_json(args.pinyin_json)
    jieba = parse_jieba_dict(args.jieba_dict)
    print(f"汉字拼音: {len(char_py)}，词条: {len(jieba)}")

    entries = build_entries(char_py, jieba, args.max_words, args.max_word_len)
    total_cands = sum(len(c) for c in entries.values())
    print(f"拼音条目: {len(entries)}，候选总数: {total_cands}")

    blob = serialize(entries)
    with open(args.out, "wb") as f:
        f.write(blob)
    print(f"写出: {args.out}（{len(blob)} 字节）")

    # 自检：重新读取头部验证
    assert blob[:8] == MAGIC, "magic mismatch"
    ver, count, crc, pool_off = struct.unpack("<IIII", blob[8:24])
    assert ver == VERSION and crc == zlib.crc32(blob[24:]) & 0xFFFFFFFF, "crc mismatch"
    assert pool_off < len(blob), "pool_off out of range"
    print(f"自检通过: version={ver} entries={count} crc=0x{crc:08x} pool_off={pool_off}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
