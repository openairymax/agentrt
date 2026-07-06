#!/usr/bin/env python3
"""Fix: (1) move #include error.h to top, (2) add braces for misleading-indentation in all files."""

import re
import os

BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

def fix_error_h_include(filepath):
    """Ensure #include 'error.h' is at the top after the first #include."""
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Find first real #include (after the main module header)
    main_header_idx = -1
    first_std_include_idx = -1
    error_h_idx = -1
    
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith('#include "') and not stripped.startswith('#include "a2a_v03_adapter.h"'):
            if main_header_idx < 0:
                main_header_idx = i
            if '<' in stripped and '>' in stripped:
                if first_std_include_idx < 0:
                    first_std_include_idx = i
        if '#include "error.h"' in stripped:
            error_h_idx = i

    if error_h_idx < 0:
        return False
    
    # If error.h is too far down (after line 50 or after first function definition),
    # move it to the top
    if error_h_idx > 50:
        # Remove the existing one
        del lines[error_h_idx]
        
        # Insert after the module header include
        insert_at = main_header_idx + 1 if main_header_idx >= 0 else 1
        lines.insert(insert_at, '#include "error.h"\n')
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(lines)
        return True
    
    return False

def add_missing_braces(filepath):
    """Fix 'if' clauses that don't guard the return due to missing braces."""
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    modified = False
    i = 0
    count = 0
    
    while i < len(lines):
        line = lines[i]
        
        # Match: if (condition) with no opening brace
        if_match = re.match(r'^(\s*)if\s*\(.+\)\s*$', line)
        if if_match and i + 2 < len(lines):
            if_indent = if_match.group(1)
            body_indent = if_indent + '    '
            
            # Check if next two lines are agentrt_error_push_ex + return at deeper indent
            # but there's no opening brace
            next_line = lines[i + 1]
            next_indent_match = re.match(r'^(\s*)agentrt_error_push_ex\(', next_line)
            
            if next_indent_match:
                next_indent = next_indent_match.group(1)
                if next_indent == body_indent:
                    # No brace — need to add one
                    # Check line after for return
                    if i + 2 < len(lines):
                        ret_line = lines[i + 2]
                        if re.match(r'^(\s*)return\s+AGENTRT_ERR_\w+\s*;', ret_line):
                            ret_indent = re.match(r'^(\s*)', ret_line).group(1)
                            if ret_indent == body_indent:
                                # Add braces: if (cond) { ... body ... }
                                lines[i] = f'{if_indent}{line.strip()} {{\n'
                                # Find where to close brace
                                # Look for the first line at same indent as 'if' that's not indented deeper
                                close_at = i + 3
                                while close_at < len(lines):
                                    check_line = lines[close_at]
                                    if check_line.strip() == '':
                                        close_at += 1
                                        continue
                                    check_indent = re.match(r'^(\s*)', check_line).group(1)
                                    if len(check_indent) <= len(if_indent) and check_line.strip():
                                        break
                                    close_at += 1
                                
                                lines.insert(close_at, f'{if_indent}}}\n')
                                count += 1
                                modified = True
                                i = close_at + 1
                                continue

        i += 1

    if modified:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(lines)
    
    return count


files_to_check = []
for root_dir in [
    os.path.join(BASE_DIR, 'agentos', 'protocols'),
    os.path.join(BASE_DIR, 'agentos', 'gateway'),
]:
    for dirpath, _, filenames in os.walk(root_dir):
        for fname in filenames:
            if fname.endswith('.c'):
                files_to_check.append(os.path.join(dirpath, fname))

total_includes = 0
total_braces = 0
for fpath in sorted(files_to_check):
    inc = fix_error_h_include(fpath)
    if inc:
        relpath = os.path.relpath(fpath, BASE_DIR)
        print(f'{relpath}: moved #include "error.h" to top')
        total_includes += 1
    
    b = add_missing_braces(fpath)
    if b > 0:
        relpath = os.path.relpath(fpath, BASE_DIR)
        print(f'{relpath}: {b} brace fixes')
        total_braces += b

print(f'\nTotal: {total_includes} include fixes, {total_braces} brace fixes')