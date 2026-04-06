﻿﻿#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 配置初始化工具
# 遵循 AgentOS 架构设计原则：反馈闭环、工程美学

"""
AgentOS 配置初始化工具

用于初始化和配置 AgentOS 系统，包括：
- 创建默认配置文件
- 验证配置完整性
- 生成环境特定配置
- 备份和恢复配置

Usage:
    python init_config.py --init
    python init_config.py --validate
    python init_config.py --generate --env production
    python init_config.py --backup --output /path/to/backup
"""

import argparse
import json
import os
import shutil
import sys
from dataclasses import dataclass, field, asdict
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional


DEFAULT_CONFIG_DIR = os.path.expanduser("~/.agentos")
CONFIG_DIRS = [
    os.path.expanduser("~/.agentos"),
    "/etc/agentos",
    os.path.join(os.getcwd(), "manager")
]


@dataclass
class ConfigTemplate:
    """配置模板"""
    name: str
    description: str
    content: str
    required: bool = True


DEFAULT_CONFIGS = {
    "agentos.conf": ConfigTemplate(
        name="agentos.conf",
        description="AgentOS 主配置文件",
        content="""# AgentOS Main Configuration
# Version: 1.0.0

# System
version = "1.0.0.6"
log_level = "info"
data_dir = "${HOME}/.agentos/data"
run_dir = "/var/run/agentos"

# Kernel
kernel.threads = 4
kernel.task_queue_size = 1024
kernel.ipc_timeout = 5000

# Memory
memory.max_alloc = "512M"
memory.gc_threshold = "256M"
memory.pool_size = "128M"

# Network
network.bind_addr = "127.0.0.1"
network.port = 8080
network.backlog = 128

# Security
security.enabled = true
security.sandbox = true
security.max_memory_per_agent = "64M"

# Logging
logging.level = "info"
logging.format = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
logging.output = "stdout"
logging.file = "${HOME}/.agentos/logs/agentos.log"

# Telemetry
telemetry.enabled = true
telemetry.endpoint = "http://localhost:9090/metrics"
telemetry.interval = 60
""",
        required=True
    ),

    "logging.conf": ConfigTemplate(
        name="logging.conf",
        description="日志配置",
        content="""# AgentOS Logging Configuration

# Log Levels: debug, info, warn, error, fatal
level = "info"

# Log Format
format = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
date_format = "%Y-%m-%d %H:%M:%S"

# Output
output = "stdout"
file = "${HOME}/.agentos/logs/agentos.log"

# Rotation
rotation.enabled = true
rotation.max_size = "10M"
rotation.max_files = 5

# Categories
[categories.agent]
level = "debug"

[categories.kernel]
level = "info"

[categories.memory]
level = "warn"

[categories.network]
level = "info"

[categories.security]
level = "error"
""",
        required=True
    ),

    "memory.conf": ConfigTemplate(
        name="memory.conf",
        description="记忆系统配置",
        content="""# AgentOS Memory System Configuration

# Memory Limits
max_memory = "512M"
gc_threshold = "256M"

# Memory Layers
[layers.l1]
type = "raw_volume"
size = "64M"
retention = 3600

[layers.l2]
type = "feature"
size = "128M"
retention = 7200
clustering = true

[layers.l3]
type = "structural"
size = "256M"
retention = 86400

[layers.l4]
type = "pattern"
size = "64M"
retention = 604800

# Forgetting Curve
forgetting.enabled = true
forgetting.decay_rate = 0.95
forgetting.min_importance = 0.1

# Clustering
clustering.enabled = true
clustering.method = "kmeans"
clustering.max_clusters = 100

# Indexing
index.type = "faiss"
index.metric = "cosine"
index.dimension = 512
""",
        required=True
    ),

    "coreloopthree.conf": ConfigTemplate(
        name="coreloopthree.conf",
        description="三层认知运行时配置",
        content="""# AgentOS CoreLoopThree Configuration

# System 1 (Fast Path)
[system1]
enabled = true
max_depth = 3
timeout_ms = 100

# System 2 (Slow Path)
[system2]
enabled = true
max_depth = 10
timeout_ms = 5000
max_iterations = 100

# Planning
[planning]
enabled = true
planner = "hierarchical"
horizon = 5
discount_factor = 0.95

# Scheduling
[scheduling]
policy = "priority"
max_concurrent_tasks = 10
task_timeout = 30000

# Agent Management
[agents]
max_agents = 32
agent_idle_timeout = 300
agent_startup_timeout = 5000
""",
        required=False
    ),

    "security.conf": ConfigTemplate(
        name="security.conf",
        description="安全配置",
        content="""# AgentOS Security Configuration

# Security Features
enabled = true
sandbox = true

# Virtual Workbench
[workbench]
max_agents = 32
memory_per_agent = "64M"
cpu_time_limit = 300

# Permissions
[permissions]
default_deny = true
allow_file_system = false
allow_network = false
allow_subprocess = false

# Sanitizer
[sanitizer]
enabled = true
max_string_length = 1048576
max_array_size = 65536
blocked_patterns = ["..", "~", "$", "`"]

# Audit
[audit]
enabled = true
log_file = "${HOME}/.agentos/logs/audit.log"
log_all_syscalls = false
log_failed_only = true
""",
        required=False
    )
}


