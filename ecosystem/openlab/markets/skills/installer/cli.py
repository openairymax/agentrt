# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentOS OpenLab: Skills Installer CLI

Command-line interface for installing, managing, and removing AgentOS skills
from the marketplace. Supports local and remote skill packages.

Usage:
    python -m ecosystem.openlab.markets.skills.installer.cli install <skill_package>
    python -m ecosystem.openlab.markets.skills.installer.cli list
    python -m ecosystem.openlab.markets.skills.installer.cli remove <skill_name>
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
import zipfile
import tarfile
import tempfile
from pathlib import Path
from typing import List, Optional


class SkillInstallerCLI:
    """Command-line interface for skill installation management."""

    def __init__(self) -> None:
        self._skills_dir: Path = Path.home() / ".agentos" / "skills"
        self._skills_dir.mkdir(parents=True, exist_ok=True)

    def _extract_package(self, package_path: Path, dest_dir: Path) -> bool:
        try:
            if package_path.suffix == '.zip':
                with zipfile.ZipFile(package_path, 'r') as zf:
                    zf.extractall(dest_dir)
                return True
            elif package_path.suffix in ('.gz', '.bz2') and package_path.name.endswith('.tar.gz') or package_path.name.endswith('.tar.bz2'):
                with tarfile.open(package_path, 'r:*') as tf:
                    tf.extractall(dest_dir)
                return True
            elif package_path.is_dir():
                shutil.copytree(package_path, dest_dir, dirs_exist_ok=True)
                return True
            else:
                shutil.copy2(package_path, dest_dir / package_path.name)
                return True
        except Exception as e:
            print(f"[AgentOS] Error extracting package: {e}", file=sys.stderr)
            return False

    def _read_manifest(self, skill_dir: Path) -> Optional[dict]:
        manifest_path = skill_dir / "manifest.json"
        if not manifest_path.exists():
            manifest_path = skill_dir / "skill.json"
        if not manifest_path.exists():
            return None
        try:
            with open(manifest_path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except Exception:
            return None

    def install(self, package_path: Path, force: bool = False) -> bool:
        """Install a skill package by extracting and registering it."""
        if not package_path.exists():
            print(f"[AgentOS] Package not found: {package_path}", file=sys.stderr)
            return False

        print(f"[AgentOS] Installing skill from: {package_path}")

        with tempfile.TemporaryDirectory(prefix="agentos_skill_") as tmp_dir:
            tmp_path = Path(tmp_dir)
            if not self._extract_package(package_path, tmp_path):
                return False

            manifest = self._read_manifest(tmp_path)
            if not manifest or 'name' not in manifest:
                for item in tmp_path.iterdir():
                    if item.is_dir():
                        manifest = self._read_manifest(item)
                        if manifest and 'name' in manifest:
                            break

            if not manifest or 'name' not in manifest:
                print("[AgentOS] No valid skill manifest found in package", file=sys.stderr)
                return False

            skill_name = manifest['name']
            skill_version = manifest.get('version', '0.0.0')
            target_dir = self._skills_dir / skill_name

            if target_dir.exists():
                if not force:
                    print(f"[AgentOS] Skill '{skill_name}' is already installed. Use --force to reinstall.", file=sys.stderr)
                    return False
                shutil.rmtree(target_dir)

            shutil.copytree(tmp_path, target_dir, dirs_exist_ok=True)

            manifest_file = target_dir / "manifest.json"
            if not manifest_file.exists() and 'name' in manifest:
                with open(manifest_file, 'w', encoding='utf-8') as f:
                    json.dump(manifest, f, indent=2)

            print(f"[AgentOS] Skill '{skill_name}' v{skill_version} installed successfully.")
            return True

    def list_skills(self) -> List[str]:
        """List all installed skills."""
        if not self._skills_dir.exists():
            return []

        skills = []
        for d in sorted(self._skills_dir.iterdir()):
            if d.is_dir():
                manifest = self._read_manifest(d)
                if manifest and 'name' in manifest:
                    version = manifest.get('version', '?')
                    skills.append(f"{manifest['name']} (v{version})")
                else:
                    skills.append(d.name)
        return skills

    def remove(self, skill_name: str) -> bool:
        """Remove an installed skill and its associated files."""
        target_dir = self._skills_dir / skill_name

        if not target_dir.exists():
            print(f"[AgentOS] Skill '{skill_name}' is not installed.", file=sys.stderr)
            return False

        try:
            shutil.rmtree(target_dir)
            print(f"[AgentOS] Skill '{skill_name}' removed successfully.")
            return True
        except Exception as e:
            print(f"[AgentOS] Failed to remove skill '{skill_name}': {e}", file=sys.stderr)
            return False


def build_parser() -> argparse.ArgumentParser:
    """Build the CLI argument parser."""
    parser = argparse.ArgumentParser(
        prog="agentos-skill-installer",
        description="AgentOS Skill Installer CLI",
    )
    subparsers = parser.add_subparsers(dest="command", help="Available commands")

    install_parser = subparsers.add_parser("install", help="Install a skill package")
    install_parser.add_argument("package", type=Path, help="Path to skill package")
    install_parser.add_argument("--force", action="store_true", help="Force install")

    subparsers.add_parser("list", help="List installed skills")

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
