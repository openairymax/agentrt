#!/usr/bin/env python3
"""
重构后的编码检查工具 - 使用面向对象设计降低复杂度
"""

import argparse
import os
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Optional, List, Tuple, Dict, Any

def find_text_files(root_dir: str, extension: Optional[str] = None) -> List[str]:
    """查找文本文件"""
    import fnmatch
    
    text_extensions = ['.txt', '.md', '.rst', '.py', '.c', '.h', '.cpp', 
                      '.hpp', '.java', '.js', '.ts', '.json', '.yml', '.yaml']
    
    if extension:
        if not extension.startswith('.'):
            extension = '.' + extension
        text_extensions = [extension]
    
    files = []
    for dirpath, dirnames, filenames in os.walk(root_dir):
        # 跳过隐藏目录和某些特殊目录
        dirnames[:] = [d for d in dirnames if not d.startswith('.')]
        
        for filename in filenames:
            file_ext = os.path.splitext(filename)[1].lower()
            if file_ext in text_extensions:
                full_path = os.path.join(dirpath, filename)
                files.append(full_path)
    
    return files

def detect_file_encoding(file_path: str) -> Tuple[Optional[str], float]:
    """检测文件编码"""
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
    """转换文件到UTF-8编码"""
    try:
        with open(file_path, 'r', encoding=source_encoding, errors='ignore') as f:
            content = f.read()
        
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        
        return True
    except Exception:
        return False

# 配置类
class EncodingCheckerConfig:
    """编码检查配置"""
    
    def __init__(self, convert: bool = False, extension: Optional[str] = None):
        self.convert = convert
        self.extension = extension
    
    @classmethod
    def from_args(cls, args) -> 'EncodingCheckerConfig':
        """从命令行参数创建配置"""
        return cls(convert=args.convert, extension=args.ext)

# 文件扫描器
class FileScanner:
    """文件扫描器"""
    
    def __init__(self, root_dir: Path):
        self.root_dir = root_dir
    
    def scan(self, extension: Optional[str] = None) -> List[Path]:
        """扫描文件"""
        files = find_text_files(str(self.root_dir), extension)
        return [Path(f) for f in files]

# 编码检测器
class EncodingDetector:
    """编码检测器"""
    
    def detect(self, file_path: Path) -> Tuple[Optional[str], float]:
        """检测文件编码"""
        return detect_file_encoding(str(file_path))
    
    def categorize(self, encoding: Optional[str], confidence: float) -> str:
        """分类编码类型"""
        if not encoding:
            return 'unknown'
        elif encoding.lower() in ['utf-8', 'utf8', 'ascii']:
            return 'utf8'
        else:
            return 'non_utf8'

# 编码转换器
class EncodingConverter:
    """编码转换器"""
    
    def __init__(self, root_dir: Path):
        self.root_dir = root_dir
    
    def convert(self, file_path: Path, source_encoding: str) -> bool:
        """转换单个文件"""
        return convert_to_utf8(str(file_path), source_encoding)
    
    def batch_convert(self, files: List[Tuple[Path, str]]) -> Dict[str, int]:
        """批量转换文件"""
        results = {'converted': 0, 'failed': 0}
        
        for file_path, encoding in files:
            if self.convert(file_path, encoding):
                results['converted'] += 1
                print(f"  ✅ Converted: {file_path.relative_to(self.root_dir)}")
            else:
                results['failed'] += 1
                print(f"  ❌ Failed: {file_path.relative_to(self.root_dir)}")
        
        return results

# 进度监控器
class ProgressMonitor:
    """进度监控器"""
    
    def __init__(self, total: int):
        self.total = total
        self.processed = 0
    
    def update(self, count: int = 1):
        """更新进度"""
        self.processed += count
        if self.processed % 100 == 0:
            print(f"  ⏳ Processed {self.processed}/{self.total} files...")
    
    def finish(self):
        """完成进度监控"""
        print(f"  ✅ Completed {self.processed}/{self.total} files")

# 编码报告
class EncodingReport:
    """编码报告"""
    
    def __init__(self):
        self.categories = defaultdict(list)
    
    def add_file(self, file_path: Path, category: str, 
                 encoding: Optional[str] = None, confidence: float = 0.0):
        """添加文件到报告"""
        if category == 'unknown':
            self.categories[category].append(file_path)
        else:
            self.categories[category].append((file_path, encoding, confidence))
    
    def print_summary(self):
        """打印摘要报告"""
        print("=" * 80)
        print(f"✅ UTF-8/ASCII files: {len(self.categories.get('utf8', []))}")
        print(f"⚠️  Non-UTF-8 files: {len(self.categories.get('non_utf8', []))}")
        print(f"❓ Unknown encoding: {len(self.categories.get('unknown', []))}")
        print("=" * 80)
    
    def print_detailed(self, root_dir: Path):
        """打印详细报告"""
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

# 主编码检查器
class EncodingChecker:
    """编码检查器主类"""
    
    def __init__(self, root_dir: Path, config: EncodingCheckerConfig):
        self.root_dir = root_dir
        self.config = config
        self.scanner = FileScanner(root_dir)
        self.detector = EncodingDetector()
        self.converter = EncodingConverter(root_dir) if config.convert else None
        self.report = EncodingReport()
    
    def run(self) -> int:
        """运行编码检查"""
        print(f"🔍 Scanning documents in: {self.root_dir}")
        
        # 扫描文件
        files = self.scanner.scan(self.config.extension)
        print(f"📊 Found {len(files)} text files")
        
        # 检测编码
        print("📋 Checking file encodings...")
        monitor = ProgressMonitor(len(files))
        
        for file_path in files:
            encoding, confidence = self.detector.detect(file_path)
            category = self.detector.categorize(encoding, confidence)
            self.report.add_file(file_path, category, encoding, confidence)
            monitor.update()
        
        monitor.finish()
        
        # 打印报告
        self.report.print_summary()
        self.report.print_detailed(self.root_dir)
        
        # 转换文件（如果需要）
        if self.config.convert and 'non_utf8' in self.report.categories:
            self._convert_files()
        
        return 0
    
    def _convert_files(self):
        """转换文件"""
        files_to_convert = self.report.categories['non_utf8']
        print(f"\n🔄 Converting {len(files_to_convert)} files to UTF-8...")
        
        # 提取路径和编码
        files = [(file_path, encoding) for file_path, encoding, _ in files_to_convert]
        
        # 执行转换
        results = self.converter.batch_convert(files)
        print(f"\n✅ Conversion complete: {results['converted']} converted, {results['failed']} failed")
        
        # 验证转换
        if results['converted'] > 0:
            self._verify_conversion(files_to_convert)
    
    def _verify_conversion(self, original_files: List[Tuple[Path, str, float]]):
        """验证转换结果"""
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

def main():
    """主函数 - 重构后复杂度<6"""
    parser = argparse.ArgumentParser(description='Check and convert document encodings')
    parser.add_argument('--convert', action='store_true', help='Convert non-UTF-8 files to UTF-8')
    parser.add_argument('--ext', type=str, help='Only check files with this extension (e.g., md)')
    args = parser.parse_args()
    
    root_dir = Path(__file__).parent.parent
    
    config = EncodingCheckerConfig(convert=args.convert, extension=args.ext)
    checker = EncodingChecker(root_dir, config)
    
    return checker.run()

if __name__ == '__main__':
    sys.exit(main())