class ConfigInitializer:
    """配置初始化器"""

    def __init__(self, config_dir: Optional[str] = None):
        self.config_dir = config_dir or DEFAULT_CONFIG_DIR
        self.created_files: List[str] = []
        self.updated_files: List[str] = []

    def create_config_dir(self):
        os.makedirs(self.config_dir, exist_ok=True)
        print(f"Configuration directory: {self.config_dir}")

    def init_configs(self, force: bool = False) -> bool:
        self.create_config_dir()

        for name, template in DEFAULT_CONFIGS.items():
            config_path = os.path.join(self.config_dir, name)

            if os.path.exists(config_path) and not force:
                print(f"  [SKIP] {name} (already exists)")
                continue

            content = self._expand_variables(template.content)

            with open(config_path, "w", encoding="utf-8") as f:
                f.write(content)

            os.chmod(config_path, 0o644)
            self.created_files.append(name)
            print(f"  [CREATE] {name}")

        return len(self.created_files) > 0

    def validate_configs(self) -> List[str]:
        errors = []

        for name, template in DEFAULT_CONFIGS.items():
            if not template.required:
                continue

            config_path = os.path.join(self.config_dir, name)

            if not os.path.exists(config_path):
                errors.append(f"Missing required manager: {name}")
                continue

            try:
                with open(config_path, "r", encoding="utf-8") as f:
                    content = f.read()

                if len(content.strip()) == 0:
                    errors.append(f"Empty manager file: {name}")

            except Exception as e:
                errors.append(f"Error reading {name}: {e}")

        return errors

    def generate_env_config(self, env: str) -> bool:
        env_configs = {
            "development": {
                "log_level": "debug",
                "security.enabled": "false",
                "telemetry.enabled": "false"
            },
            "production": {
                "log_level": "warn",
                "security.enabled": "true",
                "telemetry.enabled": "true"
            },
            "testing": {
                "log_level": "debug",
                "security.enabled": "false",
                "memory.max_alloc": "256M"
            }
        }

        if env not in env_configs:
            print(f"Unknown environment: {env}")
            print(f"Available: {', '.join(env_configs.keys())}")
            return False

        overrides = env_configs[env]
        config_path = os.path.join(self.config_dir, "agentos.conf")

        if not os.path.exists(config_path):
            print("Please run --init first")
            return False

        with open(config_path, "r", encoding="utf-8") as f:
            content = f.read()

        for key, value in overrides.items():
            if "." in key:
                section, option = key.split(".", 1)
                content += f"\n[{section}]\n{option} = {value}\n"
            else:
                content += f"{key} = {value}\n"

        backup_path = f"{config_path}.backup.{datetime.now().strftime('%Y%m%d%H%M%S')}"
        shutil.copy2(config_path, backup_path)
        print(f"Backup created: {backup_path}")

        with open(config_path, "w", encoding="utf-8") as f:
            f.write(content)

        print(f"Generated {env} configuration")
        return True

    def backup_configs(self, output_path: str) -> bool:
        if not os.path.exists(self.config_dir):
            print(f"manager directory not found: {self.config_dir}")
            return False

        os.makedirs(output_path, exist_ok=True)

        backup_file = os.path.join(
            output_path,
            f"agentos_config_backup_{datetime.now().strftime('%Y%m%d%H%M%S')}.tar.gz"
        )

        import tarfile
        with tarfile.open(backup_file, "w:gz") as tar:
            tar.add(self.config_dir, arcname=os.path.basename(self.config_dir))

        print(f"Backup created: {backup_file}")
        return True

    def restore_configs(self, backup_file: str) -> bool:
        if not os.path.exists(backup_file):
            print(f"Backup file not found: {backup_file}")
            return False

        import tarfile
        with tarfile.open(backup_file, "r:gz") as tar:
            tar.extractall(path=os.path.dirname(self.config_dir))

        print(f"Configs restored from: {backup_file}")
        return True

    def _expand_variables(self, content: str) -> str:
        home = os.path.expanduser("~")
        content = content.replace("${HOME}", home)
        return content


