#!/usr/bin/env python3
"""
检查测试文件语法
"""

import ast
import os
import sys
from pathlib import Path

def check_python_file(file_path):
    """检查Python文件语法"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        ast.parse(content)
        return True, None
    except SyntaxError as e:
        return False, f"Syntax error at line {e.lineno}: {e.msg}"
    except Exception as e:
        return False, f"Error reading file: {str(e)}"

def main():
    """主函数"""
    test_dir = Path(__file__).parent

    # 检查所有Python测试文件
    python_files = list(test_dir.rglob("*.py"))

    print(f"检查 {len(python_files)} 个Python文件...")

    errors = []
    for file_path in python_files:
        if file_path.name == "check_syntax.py":
            continue

        is_valid, error_msg = check_python_file(file_path)
        if not is_valid:
            rel_path = file_path.relative_to(test_dir)
            errors.append((rel_path, error_msg))
            print(f"❌ {rel_path}: {error_msg}")
        else:
            rel_path = file_path.relative_to(test_dir)
            print(f"✅ {rel_path}")

    if errors:
        print(f"\n发现 {len(errors)} 个语法错误:")
        for file_path, error_msg in errors:
            print(f"  - {file_path}: {error_msg}")
        return 1
    else:
        print("\n✅ 所有Python文件语法正确")
        return 0

if __name__ == "__main__":
    sys.exit(main())
