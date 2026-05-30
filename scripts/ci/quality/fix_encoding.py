#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Unified Encoding Fix Tool
=========================
Merged from three encoding utility scripts:
  - check_encoding.py: Check and convert document encodings
  - fix_bom.py: Remove BOM (Byte Order Mark) from files
  - fix_double_encoding.py: Fix double encoding issues (UTF-8 misinterpreted as GBK)

Usage:
    python fix_encoding.py check [--convert] [--ext EXT] [ROOT_DIR]
    python fix_encoding.py fix-bom [--fix] [ROOT_DIR]
    python fix_encoding.py fix-double [--scan-only] [--fix] [--file FILE] [--dir DIR] [ROOT_DIR]

If no subcommand is given, defaults to 'check'.
"""

import argparse
import codecs
import os
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Optional, List, Tuple, Dict, Any


# ============================================================
# Shared utilities
# ============================================================

def find_text_files(root_dir: str, extension: Optional[str] = None) -> List[str]:
    text_extensions = ['.txt', '.md', '.rst', '.py', '.c', '.h', '.cpp',
                       '.hpp', '.java', '.js', '.ts', '.json', '.yml', '.yaml']

    if extension:
        if not extension.startswith('.'):
            extension = '.' + extension
        text_extensions = [extension]

    files = []
    for dirpath, dirnames, filenames in os.walk(root_dir):
        dirnames[:] = [d for d in dirnames if not d.startswith('.')]
        for filename in filenames:
            file_ext = os.path.splitext(filename)[1].lower()
            if file_ext in text_extensions:
                full_path = os.path.join(dirpath, filename)
                files.append(full_path)

    return files


def detect_file_encoding(file_path: str) -> Tuple[Optional[str], float]:
    try:
        import chardet
        with open(file_path, 'rb') as f:
            raw_data = f.read(4096)
            if not raw_data:
                return None, 0.0
            result = chardet.detect(raw_data)
            return result['encoding'], result['confidence']
    except Exception:
        return None, 0.0


def convert_to_utf8(file_path: str, source_encoding: str) -> bool:
    try:
        with open(file_path, 'r', encoding=source_encoding, errors='ignore') as f:
            content = f.read()
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    except Exception:
        return False


# ============================================================
# check subcommand: encoding checker
# ============================================================

class EncodingCheckerConfig:
    def __init__(self, convert: bool = False, extension: Optional[str] = None):
        self.convert = convert
        self.extension = extension

    @classmethod
    def from_args(cls, args) -> 'EncodingCheckerConfig':
        return cls(convert=args.convert, extension=args.ext)


class FileScanner:
    def __init__(self, root_dir: Path):
        self.root_dir = root_dir

    def scan(self, extension: Optional[str] = None) -> List[Path]:
        files = find_text_files(str(self.root_dir), extension)
        return [Path(f) for f in files]


class EncodingDetector:
    def detect(self, file_path: Path) -> Tuple[Optional[str], float]:
        return detect_file_encoding(str(file_path))

    def categorize(self, encoding: Optional[str], confidence: float) -> str:
        if not encoding:
            return 'unknown'
        elif encoding.lower() in ['utf-8', 'utf8', 'ascii']:
            return 'utf8'
        else:
            return 'non_utf8'


class EncodingConverter:
    def __init__(self, root_dir: Path):
        self.root_dir = root_dir

    def convert(self, file_path: Path, source_encoding: str) -> bool:
        return convert_to_utf8(str(file_path), source_encoding)

    def batch_convert(self, files: List[Tuple[Path, str]]) -> Dict[str, int]:
        results = {'converted': 0, 'failed': 0}
        for file_path, encoding in files:
            if self.convert(file_path, encoding):
                results['converted'] += 1
                print(f"  ✅ Converted: {file_path.relative_to(self.root_dir)}")
            else:
                results['failed'] += 1
                print(f"  ❌ Failed: {file_path.relative_to(self.root_dir)}")
        return results


class ProgressMonitor:
    def __init__(self, total: int):
        self.total = total
        self.processed = 0

    def update(self, count: int = 1):
        self.processed += count
        if self.processed % 100 == 0:
            print(f"  ⏳ Processed {self.processed}/{self.total} files...")

    def finish(self):
        print(f"  ✅ Completed {self.processed}/{self.total} files")


class EncodingReport:
    def __init__(self):
        self.categories = defaultdict(list)

    def add_file(self, file_path: Path, category: str,
                 encoding: Optional[str] = None, confidence: float = 0.0):
        if category == 'unknown':
            self.categories[category].append(file_path)
        else:
            self.categories[category].append((file_path, encoding, confidence))

    def print_summary(self):
        print("=" * 80)
        print(f"✅ UTF-8/ASCII files: {len(self.categories.get('utf8', []))}")
        print(f"⚠️  Non-UTF-8 files: {len(self.categories.get('non_utf8', []))}")
        print(f"❓ Unknown encoding: {len(self.categories.get('unknown', []))}")
        print("=" * 80)

    def print_detailed(self, root_dir: Path):
        if 'non_utf8' in self.categories and self.categories['non_utf8']:
            print("\n⚠️  Files needing conversion:")
            print("-" * 80)
            for file_path, encoding, confidence in self.categories['non_utf8']:
                rel_path = file_path.relative_to(root_dir)
                status = "🔴 LOW CONFIDENCE" if confidence < 0.7 else "🟡 MEDIUM" if confidence < 0.9 else "🟢 HIGH"
                print(f"  [{status}] {rel_path}")
                print(f"       Encoding: {encoding} (confidence: {confidence:.2%})")

        if 'unknown' in self.categories and self.categories['unknown']:
            print("\n❓ Files with unknown encoding (possibly empty or binary):")
            for file_path in self.categories['unknown'][:20]:
                rel_path = file_path.relative_to(root_dir)
                print(f"  - {rel_path}")
            if len(self.categories['unknown']) > 20:
                print(f"  ... and {len(self.categories['unknown']) - 20} more")


class EncodingChecker:
    def __init__(self, root_dir: Path, config: EncodingCheckerConfig):
        self.root_dir = root_dir
        self.config = config
        self.scanner = FileScanner(root_dir)
        self.detector = EncodingDetector()
        self.converter = EncodingConverter(root_dir) if config.convert else None
        self.report = EncodingReport()

    def run(self) -> int:
        print(f"🔍 Scanning documents in: {self.root_dir}")

        files = self.scanner.scan(self.config.extension)
        print(f"📊 Found {len(files)} text files")

        print("📋 Checking file encodings...")
        monitor = ProgressMonitor(len(files))

        for file_path in files:
            encoding, confidence = self.detector.detect(file_path)
            category = self.detector.categorize(encoding, confidence)
            self.report.add_file(file_path, category, encoding, confidence)
            monitor.update()

        monitor.finish()

        self.report.print_summary()
        self.report.print_detailed(self.root_dir)

        if self.config.convert and 'non_utf8' in self.report.categories:
            self._convert_files()

        return 0

    def _convert_files(self):
        files_to_convert = self.report.categories['non_utf8']
        print(f"\n🔄 Converting {len(files_to_convert)} files to UTF-8...")

        files = [(file_path, encoding) for file_path, encoding, _ in files_to_convert]

        results = self.converter.batch_convert(files)
        print(f"\n✅ Conversion complete: {results['converted']} converted, {results['failed']} failed")

        if results['converted'] > 0:
            self._verify_conversion(files_to_convert)

    def _verify_conversion(self, original_files: List[Tuple[Path, str, float]]):
        print("\n🔍 Verifying conversions...")
        still_non_utf8 = []

        for file_path, _, _ in original_files:
            encoding, confidence = self.detector.detect(file_path)
            category = self.detector.categorize(encoding, confidence)
            if category == 'non_utf8':
                still_non_utf8.append((file_path.relative_to(self.root_dir), encoding, confidence))

        if still_non_utf8:
            print(f"⚠️  Warning: {len(still_non_utf8)} files still not UTF-8:")
            for rel_path, enc, conf in still_non_utf8:
                print(f"  - {rel_path} ({enc})")
        else:
            print("✅ All files successfully converted to UTF-8!")


def cmd_check(args) -> int:
    root_dir = Path(args.root_dir) if args.root_dir else Path.cwd()
    config = EncodingCheckerConfig(convert=args.convert, extension=args.ext)
    checker = EncodingChecker(root_dir, config)
    return checker.run()


# ============================================================
# fix-bom subcommand: BOM remover
# ============================================================

def remove_all_boms(content: bytes) -> bytes:
    bom = b'\xef\xbb\xbf'
    while content.startswith(bom):
        content = content[3:]
    return content


def fix_bom_file(file_path: str) -> Tuple[bool, Any]:
    try:
        with open(file_path, 'rb') as f:
            original_content = f.read()

        if not original_content.startswith(b'\xef\xbb\xbf'):
            return False, "no_bom"

        fixed_content = remove_all_boms(original_content)

        with open(file_path, 'wb') as f:
            f.write(fixed_content)

        original_size = len(original_content)
        fixed_size = len(fixed_content)
        removed_bytes = original_size - fixed_size

        return True, {
            'original_size': original_size,
            'fixed_size': fixed_size,
            'removed_bytes': removed_bytes,
            'bom_count': removed_bytes // 3
        }

    except Exception as e:
        return False, str(e)


def find_files_with_bom(root_dir: str) -> Tuple[List[Tuple[str, str]], int]:
    skip_dirs = {
        '.git', '__pycache__', 'node_modules', '.venv', 'venv',
        'build', 'dist', '.tox', '.mypy_cache', '.pytest_cache',
        '*.egg-info', '.next', '.nuxt', 'target', 'vendor',
        '.idea', '.vscode'
    }

    bom_files = []
    total_files = 0

    for root, dirs, files in os.walk(root_dir):
        dirs[:] = [d for d in dirs if d not in skip_dirs and not d.startswith('.')]

        for file in files:
            full_path = os.path.join(root, file)

            ext = Path(file).suffix.lower()
            binary_exts = {'.png', '.jpg', '.jpeg', '.gif', '.ico', '.webp',
                           '.pdf', '.exe', '.dll', '.so', '.zip', '.tar', '.gz'}
            if ext in binary_exts:
                continue

            total_files += 1

            try:
                with open(full_path, 'rb') as f:
                    first_bytes = f.read(3)
                    if first_bytes == b'\xef\xbb\xbf':
                        rel_path = os.path.relpath(full_path, root_dir)
                        bom_files.append((rel_path, full_path))
            except Exception:
                pass

            if total_files % 500 == 0:
                print(f"  ⏳ Scanned {total_files} files...")

    return bom_files, total_files


def cmd_fix_bom(args) -> int:
    root_dir = Path(args.root_dir) if args.root_dir else Path.cwd()

    print("=" * 80)
    print("🔧 Ultimate BOM Remover")
    print("=" * 80)
    print(f"📂 Scanning: {root_dir}")
    print()

    print("🔍 Scanning for BOM...")
    bom_files, total_files = find_files_with_bom(str(root_dir))

    print()
    print("=" * 80)
    print(f"📊 Results:")
    print(f"   Total files scanned: {total_files}")
    print(f"   Files with BOM: {len(bom_files)}")
    print(f"   Clean files: {total_files - len(bom_files)}")
    print("=" * 80)

    if not bom_files:
        print("\n✅ Perfect! No files with BOM found.")
        return 0

    print(f"\n📋 Files with BOM ({len(bom_files)}):")
    print("-" * 80)

    for rel_path, _ in bom_files:
        print(f"  📄 {rel_path}")

    if args.fix:
        print()
        print(f"\n🔧 Fixing {len(bom_files)} files...")
        print("-" * 80)

        success_count = 0
        fail_count = 0
        total_removed = 0

        for rel_path, full_path in bom_files:
            success, result = fix_bom_file(full_path)

            if success:
                success_count += 1
                info = result
                total_removed += info['removed_bytes']
                print(f"  ✅ {rel_path}")
                print(f"     Removed {info['bom_count']} BOM(s) ({info['removed_bytes']} bytes)")
                print(f"     Size: {info['original_size']} → {info['fixed_size']} bytes")
            else:
                fail_count += 1
                if result == "no_bom":
                    print(f"  ⚠️  {rel_path}: Already clean (race condition?)")
                else:
                    print(f"  ❌ {rel_path}: {result}")

        print()
        print("=" * 80)
        print(f"✅ Fix Complete:")
        print(f"   ✅ Successfully fixed: {success_count}")
        print(f"   ❌ Failed: {fail_count}")
        print(f"   💾 Total bytes removed: {total_removed}")
        print("=" * 80)

        print("\n🔍 Final verification...")
        bom_files_after, _ = find_files_with_bom(str(root_dir))

        if bom_files_after:
            print(f"\n⚠️  Warning: {len(bom_files_after)} files still have BOM!")
            for rel_path, _ in bom_files_after[:10]:
                print(f"  - {rel_path}")
            if len(bom_files_after) > 10:
                print(f"  ... and {len(bom_files_after) - 10} more")
        else:
            print("\n🎉 Success! All files are now clean UTF-8 (no BOM)!")
    else:
        print()
        print("\n💡 To fix these files, run:")
        print(f"   python {sys.argv[0]} fix-bom --fix")

    return 0


# ============================================================
# fix-double subcommand: double encoding fixer
# ============================================================

def detect_double_encoding(text: str) -> bool:
    double_encoding_patterns = [
        r'绯', r'荤', r'粺', r'鏋', r'舵', r'瀯', r'璁', r'捐', r'',
        r'涓', r'庡', r'璇', r'勫', r'', r'鑳', r'戒', r'綋'
    ]

    count = 0
    for pattern in double_encoding_patterns:
        if re.search(pattern, text):
            count += 1
            if count >= 2:
                return True
    return False


def fix_double_encoded_text(text: str) -> str:
    encodings_to_try = [
        ('gb18030', 'utf-8'),
        ('gbk', 'utf-8'),
        ('gb2312', 'utf-8'),
        ('latin-1', 'utf-8'),
        ('cp1252', 'utf-8'),
    ]

    original_text = text
    best_result = text
    best_score = 0

    for wrong_enc, target_enc in encodings_to_try:
        try:
            bytes_repr = text.encode(wrong_enc, errors='ignore')
            fixed = bytes_repr.decode(target_enc, errors='ignore')

            chinese_chars = len(re.findall(r'[\u4e00-\u9fff]', fixed))
            total_chars = len(fixed)
            score = chinese_chars / max(total_chars, 1)

            if score > best_score:
                best_score = score
                best_result = fixed

        except (UnicodeEncodeError, UnicodeDecodeError):
            continue

    if best_score > 0.05:
        return best_result
    else:
        return original_text


def fix_double_encoded_file(file_path: Path) -> Tuple[bool, str]:
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        original_content = content

        if detect_double_encoding(content):
            print(f"  🔍 检测到双重编码: {file_path}")

            fixed_content = fix_double_encoded_text(content)

            if fixed_content != content:
                backup_path = file_path.with_suffix(file_path.suffix + '.bak')
                with open(backup_path, 'w', encoding='utf-8') as f:
                    f.write(content)

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
    chinese_pattern = re.compile(r'[\u4e00-\u9fff]')
    target_extensions = ['.json', '.py', '.md', '.txt', '.yaml', '.yml']

    files = []

    for ext in target_extensions:
        for file_path in root_dir.rglob(f'*{ext}'):
            if file_path.is_file():
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read(4096)
                    if chinese_pattern.search(content):
                        files.append(file_path)
                except Exception:
                    continue

    return files


def cmd_fix_double(args) -> int:
    if args.file:
        file_path = Path(args.file)
        if file_path.exists():
            modified, message = fix_double_encoded_file(file_path)
            print(f"{'✅ 已修改' if modified else 'ℹ️  未修改'}: {message}")
        else:
            print(f"❌ 文件不存在: {args.file}")
        return 0

    if args.dir:
        root_dir = Path(args.dir)
        if not root_dir.exists():
            print(f"❌ 目录不存在: {args.dir}")
            return 1
    else:
        root_dir = Path(args.root_dir) if args.root_dir else Path.cwd()

    files = find_files_with_chinese(root_dir)

    if not files:
        print("❌ 未找到需要处理的文件")
        return 0

    print(f"📋 找到 {len(files)} 个文件")

    if args.scan_only:
        print("\n🔍 扫描结果:")
        print("-" * 80)

        for i, file_path in enumerate(files[:50]):
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read(2048)

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
        print(f"\n🔄 开始修复 {len(files)} 个文件...")
        print("-" * 80)

        fixed_count = 0
        error_count = 0

        for i, file_path in enumerate(files):
            print(f"[{i+1}/{len(files)}] 处理: {file_path.relative_to(root_dir)}")

            try:
                modified, message = fix_double_encoded_file(file_path)

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

    print("\n💡 请指定 --scan-only 或 --fix 来执行操作")
    return 0


# ============================================================
# CLI entry point
# ============================================================

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description='Unified encoding fix tool: check, fix-bom, fix-double'
    )
    subparsers = parser.add_subparsers(dest='command')

    # check
    p_check = subparsers.add_parser('check', help='Check and convert document encodings')
    p_check.add_argument('--convert', action='store_true',
                         help='Convert non-UTF-8 files to UTF-8')
    p_check.add_argument('--ext', type=str,
                         help='Only check files with this extension (e.g., md)')
    p_check.add_argument('root_dir', nargs='?', default=None,
                         help='Root directory to scan (default: current directory)')

    # fix-bom
    p_bom = subparsers.add_parser('fix-bom', help='Remove BOM from files')
    p_bom.add_argument('--fix', action='store_true',
                       help='Fix all files with BOM')
    p_bom.add_argument('root_dir', nargs='?', default=None,
                       help='Root directory to scan (default: current directory)')

    # fix-double
    p_double = subparsers.add_parser('fix-double',
                                     help='Fix double encoding issues (UTF-8 misinterpreted as GBK)')
    p_double.add_argument('--scan-only', action='store_true',
                          help='Only scan, do not fix')
    p_double.add_argument('--fix', action='store_true',
                          help='Execute fix')
    p_double.add_argument('--file', type=str,
                          help='Fix a single file')
    p_double.add_argument('--dir', type=str,
                          help='Fix files in specified directory')
    p_double.add_argument('root_dir', nargs='?', default=None,
                          help='Root directory to scan (default: current directory)')

    return parser


def main():
    parser = build_parser()

    if len(sys.argv) > 1 and sys.argv[1] in ('check', 'fix-bom', 'fix-double'):
        args = parser.parse_args()
    else:
        args = parser.parse_args(['check'] + sys.argv[1:])

    if args.command == 'check':
        return cmd_check(args)
    elif args.command == 'fix-bom':
        return cmd_fix_bom(args)
    elif args.command == 'fix-double':
        return cmd_fix_double(args)
    else:
        parser.print_help()
        return 0


if __name__ == '__main__':
    sys.exit(main())