def main():
    parser = argparse.ArgumentParser(
        description="AgentOS Configuration Initialization Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "--init",
        action="store_true",
        help="Initialize default configuration files"
    )

    parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate configuration files"
    )

    parser.add_argument(
        "--generate",
        action="store_true",
        help="Generate environment-specific configuration"
    )

    parser.add_argument(
        "--env",
        choices=["development", "production", "testing"],
        default="development",
        help="Environment for --generate"
    )

    parser.add_argument(
        "--backup",
        action="store_true",
        help="Backup configuration files"
    )

    parser.add_argument(
        "--restore",
        type=str,
        metavar="FILE",
        help="Restore from backup file"
    )

    parser.add_argument(
        "--output",
        type=str,
        help="Output directory for backup"
    )

    parser.add_argument(
        "--dir",
        type=str,
        help="Configuration directory (default: ~/.agentos)"
    )

    parser.add_argument(
        "--force",
        action="store_true",
        help="Force overwrite existing files"
    )

    parser.add_argument(
        "--list",
        action="store_true",
        help="List default configuration files"
    )

    args = parser.parse_args()

    initializer = ConfigInitializer(config_dir=args.dir)

    if args.list:
        print("\nDefault Configuration Files:")
        print("-" * 50)
        for name, template in DEFAULT_CONFIGS.items():
            required = "required" if template.required else "optional"
            print(f"  {name:<25} [{required}] {template.description}")
        return 0

    if args.init:
        print("\nInitializing AgentOS Configuration...")
        print(f"manager directory: {initializer.config_dir}")
        initializer.init_configs(force=args.force)
        print(f"\nCreated {len(initializer.created_files)} configuration file(s)")
        print("\nValidating configuration...")
        errors = initializer.validate_configs()
        if errors:
            print("\nValidation Errors:")
            for error in errors:
                print(f"  ! {error}")
            return 1
        else:
            print("  All required configurations are valid")
        return 0

    if args.validate:
        print("\nValidating AgentOS Configuration...")
        errors = initializer.validate_configs()
        if errors:
            print("\nValidation Errors:")
            for error in errors:
                print(f"  ! {error}")
            return 1
        else:
            print("  All configurations are valid")
        return 0

    if args.generate:
        return 0 if initializer.generate_env_config(args.env) else 1

    if args.backup:
        output = args.output or os.path.expanduser("~/.agentos/backups")
        return 0 if initializer.backup_configs(output) else 1

    if args.restore:
        return 0 if initializer.restore_configs(args.restore) else 1

    parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())