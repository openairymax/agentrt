#!/usr/bin/env python3
"""
修复双重编码问题（UTF-8被误解释为GBK等问题）
主要解决中文乱码问题
"""

import argparse
import codecs
import os
import re
import sys
from pathlib import Path
from typing import List, Tuple, Optional

def detect_double_encoding(text: str) -> bool:
    """
    检测文本是否可能受双重编码影响
    返回True如果文本包含典型的乱码模式
    """
    # 常见双重编码模式：UTF-8字节被当作GBK解码后的字符
    # 这些字符通常在GBK编码中存在但在正常中文中不常见
    double_encoding_patterns = [
        r'绯', r'荤', r'粺', r'鏋', r'舵', r'瀯', r'璁', r'捐', r'',
        r'涓', r'庡', r'璇', r'勫', r'', r'鑳', r'戒', r'綋'
    ]
    
    # 如果有多个这样的字符，很可能是双重编码
    count = 0
    for pattern in double_encoding_patterns:
        if re.search(pattern, text):
            count += 1
            if count >= 2:  # 至少2个这样的字符
                return True
    return False

def fix_double_encoded_text(text: str) -> str:
    """
    修复双重编码的文本
    处理UTF-8被误解码为GBK的情况
    """
    # 尝试不同编码组合
    encodings_to_try = [
        ('gb18030', 'utf-8'),  # 测试表明这是最有效的修复
        ('gbk', 'utf-8'),      # 最常见的情况：UTF-8被当作GBK
        ('gb2312', 'utf-8'),
        ('latin-1', 'utf-8'),
        ('cp1252', 'utf-8'),
    ]
    
    original_text = text
    best_result = text
    best_score = 0
    
    for wrong_enc, target_enc in encodings_to_try:
        try:
            # 将文本编码回原始字节（假设它当前是错误编码的表示）
            # 然后使用正确的编码解码
            bytes_repr = text.encode(wrong_enc, errors='ignore')
            fixed = bytes_repr.decode(target_enc, errors='ignore')
            
            # 计算修复分数：有效中文字符数量
            chinese_chars = len(re.findall(r'[\u4e00-\u9fff]', fixed))
            total_chars = len(fixed)
            score = chinese_chars / max(total_chars, 1)
            
            if score > best_score:
                best_score = score
                best_result = fixed
                
        except (UnicodeEncodeError, UnicodeDecodeError):
            continue
    
    # 如果修复后中文字符比例更高，使用修复结果
    if best_score > 0.05:  # 至少5%是中文字符
        return best_result
    else:
        return original_text

