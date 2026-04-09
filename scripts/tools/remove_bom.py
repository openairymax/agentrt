#!/usr/bin/env python3
"""
UTF-8 BOM Remover

移除指定目录下的所有UTF-8 BOM标记

Usage:
    python remove_bom.py [--dry-run] [--directory PATH]
"""

import argparse
import os
import sys
from pathlib import Path
from typing import List, Set


class BOMRemover:
    """UTF-8 BOM移除器"""

    # UTF-8 BOM 标记
    UTF8_BOM = b'\xef\xbb\xbf'

    def __init__(self, directory: Path, dry_run: bool = False):
        self.directory = directory
        self.dry_run = dry_run
        self.processed: List[str] = []
        self.removed: List[str] = []
        self.failed: List[tuple] = []

    def scan_directory(self) -> Set[Path]:
        """扫描目录下所有包含BOM的文件"""
        bom_files = set()

        for file_path in self.directory.rglob("*.py"):
            if self._has_bom(file_path):
                bom_files.add(file_path)

        return bom_files

    def _has_bom(self, file_path: Path) -> bool:
        """检查文件是否包含BOM"""
        try:
            with open(file_path, 'rb') as f:
                header = f.read(3)
                return header == self.UTF8_BOM
        except Exception as e:
            print(f"⚠️  Error reading {file_path}: {e}")
            return False

    def remove_bom(self, file_path: Path) -> bool:
        """移除文件的BOM标记"""
        try:
            # 读取文件内容
            with open(file_path, 'rb') as f:
                content = f.read()

            # 检查是否有BOM
            if content[:3] != self.UTF8_BOM:
                return False

            # 移除BOM
            content = content[3:]

            if not self.dry_run:
                # 写回文件
                with open(file_path, 'wb') as f:
                    f.write(content)

            return True

        except Exception as e:
            self.failed.append((str(file_path), str(e)))
            return False

    def process_files(self, files: Set[Path]) -> None:
        """批量处理文件"""
        print(f"\n📊 Processing {len(files)} files with BOM...\n")

        for i, file_path in enumerate(sorted(files), 1):
            print(f"[{i}/{len(files)}] Processing: {file_path}", end=" ... ")

            if self.remove_bom(file_path):
                self.removed.append(str(file_path))
                print("✅ Removed BOM")
            else:
                print("⏭️  Skipped")

        print(f"\n{'='*70}")
        print(f"📊 Summary:")
        print(f"   Total files found: {len(files)}")
        print(f"   BOM removed: {len(self.removed)}")
        print(f"   Failed: {len(self.failed)}")
        print(f"{'='*70}\n")

        if self.failed:
            print("❌ Failed files:")
            for file_path, error in self.failed:
                print(f"   - {file_path}: {error}")

    def generate_report(self) -> str:
        """生成处理报告"""
        report = f"""# UTF-8 BOM Removal Report

**Date**: {self._get_timestamp()}
**Directory**: {self.directory}
**Mode**: {"DRY RUN" if self.dry_run else "LIVE"}

## Summary

| Metric | Count |
|--------|-------|
| Files Found | {len(self.processed)} |
| BOM Removed | {len(self.removed)} |
| Failed | {len(self.failed)} |

## Removed Files

"""
        for file_path in sorted(self.removed):
            report += f"- `{file_path}`\n"

        if self.failed:
            report += "\n## Failed Files\n\n"
            for file_path, error in self.failed:
                report += f"- `{file_path}`: {error}\n"

        return report

    def _get_timestamp(self) -> str:
        """获取时间戳"""
        from datetime import datetime
        return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="Remove UTF-8 BOM from Python files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 预览模式（不实际修改）
  python remove_bom.py --dry-run

  # 移除当前目录及子目录的BOM
  python remove_bom.py

  # 指定目录
  python remove_bom.py --directory /path/to/project

  # 输出报告
  python remove_bom.py --output report.md
        """
    )

    parser.add_argument(
        "--directory", "-d",
        type=Path,
        default=Path(__file__).parent.parent,
        help="Directory to scan (default: parent of script)"
    )

    parser.add_argument(
        "--dry-run", "-n",
        action="store_true",
        help="Preview mode (don't actually remove BOM)"
    )

    parser.add_argument(
        "--output", "-o",
        type=Path,
        help="Output report file"
    )

    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Verbose output"
    )

    args = parser.parse_args()

    print(f"{'='*70}")
    print(f"🔍 UTF-8 BOM Remover")
    print(f"{'='*70}")
    print(f"Directory: {args.directory}")
    print(f"Mode: {'DRY RUN (preview only)' if args.dry_run else 'LIVE (will modify files)'}")
    print(f"{'='*70}\n")

    remover = BOMRemover(args.directory, dry_run=args.dry_run)

    # 扫描BOM文件
    print("🔍 Scanning for files with BOM...")
    bom_files = remover.scan_directory()

    if not bom_files:
        print("\n✅ No files with BOM found!")
        sys.exit(0)

    print(f"Found {len(bom_files)} files with BOM\n")

    # 处理文件
    remover.process_files(bom_files)

    # 生成报告
    report = remover.generate_report()

    # 输出报告
    if args.output:
        args.output.write_text(report, encoding='utf-8')
        print(f"📄 Report saved to {args.output}")
    else:
        print(report)

    # 返回状态码
    if remover.failed:
        sys.exit(1)
    elif args.dry_run:
        print("\n💡 This was a dry run. Run without --dry-run to actually remove BOM.")
        sys.exit(0)
    else:
        print("\n✅ BOM removal completed!")
        sys.exit(0)


if __name__ == "__main__":
    main()
