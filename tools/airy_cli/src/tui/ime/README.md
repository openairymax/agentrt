# ime — 轻量内置拼音输入法词典

**模块路径**: `tools/airy_cli/src/tui/ime/` · **版本**: 0.1.21

纯 C 的全拼前缀查询词典：一次性加载只读二进制词典文件，按前缀合并候选并按词频降序返回，供 CLI/TUI 宿主在没有系统输入法的设备（端侧、服务器、容器）上实现中文输入。

> 0.1.21 迁出：本族自 `commons/utils/ime/` 迁入 CLI TUI。终端输入法属纯体验面，
> 唯一消费者是 CLI 的 TUI 编辑器，按 0.1.19 §1.3「纯体验一律离开机制核」归位；
> 词典安装位置仍为 `share/agentrt/ime/airy_ime.dat`，运行期查找逻辑不变。

## 概述

- **运行期纯 C、零外部依赖**：`airy_ime.c` + `airy_ime.h` 为全部运行期代码，无网络、无数据库、无第三方库；词典生成脚本仅开发期使用。
- **一次性加载、查询零拷贝**：词典整体读入堆内存并校验（magic / version / 越界 / CRC32，任何一项不符即拒绝加载）；查询返回的候选文本指针直接指向词典内部，无逐条复制。
- **跨端序一致**：词典为显式小端二进制布局，加载时逐字段小端读取，x86 / ARM / RISC-V（含大端）行为一致。
- **查询语义**：全拼前缀匹配（如 `zhong` 命中 `zhong`、`zhongguo` 等全部条目），多条目候选合并后按频次降序、同频按文本字典序排列；拼音约定去声调、ü 以 `v` 表示。
- 当前内置词典含 35,626 个拼音条目（约 1.7 MB），数据来源与重新生成方式见 [data/README.md](data/README.md)。

## 目录结构

```
tools/airy_cli/src/tui/ime/
├── airy_ime.h          公共 API：不透明句柄、候选结构、load/destroy/query
├── airy_ime.c          运行期实现（纯 C：加载校验、二分前缀定位、候选排序）
├── gen_ime_dict.py     词典生成工具（仅开发期，运行期不依赖）
├── data/
│   ├── airy_ime.dat    内置词典（只读二进制，随安装分发至 share/agentrt/ime/）
│   └── README.md       词典数据来源、格式与重新生成说明
└── README.md
```

## 数据结构

| 类型 | 说明 |
|---|---|
| `airy_ime_t` | 不透明句柄：加载后的词典内存视图（只读） |
| `airy_ime_cand_t` | 候选条目：`const char *text`（UTF-8，指向词典内部）+ `uint32_t freq` |

词典二进制布局（全部小端）：

```
Header(24B): magic[8]="AIRYIME1" | u32 version | u32 entry_count | u32 crc32 | u32 pool_off
Entries(N*12B): u32 pinyin_off | u32 cand_off | u16 cand_count | u16 pad
候选区(每候选 8B): u32 text_off | u32 freq
字符串池: 拼音串与候选文本的 UTF-8 连续存储（'\0' 结尾）
```

`crc32` 覆盖 Header 之后的全部数据区。

## 接口

| 函数 | 语义 |
|---|---|
| `airy_ime_load(path)` | 加载并校验词典。路径为空、文件缺失或过短、magic / version 不符、区间越界、CRC 不匹配一律返回 NULL（fail-closed，损坏词典不可用） |
| `airy_ime_destroy(ime)` | 释放句柄与词典内存（NULL 安全）。返回后一切 `airy_ime_cand_t.text` 指针失效 |
| `airy_ime_query(ime, pinyin, out, out_cap)` | 全拼前缀查询。`pinyin` 仅接受非空小写字母串 `[a-z]+`；返回写入 `out` 的候选数（按频次降序），多于 `out_cap` 时截断；非法输入、无匹配或句柄/缓冲为 NULL 返回 0。单次查询内部收集上限 4096 个候选 |

## 用法示例

```c
#include "airy_ime.h"

void pick_candidate(void)
{
    airy_ime_t *ime = airy_ime_load("share/agentrt/ime/airy_ime.dat");
    if (ime == NULL) {
        return; /* 词典缺失或校验失败 */
    }

    airy_ime_cand_t cands[16];
    int n = airy_ime_query(ime, "zhongguo", cands, 16);
    for (int i = 0; i < n; i++) {
        /* cands[i].text 指向词典内部，随宿主 UI 展示；勿释放 */
    }

    airy_ime_destroy(ime); /* 此后 cands[] 中的指针失效 */
}
```

## 构建与依赖

`airy_ime.c` 随 `airy_common` 静态库编译；本目录头文件随安装规则导出，词典 `data/airy_ime.dat` 安装至 `share/agentrt/ime/`，宿主按该路径加载。

| 依赖 | 用途 |
|---|---|
| `airy_memory.h`（utils/memory） | 词典缓冲与查询暂存的分配/释放 |
| C 标准库 | `stdio.h` 文件读取、`string.h`、`stdint.h` |

本模块无 agentrt 内部上游依赖。

---

*SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0*
*Copyright (c) 2025-2026 SPHARX Ltd. 及贡献者，详见 [LICENSE](../../LICENSE)。*
