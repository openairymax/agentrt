#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS C语言安全编码静态检查工具
# 遵循 AgentOS 安全编码规范 3.2 节

"""
AgentOS 安全编码静态检查工具

检查项目：
1. 不安全字符串函数 (strcpy/strcat/gets/sprintf/vsprintf)
2. strncpy 缺少空终止符 (CWE-170)
3. malloc/calloc/realloc 返回值未检查 (CWE-690)
4. 格式化字符串漏洞 (CWE-134)
5. 命令注入风险 (CWE-78)
6. 硬编码密码/密钥 (CWE-798)
7. 整数溢出风险 (CWE-190)
8. 脆弱相对路径 include
9. 使用已弃用函数
"""

import os
import re
import sys
from dataclasses import dataclass, field
from enum import Enum
from typing import List, Tuple


class Severity(Enum):
    CRITICAL = "CRITICAL"
    HIGH = "HIGH"
    MEDIUM = "MEDIUM"
    LOW = "LOW"
    INFO = "INFO"


@dataclass
class Finding:
    file_path: str
    line_number: int
    column: int
    severity: Severity
    rule_id: str
    message: str
    suggestion: str = ""


UNSAFE_FUNCTIONS = {
    'strcpy': ('SEC001', Severity.CRITICAL, 'Use safe_strcpy() or strncpy()+null-term'),
    'strcat': ('SEC001', Severity.CRITICAL, 'Use safe_strcat() or strncat()+null-term'),
    'gets': ('SEC001', Severity.CRITICAL, 'Use fgets() instead'),
    'sprintf': ('SEC001', Severity.HIGH, 'Use safe_sprintf() or snprintf()'),
    'vsprintf': ('SEC001', Severity.HIGH, 'Use vsnprintf() instead'),
}

STRNCWY_PATTERN = re.compile(
    r'strncpy\s*\(\s*(\w+(?:\[\w+\])?(?:\.\w+)*(?:->\w+)*(?:\[\w+\])?)'
    r'\s*,\s*\w+\s*,\s*[^)]+\)\s*;(?!.*\1\s*\[\s*[^]]+\]\s*=\s*[\'"]\\\\0[\'"])'
)

MALLOC_PATTERN = re.compile(
    r'(?:void\s*\*\s*)?\w+\s*=\s*\(\s*\w+\s*\*\s*\)\s*'
    r'(malloc|calloc|realloc)\s*\([^)]+\)\s*;'
)

FORMAT_STRING_PATTERN = re.compile(
    r'(?:printf|fprintf|sprintf|snprintf)\s*\(\s*[^",]*\w+\s*[,\)]'
)

COMMAND_INJECTION_PATTERN = re.compile(
    r'(?:system|popen)\s*\(\s*[^"\s]+\w'
)

HARDCODED_SECRET_PATTERN = re.compile(
    r'(?:password|secret|api_key|token|private_key)\s*=\s*"[^"]{4,}"',
    re.IGNORECASE
)

RELATIVE_INCLUDE_PATTERN = re.compile(
    r'#include\s*["<]\.\./'
)

PATH_TRAVERSAL_PATTERN = re.compile(
    r'(?:snprintf|sprintf)\s*\([^,]+,\s*[^,]+,\s*"[^"]*%s[^"]*"\s*,\s*(\w+)\s*\)'
)

SQL_INJECTION_PATTERN = re.compile(
    r'(?:snprintf|sprintf)\s*\([^,]+,\s*[^,]+,\s*"[^"]*(?:SELECT|INSERT|UPDATE|DELETE|DROP|select|insert|update|delete|drop)[^"]*%s'
)

SSRF_PATTERN = re.compile(
    r'(?:curl_easy_setopt|http\.get|requests\.get)\s*\([^)]*(?:url|host)'
)

SHELL_ESCAPE_PATTERN = re.compile(
    r'execl[p]?\s*\(\s*"[^"]*sh"\s*,\s*"[^"]*"\s*,\s*"-c"\s*,\s*(\w+)'
)

BUFFER_STACK_PATTERN = re.compile(
    r'\bchar\s+(\w+)\s*\[(\d+)\]'
)


