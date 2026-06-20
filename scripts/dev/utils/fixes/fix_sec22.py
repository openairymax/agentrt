#!/usr/bin/env python3
"""Fix SEC-22 violations: Add ptr = NULL after AGENTOS_FREE(ptr) calls.

自动检测并修复 SEC-22 违规：在 AGENTOS_FREE(ptr) 之后缺少 ptr = NULL 赋值。

排除规则（不添加 NULL 赋值的情况）：
  Rule 1: 下一行已有 ptr = NULL
  Rule 2: 紧接 return/continue/break/goto
  Rule 3: 函数体内无后续代码（销毁函数末尾）
  Rule 4: 包含该字段的结构体即将被 free
  Rule 5: 指针在同一行已被重新赋值

用法:
  python3 fix_sec22.py file1.c file2.c ...
  python3 fix_sec22.py --debug file.c   # 调试模式，打印判断过程
"""

import re
import sys

DEBUG = '--debug' in sys.argv
if DEBUG:
    sys.argv.remove('--debug')


def compute_brace_depths(lines):
    """Compute the brace depth at the END of each line (0-indexed)."""
    depths = [0] * len(lines)
    depth = 0
    for i, line in enumerate(lines):
        depth += line.count('{') - line.count('}')
        depths[i] = depth
    return depths


def find_function_bounds(lines, line_idx, depths):
    """Find the function containing line_idx using precomputed brace depths.

    Returns (func_name, func_open, func_end) where:
    - func_name: name of the function
    - func_open: 0-indexed line of the function's opening brace
    - func_end: 0-indexed line of the function's closing brace
    """
    # Walk backwards to find where brace depth drops to 0
    # The function's opening brace is the line where depth transitions from 0 to >0
    func_open = -1
    for i in range(line_idx, -1, -1):
        depth_before = depths[i - 1] if i > 0 else 0
        depth_after = depths[i]
        # Opening brace that takes us from depth 0 into a function
        if depth_before == 0 and depth_after > 0:
            func_open = i
            break

    if func_open == -1:
        return '', 0, len(lines) - 1

    # Find function name by searching backwards from the opening brace
    func_name = ''
    for i in range(func_open, max(func_open - 15, -1), -1):
        m = re.search(r'(\w+)\s*\(', lines[i])
        if m and 'AGENTOS_FREE' not in lines[i]:
            word = m.group(1)
            if word not in ('if', 'for', 'while', 'switch', 'return', 'sizeof',
                           'AGENTOS_FREE', 'AGENTOS_CALLOC', 'AGENTOS_MALLOC',
                           'AGENTOS_REALLOC', 'AGENTOS_STRDUP'):
                func_name = word
                break

    # Find function end: walk forward from func_open until depth returns to 0
    func_end = func_open
    for i in range(func_open + 1, len(lines)):
        if depths[i] == 0:
            func_end = i
            break

    return func_name, func_open, func_end


def extract_ptr_name(ptr_expr):
    """Extract the actual variable/field name, removing casts."""
    ptr = ptr_expr.strip()
    ptr = re.sub(r'^\(\s*void\s*\*\s*\)\s*', '', ptr)
    ptr = re.sub(r'^\(\s*\w+\s*\*\s*\)\s*', '', ptr)
    return ptr.strip()


def get_root_variable(ptr_name):
    """Get the root variable from a pointer expression."""
    if '->' in ptr_name:
        root = ptr_name.split('->')[0].strip()
        root = re.sub(r'\[.*?\]', '', root).strip()
        return root
    if '.' in ptr_name:
        root = ptr_name.split('.')[0].strip()
        root = re.sub(r'\[.*?\]', '', root).strip()
        return root
    return ptr_name


def get_containing_struct(ptr_name):
    """Get the struct that directly contains the field being freed."""
    if '->' in ptr_name:
        parts = ptr_name.split('->')
        if len(parts) > 1:
            return '->'.join(parts[:-1])
        return parts[0]
    if '.' in ptr_name:
        parts = ptr_name.split('.')
        if len(parts) > 1:
            return '.'.join(parts[:-1])
        return parts[0]
    return None


