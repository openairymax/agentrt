#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
#
# syscall_gen.py — agentrt 用户态 syscall 层契约代码生成器（P2-3 SSoT）
#
# 解析 syscall.xml 契约源（唯一真值源 SSoT），生成：
#   a. syscall_ids.h        系统调用号枚举（airy_syscall_num_t）
#   b. syscall_table_gen.h  系统调用分发表（处理函数声明 + designated initializer 表）
#
# 输入: agentrt/atoms/syscall/include/syscall.xml
# 输出: agentrt/atoms/syscall/include/syscall_ids.h
#       agentrt/atoms/syscall/include/syscall_table_gen.h
#
# 两种模式:
#   --gen   重新生成并写回产物（开发者修改 XML 后使用）
#   --check 与仓库现有产物 diff，不一致返回非零退出码（构建/CI 防漂移）
#
# 仅使用 Python 标准库（xml.etree.ElementTree），无第三方依赖。
# 生成器结构借鉴 agent-linux/tools/codegen/syscall_gen.py（parse_xml /
# render_* / check_mode 三段式），但与本项目的 syscall.xml 完全独立——
# agent-linux 内核侧 SSoT（编号 548+）与本用户态 syscall（编号 1-23）
# 互不相干，勿混用。
#
# Generator version: 1.0.0

import argparse
import difflib
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

GENERATOR_VERSION = "1.0.0"

# 生成产物文件名（保持稳定，勿随意改名）
OUTPUT_IDS = "syscall_ids.h"
OUTPUT_TABLE = "syscall_table_gen.h"

# 脚本所在目录推导仓库根（agentrt/）：codegen -> tools -> agentrt
SCRIPT_DIR = Path(__file__).resolve().parent
AGENTRT_ROOT = SCRIPT_DIR.parents[1]

DEFAULT_INPUT = AGENTRT_ROOT / "atoms" / "syscall" / "include" / "syscall.xml"
DEFAULT_OUTPUT_DIR = AGENTRT_ROOT / "atoms" / "syscall" / "include"

# 生成产物均为 .h 头文件，版权声明遵循 Linux 社区 SPDX 规范（/* */ 块注释风格）
SPDX_HEADER = [
    "/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */",
    "/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */",
]


class GenError(Exception):
    """生成器校验/解析错误（带中文说明，便于快速定位）"""


def parse_xml(xml_path):
    """解析 syscall.xml，返回结构化数据字典。

    数据结构:
      {
        version, count, max,
        meta: { generator, generator_version, schema_version, enum_type,
                prefix_symbol, prefix_handler, sentry },
        syscalls: [ { name, number, category, doc, symbol, handler } ],
      }
    """
    tree = ET.parse(str(xml_path))
    root = tree.getroot()

    version = root.get("version", "1.0")
    count = int(root.get("count", "0"))
    max_nr = int(root.get("max", "0"))

    meta = {}
    meta_elem = root.find("meta")
    if meta_elem is not None:
        for child in meta_elem:
            meta[child.tag] = (child.text or "").strip()

    prefix_symbol = meta.get("prefix_symbol", "SYS_")
    prefix_handler = meta.get("prefix_handler", "sys_")
    sentry = meta.get("sentry", "SYS_MAX")

    syscalls = []
    for sc_elem in root.findall("syscall"):
        name = (sc_elem.findtext("name") or "").strip()
        number = int((sc_elem.findtext("number") or "0").strip())
        category = (sc_elem.findtext("category") or "").strip()
        doc = (sc_elem.findtext("doc") or "").strip()
        syscalls.append({
            "name": name,
            "number": number,
            "category": category,
            "doc": doc,
            "symbol": prefix_symbol + name.upper(),
            "handler": prefix_handler + name,
        })

    return {
        "version": version,
        "count": count,
        "max": max_nr,
        "meta": meta,
        "syscalls": syscalls,
        "sentry": sentry,
    }


