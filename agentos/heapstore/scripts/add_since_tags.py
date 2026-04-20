#!/usr/bin/env python3
"""
批量为 heapstore 头文件添加 @since 版本标记

使用示例:
    python3 add_since_tags.py

此脚本会：
1. 扫描 include/*.h 文件
2. 查找所有缺少 @since 标记的公共 API
3. 在 @see 或 */ 之前添加 @since v1.0.0
4. 保留已有 @since 标记的 API（如 token 模块的 v1.0.0.6）
"""

import os
import re
import glob

def add_since_to_file(filepath, default_version="v1.0.0"):
    """为单个文件添加 @since 标记"""
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    count = 0
    
    # 匹配 Doxygen 注释块中的函数声明
    # 查找 /** ... */ 后紧跟函数声明的模式
    pattern = r'(/\*\*.*?)(@see\s+\w+\(\))(\s*\*/\s*\n\s*\w+\s+\w+\()'
    
    def replace_func(match):
        nonlocal count
        prefix = match.group(1)
        see_tag = match.group(2)
        closing = match.group(3)
        
        # 检查是否已有 @since 标记
        if '@since' in prefix:
            return match.group(0)  # 已有，跳过
        
        # 检查是否是公共 API（heapstore_ 开头）
        full_match = match.group(0)
        if 'heapstore_' not in full_match:
            return match.group(0)
        
        # 添加 @since 标记
        count += 1
        return f'{prefix}{see_tag}\n * @since {default_version}{closing}'
    
    content = re.sub(pattern, replace_func, content, flags=re.DOTALL)
    
    # 处理没有 @see 但有函数声明的注释块
    pattern2 = r'(/\*\*.*?)(@reentrant\s+\w+\s*)(\*/\s*\n\s*\w+\s+\w+\()'
    
    def replace_func2(match):
        nonlocal count
        prefix = match.group(1)
        reentrant = match.group(2)
        closing = match.group(3)
        
        if '@since' in prefix:
            return match.group(0)
        
        full_match = match.group(0)
        if 'heapstore_' not in full_match:
            return match.group(0)
        
        count += 1
        return f'{prefix}{reentrant}\n * @since {default_version}{closing}'
    
    content = re.sub(pattern2, replace_func2, content, flags=re.DOTALL)
    
    # 写回文件
    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  ✅ {filepath}: 添加了 {count} 个 @since 标记")
    else:
        print(f"  ℹ️  {filepath}: 无需修改")
    
    return count

def main():
    print("=" * 60)
    print(" heapstore 模块 @since 标记批量添加工具")
    print("=" * 60)
    print()
    
    # 扫描 include 目录下的所有.h 文件
    include_dir = os.path.join(os.path.dirname(__file__), 'include')
    header_files = glob.glob(os.path.join(include_dir, '*.h'))
    
    total_count = 0
    
    for filepath in sorted(header_files):
        filename = os.path.basename(filepath)
        print(f"处理：{filename}")
        
        # 特殊处理 token 模块（已有部分@since 标记）
        if 'token' in filename:
            count = add_since_to_file(filepath, "v1.0.0.6")
        elif 'batch' in filename:
            count = add_since_to_file(filepath, "v1.0.0.5")
        else:
            count = add_since_to_file(filepath, "v1.0.0")
        
        total_count += count
    
    print()
    print("=" * 60)
    print(f"完成！共添加 {total_count} 个 @since 标记")
    print("=" * 60)

if __name__ == '__main__':
    main()
