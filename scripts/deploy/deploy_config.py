﻿#!/usr/bin/env python3
"""
AgentOS 配置部署脚本
根据环境变量部署配置文件
"""

import argparse
import shutil
import sys
from pathlib import Path

import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'dev'))

from constants import REQUIRED_CONFIG_FILES


# 环境配置映射 (所有环境使用相同的文件映射)
ENV_CONFIG_MAPPING = {
    'development': REQUIRED_CONFIG_FILES,
    'staging': REQUIRED_CONFIG_FILES,
    'production': REQUIRED_CONFIG_FILES,
}


def deploy_single_file(
    source_path: Path,
    target_path: Path,
    backup: bool = True,
    dry_run: bool = False
) -> bool:
    """
    部署单个配置文件
    
    Args:
        source_path: 源文件路径
        target_path: 目标文件路径
        backup: 是否备份现有文件
        dry_run: 是否为模拟模式
        
    Returns:
        bool: 部署是否成功
    """
    if not source_path.exists():
        print(f"⚠ 源文件不存在：{source_path}")
        return False
    
    if dry_run:
        print(f"[DRY RUN] 将复制 {source_path} -> {target_path}")
        return True
    
    # 备份目标文件
    if backup and target_path.exists():
        backup_path = target_path.with_suffix('.backup')
        shutil.copy2(target_path, backup_path)
        print(f"✓ 已备份：{backup_path}")
    
    # 复制配置文件
    try:
        target_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_path, target_path)
        print(f"✓ 已部署：{target_path}")
        return True
    except Exception as e:
        print(f"✗ 部署失败：{target_path} - {e}")
        return False


def deploy_config(
    source_dir: Path,
    target_env: str,
    backup: bool = True,
    dry_run: bool = False
) -> bool:
    """部署配置到指定环境"""

    # 确定目标目录
    target_dir = source_dir / 'env' / target_env
    if not target_dir.exists():
        print(f"✗ 目标环境目录不存在：{target_dir}")
        return False

    # 获取当前环境的配置文件列表
    config_files = ENV_CONFIG_MAPPING.get(target_env)
    if not config_files:
        print(f"✗ 不支持的环境：{target_env}")
        return False

    # 部署配置文件
    success_count = 0
    error_count = 0

    for config_file in config_files:
        source_path = source_dir / config_file
        target_path = target_dir / config_file

        if deploy_single_file(source_path, target_path, backup, dry_run):
            success_count += 1
        else:
            error_count += 1

    # 总结
    print(f"\n部署完成:")
    print(f"  成功：{success_count}")
    print(f"  失败：{error_count}")

    return error_count == 0


def validate_deployment(
    source_dir: Path,
    target_env: str
) -> bool:
    """验证部署结果"""

    target_dir = source_dir / 'env' / target_env
    if not target_dir.exists():
        print(f"✗ 目标环境目录不存在：{target_dir}")
        return False

    # 获取当前环境的配置文件列表
    config_files = ENV_CONFIG_MAPPING.get(target_env)
    if not config_files:
        print(f"✗ 不支持的环境：{target_env}")
        return False

    # 检查必需的配置文件
    missing_files = []
    for file in config_files:
        if not (target_dir / file).exists():
            missing_files.append(file)

    if missing_files:
        print(f"✗ 缺少必需的配置文件:")
        for file in missing_files:
            print(f"  - {file}")
        return False

    print("✓ 所有必需的配置文件已部署")
    return True


def main():
    parser = argparse.ArgumentParser(description='AgentOS 配置部署工具')
    parser.add_argument('--env', '-e',
                       required=True,
                       choices=['development', 'staging', 'production'],
                       help='目标环境 (development/staging/production)')
    parser.add_argument('--source', '-s',
                       default='manager',
                       help='源配置目录')
    parser.add_argument('--no-backup', '-n',
                       action='store_true',
                       help='不备份现有配置')
    parser.add_argument('--dry-run', '-d',
                       action='store_true',
                       help='模拟部署（不实际复制文件）')
    parser.add_argument('--validate', '-v',
                       action='store_true',
                       help='验证部署结果')
    
    args = parser.parse_args()
    
    source_dir = Path(args.source)
    
    print(f"=== AgentOS 配置部署工具 ===")
    print(f"目标环境: {args.env}")
    print(f"源目录: {source_dir}")
    print(f"备份: {'禁用' if args.no_backup else '启用'}")
    print(f"模拟模式: {'启用' if args.dry_run else '禁用'}")
    print()
    
    # 执行部署
    success = deploy_config(
        source_dir=source_dir,
        target_env=args.env,
        backup=not args.no_backup,
        dry_run=args.dry_run
    )
    
    if success and args.validate:
        validate_deployment(source_dir, args.env)
    
    if success:
        print("\n✓ 部署成功!")
        print(f"\n下一步:")
        print(f"  1. 验证环境变量: {args.env.upper()}_*")
        print(f"  2. 重启相关服务以加载新配置")
        print(f"  3. 检查日志确认配置生效")
        return 0
    else:
        print("\n✗ 部署失败!")
        return 1


if __name__ == '__main__':
    sys.exit(main())
