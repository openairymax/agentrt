#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
项目级 UTF-8 编码统一转换脚本
修复所有非 UTF-8 编码或编码损坏的文件

作者：AgentOS Team
日期：2026-04-09
"""

import os
import sys
from pathlib import Path
from typing import Tuple, Optional
import chardet

# 需要转换的文件扩展名
TARGET_EXTENSIONS = {
    '.md', '.txt', '.json', '.yaml', '.yml', '.toml',
    '.py', '.js', '.ts', '.java', '.c', '.cpp', '.h', '.hpp',
    '.go', '.rs', '.sh', '.ps1', '.xml', '.html', '.css',
    '.sql', '.ini', '.cfg', '.conf', '.env', '.gitignore'
}

# 需要排除的目录
EXCLUDE_DIRS = {
    '.git', 'node_modules', '__pycache__', 'venv', 'env',
    '.venv', 'build', 'dist', 'target', 'bin', 'obj',
    '.vscode', '.idea', '.trae'
}

# 乱码特征字符（UTF-8 被当作 GBK 读取后的典型乱码）
GARBLED_PATTERNS = [
    '宸ョ▼', '涓よ', '缁存', '姝ｄ氦', '浣撶郴', '骞惰', '璁哄',
    '鎶€鏈', '鏋舵瀯', '璁捐', '鍘熷垯', '鐞嗚', '鍐呮牳', '璁ょ煡',
    '璁板繂', '绠＄悊', '寮€鍙', '鎸囧崡', '瑙勮寖', '瀹夊叏',
    '缂栫爜', '鏃ュ織', '浠诲姟', '浼氳瘽', '鍙傝€', '鏂囨。',
    '閮ㄧ讲', '杩佺Щ', '璋冧紭', '鏁呴殰', '鎺掗櫎', '蹇€熷叆闂',
    '鍒涘缓', '鎶€鏈櫧鐨', '閫傚悎', '娴佺▼', '妯℃澘', '椤圭洰',
    '椤圭洰浠ｇ爜', '椤圭洰浠ｇ爜椋庢牸', '椤圭洰浠ｇ爜椋庢牸缁熶竴瑙勮寖',
    '閿?', '锘?', '?--', '?#', '?Copyright'
]


def is_garbled(content: str) -> bool:
    """检测文本是否包含乱码"""
    for pattern in GARBLED_PATTERNS:
        if pattern in content:
            return True
    return False


def detect_encoding(file_path: Path) -> str:
    """检测文件编码"""
    with open(file_path, 'rb') as f:
        raw = f.read(100000)  # 读取前 100KB
    
    result = chardet.detect(raw)
    encoding = result['encoding']
    confidence = result['confidence']
    
    # 如果检测结果是 ASCII，当作 UTF-8 处理
    if encoding == 'ascii':
        return 'utf-8'
    
    # 如果置信度低于 0.7，默认使用 UTF-8
    if confidence < 0.7:
        return 'utf-8'
    
    return encoding.lower() if encoding else 'utf-8'


def try_decode(content_bytes: bytes, encoding: str) -> Tuple[bool, Optional[str]]:
    """尝试用指定编码解码"""
    try:
        decoded = content_bytes.decode(encoding)
        return True, decoded
    except (UnicodeDecodeError, LookupError):
        return False, None


def fix_garbled_content(content: str) -> str:
    """修复乱码内容（UTF-8 被当作 GBK 读取的情况）"""
    try:
        # 将当前字符串编码为 UTF-8 字节
        utf8_bytes = content.encode('utf-8')
        # 尝试用 GBK 解码
        try:
            fixed = utf8_bytes.decode('gbk')
            # 验证修复后的内容是否合理
            if is_garbled(fixed):
                return content  # 修复后仍有乱码，返回原内容
            return fixed
        except (UnicodeDecodeError, UnicodeEncodeError):
            return content
    except (UnicodeEncodeError, UnicodeDecodeError):
        return content


def process_file(file_path: Path, dry_run: bool = False) -> Tuple[bool, str]:
    """
    处理单个文件
    返回：(是否已修复，消息)
    """
    try:
        # 读取原始字节
        with open(file_path, 'rb') as f:
            raw_bytes = f.read()
        
        # 检测编码
        detected_encoding = detect_encoding(file_path)
        
        # 尝试用检测到的编码解码
        success, content = try_decode(raw_bytes, detected_encoding)
        
        if not success:
            # 如果检测的编码解码失败，尝试 UTF-8
            success, content = try_decode(raw_bytes, 'utf-8')
        
        if not success:
            # 如果 UTF-8 也失败，尝试 GBK
            success, content = try_decode(raw_bytes, 'gbk')
        
        if not success:
            return False, f"无法解码文件（尝试了 {detected_encoding}, UTF-8, GBK）"
        
        # 检查是否有乱码
        has_garbled = is_garbled(content)
        
        if has_garbled:
            # 尝试修复乱码
            fixed_content = fix_garbled_content(content)
            if not is_garbled(fixed_content):
                content = fixed_content
                message = f"已修复乱码（原编码：{detected_encoding}）"
            else:
                message = f"警告：检测到乱码但无法自动修复（原编码：{detected_encoding}）"
        else:
            message = f"编码正常（{detected_encoding}）"
        
        # 写入 UTF-8（无 BOM）
        if not dry_run:
            with open(file_path, 'w', encoding='utf-8', newline='') as f:
                f.write(content)
        
        return True, message
        
    except Exception as e:
        return False, f"处理失败：{str(e)}"


def scan_and_fix(root_dir: Path, dry_run: bool = False) -> dict:
    """扫描并修复目录下所有文件"""
    stats = {
        'total_scanned': 0,
        'total_fixed': 0,
        'total_failed': 0,
        'total_skipped': 0,
        'garbled_found': 0,
        'details': []
    }
    
    print(f"开始扫描目录：{root_dir}")
    print(f"目标文件扩展名：{', '.join(sorted(TARGET_EXTENSIONS))}")
    print(f"排除目录：{', '.join(sorted(EXCLUDE_DIRS))}")
    print("=" * 80)
    
    for file_path in root_dir.rglob('*'):
        # 跳过目录
        if file_path.is_dir():
            continue
        
        # 检查扩展名
        if file_path.suffix.lower() not in TARGET_EXTENSIONS:
            continue
        
        # 跳过排除的目录
        skip = False
        for part in file_path.parts:
            if part in EXCLUDE_DIRS:
                skip = True
                break
        if skip:
            continue
        
        stats['total_scanned'] += 1
        relative_path = file_path.relative_to(root_dir)
        
        # 处理文件
        success, message = process_file(file_path, dry_run)
        
        if success:
            if '已修复' in message or '警告' in message:
                stats['total_fixed'] += 1
                if '已修复' in message:
                    stats['garbled_found'] += 1
                print(f"✓ {relative_path}")
                print(f"  {message}")
            else:
                stats['total_skipped'] += 1
        else:
            stats['total_failed'] += 1
            print(f"✗ {relative_path}")
            print(f"  {message}")
        
        stats['details'].append({
            'file': str(relative_path),
            'success': success,
            'message': message
        })
    
    print("=" * 80)
    print(f"扫描完成!")
    print(f"总扫描文件数：{stats['total_scanned']}")
    print(f"已修复：{stats['total_fixed']} (其中发现乱码：{stats['garbled_found']})")
    print(f"失败：{stats['total_failed']}")
    print(f"跳过（无需修复）：{stats['total_skipped']}")
    
    return stats


def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description='项目级 UTF-8 编码统一转换工具')
    parser.add_argument('root_dir', nargs='?', default='.', 
                       help='要扫描的根目录（默认：当前目录）')
    parser.add_argument('--dry-run', action='store_true',
                       help='仅检测，不实际修改文件')
    parser.add_argument('--output-stats', type=str,
                       help='输出统计信息到指定文件')
    
    args = parser.parse_args()
    
    root_path = Path(args.root_dir).resolve()
    
    if not root_path.exists():
        print(f"错误：目录不存在：{root_path}")
        sys.exit(1)
    
    print(f"\n{'=' * 80}")
    print(f"项目级 UTF-8 编码统一转换工具")
    print(f"{'=' * 80}\n")
    
    if args.dry_run:
        print("【干运行模式】仅检测，不修改文件\n")
    
    stats = scan_and_fix(root_path, args.dry_run)
    
    # 输出统计信息到文件
    if args.output_stats:
        import json
        with open(args.output_stats, 'w', encoding='utf-8') as f:
            json.dump(stats, f, ensure_ascii=False, indent=2)
        print(f"\n统计信息已保存到：{args.output_stats}")
    
    # 如果有失败的文件，返回非零退出码
    sys.exit(0 if stats['total_failed'] == 0 else 1)


if __name__ == '__main__':
    main()
