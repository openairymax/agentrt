#!/usr/bin/env python3
"""
AgentOS Manager 模块配置版本历史清理工具

本工具用于清理过期的配置版本历史，包括：
- 自动清理超过保留期限的版本
- 压缩旧版本以节省空间
- 生成清理报告

使用方法:
    python config_version_cleanup.py --dry-run
    python config_version_cleanup.py --days 90
    python config_version_cleanup.py --max-versions 100
    python config_version_cleanup.py --compress
"""

import argparse
import gzip
import hashlib
import json
import os
import shutil
import sys
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any, Dict, List, Optional


@dataclass
class VersionInfo:
    """版本信息"""
    version_id: str
    timestamp: datetime
    file_path: str
    checksum: str
    size_bytes: int
    compressed: bool = False


@dataclass
class CleanupResult:
    """清理结果"""
    timestamp: str
    total_versions: int = 0
    kept_versions: int = 0
    deleted_versions: int = 0
    compressed_versions: int = 0
    freed_space_bytes: int = 0
    errors: List[str] = field(default_factory=list)
    deleted_files: List[str] = field(default_factory=list)
    kept_files: List[str] = field(default_factory=list)


class ConfigVersionCleanup:
    """配置版本清理器"""

    def __init__(
        self,
        history_dir: str,
        dry_run: bool = False,
        verbose: bool = False
    ):
        self.history_dir = Path(history_dir)
        self.dry_run = dry_run
        self.verbose = verbose
        self.results: List[CleanupResult] = []

    def log(self, message: str) -> None:
        """打印日志"""
        if self.verbose:
            print(f"  [Cleanup] {message}")

    def calculate_checksum(self, file_path: Path) -> str:
        """计算文件SHA256校验和"""
        sha256 = hashlib.sha256()
        with open(file_path, 'rb') as f:
            for chunk in iter(lambda: f.read(8192), b''):
                sha256.update(chunk)
        return sha256.hexdigest()

    def get_all_versions(self) -> List[VersionInfo]:
        """获取所有版本信息"""
        versions: List[VersionInfo] = []

        if not self.history_dir.exists():
            self.log(f"历史目录不存在: {self.history_dir}")
            return versions

        for version_file in self.history_dir.rglob("*.json"):
            # 跳过元数据文件
            if version_file.stem.startswith('.'):
                continue

            try:
                stat = version_file.stat()
                timestamp = datetime.fromtimestamp(stat.st_mtime)

                versions.append(VersionInfo(
                    version_id=version_file.stem,
                    timestamp=timestamp,
                    file_path=str(version_file),
                    checksum=self.calculate_checksum(version_file),
                    size_bytes=stat.st_size,
                    compressed=version_file.suffix == '.gz'
                ))
            except Exception as e:
                self.log(f"读取版本文件失败: {version_file}, 错误: {e}")

        # 按时间排序
        versions.sort(key=lambda v: v.timestamp, reverse=True)
        return versions

    def cleanup_by_retention_days(
        self,
        versions: List[VersionInfo],
        retention_days: int
    ) -> CleanupResult:
        """按保留天数清理

        Args:
            versions: 版本列表
            retention_days: 保留天数

        Returns:
            CleanupResult: 清理结果
        """
        result = CleanupResult(timestamp=datetime.now().isoformat())
        cutoff_date = datetime.now() - timedelta(days=retention_days)

        self.log(f"开始清理 {retention_days} 天前的版本...")
        self.log(f"截止日期: {cutoff_date.isoformat()}")

        for version in versions:
            result.total_versions += 1

            if version.timestamp < cutoff_date:
                if self.dry_run:
                    self.log(f"  [DRY-RUN] 将删除: {version.version_id}")
                    result.deleted_versions += 1
                else:
                    try:
                        os.remove(version.file_path)
                        self.log(f"  删除: {version.version_id}")
                        result.deleted_versions += 1
                        result.freed_space_bytes += version.size_bytes
                        result.deleted_files.append(version.version_id)
                    except Exception as e:
                        error_msg = f"删除失败: {version.version_id}, 错误: {e}"
                        self.log(f"  ❌ {error_msg}")
                        result.errors.append(error_msg)
            else:
                result.kept_versions += 1
                result.kept_files.append(version.version_id)

        return result

    def cleanup_by_max_versions(
        self,
        versions: List[VersionInfo],
        max_versions: int,
        compress_after: int = 0
    ) -> CleanupResult:
        """按最大版本数清理

        Args:
            versions: 版本列表
            max_versions: 最大保留版本数
            compress_after: 超过此数量后压缩旧版本

        Returns:
            CleanupResult: 清理结果
        """
        result = CleanupResult(timestamp=datetime.now().isoformat())

        self.log(f"开始清理，保留最近 {max_versions} 个版本...")

        for i, version in enumerate(versions):
            result.total_versions += 1

            if i >= max_versions:
                # 检查是否需要压缩而不是删除
                if compress_after > 0 and i < max_versions + compress_after:
                    # 压缩而不是删除
                    if not version.compressed and not self.dry_run:
                        try:
                            compressed_path = gzip.compress(
                                version.file_path.encode()
                            )
                            with open(version.file_path + '.gz', 'wb') as f:
                                f.write(compressed_path)
                            os.remove(version.file_path)

                            self.log(f"  压缩: {version.version_id}")
                            result.compressed_versions += 1
                        except Exception as e:
                            error_msg = f"压缩失败: {version.version_id}, 错误: {e}"
                            self.log(f"  ❌ {error_msg}")
                            result.errors.append(error_msg)
                    elif not self.dry_run:
                        result.compressed_versions += 1
                else:
                    # 删除
                    if self.dry_run:
                        self.log(f"  [DRY-RUN] 将删除: {version.version_id}")
                        result.deleted_versions += 1
                    else:
                        try:
                            os.remove(version.file_path)
                            self.log(f"  删除: {version.version_id}")
                            result.deleted_versions += 1
                            result.freed_space_bytes += version.size_bytes
                            result.deleted_files.append(version.version_id)
                        except Exception as e:
                            error_msg = f"删除失败: {version.version_id}, 错误: {e}"
                            self.log(f"  ❌ {error_msg}")
                            result.errors.append(error_msg)
            else:
                result.kept_versions += 1
                result.kept_files.append(version.version_id)

        return result

    def cleanup(
        self,
        retention_days: Optional[int] = None,
        max_versions: Optional[int] = None,
        compress_after: int = 0
    ) -> CleanupResult:
        """执行清理

        Args:
            retention_days: 保留天数
            max_versions: 最大版本数
            compress_after: 压缩超过此数量的旧版本

        Returns:
            CleanupResult: 清理结果
        """
        self.log("开始配置版本历史清理...")
        self.log(f"历史目录: {self.history_dir}")
        self.log(f"干运行模式: {'是' if self.dry_run else '否'}")

        # 获取所有版本
        versions = self.get_all_versions()
        self.log(f"找到 {len(versions)} 个版本")

        if retention_days is not None:
            result = self.cleanup_by_retention_days(versions, retention_days)
        elif max_versions is not None:
            result = self.cleanup_by_max_versions(versions, max_versions, compress_after)
        else:
            result = CleanupResult(timestamp=datetime.now().isoformat())
            result.errors.append("未指定清理条件")

        self.results.append(result)
        return result

    def get_summary(self) -> Dict[str, Any]:
        """获取清理汇总"""
        return {
            "total_results": len(self.results),
            "total_deleted": sum(r.deleted_versions for r in self.results),
            "total_compressed": sum(r.compressed_versions for r in self.results),
            "total_freed_bytes": sum(r.freed_space_bytes for r in self.results),
            "total_errors": sum(len(r.errors) for r in self.results)
        }


