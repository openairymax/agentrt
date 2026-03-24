# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Agent Installer Core
====================

This module provides the core installation logic for agents in the OpenHub Market.
It handles downloading, validating, installing, and managing agent packages.

Features:
- Agent package downloading from various sources (URL, local file, git)
- Dependency resolution and installation
- Agent registration in the system
- Installation rollback on failure
- Version management and updates
"""

import asyncio
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple, Union
from dataclasses import dataclass, field
from enum import Enum
import logging

try:
    import aiofiles
    import aiohttp
except ImportError:
    print("Error: aiofiles and aiohttp packages are required.")
    print("Install with: pip install aiofiles aiohttp")
    sys.exit(1)

# Import local modules
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent))
try:
    from markets.agents.contracts.validator import (
        AgentContractValidator,
        ValidationResult,
        validate_contract
    )
except ImportError:
    # Fallback for direct execution
    from ..contracts.validator import (
        AgentContractValidator,
        ValidationResult,
        validate_contract
    )


class InstallationSource(Enum):
    """Source types for agent installation."""
    LOCAL_FILE = "local_file"
    URL = "url"
    GIT_REPO = "git_repo"
    MARKET_REGISTRY = "market_registry"


class InstallationStatus(Enum):
    """Status of agent installation."""
    PENDING = "pending"
    DOWNLOADING = "downloading"
    VALIDATING = "validating"
    INSTALLING = "installing"
    REGISTERING = "registering"
    COMPLETED = "completed"
    FAILED = "failed"
    ROLLED_BACK = "rolled_back"


@dataclass
class InstallationStep:
    """Represents a step in the installation process."""
    name: str
    status: InstallationStatus
    start_time: Optional[float] = None
    end_time: Optional[float] = None
    error: Optional[str] = None
    details: Dict[str, Any] = field(default_factory=dict)


@dataclass
class InstallationResult:
    """Result of agent installation."""
    success: bool
    agent_id: str
    version: str
    install_path: Path
    steps: List[InstallationStep] = field(default_factory=list)
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)

    def add_step(self, step: InstallationStep) -> None:
        """Add an installation step to the result."""
        self.steps.append(step)

    def add_error(self, error: str) -> None:
        """Add an error to the result."""
        self.errors.append(error)
        self.success = False

    def add_warning(self, warning: str) -> None:
        """Add a warning to the result."""
        self.warnings.append(warning)

    def to_dict(self) -> Dict[str, Any]:
        """Convert installation result to dictionary."""
        return {
            "success": self.success,
            "agent_id": self.agent_id,
            "version": self.version,
            "install_path": str(self.install_path),
            "steps": [
                {
                    "name": step.name,
                    "status": step.status.value,
                    "error": step.error,
                    "details": step.details
                }
                for step in self.steps
            ],
            "errors": self.errors,
            "warnings": self.warnings,
            "metadata": self.metadata
        }


class AgentInstaller:
    """
    Core installer for agents in the OpenHub Market.
    
    This class handles the complete installation process for agents,
    including downloading, validation, dependency installation, and
    system registration.
    """
    
    def __init__(
        self,
        install_root: Optional[Union[str, Path]] = None,
        temp_dir: Optional[Union[str, Path]] = None,
        logger: Optional[logging.Logger] = None
    ):
        """
        Initialize the agent installer.
        
        Args:
            install_root: Root directory for agent installations.
                         Default: ~/.openhub/agents
            temp_dir: Temporary directory for downloads and extraction.
                     Default: system temp directory
            logger: Logger instance for logging. If None, creates a default logger.
        """
        if install_root is None:
            # Default installation root
            home = Path.home()
            self.install_root = home / ".openhub" / "agents"
        else:
            self.install_root = Path(install_root)
        
        if temp_dir is None:
            self.temp_dir = Path(tempfile.gettempdir()) / "openhub_install"
        else:
            self.temp_dir = Path(temp_dir)
        
        # Ensure directories exist
        self.install_root.mkdir(parents=True, exist_ok=True)
        self.temp_dir.mkdir(parents=True, exist_ok=True)
        
        # Setup logger
        if logger is None:
            self.logger = logging.getLogger(__name__)
            if not self.logger.handlers:
                handler = logging.StreamHandler()
                formatter = logging.Formatter(
                    '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
                )
                handler.setFormatter(formatter)
                self.logger.addHandler(handler)
                self.logger.setLevel(logging.INFO)
        else:
            self.logger = logger
        
        # Validator for agent contracts
        self.validator = AgentContractValidator()
        
        # HTTP session for downloads
        self.http_session = None
    
    async def __aenter__(self):
        """Async context manager entry."""
        await self._setup_http_session()
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Async context manager exit."""
        await self._cleanup_http_session()
    
    async def _setup_http_session(self) -> None:
        """Setup HTTP session for downloads."""
        self.http_session = aiohttp.ClientSession(
            timeout=aiohttp.ClientTimeout(total=300)
        )
    
    async def _cleanup_http_session(self) -> None:
        """Cleanup HTTP session."""
        if self.http_session:
            await self.http_session.close()
            self.http_session = None
    
    async def install(
        self,
        source: Union[str, Path, Dict[str, Any]],
        source_type: Optional[InstallationSource] = None,
        force: bool = False,
        validate_only: bool = False
    ) -> InstallationResult:
        """
        Install an agent from the given source.
        
        Args:
            source: Source of the agent (path, URL, git repo, or contract dict)
            source_type: Type of source. If None, auto-detects from source.
            force: Force installation even if agent already exists.
            validate_only: Only validate without installing.
            
        Returns:
            InstallationResult object containing installation results.
        """
        result = InstallationResult(
            success=False,
            agent_id="",
            version="",
            install_path=Path()
        )
        
        try:
            # Determine source type if not provided
            if source_type is None:
                source_type = self._detect_source_type(source)
            
            # Create installation steps
            steps = []
            
            # Step 1: Download/load agent package
            download_step = InstallationStep(
                name="download",
                status=InstallationStatus.PENDING
            )
            steps.append(download_step)
            
            download_step.start_time = asyncio.get_event_loop().time()
            download_step.status = InstallationStatus.DOWNLOADING
            
            package_path, contract_data = await self._download_package(
                source, source_type
            )
            
            download_step.end_time = asyncio.get_event_loop().time()
            download_step.status = InstallationStatus.COMPLETED
            download_step.details = {
                "package_path": str(package_path),
                "source_type": source_type.value
            }
            
            # Step 2: Validate agent contract
            validation_step = InstallationStep(
                name="validation",
                status=InstallationStatus.PENDING
            )
            steps.append(validation_step)
            
            validation_step.start_time = asyncio.get_event_loop().time()
            validation_step.status = InstallationStatus.VALIDATING
            
            validation_result = self.validator.validate(contract_data)
            
            validation_step.end_time = asyncio.get_event_loop().time()
            if validation_result.is_valid and not validation_result.has_errors():
                validation_step.status = InstallationStatus.COMPLETED
            else:
                validation_step.status = InstallationStatus.FAILED
                validation_step.error = "Contract validation failed"
                validation_step.details = {
                    "errors": [str(e) for e in validation_result.get_errors()],
                    "warnings": [str(w) for w in validation_result.get_warnings()]
                }
            
            # Check validation result
            if not validation_result.is_valid or validation_result.has_errors():
                result.add_error("Agent contract validation failed")
                for error in validation_result.get_errors():
                    result.add_error(str(error))
                result.steps = steps
                return result
            
            # Extract agent info
            agent_id = contract_data["agent_id"]
            version = contract_data["version"]
            
            result.agent_id = agent_id
            result.version = version
            
            # Check if agent already exists
            agent_dir = self.install_root / agent_id / version
            if agent_dir.exists() and not force:
                result.add_error(f"Agent {agent_id} version {version} already exists")
                result.steps = steps
                return result
            
            if validate_only:
                # Stop here if only validation is requested
                result.success = True
                result.steps = steps
                result.metadata = {
                    "contract": contract_data,
                    "validation_result": validation_result.to_dict()
                }
                return result
            
            # Step 3: Extract and prepare agent
            extraction_step = InstallationStep(
                name="extraction",
                status=InstallationStatus.PENDING
            )
            steps.append(extraction_step)
            
            extraction_step.start_time = asyncio.get_event_loop().time()
            extraction_step.status = InstallationStatus.INSTALLING
            
            # Create agent directory
            agent_dir.mkdir(parents=True, exist_ok=True)
            
            # Extract package if it's an archive
            if package_path.suffix in ['.zip', '.tar', '.gz', '.bz2']:
                await self._extract_package(package_path, agent_dir)
            else:
                # Copy single file or directory
                if package_path.is_dir():
                    shutil.copytree(package_path, agent_dir, dirs_exist_ok=True)
                else:
                    shutil.copy2(package_path, agent_dir)
            
            extraction_step.end_time = asyncio.get_event_loop().time()
            extraction_step.status = InstallationStatus.COMPLETED
            extraction_step.details = {
                "agent_dir": str(agent_dir),
                "package_type": package_path.suffix
            }
            
            # Step 4: Install dependencies
            dependencies_step = InstallationStep(
                name="dependencies",
                status=InstallationStatus.PENDING
            )
            steps.append(dependencies_step)
            
            dependencies_step.start_time = asyncio.get_event_loop().time()
            dependencies_step.status = InstallationStatus.INSTALLING
            
            dependencies = contract_data.get("dependencies", [])
            if dependencies:
                dep_result = await self._install_dependencies(dependencies, agent_dir)
                if not dep_result["success"]:
                    dependencies_step.status = InstallationStatus.FAILED
                    dependencies_step.error = "Dependency installation failed"
                    dependencies_step.details = dep_result
                    result.add_error("Failed to install dependencies")
                else:
                    dependencies_step.status = InstallationStatus.COMPLETED
                    dependencies_step.details = dep_result
            else:
                dependencies_step.status = InstallationStatus.COMPLETED
                dependencies_step.details = {"message": "No dependencies to install"}
            
            # Step 5: Register agent in system
            registration_step = InstallationStep(
                name="registration",
                status=InstallationStatus.PENDING
            )
            steps.append(registration_step)
            
            registration_step.start_time = asyncio.get_event_loop().time()
            registration_step.status = InstallationStatus.REGISTERING
            
            # Save contract to agent directory
            contract_file = agent_dir / "contract.json"
            async with aiofiles.open(contract_file, 'w', encoding='utf-8') as f:
                await f.write(json.dumps(contract_data, indent=2))
            
            # Create metadata file
            metadata = {
                "agent_id": agent_id,
                "version": version,
                "install_time": asyncio.get_event_loop().time(),
                "install_path": str(agent_dir),
                "contract": contract_data
            }
            
            metadata_file = agent_dir / "metadata.json"
            async with aiofiles.open(metadata_file, 'w', encoding='utf-8') as f:
                await f.write(json.dumps(metadata, indent=2))
            
            registration_step.end_time = asyncio.get_event_loop().time()
            registration_step.status = InstallationStatus.COMPLETED
            registration_step.details = {
                "contract_file": str(contract_file),
                "metadata_file": str(metadata_file)
            }
            
            # Update result
            result.success = True
            result.install_path = agent_dir
            result.metadata = metadata
            
            # Add warnings from validation
            for warning in validation_result.get_warnings():
                result.add_warning(str(warning))
            
            self.logger.info(
                f"Successfully installed agent {agent_id} version {version} "
                f"to {agent_dir}"
            )
            
        except Exception as e:
            self.logger.error(f"Installation failed: {str(e)}", exc_info=True)
            result.add_error(f"Installation failed: {str(e)}")
            
            # Create error step
            error_step = InstallationStep(
                name="error_handling",
                status=InstallationStatus.FAILED,
                error=str(e)
            )
            steps.append(error_step)
            
            # Attempt rollback
            try:
                await self._rollback_installation(result)
                rollback_step = InstallationStep(
                    name="rollback",
                    status=InstallationStatus.ROLLED_BACK
                )
                steps.append(rollback_step)
            except Exception as rollback_error:
                self.logger.error(f"Rollback failed: {str(rollback_error)}")
        
        result.steps = steps
        return result
    
    def _detect_source_type(self, source: Union[str, Path, Dict[str, Any]]) -> InstallationSource:
        """
        Detect the source type from the source value.
        
        Args:
            source: Source value (path, URL, git repo, or contract dict)
            
        Returns:
            InstallationSource enum value.
        """
        if isinstance(source, dict):
            return InstallationSource.MARKET_REGISTRY
        
        source_str = str(source)
        
        if source_str.startswith(('http://', 'https://')):
            if source_str.endswith('.git') or 'github.com' in source_str or 'gitlab.com' in source_str:
                return InstallationSource.GIT_REPO
            else:
                return InstallationSource.URL
        
        path = Path(source_str)
        if path.exists():
            return InstallationSource.LOCAL_FILE
        
        # Default to URL if it looks like a URL
        if '://' in source_str:
            return InstallationSource.URL
        
        # Default to local file
        return InstallationSource.LOCAL_FILE
    
    async def _download_package(
        self,
        source: Union[str, Path, Dict[str, Any]],
        source_type: InstallationSource
    ) -> Tuple[Path, Dict[str, Any]]:
        """
        Download or load agent package from source.
        
        Args:
            source: Source of the agent
            source_type: Type of source
            
        Returns:
            Tuple of (package_path, contract_data)
            
        Raises:
            ValueError: If source type is unsupported or source is invalid.
            IOError: If download fails.
        """
        if source_type == InstallationSource.LOCAL_FILE:
            return await self._download_local_file(source)
        elif source_type == InstallationSource.URL:
            return await self._download_from_url(source)
        elif source_type == InstallationSource.GIT_REPO:
            return await self._download_from_git(source)
        elif source_type == InstallationSource.MARKET_REGISTRY:
            return await self._load_from_registry(source)
        else:
            raise ValueError(f"Unsupported source type: {source_type}")
    
    async def _download_local_file(
        self,
        source: Union[str, Path]
    ) -> Tuple[Path, Dict[str, Any]]:
        """
        Load agent package from local file.
        
        Args:
            source: Path to local file or directory
            
        Returns:
            Tuple of (package_path, contract_data)
        """
        path = Path(source)
        
        if not path.exists():
            raise FileNotFoundError(f"Source file not found: {path}")
        
        # Check if it's a contract file or package
        if path.is_file() and path.suffix == '.json':
            # Load contract from JSON file
            async with aiofiles.open(path, 'r', encoding='utf-8') as f:
                content = await f.read()
                contract_data = json.loads(content)
            
            # For JSON contract files, the package is the parent directory
            package_path = path.parent
        elif path.is_dir():
            # Look for contract.json in directory
            contract_file = path / "contract.json"
            if contract_file.exists():
                async with aiofiles.open(contract_file, 'r', encoding='utf-8') as f:
                    content = await f.read()
                    contract_data = json.loads(content)
            else:
                raise FileNotFoundError(
                    f"No contract.json found in directory: {path}"
                )
            package_path = path
        else:
            # Assume it's a package file (zip, tar, etc.)
            # Extract to temp directory to find contract
            temp_extract = self.temp_dir / "extract"
            temp_extract.mkdir(exist_ok=True)
            
            await self._extract_package(path, temp_extract)
            
            # Look for contract in extracted files
            contract_file = temp_extract / "contract.json"
            if not contract_file.exists():
                # Search for contract.json in subdirectories
                for root, dirs, files in os.walk(temp_extract):
                    if "contract.json" in files:
                        contract_file = Path(root) / "contract.json"
                        break
            
            if not contract_file.exists():
                raise FileNotFoundError(
                    f"No contract.json found in package: {path}"
                )
            
            async with aiofiles.open(contract_file, 'r', encoding='utf-8') as f:
                content = await f.read()
                contract_data = json.loads(content)
            
            package_path = path
        
        return package_path, contract_data
    
    async def _download_from_url(
        self,
        url: str
    ) -> Tuple[Path, Dict[str, Any]]:
        """
        Download agent package from URL.
        
        Args:
            url: URL to download from
            
        Returns:
            Tuple of (package_path, contract_data)
        """
        if not self.http_session:
            raise RuntimeError("HTTP session not initialized")
        
        self.logger.info(f"Downloading from URL: {url}")
        
        # Create temp file for download
        temp_file = self.temp_dir / f"download_{hash(url)}.tmp"
        
        try:
            async with self.http_session.get(url) as response:
                response.raise_for_status()
                
                async with aiofiles.open(temp_file, 'wb') as f:
                    async for chunk in response.content.iter_chunked(8192):
                        await f.write(chunk)
            
            self.logger.info(f"Downloaded to: {temp_file}")
            
            # Process downloaded file
            return await self._download_local_file(temp_file)
            
        except Exception as e:
            if temp_file.exists():
                temp_file.unlink()
            raise IOError(f"Failed to download from URL {url}: {str(e)}")
    
    async def _download_from_git(
        self,
        git_url: str
    ) -> Tuple[Path, Dict[str, Any]]:
        """
        Clone agent package from git repository.
        
        Args:
            git_url: Git repository URL
            
        Returns:
            Tuple of (package_path, contract_data)
        """
        self.logger.info(f"Cloning git repository: {git_url}")
        
        # Create temp directory for clone
        temp_clone = self.temp_dir / f"clone_{hash(git_url)}"
        temp_clone.mkdir(exist_ok=True)
        
        try:
            # Clone repository
            clone_cmd = ["git", "clone", "--depth", "1", git_url, str(temp_clone)]
            
            process = await asyncio.create_subprocess_exec(
                *clone_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            stdout, stderr = await process.communicate()
            
            if process.returncode != 0:
                raise subprocess.CalledProcessError(
                    process.returncode,
                    clone_cmd,
                    stdout.decode(),
                    stderr.decode()
                )
            
            self.logger.info(f"Cloned to: {temp_clone}")
            
            # Process cloned repository
            return await self._download_local_file(temp_clone)
            
        except Exception as e:
            if temp_clone.exists():
                shutil.rmtree(temp_clone, ignore_errors=True)
            raise IOError(f"Failed to clone git repository {git_url}: {str(e)}")
    
    async def _load_from_registry(
        self,
        contract_data: Dict[str, Any]
    ) -> Tuple[Path, Dict[str, Any]]:
        """
        Load agent from market registry contract.
        
        Args:
            contract_data: Contract data from registry
            
        Returns:
            Tuple of (package_path, contract_data)
            
        Note:
            This method would typically download from a registry URL
            specified in the contract. For now, it returns the contract
            data directly.
        """
        # Check if contract has a download URL
        download_url = contract_data.get("download_url")
        if download_url:
            return await self._download_from_url(download_url)
        
        # If no download URL, create a temporary contract file
        temp_contract = self.temp_dir / f"contract_{hash(str(contract_data))}.json"
        
        async with aiofiles.open(temp_contract, 'w', encoding='utf-8') as f:
            await f.write(json.dumps(contract_data, indent=2))
        
        return temp_contract.parent, contract_data
    
    async def _extract_package(
        self,
        package_path: Path,
        extract_dir: Path
    ) -> None:
        """
        Extract package archive.
        
        Args:
            package_path: Path to package archive
            extract_dir: Directory to extract to
            
        Raises:
            ValueError: If package format is unsupported.
            IOError: If extraction fails.
        """
        if package_path.suffix == '.zip':
            with zipfile.ZipFile(package_path, 'r') as zip_ref:
                zip_ref.extractall(extract_dir)
        else:
            # For other archive formats, we would need additional libraries
            raise ValueError(f"Unsupported package format: {package_path.suffix}")
    
    async def _install_dependencies(
        self,
        dependencies: List[Dict[str, Any]],
        install_dir: Path
    ) -> Dict[str, Any]:
        """
        Install agent dependencies.
        
        Args:
            dependencies: List of dependency specifications
            install_dir: Installation directory
            
        Returns:
            Dictionary with installation results.
        """
        result = {
            "success": True,
            "installed": [],
            "failed": [],
            "skipped": []
        }
        
        # Create requirements.txt for pip
        requirements_file = install_dir / "requirements.txt"
        requirements = []
        
        for dep in dependencies:
            name = dep.get("name", "")
            version = dep.get("version", "")
            
            if not name:
                result["failed"].append({"dep": dep, "error": "Missing name"})
                result["success"] = False
                continue
            
            if version:
                requirements.append(f"{name}{version}")
            else:
                requirements.append(name)
        
        if not requirements:
            return result
        
        # Write requirements file
        async with aiofiles.open(requirements_file, 'w', encoding='utf-8') as f:
            await f.write("\n".join(requirements))
        
        # Install using pip
        try:
            pip_cmd = [
                sys.executable, "-m", "pip", "install",
                "-r", str(requirements_file),
                "--target", str(install_dir / "deps")
            ]
            
            process = await asyncio.create_subprocess_exec(
                *pip_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                cwd=install_dir
            )
            
            stdout, stderr = await process.communicate()
            
            if process.returncode == 0:
                result["installed"] = requirements
                self.logger.info(f"Installed dependencies: {requirements}")
            else:
                result["failed"] = [{"dep": req, "error": stderr.decode()} for req in requirements]
                result["success"] = False
                self.logger.error(f"Failed to install dependencies: {stderr.decode()}")
        
        except Exception as e:
            result["failed"] = [{"dep": req, "error": str(e)} for req in requirements]
            result["success"] = False
            self.logger.error(f"Exception installing dependencies: {str(e)}")
        
        return result
    
    async def _rollback_installation(
        self,
        result: InstallationResult
    ) -> None:
        """
        Rollback failed installation.
        
        Args:
            result: Installation result with failure information
        """
        if result.install_path and result.install_path.exists():
            try:
                shutil.rmtree(result.install_path, ignore_errors=True)
                self.logger.info(f"Rolled back installation: {result.install_path}")
            except Exception as e:
                self.logger.error(f"Failed to rollback installation: {str(e)}")
    
    async def uninstall(
        self,
        agent_id: str,
        version: Optional[str] = None,
        force: bool = False
    ) -> bool:
        """
        Uninstall an agent.
        
        Args:
            agent_id: ID of the agent to uninstall
            version: Specific version to uninstall. If None, uninstalls all versions.
            force: Force uninstallation even if agent is in use.
            
        Returns:
            True if uninstallation was successful, False otherwise.
        """
        agent_base_dir = self.install_root / agent_id
        
        if not agent_base_dir.exists():
            self.logger.warning(f"Agent {agent_id} not found")
            return False
        
        try:
            if version:
                # Uninstall specific version
                agent_dir = agent_base_dir / version
                if agent_dir.exists():
                    shutil.rmtree(agent_dir, ignore_errors=force)
                    self.logger.info(f"Uninstalled agent {agent_id} version {version}")
                else:
                    self.logger.warning(f"Agent {agent_id} version {version} not found")
                    return False
            else:
                # Uninstall all versions
                shutil.rmtree(agent_base_dir, ignore_errors=force)
                self.logger.info(f"Uninstalled all versions of agent {agent_id}")
            
            return True
            
        except Exception as e:
            self.logger.error(f"Failed to uninstall agent {agent_id}: {str(e)}")
            return False
    
    async def list_installed(self) -> List[Dict[str, Any]]:
        """
        List all installed agents.
        
        Returns:
            List of installed agent information.
        """
        agents = []
        
        if not self.install_root.exists():
            return agents
        
        for agent_dir in self.install_root.iterdir():
            if not agent_dir.is_dir():
                continue
            
            agent_id = agent_dir.name
            
            for version_dir in agent_dir.iterdir():
                if not version_dir.is_dir():
                    continue
                
                version = version_dir.name
                
                # Load metadata if available
                metadata_file = version_dir / "metadata.json"
                metadata = {}
                if metadata_file.exists():
                    try:
                        async with aiofiles.open(metadata_file, 'r', encoding='utf-8') as f:
                            content = await f.read()
                            metadata = json.loads(content)
                    except Exception:
                        pass
                
                agents.append({
                    "agent_id": agent_id,
                    "version": version,
                    "install_path": str(version_dir),
                    "metadata": metadata
                })
        
        return agents
    
    async def get_agent_info(
        self,
        agent_id: str,
        version: Optional[str] = None
    ) -> Optional[Dict[str, Any]]:
        """
        Get information about an installed agent.
        
        Args:
            agent_id: ID of the agent
            version: Specific version. If None, gets latest version.
            
        Returns:
            Agent information dictionary, or None if not found.
        """
        agent_base_dir = self.install_root / agent_id
        
        if not agent_base_dir.exists():
            return None
        
        if version:
            agent_dir = agent_base_dir / version
            if not agent_dir.exists():
                return None
            versions = [version]
        else:
            # Get all versions and sort by version number
            versions = []
            for version_dir in agent_base_dir.iterdir():
                if version_dir.is_dir():
                    versions.append(version_dir.name)
            
            if not versions:
                return None
            
            # Sort versions (simple string sort for now)
            versions.sort(reverse=True)
            version = versions[0]
            agent_dir = agent_base_dir / version
        
        # Load metadata
        metadata_file = agent_dir / "metadata.json"
        if not metadata_file.exists():
            return None
        
        try:
            async with aiofiles.open(metadata_file, 'r', encoding='utf-8') as f:
                content = await f.read()
                metadata = json.loads(content)
            
            return metadata
            
        except Exception:
            return None


async def install_agent(
    source: Union[str, Path, Dict[str, Any]],
    source_type: Optional[InstallationSource] = None,
    install_root: Optional[Union[str, Path]] = None,
    force: bool = False,
    validate_only: bool = False
) -> InstallationResult:
    """
    Convenience function to install an agent.
    
    Args:
        source: Source of the agent
        source_type: Type of source. If None, auto-detects.
        install_root: Root directory for installations.
        force: Force installation even if agent exists.
        validate_only: Only validate without installing.
        
    Returns:
        InstallationResult object.
    """
    async with AgentInstaller(install_root) as installer:
        return await installer.install(
            source, source_type, force, validate_only
        )


async def main():
    """Command-line interface for agent installation."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Install agents for OpenHub Market"
    )
    parser.add_argument(
        "source",
        help="Source of the agent (file path, URL, or git repository)"
    )
    parser.add_argument(
        "--type",
        choices=["file", "url", "git", "registry"],
        help="Source type (auto-detected if not specified)"
    )
    parser.add_argument(
        "--install-root",
        help="Root directory for agent installations"
    )
    parser.add_argument(
        "--force", "-f",
        action="store_true",
        help="Force installation even if agent exists"
    )
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="Only validate without installing"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Show detailed installation progress"
    )
    
    args = parser.parse_args()
    
    # Map type string to enum
    type_map = {
        "file": InstallationSource.LOCAL_FILE,
        "url": InstallationSource.URL,
        "git": InstallationSource.GIT_REPO,
        "registry": InstallationSource.MARKET_REGISTRY
    }
    
    source_type = type_map.get(args.type) if args.type else None
    
    try:
        result = await install_agent(
            source=args.source,
            source_type=source_type,
            install_root=args.install_root,
            force=args.force,
            validate_only=args.validate_only
        )
        
        if args.verbose:
            print(json.dumps(result.to_dict(), indent=2))
        else:
            if result.success:
                print(f"✓ Successfully installed agent {result.agent_id} version {result.version}")
                if result.warnings:
                    print(f"  Warnings: {len(result.warnings)}")
            else:
                print(f"✗ Failed to install agent")
                for error in result.errors:
                    print(f"  - {error}")
        
        sys.exit(0 if result.success else 1)
        
    except Exception as e:
        print(f"Error: {str(e)}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())