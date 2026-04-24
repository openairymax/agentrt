# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentOS OpenLab: Skills Installer CLI

Command-line interface for installing, managing, and removing AgentOS skills
from the marketplace. Supports local and remote skill packages.

Usage:
    python -m agentos.openlab.markets.skills.installer.cli install <skill_package>
    python -m agentos.openlab.markets.skills.installer.cli list
    python -m agentos.openlab.markets.skills.installer.cli remove <skill_name>
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import List, Optional


class SkillInstallerCLI:
    """Command-line interface for skill installation management."""

    def __init__(self) -> None:
        self._skills_dir: Path = Path.home() / ".agentos" / "skills"

    def install(self, package_path: Path, force: bool = False) -> bool:
        """Install a skill package."""
        # TODO: Implement package installation logic
        print(f"[AgentOS] Installing skill from: {package_path}")
        return True

    def list_skills(self) -> List[str]:
        """List all installed skills."""
        if not self._skills_dir.exists():
            return []
        return [d.name for d in self._skills_dir.iterdir() if d.is_dir()]

    def remove(self, skill_name: str) -> bool:
        """Remove an installed skill."""
        # TODO: Implement skill removal logic
        print(f"[AgentOS] Removing skill: {skill_name}")
        return True


def build_parser() -> argparse.ArgumentParser:
    """Build the CLI argument parser."""
    parser = argparse.ArgumentParser(
        prog="agentos-skill-installer",
        description="AgentOS Skill Installer CLI",
    )
    subparsers = parser.add_subparsers(dest="command", help="Available commands")

    # install command
    install_parser = subparsers.add_parser("install", help="Install a skill package")
    install_parser.add_argument("package", type=Path, help="Path to skill package")
    install_parser.add_argument("--force", action="store_true", help="Force install")

    # list command
    subparsers.add_parser("list", help="List installed skills")

    # remove command
    remove_parser = subparsers.add_parser("remove", help="Remove an installed skill")
    remove_parser.add_argument("skill_name", type=str, help="Name of skill to remove")

    return parser


def main() -> None:
    """CLI entry point."""
    parser = build_parser()
    args = parser.parse_args()

    installer = SkillInstallerCLI()

    if args.command == "install":
        success = installer.install(args.package, force=getattr(args, "force", False))
        sys.exit(0 if success else 1)
    elif args.command == "list":
        skills = installer.list_skills()
        if skills:
            print("[AgentOS] Installed skills:")
            for skill in skills:
                print(f"  - {skill}")
        else:
            print("[AgentOS] No skills installed.")
        sys.exit(0)
    elif args.command == "remove":
        success = installer.remove(args.skill_name)
        sys.exit(0 if success else 1)
    else:
        parser.print_help()
        sys.exit(0)


if __name__ == "__main__":
    main()
