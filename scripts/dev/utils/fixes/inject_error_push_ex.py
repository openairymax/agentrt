#!/usr/bin/env python3
"""
注入 error_push_ex 到 atoms/ 层文件
为所有 return AGENTOS_E* 返回点自动注入 error_push_ex 上下文追踪

用法: python3 inject_error_push_ex.py <file.c>
"""
import re
import sys

MACRO_NAME = "ATM_RET_ERR"
MACRO_DEF = (
    f"#define {MACRO_NAME}(c) \\\n"
    f"    do {{ agentos_error_push_ex((c), __FILE__, __LINE__, __func__, "
    f'"%s", agentos_error_str(c)); return (c); }} while(0)\n'
)

INCLUDE_LINE = '#include "error_compat.h"\n'

RET_PATTERN = re.compile(r"return\s+(AGENTOS_E\w+)\s*;")


def inject(filepath):
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()

    # 1. Add #include "error_compat.h" if not present (after last #include)
    has_error_compat = any("error_compat.h" in l for l in lines)
    has_macro = any(MACRO_NAME in l for l in lines)
    has_error_push = any("error_push_ex" in l for l in lines)

    if has_error_push and has_macro:
        print(f"  SKIP (already injected): {filepath}")
        return 0

    # Find last #include line
    last_include_idx = -1
    for i, line in enumerate(lines):
        if line.strip().startswith("#include"):
            last_include_idx = i

    if last_include_idx < 0:
        print(f"  ERROR: no #include found in {filepath}")
        return 0

    # Insert error_compat.h after last include
    if not has_error_compat and not has_macro:
        lines.insert(last_include_idx + 1, INCLUDE_LINE)
        # Adjust index for the new line
        lines.insert(last_include_idx + 2, "\n")
        lines.insert(last_include_idx + 3, MACRO_DEF)
        lines.insert(last_include_idx + 4, "\n")
    elif not has_macro:
        # error_compat.h exists but macro doesn't
        insert_at = last_include_idx + 1
        if not has_error_compat:
            pass
        else:
            # Find where error_compat.h is and insert after it
            for i, line in enumerate(lines):
                if "error_compat.h" in line:
                    insert_at = i + 1
                    break
        lines.insert(insert_at, "\n")
        lines.insert(insert_at + 1, MACRO_DEF)
        lines.insert(insert_at + 2, "\n")

    # 2. Count and replace return AGENTOS_E*;
    count = 0
    new_lines = []
    for line in lines:
        match = RET_PATTERN.search(line)
        if match:
            err_code = match.group(1)
            indent = line[:match.start()]
            new_line = f"{indent}{MACRO_NAME}({err_code});\n"
            new_lines.append(new_line)
            count += 1
        else:
            new_lines.append(line)

    with open(filepath, "w", encoding="utf-8") as f:
        f.writelines(new_lines)

    print(f"  INJECTED {count} × error_push_ex: {filepath}")
    return count


def main():
    total = 0
    for filepath in sys.argv[1:]:
        total += inject(filepath)
    print(f"\nTotal: {total} AGENTOS_E_* → error_push_ex injections")


if __name__ == "__main__":
    total = 0
    for filepath in sys.argv[1:]:
        total += inject(filepath)
    print(f"\nTotal: {total} AGENTOS_E_* → error_push_ex injections")