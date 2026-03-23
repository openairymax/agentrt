#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 注册表更新工具
# 遵循 AgentOS 架构设计原则：反馈闭环、工程美学

"""
AgentOS 注册表更新工具

用于管理 AgentOS 模块注册表，包括：
- 注册新模块
- 更新模块信息
- 查询已注册模块
- 导出注册表

Usage:
    python update_registry.py --register --module path/to/module
    python update_registry.py --query --name module_name
    python update_registry.py --list
    python update_registry.py --export
"""

import argparse
import json
import os
import sys
from dataclasses import dataclass, field, asdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any


REGISTRY_FILE = os.path.expanduser("~/.agentos/registry.json")


@dataclass
class ModuleInfo:
    """模块信息"""
    name: str
    path: str
    version: str = "1.0.0"
    author: str = ""
    description: str = ""
    dependencies: List[str] = field(default_factory=list)
    registered_at: str = field(default_factory=lambda: datetime.now().isoformat())
    last_updated: str = field(default_factory=lambda: datetime.now().isoformat())
    status: str = "active"


@dataclass
class Registry:
    """注册表"""
    version: str = "1.0.0"
    modules: Dict[str, ModuleInfo] = field(default_factory=dict)
    last_updated: str = field(default_factory=lambda: datetime.now().isoformat())

    def add_module(self, module: ModuleInfo):
        self.modules[module.name] = module
        self.last_updated = datetime.now().isoformat()

    def remove_module(self, name: str) -> bool:
        if name in self.modules:
            del self.modules[name]
            self.last_updated = datetime.now().isoformat()
            return True
        return False

    def get_module(self, name: str) -> Optional[ModuleInfo]:
        return self.modules.get(name)

    def list_modules(self) -> List[ModuleInfo]:
        return list(self.modules.values())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "version": self.version,
            "modules": {k: asdict(v) for k, v in self.modules.items()},
            "last_updated": self.last_updated
        }

    @staticmethod
    def from_dict(data: Dict[str, Any]) -> "Registry":
        registry = Registry(version=data.get("version", "1.0.0"))
        for name, module_data in data.get("modules", {}).items():
            registry.modules[name] = ModuleInfo(**module_data)
        registry.last_updated = data.get("last_updated", datetime.now().isoformat())
        return registry