def validate(data):
    """校验 SSoT 数据一致性（编号连续、唯一、哨兵值正确、标识符合法）。

    规则:
      - syscall 编号必须从 1 开始连续递增（ABI 约定，禁止跳号/插号）
      - 数量 count 必须等于 <syscall> 节点数
      - 哨兵 SYS_MAX 必须等于 count + 1
      - name 必须是小写下划线标识符（用于派生 SYS_* 宏与 sys_* 函数名）
    """
    syscalls = data["syscalls"]
    count = data["count"]
    max_nr = data["max"]

    if not syscalls:
        raise GenError("syscall.xml 中没有任何 <syscall> 节点")

    if len(syscalls) != count:
        raise GenError("count 属性=%d 与 <syscall> 节点数=%d 不一致"
                       % (count, len(syscalls)))

    numbers = [sc["number"] for sc in syscalls]
    if len(set(numbers)) != len(numbers):
        raise GenError("syscall 编号存在重复: %s" % sorted(numbers))

    expected = list(range(1, count + 1))
    if numbers != expected:
        raise GenError("syscall 编号必须从 1 开始连续递增，期望 %s，实际 %s"
                       % (expected, numbers))

    if max_nr != count + 1:
        raise GenError("max 属性=%d 必须等于 count+1=%d（哨兵值 SYS_MAX）"
                       % (max_nr, count + 1))

    for sc in syscalls:
        if not re.match(r"^[a-z][a-z0-9_]*$", sc["name"]):
            raise GenError("syscall name 非法（须为小写下划线标识符）: %r" % sc["name"])

    return data


def _file_header(filename, brief):
    """生成产物文件头（SPDX + @generated 标记，禁止手工修改）。"""
    lines = list(SPDX_HEADER)
    lines.append("/**")
    lines.append(" * @file %s" % filename)
    lines.append(" * @brief %s" % brief)
    lines.append(" * @details 由 syscall_gen.py v%s 从 syscall.xml（P2-3 SSoT）自动生成，" % GENERATOR_VERSION)
    lines.append(" *          禁止手工修改。改动 syscall.xml 后运行")
    lines.append(" *          `python3 agentrt/tools/codegen/syscall_gen.py --gen` 并提交产物。")
    lines.append(" * @generated DO NOT EDIT")
    lines.append(" */")
    lines.append("")
    return lines


def render_ids(data):
    """渲染 syscall_ids.h：系统调用号枚举（由 XML 驱动，编号全部显式写出）。"""
    syscalls = data["syscalls"]
    sentry = data["sentry"]
    enum_type = data["meta"].get("enum_type", "airy_syscall_num_t")

    lines = _file_header(OUTPUT_IDS, "系统调用号枚举（生成产物）")
    lines.append("#ifndef AIRY_SYSCALL_IDS_H")
    lines.append("#define AIRY_SYSCALL_IDS_H")
    lines.append("")
    lines.append("/**")
    lines.append(" * @brief 系统调用号枚举（公共 API，类型名 %s 保持稳定）" % enum_type)
    lines.append(" * @details 单一真值源：syscall.xml → syscall_ids.h（codegen 产物）。")
    lines.append(" * 值约定：从 1 开始连续递增，%s 为哨兵值（合法 syscall 号 < %s）。" % (sentry, sentry))
    lines.append(" * 在 %s 之前追加新项即可扩展，不得在中间插入（会破坏 ABI 兼容性）。" % sentry)
    lines.append(" */")
    lines.append("typedef enum {")

    # 符号列宽对齐：含哨兵符号取最大长度
    max_sym = max([len(sc["symbol"]) for sc in syscalls] + [len(sentry)])

    def emit(sym, value):
        lines.append("    %s = %d," % (sym.ljust(max_sym), value))

    for sc in syscalls:
        emit(sc["symbol"], sc["number"])
    emit(sentry, data["max"])

    lines.append("} %s;" % enum_type)
    lines.append("")
    lines.append("#endif /* AIRY_SYSCALL_IDS_H */")
    lines.append("")
    return "\n".join(lines)


