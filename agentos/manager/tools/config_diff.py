#!/usr/bin/env python3
"""
AgentOS Manager 模块配置差异对比工具

本工具用于比较两个配置文件之间的差异，支持：
- YAML 配置文件的比较
- JSON Schema 的比较
- 多文件批量比较
- 生成差异报告

使用方法:
    python config_diff.py file1.yaml file2.yaml
    python config_diff.py --dir ./config --baseline ./baseline
    python config_diff.py file1.yaml file2.yaml --output diff_report.json
"""

import argparse
import json
import sys
import os
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union


class DiffType(Enum):
    """差异类型"""
    ADDED = "added"
    REMOVED = "removed"
    MODIFIED = "modified"
    UNCHANGED = "unchanged"


class ValueType(Enum):
    """值类型"""
    STRING = "string"
    NUMBER = "number"
    BOOLEAN = "boolean"
    OBJECT = "object"
    ARRAY = "array"
    NULL = "null"


@dataclass
class DiffEntry:
    """差异条目"""
    path: str
    diff_type: DiffType
    old_value: Any = None
    new_value: Any = None
    value_type: ValueType = ValueType.STRING

    def to_dict(self) -> Dict[str, Any]:
        return {
            "path": self.path,
            "type": self.diff_type.value,
            "old_value": self.old_value,
            "new_value": self.new_value,
            "value_type": self.value_type.value
        }


@dataclass
class DiffResult:
    """差异比较结果"""
    file1: str
    file2: str
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())
    added_count: int = 0
    removed_count: int = 0
    modified_count: int = 0
    unchanged_count: int = 0
    diffs: List[DiffEntry] = field(default_factory=list)
    errors: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "file1": self.file1,
            "file2": self.file2,
            "timestamp": self.timestamp,
            "summary": {
                "added": self.added_count,
                "removed": self.removed_count,
                "modified": self.modified_count,
                "unchanged": self.unchanged_count
            },
            "diffs": [d.to_dict() for d in self.diffs],
            "errors": self.errors
        }

    @property
    def has_changes(self) -> bool:
        """是否存在变更"""
        return self.added_count > 0 or self.removed_count > 0 or self.modified_count > 0


def detect_value_type(value: Any) -> ValueType:
    """检测值的类型"""
    if value is None:
        return ValueType.NULL
    elif isinstance(value, bool):
        return ValueType.BOOLEAN
    elif isinstance(value, (int, float)):
        return ValueType.NUMBER
    elif isinstance(value, str):
        return ValueType.STRING
    elif isinstance(value, dict):
        return ValueType.OBJECT
    elif isinstance(value, list):
        return ValueType.ARRAY
    else:
        return ValueType.STRING


def normalize_value(value: Any) -> Any:
    """标准化值以便比较"""
    if isinstance(value, dict):
        return {k: normalize_value(v) for k, v in sorted(value.items())}
    elif isinstance(value, list):
        return [normalize_value(v) for v in value]
    elif isinstance(value, str):
        value = value.strip()
        # 尝试转换为数字
        try:
            if '.' in value:
                return float(value)
            return int(value)
        except (ValueError, TypeError):
            pass
        # 转换布尔值
        if value.lower() in ('true', 'false'):
            return value.lower() == 'true'
        return value
    return value