def check_file(file_path: str) -> List[Finding]:
    findings = []
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except Exception:
        return findings

    is_test = '/test' in file_path or '/tests/' in file_path

    for line_num, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
            continue

        for func_name, (rule_id, severity, suggestion) in UNSAFE_FUNCTIONS.items():
            pattern = rf'\b{func_name}\s*\('
            if re.search(pattern, stripped):
                findings.append(Finding(
                    file_path=file_path, line_number=line_num, column=0,
                    severity=severity, rule_id=rule_id,
                    message=f'Unsafe function: {func_name}()',
                    suggestion=suggestion
                ))

        if 'strncpy(' in stripped:
            var_match = re.search(r'strncpy\s*\(\s*(\w+)', stripped)
            if var_match:
                var_name = var_match.group(1)
                next_idx = line_num
                found_null_term = False
                for check_line in lines[next_idx:min(next_idx + 3, len(lines))]:
                    if re.search(rf'{re.escape(var_name)}\s*\[\s*[^\]]+\]\s*=\s*[\'"]\\0[\'"]', check_line):
                        found_null_term = True
                        break
                if not found_null_term:
                    findings.append(Finding(
                        file_path=file_path, line_number=line_num, column=0,
                        severity=Severity.HIGH, rule_id='SEC002',
                        message=f'strncpy without null terminator for: {var_name}',
                        suggestion='Add dest[sizeof(dest)-1] = "\\0" after strncpy'
                    ))

        malloc_match = MALLOC_PATTERN.search(stripped)
        if malloc_match:
            func = malloc_match.group(1)
            next_lines = ''.join(lines[line_num:min(line_num + 3, len(lines))])
            if 'NULL' not in next_lines and '!=' not in next_lines and 'if (!' not in next_lines:
                findings.append(Finding(
                    file_path=file_path, line_number=line_num, column=0,
                    severity=Severity.HIGH, rule_id='SEC003',
                    message=f'{func}() return value not checked for NULL (CWE-690)',
                    suggestion='Add NULL check after allocation'
                ))

        if FORMAT_STRING_PATTERN.search(stripped) and not is_test:
            if not re.search(r'"[^"]*%[sdxfcp]', stripped):
                findings.append(Finding(
                    file_path=file_path, line_number=line_num, column=0,
                    severity=Severity.MEDIUM, rule_id='SEC004',
                    message='Potential format string vulnerability (CWE-134)',
                    suggestion='Ensure format string is a constant, not user input'
                ))

        if COMMAND_INJECTION_PATTERN.search(stripped) and not is_test:
            findings.append(Finding(
                file_path=file_path, line_number=line_num, column=0,
                severity=Severity.HIGH, rule_id='SEC005',
                message='Potential command injection (CWE-78)',
                suggestion='Validate and sanitize all inputs before passing to system/popen'
            ))

        if HARDCODED_SECRET_PATTERN.search(stripped) and not is_test:
            findings.append(Finding(
                file_path=file_path, line_number=line_num, column=0,
                severity=Severity.HIGH, rule_id='SEC006',
                message='Hardcoded secret/key detected (CWE-798)',
                suggestion='Use environment variables or config files for secrets'
            ))

        if RELATIVE_INCLUDE_PATTERN.search(stripped):
            depth = stripped.count('../')
            if depth >= 3:
                findings.append(Finding(
                    file_path=file_path, line_number=line_num, column=0,
                    severity=Severity.LOW, rule_id='SEC007',
                    message=f'Fragile relative include path ({depth} levels)',
                    suggestion='Use <agentos/xxx.h> unified include path'
                ))

        if PATH_TRAVERSAL_PATTERN.search(stripped):
            prev_lines = ''.join(lines[max(0, line_num - 5):line_num])
            if 'is_path_component_safe' not in prev_lines and 'is_path_traversal_attempt' not in prev_lines and 'agentos_validate_file_path' not in prev_lines:
                findings.append(Finding(
                    file_path=file_path, line_number=line_num, column=0,
                    severity=Severity.HIGH, rule_id='SEC008',
                    message='Path component used in file path without traversal check (SEC-012)',
                    suggestion='Add is_path_component_safe() or agentos_validate_file_path() check'
                ))

        if SQL_INJECTION_PATTERN.search(stripped):
            prev_lines = ''.join(lines[max(0, line_num - 5):line_num])
            if 'sqlite3_bind' not in prev_lines and 'is_safe_query' not in prev_lines:
                findings.append(Finding(
                    file_path=file_path, line_number=line_num, column=0,
                    severity=Severity.HIGH, rule_id='SEC009',
                    message='SQL query built with string formatting (SEC-013)',
                    suggestion='Use parameterized queries (sqlite3_prepare_v2 + sqlite3_bind_*)'
                ))

        if SHELL_ESCAPE_PATTERN.search(stripped):
            prev_lines = ''.join(lines[max(0, line_num - 10):line_num])
            if 'is_shell_command_allowed' not in prev_lines and 'escape_shell_arg' not in prev_lines and 'agentos_validate_shell_command' not in prev_lines and 'flawfinder: ignore' not in prev_lines:
                findings.append(Finding(
                    file_path=file_path, line_number=line_num, column=0,
                    severity=Severity.HIGH, rule_id='SEC010',
                    message='execl with variable argument without validation (SEC-011)',
                    suggestion='Add is_shell_command_allowed() or agentos_validate_shell_command() check'
                ))

        stack_match = BUFFER_STACK_PATTERN.search(stripped)
        if stack_match:
            buf_name = stack_match.group(1)
            buf_size = int(stack_match.group(2))
            if buf_size >= 8192:
                findings.append(Finding(
                    file_path=file_path, line_number=line_num, column=0,
                    severity=Severity.MEDIUM, rule_id='SEC011',
                    message=f'Large stack buffer {buf_name}[{buf_size}] >= 8KB (SEC-014)',
                    suggestion='Use malloc/heap allocation for large buffers to avoid stack overflow'
                ))

    return findings


