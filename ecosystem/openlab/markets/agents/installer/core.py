# Copyright (c) 2026 SPHARX. All Rights Reserved.
"""
AgentRT OpenLab: Agent Installer Core

Handles the agent installation lifecycle:
  1. Resolve agent source (local file, URL, git, market registry)
  2. Validate agent contract
  3. Call market_d via MarketClient for registration/installation
  4. Extract and deploy agent files

Implements the flow: agentrt install → OpenLab Markets → market_d → install
"""

from __future__ import annotations

import asyncio
import json
import shutil
import tempfile
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional

from ...client import (
    AgentInfo,
    InstallRequest,
    InstallResult,
    MarketClient,
    MarketConnectionError,
    MarketError,
)


class InstallationSource(Enum):
    """Source types for agent installation."""
    LOCAL_FILE = "file"
    URL = "url"
    GIT_REPO = "git"
    MARKET_REGISTRY = "registry"


@dataclass
class InstallationStep:
    """Records each step in the installation process for auditability."""
    step: str
    status: str  # "started", "completed", "failed"
    details: str = ""
    timestamp: float = 0.0


@dataclass
class InstallationResult:
    """Result of an agent installation operation."""
    success: bool = False
    agent_id: str = ""
    version: str = ""
    install_path: str = ""
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    steps: List[InstallationStep] = field(default_factory=list)
    raw_result: Optional[Dict[str, Any]] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "success": self.success,
            "agent_id": self.agent_id,
            "version": self.version,
            "install_path": self.install_path,
            "errors": self.errors,
            "warnings": self.warnings,
        }