def fix_file(file_path: Path) -> Tuple[bool, str]:
    """
    修复单个文件的双重编码问题
    返回 (是否修改, 错误信息)
    """
    try:
        # 读取文件内容
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        original_content = content
        
        # 检测并修复双重编码
        if detect_double_encoding(content):
            print(f"  🔍 检测到双重编码: {file_path}")
            
            # 修复整个文件内容
            fixed_content = fix_double_encoded_text(content)
            
            if fixed_content != content:
                # 备份原文件
                backup_path = file_path.with_suffix(file_path.suffix + '.bak')
                with open(backup_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                
                # 写入修复后的内容
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(fixed_content)
                
                return True, f"已修复 {file_path}，备份在 {backup_path}"
            else:
                return False, f"检测到问题但无需修复 {file_path}"
        else:
            return False, f"无双重编码问题 {file_path}"
            
    except Exception as e:
        return False, f"处理文件时出错 {file_path}: {str(e)}"

def find_files_with_chinese(root_dir: Path) -> List[Path]:
    """
    查找包含中文字符的文件
    """
    chinese_pattern = re.compile(r'[\u4e00-\u9fff]')
    target_extensions = ['.json', '.py', '.md', '.txt', '.yaml', '.yml']
    
    files = []
    
    for ext in target_extensions:
        for file_path in root_dir.rglob(f'*{ext}'):
            if file_path.is_file():
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read(4096)  # 只读取前4KB来检测
                    
                    if chinese_pattern.search(content):
                        files.append(file_path)
                        
                except Exception:
                    continue
    
    return files

def scan_openlab_files() -> List[Path]:
    """
    扫描openlab模块中的文件
    """
    openlab_root = Path("d:/SPHARX-CN/SpharxWorks/AgentOS/agentos/openlab")
    
    if not openlab_root.exists():
        print(f"❌ Openlab目录不存在: {openlab_root}")
        return []
    
    print(f"🔍 扫描Openlab目录: {openlab_root}")
    
    # 查找contract.json文件（这些文件问题最多）
    contract_files = list(openlab_root.rglob('contract.json'))
    
    # 查找Python文件
    py_files = list(openlab_root.rglob('*.py'))
    
    # 查找Markdown文件
    md_files = list(openlab_root.rglob('*.md'))
    
    # 合并所有文件
    all_files = contract_files + py_files + md_files
    
    # 去重
    unique_files = list(set(all_files))
    unique_files.sort()
    
    print(f"📊 找到 {len(unique_files)} 个潜在文件")
    return unique_files

def main():
    parser = argparse.ArgumentParser(description='修复双重编码问题（UTF-8被误解释为GBK）')
    parser.add_argument('--scan-only', action='store_true', help='仅扫描不修复')
    parser.add_argument('--fix', action='store_true', help='执行修复')
    parser.add_argument('--file', type=str, help='修复单个文件')
    parser.add_argument('--dir', type=str, help='修复指定目录')
    
    args = parser.parse_args()
    
    if args.file:
        # 修复单个文件
        file_path = Path(args.file)
        if file_path.exists():
            modified, message = fix_file(file_path)
            print(f"{'✅ 已修改' if modified else 'ℹ️  未修改'}: {message}")
        else:
            print(f"❌ 文件不存在: {args.file}")
        return 0
    
    if args.dir:
        # 修复指定目录
        root_dir = Path(args.dir)
        if not root_dir.exists():
            print(f"❌ 目录不存在: {args.dir}")
            return 1
    else:
        # 默认扫描openlab目录
        root_dir = Path("d:/SPHARX-CN/SpharxWorks/AgentOS/agentos/openlab")
    
    # 查找文件
    if root_dir.name == 'openlab':
        files = scan_openlab_files()
    else:
        files = find_files_with_chinese(root_dir)
    
    if not files:
        print("❌ 未找到需要处理的文件")
        return 0
    
    print(f"📋 找到 {len(files)} 个文件")
    
    if args.scan_only:
        # 仅扫描
        print("\n🔍 扫描结果:")
        print("-" * 80)
        
        for i, file_path in enumerate(files[:50]):  # 只显示前50个
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read(2048)  # 读取前2KB
                
                if detect_double_encoding(content):
                    print(f"  ❌ [{i+1}] {file_path.relative_to(root_dir)}")
                else:
                    print(f"  ✅ [{i+1}] {file_path.relative_to(root_dir)}")
                    
            except Exception as e:
                print(f"  ⚠️  [{i+1}] {file_path.relative_to(root_dir)} (错误: {str(e)})")
        
        if len(files) > 50:
            print(f"  ... 还有 {len(files) - 50} 个文件未显示")
        
        return 0
    
    if args.fix:
        # 执行修复
        print(f"\n🔄 开始修复 {len(files)} 个文件...")
        print("-" * 80)
        
        fixed_count = 0
        error_count = 0
        
        for i, file_path in enumerate(files):
            print(f"[{i+1}/{len(files)}] 处理: {file_path.relative_to(root_dir)}")
            
            try:
                modified, message = fix_file(file_path)
                
                if modified:
                    fixed_count += 1
                    print(f"  ✅ {message}")
                else:
                    print(f"  ℹ️  {message}")
                    
            except Exception as e:
                error_count += 1
                print(f"  ❌ 错误: {str(e)}")
        
        print("\n" + "=" * 80)
        print(f"📊 修复完成:")
        print(f"  ✅ 修复文件数: {fixed_count}")
        print(f"  ℹ️  未修改文件数: {len(files) - fixed_count - error_count}")
        print(f"  ❌ 错误文件数: {error_count}")
        
        if fixed_count > 0:
            print(f"\n💾 备份文件已保存为 .bak 扩展名")
        
        return 0
    
    # 默认显示帮助信息
    parser.print_help()
    return 0

if __name__ == '__main__':
    sys.exit(main())