def is_freed_in_function(lines, func_start, func_end, name):
    """Check if a variable/struct is freed within the function via AGENTOS_FREE."""
    for i in range(func_start, func_end + 1):
        line = lines[i]
        pattern = rf'AGENTOS_FREE\s*\(\s*(?:\(.*?\)\s*)?{re.escape(name)}\s*\)\s*;'
        if re.search(pattern, line):
            if DEBUG:
                print(f'    DEBUG: Found AGENTOS_FREE({name}) at line {i+1}')
            return True
    return False


def is_function_end_after(lines, line_idx, func_end):
    """Check if there's essentially no more code between line_idx and func_end."""
    for i in range(line_idx + 1, func_end + 1):
        stripped = lines[i].strip()
        if stripped and stripped != '}':
            return False
    return True


def process_file(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()

    # Precompute brace depths for the entire file
    depths = compute_brace_depths(lines)

    result = []
    i = 0
    changes = 0
    skipped = []

    while i < len(lines):
        line = lines[i]
        result.append(line)

        match = re.search(r'AGENTOS_FREE\s*\(\s*(.+?)\s*\)\s*;', line)
        if not match:
            i += 1
            continue

        ptr_expr = match.group(1).strip()
        ptr_name = extract_ptr_name(ptr_expr)

        if not ptr_name:
            i += 1
            continue

        # Check if already followed by ptr = NULL
        if i + 1 < len(lines):
            next_line = lines[i + 1]
            null_pattern = re.compile(rf'^\s*{re.escape(ptr_name)}\s*=\s*NULL\s*;')
            if null_pattern.search(next_line):
                i += 1
                continue

        # Rule 2: Skip if next non-empty line is return/continue/break/goto
        next_idx = i + 1
        while next_idx < len(lines) and lines[next_idx].strip() == '':
            next_idx += 1
        if next_idx < len(lines):
            stripped = lines[next_idx].strip()
            if re.match(r'^(return|continue|break|goto)\b', stripped):
                skipped.append((i + 1, ptr_name, 'Rule2'))
                i += 1
                continue

        # Get function context using precomputed depths
        func_name, func_start, func_end = find_function_bounds(lines, i, depths)

        if DEBUG:
            print(f'  DEBUG line {i+1}: ptr={ptr_name}, func={func_name}, bounds=[{func_start+1},{func_end+1}]')

        # Rule 4: Skip if struct containing the pointer is about to be freed
        should_add = True
        root = get_root_variable(ptr_name)

        if root and ('->' in ptr_name or '.' in ptr_name):
            if is_freed_in_function(lines, func_start, func_end, root):
                should_add = False
                skipped.append((i + 1, ptr_name, f'Rule4:root({root})'))
            else:
                containing = get_containing_struct(ptr_name)
                if containing and containing != root and is_freed_in_function(lines, func_start, func_end, containing):
                    should_add = False
                    skipped.append((i + 1, ptr_name, f'Rule4:containing({containing})'))

        # Rule 3 (extended): Skip if simple variable and function ends right after
        if should_add and not ('->' in ptr_name or '.' in ptr_name):
            if is_function_end_after(lines, i, func_end):
                should_add = False
                skipped.append((i + 1, ptr_name, 'Rule3:func_end'))

        if should_add:
            indent_match = re.match(r'^(\s*)', line)
            indent = indent_match.group(1) if indent_match else ''
            stripped_line = line.lstrip()
            if not stripped_line.startswith('AGENTOS_FREE'):
                agentos_pos = line.find('AGENTOS_FREE')
                indent = line[:agentos_pos]
            null_line = f'{indent}{ptr_name} = NULL;\n'
            result.append(null_line)
            changes += 1

        i += 1

    with open(filepath, 'w') as f:
        f.writelines(result)

    print(f'{filepath}: {changes} added, {len(skipped)} skipped')
    for line_no, ptr, reason in skipped:
        print(f'  L{line_no}: {ptr} ({reason})')
    return changes


if __name__ == '__main__':
    total = 0
    for filepath in sys.argv[1:]:
        total += process_file(filepath)
    print(f'\nTotal: {total} NULL assignments added')