def deep_diff(
    path: str,
    old_value: Any,
    new_value: Any,
    diffs: List[DiffEntry]
) -> None:
    """深度比较两个值，递归收集差异"""
    old_norm = normalize_value(old_value)
    new_norm = normalize_value(new_value)

    if old_norm == new_norm:
        return

    old_type = detect_value_type(old_value)
    new_type = detect_value_type(new_value)

    # 类型不同视为修改
    if old_type != new_type:
        diffs.append(DiffEntry(
            path=path,
            diff_type=DiffType.MODIFIED,
            old_value=old_value,
            new_value=new_value,
            value_type=new_type
        ))
        return

    # 处理不同类型
    if old_type == ValueType.OBJECT:
        old_keys = set(old_norm.keys())
        new_keys = set(new_norm.keys())

        # 新增的键
        for key in new_keys - old_keys:
            diffs.append(DiffEntry(
                path=f"{path}.{key}" if path else key,
                diff_type=DiffType.ADDED,
                old_value=None,
                new_value=new_value[key],
                value_type=detect_value_type(new_value[key])
            ))

        # 删除的键
        for key in old_keys - new_keys:
            diffs.append(DiffEntry(
                path=f"{path}.{key}" if path else key,
                diff_type=DiffType.REMOVED,
                old_value=old_value[key],
                new_value=None,
                value_type=detect_value_type(old_value[key])
            ))

        # 共同的键递归比较
        for key in old_keys & new_keys:
            deep_diff(
                f"{path}.{key}" if path else key,
                old_value[key],
                new_value[key],
                diffs
            )

    elif old_type == ValueType.ARRAY:
        # 数组比较
        max_len = max(len(old_norm), len(new_norm))
        for i in range(max_len):
            if i >= len(old_norm):
                diffs.append(DiffEntry(
                    path=f"{path}[{i}]",
                    diff_type=DiffType.ADDED,
                    old_value=None,
                    new_value=new_value[i],
                    value_type=detect_value_type(new_value[i])
                ))
            elif i >= len(new_norm):
                diffs.append(DiffEntry(
                    path=f"{path}[{i}]",
                    diff_type=DiffType.REMOVED,
                    old_value=old_value[i],
                    new_value=None,
                    value_type=detect_value_type(old_value[i])
                ))
            else:
                deep_diff(f"{path}[{i}]", old_value[i], new_value[i], diffs)

    else:
        # 基本类型视为修改
        diffs.append(DiffEntry(
            path=path,
            diff_type=DiffType.MODIFIED,
            old_value=old_value,
            new_value=new_value,
            value_type=new_type
        ))


def load_config_file(filepath: str) -> Tuple[Optional[Dict], Optional[str]]:
    """加载配置文件

    Returns:
        (配置字典, 错误信息)
    """
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        # 移除 BOM
        if content.startswith('\ufeff'):
            content = content[1:]

        # 根据扩展名加载
        ext = os.path.splitext(filepath)[1].lower()
        if ext in ('.yaml', '.yml'):
            import yaml
            return yaml.safe_load(content), None
        elif ext == '.json':
            return json.loads(content), None
        else:
            return None, f"不支持的文件类型: {ext}"

    except FileNotFoundError:
        return None, f"文件不存在: {filepath}"
    except Exception as e:
        return None, f"加载文件失败: {filepath}, 错误: {str(e)}"


def compare_configs(file1: str, file2: str) -> DiffResult:
    """比较两个配置文件

    Args:
        file1: 文件1路径
        file2: 文件2路径

    Returns:
        DiffResult: 比较结果
    """
    result = DiffResult(file1=file1, file2=file2)

    # 加载文件
    config1, error1 = load_config_file(file1)
    if error1:
        result.errors.append(error1)
        return result

    config2, error2 = load_config_file(file2)
    if error2:
        result.errors.append(error2)
        return result

    # 处理空值
    if config1 is None:
        config1 = {}
    if config2 is None:
        config2 = {}

    # 移除元数据字段进行实际配置比较
    def remove_metadata(cfg):
        if isinstance(cfg, dict):
            return {
                k: remove_metadata(v)
                for k, v in cfg.items()
                if not k.startswith('_')
            }
        elif isinstance(cfg, list):
            return [remove_metadata(item) for item in cfg]
        return cfg

    config1_clean = remove_metadata(config1)
    config2_clean = remove_metadata(config2)

    # 执行深度比较
    diffs: List[DiffEntry] = []
    deep_diff("", config1_clean, config2_clean, diffs)

    # 分类统计
    for diff in diffs:
        if diff.diff_type == DiffType.ADDED:
            result.added_count += 1
        elif diff.diff_type == DiffType.REMOVED:
            result.removed_count += 1
        elif diff.diff_type == DiffType.MODIFIED:
            result.modified_count += 1

    result.diffs = diffs

    return result


def format_diff_entry(diff: DiffEntry, color: bool = True) -> str:
    """格式化差异条目"""
    symbols = {
        DiffType.ADDED: "+",
        DiffType.REMOVED: "-",
        DiffType.MODIFIED: "~",
        DiffType.UNCHANGED: "="
    }

    symbol = symbols.get(diff.diff_type, "?")

    if color:
        colors = {
            DiffType.ADDED: "\033[32m",    # 绿色
            DiffType.REMOVED: "\033[31m",  # 红色
            DiffType.MODIFIED: "\033[33m", # 黄色
            DiffType.UNCHANGED: "\033[0m"  # 默认
        }
        color_code = colors.get(diff.diff_type, "")
        reset = "\033[0m"
    else:
        color_code = ""
        reset = ""

    lines = [
        f"{color_code}{symbol} {diff.path}{reset}"
    ]

    if diff.diff_type == DiffType.MODIFIED:
        lines.append(f"    旧值: {diff.old_value}")
        lines.append(f"    新值: {diff.new_value}")
    elif diff.diff_type == DiffType.ADDED:
        lines.append(f"    新值: {diff.new_value}")
    elif diff.diff_type == DiffType.REMOVED:
        lines.append(f"    旧值: {diff.old_value}")

    return "\n".join(lines)