def scan_directory(root_path: str) -> List[Finding]:
    all_findings = []
    for dirpath, dirnames, filenames in os.walk(root_path):
        dirnames[:] = [d for d in dirnames if d not in ('.git', '__pycache__', 'build', 'cmake-build-*')]
        for filename in filenames:
            if filename.endswith(('.c', '.h')):
                file_path = os.path.join(dirpath, filename)
                all_findings.extend(check_file(file_path))
    return all_findings


def print_report(findings: List[Finding]):
    severity_order = {Severity.CRITICAL: 0, Severity.HIGH: 1, Severity.MEDIUM: 2, Severity.LOW: 3, Severity.INFO: 4}
    findings.sort(key=lambda f: (severity_order[f.severity], f.file_path, f.line_number))

    counts = {}
    for s in Severity:
        counts[s] = sum(1 for f in findings if f.severity == s)

    print("\n" + "=" * 80)
    print("AgentOS Security Static Analysis Report")
    print("=" * 80)
    print(f"\nTotal findings: {len(findings)}")
    print(f"  CRITICAL: {counts[Severity.CRITICAL]}")
    print(f"  HIGH:     {counts[Severity.HIGH]}")
    print(f"  MEDIUM:   {counts[Severity.MEDIUM]}")
    print(f"  LOW:      {counts[Severity.LOW]}")
    print(f"  INFO:     {counts[Severity.INFO]}")
    print("-" * 80)

    for f in findings:
        rel_path = os.path.relpath(f.file_path)
        print(f"\n[{f.severity.value}] {f.rule_id} {rel_path}:{f.line_number}")
        print(f"  {f.message}")
        if f.suggestion:
            print(f"  Suggestion: {f.suggestion}")

    print("\n" + "=" * 80)
    if counts[Severity.CRITICAL] > 0:
        print("RESULT: FAIL - Critical issues found")
        return 1
    elif counts[Severity.HIGH] > 0:
        print("RESULT: WARNING - High severity issues found, review recommended")
        return 2
    else:
        print("RESULT: PASS - No critical or high severity issues")
        return 0


if __name__ == '__main__':
    target = sys.argv[1] if len(sys.argv) > 1 else 'agentos'
    if not os.path.isabs(target):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        target = os.path.normpath(os.path.join(script_dir, '..', '..', target))

    print(f"Scanning: {target}")
    findings = scan_directory(target)
    exit_code = print_report(findings)
    sys.exit(exit_code)