class AgentInstaller:
    """Manages agent installation from various sources through market_d.

    Usage:
        async with AgentInstaller() as installer:
            result = await installer.install_from_registry("community/code-review")
    """

    DEFAULT_INSTALL_ROOT = Path.home() / ".agentos" / "agents"

    def __init__(self, install_root: Optional[str] = None):
        """Initialize the installer.

        Args:
            install_root: Root directory for agent installations.
                          Defaults to ~/.agentos/agents/.
        """
        self._install_root = Path(install_root) if install_root else self.DEFAULT_INSTALL_ROOT
        self._install_root.mkdir(parents=True, exist_ok=True)
        self._market_client: Optional[MarketClient] = None

    async def __aenter__(self):
        self._ensure_client()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        return False

    def _ensure_client(self):
        """Lazy-initialize the MarketClient."""
        if self._market_client is None:
            self._market_client = MarketClient()

    def _add_step(
        self,
        result: InstallationResult,
        step_name: str,
        status: str,
        details: str = "",
    ):
        import time
        result.steps.append(
            InstallationStep(
                step=step_name,
                status=status,
                details=details,
                timestamp=time.time(),
            )
        )

    async def install_from_registry(
        self,
        agent_id: str,
        version: str = "latest",
        force: bool = False,
        validate_only: bool = False,
    ) -> InstallationResult:
        """Install an agent from the market registry via market_d.

        This implements the full flow:
          agentrt install community/xxx → OpenLab Markets → market_d → install

        Args:
            agent_id: The agent identifier (e.g., "community/code-review").
            version: Version to install, "latest" for newest.
            force: If True, reinstall even if already installed.
            validate_only: If True, only validate without installing.

        Returns:
            InstallationResult with installation details.
        """
        result = InstallationResult(agent_id=agent_id, version=version)
        self._ensure_client()

        # Step 1: Resolve agent_id (strip prefix if needed)
        self._add_step(result, "resolve", "started")
        resolved_id = self._resolve_agent_id(agent_id)
        self._add_step(result, "resolve", "completed",
                       f"Resolved '{agent_id}' → '{resolved_id}'")

        # Step 2: Check market_d connectivity
        self._add_step(result, "connect", "started")
        try:
            healthy = self._market_client.ping()
        except MarketConnectionError as e:
            self._add_step(result, "connect", "failed", str(e))
            result.errors.append(f"market_d unreachable: {e}")
            result.success = False
            return result
        self._add_step(result, "connect", "completed",
                       "market_d healthy" if healthy else "market_d reachable")

        # Step 3: Search for agent in registry
        self._add_step(result, "search", "started")
        try:
            from ...client.models import SearchParams
            agents = self._market_client.search_agents(
                SearchParams(query=resolved_id, limit=5)
            )
        except MarketError as e:
            self._add_step(result, "search", "failed", str(e))
            result.errors.append(f"Search failed: {e}")
            result.success = False
            return result

        matching = [a for a in agents if a.agent_id == resolved_id or a.name == resolved_id]
        if not matching:
            self._add_step(result, "search", "completed",
                           f"Agent '{resolved_id}' not found in registry")
            if not validate_only:
                result.errors.append(f"Agent '{resolved_id}' not found in market")
                result.success = False
            return result

        agent_info = matching[0]
        self._add_step(result, "search", "completed",
                       f"Found {agent_info.name} v{agent_info.version}")

        # Step 4: Validate-only mode
        if validate_only:
            result.success = True
            result.agent_id = agent_info.agent_id
            result.version = agent_info.version
            return result

        # Step 5: Install via market_d
        self._add_step(result, "install", "started")
        try:
            install_req = InstallRequest(
                id=resolved_id,
                version=version,
                force_update=force,
                install_path=str(self._install_root),
            )
            market_result = self._market_client.install_agent(install_req)
        except MarketError as e:
            self._add_step(result, "install", "failed", str(e))
            result.errors.append(f"Installation failed: {e}")
            result.success = False
            return result

        if market_result.success:
            self._add_step(result, "install", "completed",
                           f"Installed v{market_result.installed_version}")
            result.success = True
            result.agent_id = resolved_id
            result.version = market_result.installed_version or version
            result.install_path = market_result.install_path or str(
                self._install_root / resolved_id
            )
            result.raw_result = market_result.to_dict() if hasattr(market_result, 'to_dict') else None
        else:
            self._add_step(result, "install", "failed",
                           market_result.message)
            result.errors.append(market_result.message)
            result.success = False

        return result

    async def install_from_file(
        self,
        file_path: str,
        force: bool = False,
        validate_only: bool = False,
    ) -> InstallationResult:
        """Install an agent from a local contract file.

        Args:
            file_path: Path to agent contract JSON file.
            force: Force installation.
            validate_only: Only validate.

        Returns:
            InstallationResult.
        """
        file_path = Path(file_path)
        result = InstallationResult()

        if not file_path.exists():
            result.errors.append(f"Contract file not found: {file_path}")
            result.success = False
            return result

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                contract = json.load(f)
        except json.JSONDecodeError as e:
            result.errors.append(f"Invalid JSON contract: {e}")
            result.success = False
            return result

        agent_id = contract.get("agent_id", "")
        if not agent_id:
            result.errors.append("Contract missing required field: agent_id")
            result.success = False
            return result

        result.agent_id = agent_id
        result.version = contract.get("version", "0.0.0")

        # Validate contract
        from ..contracts.validator import (
            AgentContractValidator,
        )
        validator = AgentContractValidator()
        validation_result = validator.validate(contract)

        if not validation_result.is_valid:
            result.errors.extend(str(e) for e in validation_result.get_errors())
            result.warnings.extend(str(w) for w in validation_result.get_warnings())
            result.success = False
            return result

        if validate_only:
            result.success = True
            return result

        # Install via market_d
        self._ensure_client()
        install_req = InstallRequest(
            id=agent_id,
            version=result.version,
            force_update=force,
            install_path=str(self._install_root),
        )

        try:
            market_result = self._market_client.install_agent(install_req)
        except MarketError as e:
            result.errors.append(f"Installation failed: {e}")
            result.success = False
            return result

        result.success = market_result.success
        result.install_path = market_result.install_path or str(
            self._install_root / agent_id
        )
        return result

    async def uninstall(
        self,
        agent_id: str,
        version: Optional[str] = None,
        force: bool = False,
    ) -> bool:
        """Uninstall an agent.

        Args:
            agent_id: ID of the agent to uninstall.
            version: Specific version, or None for all versions.
            force: Force removal.

        Returns:
            True if successfully uninstalled.
        """
        agent_dir = self._install_root / agent_id
        if version:
            agent_dir = agent_dir / version

        if not agent_dir.exists():
            return False

        try:
            if version:
                shutil.rmtree(agent_dir)
            else:
                shutil.rmtree(self._install_root / agent_id)
            return True
        except OSError:
            return False

    async def list_installed(self) -> List[Dict[str, Any]]:
        """List all locally installed agents.

        Returns:
            List of dicts with agent_id, version, and install_path.
        """
        agents: List[Dict[str, Any]] = []
        if not self._install_root.exists():
            return agents

        for agent_dir in sorted(self._install_root.iterdir()):
            if not agent_dir.is_dir():
                continue
            # Check for version subdirectories
            for version_dir in sorted(agent_dir.iterdir()):
                if version_dir.is_dir():
                    manifest = version_dir / "manifest.json"
                    if manifest.exists():
                        try:
                            with open(manifest, 'r', encoding='utf-8') as f:
                                data = json.load(f)
                        except json.JSONDecodeError:
                            data = {}
                    else:
                        data = {}

                    agents.append({
                        "agent_id": agent_dir.name,
                        "version": data.get("version", version_dir.name),
                        "install_path": str(version_dir),
                    })

            # If no version subdirs, check for manifest directly
            if not any(agents):
                manifest = agent_dir / "manifest.json"
                if manifest.exists():
                    try:
                        with open(manifest, 'r', encoding='utf-8') as f:
                            data = json.load(f)
                    except json.JSONDecodeError:
                        data = {}
                    agents.append({
                        "agent_id": agent_dir.name,
                        "version": data.get("version", "0.0.0"),
                        "install_path": str(agent_dir),
                    })

        return agents

    async def get_agent_info(
        self,
        agent_id: str,
        version: Optional[str] = None,
    ) -> Optional[Dict[str, Any]]:
        """Get detailed info about an installed agent.

        Args:
            agent_id: Agent ID.
            version: Specific version, or None for latest.

        Returns:
            Dict with agent metadata, or None if not found.
        """
        agent_dir = self._install_root / agent_id

        if version:
            agent_dir = agent_dir / version
        elif agent_dir.exists():
            versions = sorted(
                [d for d in agent_dir.iterdir() if d.is_dir()],
                reverse=True,
            )
            if versions:
                agent_dir = versions[0]

        manifest = agent_dir / "manifest.json"
        if not manifest.exists():
            return None

        try:
            with open(manifest, 'r', encoding='utf-8') as f:
                data = json.load(f)
        except json.JSONDecodeError:
            return None

        data["install_path"] = str(agent_dir)
        return data

    @staticmethod
    def _resolve_agent_id(agent_id: str) -> str:
        """Resolve an agent ID from user input to canonical form.

        Supports:
          - "community/code-review" → "code-review"
          - "code-review" → "code-review"
          - "openlab/code-review" → "openlab/code-review"
        """
        if "/" in agent_id:
            parts = agent_id.split("/", 1)
            if parts[0] in ("community", "market"):
                return parts[1]
        return agent_id