def render_table(data):
    """渲染 syscall_table_gen.h：处理函数声明 + 分发表（designated initializer）。"""
    syscalls = data["syscalls"]
    sentry = data["sentry"]

    lines = _file_header(OUTPUT_TABLE, "系统调用分发表（生成产物）")
    lines.append("#ifndef AIRY_SYSCALL_TABLE_GEN_H")
    lines.append("#define AIRY_SYSCALL_TABLE_GEN_H")
    lines.append("")
    lines.append('#include "syscall_ids.h"')
    lines.append("")
    lines.append("#include <stddef.h>")
    lines.append("")
    lines.append("/** 系统调用处理函数类型：args 为参数数组，argc 为参数个数，返回 intptr 编码结果 */")
    lines.append("typedef void *(*syscall_func_t)(void **args, int argc);")
    lines.append("")
    lines.append("/* 处理函数声明（实现位于 syscall_entry.c，由 XML 驱动） */")
    for sc in syscalls:
        lines.append("extern void *%s(void **args, int argc);" % sc["handler"])
    lines.append("")
    lines.append("/* 分发表：syscall 号 → 处理函数（designated initializer，由 XML 驱动） */")
    lines.append("static const syscall_func_t syscall_table[%s] = {" % sentry)
    for sc in syscalls:
        lines.append("    [%s] = %s," % (sc["symbol"], sc["handler"]))
    lines.append("};")
    lines.append("")
    lines.append("#endif /* AIRY_SYSCALL_TABLE_GEN_H */")
    lines.append("")
    return "\n".join(lines)


def generate(input_path):
    """解析并校验 XML，渲染全部生成产物，返回 {文件名: 内容}。"""
    data = validate(parse_xml(input_path))
    return {
        OUTPUT_IDS: render_ids(data),
        OUTPUT_TABLE: render_table(data),
    }


def write_outputs(contents, output_dir):
    """将生成产物写回 output_dir（--gen 模式）。"""
    out_dir = Path(output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for fname, content in contents.items():
        out = out_dir / fname
        out.write_text(content, encoding="utf-8")
        print("Generated: %s" % out)


def check_outputs(contents, output_dir):
    """对比生成产物与仓库现有文件，任一不一致即返回非零（--check 模式）。

    返回值：
      0 — 全部产物与已提交文件一致
      1 — 存在不一致或产物缺失
    """
    out_dir = Path(output_dir)
    rc = 0
    for fname, content in contents.items():
        out = out_dir / fname
        if not out.exists():
            print("ERROR: output file does not exist: %s" % out, file=sys.stderr)
            print("       Run 'syscall_gen.py --gen' first and commit the result.",
                  file=sys.stderr)
            rc = 1
            continue

        existing = out.read_text(encoding="utf-8")
        if existing == content:
            print("OK: generated content matches committed file (%s)" % out)
            continue

        print("ERROR: generated content differs from committed file: %s" % out,
              file=sys.stderr)
        print("       Run 'syscall_gen.py --gen' to regenerate and commit the result.",
              file=sys.stderr)
        diff = difflib.unified_diff(
            existing.splitlines(keepends=True),
            content.splitlines(keepends=True),
            fromfile=str(out) + " (committed)",
            tofile=str(out) + " (generated)",
        )
        sys.stderr.writelines(diff)
        rc = 1
    return rc


def main():
    parser = argparse.ArgumentParser(
        description="syscall.xml → syscall_ids.h / syscall_table_gen.h codegen (P2-3 SSoT)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
示例:
  %(prog)s --gen                          # 生成/写回产物（默认路径）
  %(prog)s --check                        # 防漂移校验（构建/CI 使用）
  %(prog)s --gen --input custom.xml       # 自定义输入
  %(prog)s --check --output-dir out/      # 自定义输出目录
""",
    )
    parser.add_argument(
        "--input", "-i",
        default=str(DEFAULT_INPUT),
        help="输入 syscall.xml 路径（默认: %(default)s）",
    )
    parser.add_argument(
        "--output-dir", "-o",
        default=str(DEFAULT_OUTPUT_DIR),
        help="生成产物输出目录（默认: %(default)s）",
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--gen",
        action="store_true",
        help="生成模式：重新生成并写回产物",
    )
    mode.add_argument(
        "--check",
        action="store_true",
        help="校验模式：与仓库产物 diff，不一致返回非零退出码（防漂移）",
    )
    args = parser.parse_args()

    input_path = Path(args.input).resolve()
    if not input_path.exists():
        print("ERROR: input file not found: %s" % input_path, file=sys.stderr)
        return 1

    try:
        contents = generate(input_path)
    except GenError as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
        return 1
    except ET.ParseError as exc:
        print("ERROR: syscall.xml 解析失败: %s" % exc, file=sys.stderr)
        return 1

    if args.check:
        return check_outputs(contents, Path(args.output_dir).resolve())

    write_outputs(contents, Path(args.output_dir).resolve())
    return 0


if __name__ == "__main__":
    sys.exit(main())