class RegistryManager:
    """注册表管理器"""

    def __init__(self, registry_path: Optional[str] = None):
        self.registry_path = registry_path or REGISTRY_FILE
        self.registry = self._load()

    def _load(self) -> Registry:
        if not os.path.exists(self.registry_path):
            return Registry()

        try:
            with open(self.registry_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            return Registry.from_dict(data)
        except Exception as e:
            print(f"Warning: Failed to load registry: {e}")
            return Registry()

    def _save(self):
        os.makedirs(os.path.dirname(self.registry_path), exist_ok=True)
        with open(self.registry_path, "w", encoding="utf-8") as f:
            json.dump(self.registry.to_dict(), f, indent=2, ensure_ascii=False)

    def register_module(self, name: str, path: str, version: str = "1.0.0",
                       author: str = "", description: str = "",
                       dependencies: Optional[List[str]] = None) -> bool:
        if self.registry.get_module(name):
            print(f"Module '{name}' already registered. Use --update to update.")
            return False

        module = ModuleInfo(
            name=name,
            path=path,
            version=version,
            author=author,
            description=description,
            dependencies=dependencies or []
        )

        self.registry.add_module(module)
        self._save()
        print(f"Module '{name}' registered successfully.")
        return True

    def unregister_module(self, name: str) -> bool:
        if self.registry.remove_module(name):
            self._save()
            print(f"Module '{name}' unregistered successfully.")
            return True
        print(f"Module '{name}' not found in registry.")
        return False

    def update_module(self, name: str, **kwargs) -> bool:
        module = self.registry.get_module(name)
        if not module:
            print(f"Module '{name}' not found. Use --register to add.")
            return False

        for key, value in kwargs.items():
            if hasattr(module, key):
                setattr(module, key, value)

        module.last_updated = datetime.now().isoformat()
        self.registry.last_updated = datetime.now().isoformat()
        self._save()
        print(f"Module '{name}' updated successfully.")
        return True

    def query_module(self, name: str) -> Optional[ModuleInfo]:
        return self.registry.get_module(name)

    def list_modules(self, status: Optional[str] = None) -> List[ModuleInfo]:
        modules = self.registry.list_modules()
        if status:
            modules = [m for m in modules if m.status == status]
        return modules

    def export_registry(self, output_path: str):
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(self.registry.to_dict(), f, indent=2, ensure_ascii=False)
        print(f"Registry exported to: {output_path}")

    def import_registry(self, input_path: str):
        with open(input_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        self.registry = Registry.from_dict(data)
        self._save()
        print(f"Registry imported from: {input_path}")

    def validate_registry(self) -> List[str]:
        errors = []
        for name, module in self.registry.modules.items():
            if not os.path.exists(module.path):
                errors.append(f"Module '{name}': path does not exist: {module.path}")

            if not module.version:
                errors.append(f"Module '{name}': version is empty")

            for dep in module.dependencies:
                if not self.registry.get_module(dep):
                    errors.append(f"Module '{name}': dependency '{dep}' not registered")

        return errors


def main():
    parser = argparse.ArgumentParser(
        description="AgentOS Registry Manager",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "--register",
        action="store_true",
        help="Register a new module"
    )

    parser.add_argument(
        "--unregister",
        action="store_true",
        help="Unregister a module"
    )

    parser.add_argument(
        "--update",
        action="store_true",
        help="Update module information"
    )

    parser.add_argument(
        "--query",
        action="store_true",
        help="Query a module"
    )

    parser.add_argument(
        "--list",
        action="store_true",
        help="List all registered modules"
    )

    parser.add_argument(
        "--export",
        type=str,
        metavar="PATH",
        help="Export registry to a file"
    )

    parser.add_argument(
        "--import",
        type=str,
        dest="import_path",
        metavar="PATH",
        help="Import registry from a file"
    )

    parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate registry integrity"
    )

    parser.add_argument(
        "--name",
        type=str,
        help="Module name"
    )

    parser.add_argument(
        "--path",
        type=str,
        help="Module path"
    )

    parser.add_argument(
        "--version",
        type=str,
        default="1.0.0",
        help="Module version"
    )

    parser.add_argument(
        "--author",
        type=str,
        help="Module author"
    )

    parser.add_argument(
        "--description",
        type=str,
        help="Module description"
    )

    parser.add_argument(
        "--deps",
        type=str,
        nargs="+",
        help="Module dependencies"
    )

    parser.add_argument(
        "--status",
        type=str,
        choices=["active", "inactive", "deprecated"],
        help="Module status (for filtering)"
    )

    parser.add_argument(
        "--registry",
        type=str,
        help="Custom registry file path"
    )

    args = parser.parse_args()

    manager = RegistryManager(registry_path=args.registry)

    if args.register:
        if not args.name or not args.path:
            print("Error: --register requires --name and --path")
            return 1

        manager.register_module(
            name=args.name,
            path=args.path,
            version=args.version,
            author=args.author or "",
            description=args.description or "",
            dependencies=args.deps or []
        )

    elif args.unregister:
        if not args.name:
            print("Error: --unregister requires --name")
            return 1

        manager.unregister_module(args.name)

    elif args.update:
        if not args.name:
            print("Error: --update requires --name")
            return 1

        updates = {}
        if args.version:
            updates["version"] = args.version
        if args.author:
            updates["author"] = args.author
        if args.description:
            updates["description"] = args.description
        if args.deps:
            updates["dependencies"] = args.deps

        manager.update_module(args.name, **updates)

    elif args.query:
        if not args.name:
            print("Error: --query requires --name")
            return 1

        module = manager.query_module(args.name)
        if module:
            print(f"\nModule: {module.name}")
            print(f"  Path: {module.path}")
            print(f"  Version: {module.version}")
            print(f"  Author: {module.author}")
            print(f"  Description: {module.description}")
            print(f"  Dependencies: {', '.join(module.dependencies) or 'None'}")
            print(f"  Status: {module.status}")
            print(f"  Registered: {module.registered_at}")
            print(f"  Last Updated: {module.last_updated}")
        else:
            print(f"Module '{args.name}' not found.")

    elif args.list:
        modules = manager.list_modules(status=args.status)
        if modules:
            print(f"\nRegistered Modules ({len(modules)}):")
            print("-" * 70)
            for m in modules:
                print(f"  {m.name:<20} {m.version:<10} {m.status:<10} {m.path}")
        else:
            print("No modules registered.")

    elif args.export:
        manager.export_registry(args.export)

    elif args.import_path:
        manager.import_registry(args.import_path)

    elif args.validate:
        errors = manager.validate_registry()
        if errors:
            print("\nValidation Errors:")
            for error in errors:
                print(f"  ! {error}")
            return 1
        else:
            print("\nRegistry is valid.")
            return 0

    else:
        parser.print_help()
        return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())