async def install_agent(
    source: str,
    source_type: Optional[InstallationSource] = None,
    install_root: Optional[str] = None,
    force: bool = False,
    validate_only: bool = False,
) -> InstallationResult:
    """Convenience function to install an agent from any source.

    Args:
        source: Agent source (file path, URL, git repo, or registry ID).
        source_type: Source type, auto-detected if None.
        install_root: Installation root directory.
        force: Force reinstall.
        validate_only: Only validate without installing.

    Returns:
        InstallationResult.
    """
    async with AgentInstaller(install_root) as installer:
        if source_type is None:
            source_type = _detect_source_type(source)

        if source_type == InstallationSource.REGISTRY:
            return await installer.install_from_registry(
                source, force=force, validate_only=validate_only
            )
        elif source_type == InstallationSource.LOCAL_FILE:
            return await installer.install_from_file(
                source, force=force, validate_only=validate_only
            )
        else:
            result = InstallationResult(success=False)
            result.errors.append(f"Unsupported source type: {source_type}")
            return result


def _detect_source_type(source: str) -> InstallationSource:
    """Auto-detect the installation source type."""
    # Check if it's a file path
    if source.endswith(('.json', '.yaml', '.yml')):
        return InstallationSource.LOCAL_FILE
    # Check if it's a URL
    if source.startswith(('http://', 'https://', 'git://', 'git+https://')):
        if 'github.com' in source or 'gitlab.com' in source:
            return InstallationSource.GIT_REPO
        return InstallationSource.URL
    # Default to registry lookup
    return InstallationSource.MARKET_REGISTRY