def format_bytes(bytes_count: int) -> str:
    """格式化字节数"""
    for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
        if bytes_count < 1024.0:
            return f"{bytes_count:.2f} {unit}"
        bytes_count /= 1024.0
    return f"{bytes_count:.2f} PB"


def print_result(result: CleanupResult) -> None:
    """打印清理结果"""
    print("\n" + "=" * 70)
    print("配置版本历史清理报告")
    print("=" * 70)
    print(f"清理时间: {result.timestamp}")
    print("-" * 70)
    print(f"总版本数: {result.total_versions}")
    print(f"保留版本: {result.kept_versions}")
    print(f"删除版本: {result.deleted_versions}")
    print(f"压缩版本: {result.compressed_versions}")
    print(f"释放空间: {format_bytes(result.freed_space_bytes)}")

    if result.errors:
        print("-" * 70)
        print("错误:")
        for error in result.errors:
            print(f"  ❌ {error}")

    print("=" * 70)


def main():
    parser = argparse.ArgumentParser(
        description="AgentOS Manager 配置版本历史清理工具"
    )
    parser.add_argument(
        "--history-dir",
        type=str,
        default="${AGENTOS_DATA_DIR}/manager-history",
        help="版本历史目录路径"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="干运行模式，不实际删除任何文件"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="显示详细日志"
    )
    parser.add_argument(
        "--days",
        type=int,
        default=None,
        help="保留最近N天的版本"
    )
    parser.add_argument(
        "--max-versions",
        type=int,
        default=None,
        help="保留最近N个版本"
    )
    parser.add_argument(
        "--compress-after",
        type=int,
        default=10,
        help="超过max-versions后额外保留N个压缩版本"
    )
    parser.add_argument(
        "-o", "--output",
        type=str,
        help="输出JSON报告文件路径"
    )

    args = parser.parse_args()

    # 展开环境变量
    history_dir = os.path.expandvars(args.history_dir)

    print("=" * 70)
    print("AgentOS Manager 配置版本历史清理工具")
    print("=" * 70)
    print(f"历史目录: {history_dir}")
    print(f"干运行模式: {'是' if args.dry_run else '否'}")
    print("-" * 70)

    cleanup = ConfigVersionCleanup(
        history_dir=history_dir,
        dry_run=args.dry_run,
        verbose=args.verbose
    )

    # 检查条件
    if args.days is None and args.max_versions is None:
        print("错误: 请指定 --days 或 --max-versions")
        return 1

    if args.days is not None and args.max_versions is not None:
        print("错误: 不能同时指定 --days 和 --max-versions")
        return 1

    # 执行清理
    try:
        result = cleanup.cleanup(
            retention_days=args.days,
            max_versions=args.max_versions,
            compress_after=args.compress_after
        )

        print_result(result)

        # 保存报告
        if args.output:
            report = {
                "timestamp": result.timestamp,
                "history_dir": history_dir,
                "dry_run": args.dry_run,
                "result": {
                    "total_versions": result.total_versions,
                    "kept_versions": result.kept_versions,
                    "deleted_versions": result.deleted_versions,
                    "compressed_versions": result.compressed_versions,
                    "freed_space_bytes": result.freed_space_bytes,
                    "errors": result.errors,
                    "deleted_files": result.deleted_files,
                    "kept_files": result.kept_files
                }
            }

            with open(args.output, 'w', encoding='utf-8') as f:
                json.dump(report, f, indent=2, ensure_ascii=False)

            print(f"\n报告已保存到: {args.output}")

        return 0

    except Exception as e:
        print(f"错误: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