def print_diff_result(result: DiffResult, color: bool = True) -> None:
    """打印差异结果"""
    print("=" * 70)
    print(f"配置文件比较:")
    print(f"  文件1: {result.file1}")
    print(f"  文件2: {result.file2}")
    print("-" * 70)

    if result.errors:
        print("错误:")
        for error in result.errors:
            print(f"  ❌ {error}")
        print("-" * 70)

    print("统计:")
    print(f"  🟢 新增: {result.added_count}")
    print(f"  🔴 删除: {result.removed_count}")
    print(f"  🟡 修改: {result.modified_count}")
    print("-" * 70)

    if result.diffs:
        print("详细差异:")
        for diff in result.diffs:
            print(format_diff_entry(diff, color=color))
            print()
    else:
        print("✅ 两个配置文件完全相同")

    print("=" * 70)


def main():
    parser = argparse.ArgumentParser(
        description="AgentOS Manager 配置差异对比工具"
    )
    parser.add_argument(
        "file1",
        nargs="?",
        help="第一个配置文件路径"
    )
    parser.add_argument(
        "file2",
        nargs="?",
        help="第二个配置文件路径"
    )
    parser.add_argument(
        "-d", "--dir",
        help="配置文件目录"
    )
    parser.add_argument(
        "-b", "--baseline",
        help="基准配置目录"
    )
    parser.add_argument(
        "-o", "--output",
        help="输出JSON报告文件路径"
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="禁用彩色输出"
    )
    parser.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="仅输出摘要"
    )

    args = parser.parse_args()

    # 验证参数
    if args.dir and args.baseline:
        # 批量比较模式
        dir1 = Path(args.dir)
        dir2 = Path(args.baseline)

        if not dir1.exists():
            print(f"错误: 目录不存在: {dir1}")
            return 1

        if not dir2.exists():
            print(f"错误: 目录不存在: {dir2}")
            return 1

        results: List[DiffResult] = []

        for file1_path in sorted(dir1.rglob("*.yaml")):
            rel_path = file1_path.relative_to(dir1)
            file2_path = dir2 / rel_path

            if file2_path.exists():
                result = compare_configs(str(file1_path), str(file2_path))
                results.append(result)

                if not args.quiet:
                    print(f"\n比较: {rel_path}")
                    print_diff_result(result, color=not args.no_color)
            else:
                print(f"⚠️  跳过 (基准文件中不存在): {rel_path}")

        # 输出汇总
        total_added = sum(r.added_count for r in results)
        total_removed = sum(r.removed_count for r in results)
        total_modified = sum(r.modified_count for r in results)

        print("\n" + "=" * 70)
        print("批量比较汇总:")
        print(f"  比较文件数: {len(results)}")
        print(f"  🟢 新增总数: {total_added}")
        print(f"  🔴 删除总数: {total_removed}")
        print(f"  🟡 修改总数: {total_modified}")
        print("=" * 70)

        if args.output:
            with open(args.output, 'w', encoding='utf-8') as f:
                json.dump({
                    "timestamp": datetime.now().isoformat(),
                    "dir1": str(dir1),
                    "dir2": str(dir2),
                    "summary": {
                        "total_files": len(results),
                        "total_added": total_added,
                        "total_removed": total_removed,
                        "total_modified": total_modified
                    },
                    "results": [r.to_dict() for r in results]
                }, f, indent=2, ensure_ascii=False)
            print(f"\n汇总报告已保存到: {args.output}")

        return 0

    elif args.file1 and args.file2:
        # 单文件比较模式
        result = compare_configs(args.file1, args.file2)
        print_diff_result(result, color=not args.no_color)

        if args.output:
            with open(args.output, 'w', encoding='utf-8') as f:
                json.dump(result.to_dict(), f, indent=2, ensure_ascii=False)
            print(f"\n报告已保存到: {args.output}")

        return 0

    else:
        parser.print_help()
        return 0


if __name__ == "__main__":
    sys.exit(main())
