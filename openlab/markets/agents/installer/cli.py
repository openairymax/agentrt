# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Agent Installer CLI
===================

Command-line interface for managing agents in the openlab Market.
This module provides a user-friendly CLI for installing, uninstalling,
listing, and managing agents.

Features:
- Interactive installation with progress display
- Multiple output formats (text, JSON, YAML)
- Tab completion for agent IDs and versions
- Colorful output with rich formatting (if available)
- Comprehensive help and documentation
"""

import argparse
import asyncio
import json
import sys
from pathlib import Path
from typing import Optional, List, Dict, Any

try:
    import yaml
    YAML_AVAILABLE = True
except ImportError:
    YAML_AVAILABLE = False

try:
    from rich.console import Console
    from rich.table import Table
    from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn
    from rich.panel import Panel
    from rich.syntax import Syntax
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False

# Import local modules
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent))
try:
    from markets.agents.installer.core import (
        AgentInstaller,
        InstallationSource,
        install_agent,
        InstallationResult
    )
    from markets.agents.contracts.validator import (
        AgentContractValidator,
        validate_contract
    )
except ImportError:
    # Fallback for direct execution
    from ..installer.core import (
        AgentInstaller,
        InstallationSource,
        install_agent,
        InstallationResult
    )
    from ..contracts.validator import (
        AgentContractValidator,
        validate_contract
    )


class AgentCLI:
    """
    Command-line interface for agent management.

    This class provides a comprehensive CLI for managing agents
    in the openlab Market, with support for multiple output formats
    and interactive features.
    """

    def __init__(
        self,
        install_root: Optional[str] = None,
        output_format: str = "text",
        verbose: bool = False
    ):
        """
        Initialize the CLI.

        Args:
            install_root: Root directory for agent installations.
            output_format: Output format (text, json, yaml).
            verbose: Enable verbose output.
        """
        self.install_root = install_root
        self.output_format = output_format
        self.verbose = verbose

        # Setup console for rich output
        if RICH_AVAILABLE:
            self.console = Console()
        else:
            self.console = None

    def print_result(self, result: Any, title: Optional[str] = None) -> None:
        """
        Print result in the configured output format.

        Args:
            result: Result to print.
            title: Optional title for the output.
        """
        if self.output_format == "json":
            output = json.dumps(result, indent=2, default=str)
            if title and self.console and RICH_AVAILABLE:
                self.console.print(Panel(Syntax(output, "json"), title=title))
            else:
                print(output)

        elif self.output_format == "yaml" and YAML_AVAILABLE:
            output = yaml.dump(result, default_flow_style=False)
            if title and self.console and RICH_AVAILABLE:
                self.console.print(Panel(Syntax(output, "yaml"), title=title))
            else:
                print(output)

        else:
            # Text format
            if isinstance(result, dict):
                self._print_dict(result, title)
            elif isinstance(result, list):
                self._print_list(result, title)
            else:
                if title and self.console and RICH_AVAILABLE:
                    self.console.print(Panel(str(result), title=title))
                else:
                    if title:
                        print(f"\n{title}")
                        print("=" * len(title))
                    print(str(result))

    def _print_dict(self, data: Dict[str, Any], title: Optional[str] = None) -> None:
        """
        Print dictionary in text format.

        Args:
            data: Dictionary to print.
            title: Optional title.
        """
        if title and self.console and RICH_AVAILABLE:
            table = Table(title=title, show_header=True, header_style="bold")
            table.add_column("Key", style="cyan")
            table.add_column("Value", style="green")

            for key, value in data.items():
                table.add_row(str(key), str(value))

            self.console.print(table)
        else:
            if title:
                print(f"\n{title}")
                print("=" * len(title))

            for key, value in data.items():
                print(f"{key}: {value}")

    def _print_list(self, data: List[Any], title: Optional[str] = None) -> None:
        """
        Print list in text format.

        Args:
            data: List to print.
            title: Optional title.
        """
        if not data:
            if title:
                print(f"\n{title}: No data")
            else:
                print("No data")
            return

        if self.console and RICH_AVAILABLE:
            # Try to detect if it's a list of dictionaries
            if all(isinstance(item, dict) for item in data):
                # Create table with columns from first item
                first_item = data[0]
                table = Table(title=title, show_header=True,
                              header_style="bold")

                # Add columns
                for key in first_item.keys():
                    table.add_column(str(key), style="cyan")

                # Add rows
                for item in data:
                    table.add_row(*[str(item.get(key, ""))
                                  for key in first_item.keys()])

                self.console.print(table)
            else:
                # Simple list
                if title:
                    self.console.print(Panel("\n".join(str(item)
                                       for item in data), title=title))
                else:
                    for item in data:
                        self.console.print(item)
        else:
            if title:
                print(f"\n{title}")
                print("=" * len(title))

            for i, item in enumerate(data, 1):
                if isinstance(item, dict):
                    print(f"\n{i}. ")
                    for key, value in item.items():
                        print(f"  {key}: {value}")
                else:
                    print(f"{i}. {item}")

    def print_error(self, message: str, details: Optional[str] = None) -> None:
        """
        Print error message.

        Args:
            message: Error message.
            details: Optional details.
        """
        if self.console and RICH_AVAILABLE:
            self.console.print(f"[bold red]Error:[/bold red] {message}")
            if details:
                self.console.print(f"[red]{details}[/red]")
        else:
            print(f"Error: {message}", file=sys.stderr)
            if details:
                print(f"Details: {details}", file=sys.stderr)

    def print_success(self, message: str) -> None:
        """
        Print success message.

        Args:
            message: Success message.
        """
        if self.console and RICH_AVAILABLE:
            self.console.print(f"[bold green]Success:[/bold green] {message}")
        else:
            print(f"Success: {message}")

    def print_warning(self, message: str) -> None:
        """
        Print warning message.

        Args:
            message: Warning message.
        """
        if self.console and RICH_AVAILABLE:
            self.console.print(
                f"[bold yellow]Warning:[/bold yellow] {message}")
        else:
            print(f"Warning: {message}")

    def print_info(self, message: str) -> None:
        """
        Print info message.

        Args:
            message: Info message.
        """
        if self.console and RICH_AVAILABLE:
            self.console.print(f"[bold blue]Info:[/bold blue] {message}")
        else:
            print(f"Info: {message}")

    async def install(
        self,
        source: str,
        source_type: Optional[str] = None,
        force: bool = False,
        validate_only: bool = False
    ) -> int:
        """
        Install an agent.

        Args:
            source: Source of the agent.
            source_type: Type of source (file, url, git, registry).
            force: Force installation.
            validate_only: Only validate without installing.

        Returns:
            Exit code (0 for success, 1 for failure).
        """
        # Map source type string to enum
        type_map = {
            "file": InstallationSource.LOCAL_FILE,
            "url": InstallationSource.URL,
            "git": InstallationSource.GIT_REPO,
            "registry": InstallationSource.MARKET_REGISTRY
        }

        source_type_enum = type_map.get(source_type) if source_type else None

        try:
            if self.verbose:
                self.print_info(f"Installing agent from: {source}")
                if source_type:
                    self.print_info(f"Source type: {source_type}")

            if self.console and RICH_AVAILABLE and not validate_only:
                # Show progress with rich
                with Progress(
                    SpinnerColumn(),
                    TextColumn("[progress.description]{task.description}"),
                    BarColumn(),
                    TextColumn(
                        "[progress.percentage]{task.percentage:>3.0f}%"),
                    console=self.console
                ) as progress:
                    task = progress.add_task("Installing agent...", total=100)

                    # We'll update progress based on installation steps
                    # For now, just show indeterminate progress
                    result = await install_agent(
                        source=source,
                        source_type=source_type_enum,
                        install_root=self.install_root,
                        force=force,
                        validate_only=validate_only
                    )

                    progress.update(task, completed=100)
            else:
                # Simple installation
                result = await install_agent(
                    source=source,
                    source_type=source_type_enum,
                    install_root=self.install_root,
                    force=force,
                    validate_only=validate_only
                )

            if result.success:
                if validate_only:
                    self.print_success(
                        f"Agent contract is valid: {result.agent_id} version {result.version}")
                else:
                    self.print_success(
                        f"Installed agent: {result.agent_id} version {result.version}")
                    self.print_info(
                        f"Installation path: {result.install_path}")

                if result.warnings:
                    for warning in result.warnings:
                        self.print_warning(warning)

                if self.verbose:
                    self.print_result(result.to_dict(), "Installation Details")

                return 0
            else:
                self.print_error(f"Failed to install agent")
                for error in result.errors:
                    self.print_error(error)

                if self.verbose and result.steps:
                    self.print_result(
                        [step.__dict__ for step in result.steps],
                        "Installation Steps"
                    )

                return 1

        except Exception as e:
            self.print_error(f"Installation failed: {str(e)}")
            if self.verbose:
                import traceback
                self.print_error("Traceback:", traceback.format_exc())
            return 1

    async def uninstall(
        self,
        agent_id: str,
        version: Optional[str] = None,
        force: bool = False
    ) -> int:
        """
        Uninstall an agent.

        Args:
            agent_id: ID of the agent to uninstall.
            version: Specific version to uninstall.
            force: Force uninstallation.

        Returns:
            Exit code (0 for success, 1 for failure).
        """
        try:
            async with AgentInstaller(self.install_root) as installer:
                if self.verbose:
                    if version:
                        self.print_info(
                            f"Uninstalling agent: {agent_id} version {version}")
                    else:
                        self.print_info(
                            f"Uninstalling all versions of agent: {agent_id}")

                success = await installer.uninstall(agent_id, version, force)

                if success:
                    if version:
                        self.print_success(
                            f"Uninstalled agent: {agent_id} version {version}")
                    else:
                        self.print_success(
                            f"Uninstalled all versions of agent: {agent_id}")
                    return 0
                else:
                    self.print_error(f"Failed to uninstall agent: {agent_id}")
                    return 1

        except Exception as e:
            self.print_error(f"Uninstallation failed: {str(e)}")
            if self.verbose:
                import traceback
                self.print_error("Traceback:", traceback.format_exc())
            return 1

    async def list_agents(
        self,
        agent_id: Optional[str] = None,
        detailed: bool = False
    ) -> int:
        """
        List installed agents.

        Args:
            agent_id: Optional agent ID to filter by.
            detailed: Show detailed information.

        Returns:
            Exit code (0 for success, 1 for failure).
        """
        try:
            async with AgentInstaller(self.install_root) as installer:
                agents = await installer.list_installed()

                if agent_id:
                    agents = [a for a in agents if a["agent_id"] == agent_id]

                if not agents:
                    self.print_info("No agents installed")
                    return 0

                if detailed:
                    # Show detailed information
                    detailed_agents = []
                    for agent in agents:
                        info = await installer.get_agent_info(
                            agent["agent_id"],
                            agent["version"]
                        )
                        if info:
                            detailed_agents.append(info)

                    self.print_result(
                        detailed_agents, "Installed Agents (Detailed)")
                else:
                    # Show summary
                    summary = []
                    for agent in agents:
                        summary.append({
                            "agent_id": agent["agent_id"],
                            "version": agent["version"],
                            "install_path": agent["install_path"]
                        })

                    self.print_result(summary, "Installed Agents")

                return 0

        except Exception as e:
            self.print_error(f"Failed to list agents: {str(e)}")
            if self.verbose:
                import traceback
                self.print_error("Traceback:", traceback.format_exc())
            return 1

    async def info(
        self,
        agent_id: str,
        version: Optional[str] = None
    ) -> int:
        """
        Show information about an installed agent.

        Args:
            agent_id: ID of the agent.
            version: Specific version. If None, shows latest version.

        Returns:
            Exit code (0 for success, 1 for failure).
        """
        try:
            async with AgentInstaller(self.install_root) as installer:
                info = await installer.get_agent_info(agent_id, version)

                if not info:
                    self.print_error(f"Agent not found: {agent_id}")
                    return 1

                self.print_result(info, f"Agent Information: {agent_id}")
                return 0

        except Exception as e:
            self.print_error(f"Failed to get agent info: {str(e)}")
            if self.verbose:
                import traceback
                self.print_error("Traceback:", traceback.format_exc())
            return 1

    async def validate(
        self,
        contract_file: str,
        strict: bool = False
    ) -> int:
        """
        Validate an agent contract.

        Args:
            contract_file: Path to contract file.
            strict: Treat warnings as errors.

        Returns:
            Exit code (0 for success, 1 for failure).
        """
        try:
            validator = AgentContractValidator()
            result = validator.validate_file(contract_file)

            if strict:
                # Treat warnings as errors
                for warning in result.get_warnings():
                    result.add_issue(warning._replace(severity="error"))

            if result.is_valid and not result.has_errors():
                self.print_success(f"Contract is valid: {contract_file}")

                if result.get_warnings():
                    self.print_warning("Warnings found:")
                    for warning in result.get_warnings():
                        self.print_warning(f"  - {warning}")

                if self.verbose:
                    self.print_result(result.to_dict(), "Validation Details")

                return 0
            else:
                self.print_error(f"Contract is invalid: {contract_file}")

                for error in result.get_errors():
                    self.print_error(f"  - {error}")

                if result.get_warnings():
                    self.print_warning("Warnings:")
                    for warning in result.get_warnings():
                        self.print_warning(f"  - {warning}")

                return 1

        except Exception as e:
            self.print_error(f"Validation failed: {str(e)}")
            if self.verbose:
                import traceback
                self.print_error("Traceback:", traceback.format_exc())
            return 1


def create_parser() -> argparse.ArgumentParser:
    """
    Create argument parser for the CLI.

    Returns:
        Configured argument parser.
    """
    parser = argparse.ArgumentParser(
        description="openlab Agent Management CLI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s install ./agent_contract.json
  %(prog)s install https://github.com/user/agent-repo.git --type git
  %(prog)s list
  %(prog)s info architect-agent
  %(prog)s uninstall architect-agent --version 1.0.0
  %(prog)s validate ./agent_contract.json --strict
        """
    )

    # Global options
    parser.add_argument(
        "--install-root",
        help="Root directory for agent installations (default: ~/.openlab/agents)"
    )
    parser.add_argument(
        "--format",
        choices=["text", "json", "yaml"],
        default="text",
        help="Output format (default: text)"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose output"
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable colored output"
    )

    # Subcommands
    subparsers = parser.add_subparsers(
        dest="command",
        title="commands",
        description="Available commands",
        required=True
    )

    # Install command
    install_parser = subparsers.add_parser(
        "install",
        help="Install an agent"
    )
    install_parser.add_argument(
        "source",
        help="Source of the agent (file path, URL, git repository, or contract JSON)"
    )
    install_parser.add_argument(
        "--type", "-t",
        choices=["file", "url", "git", "registry"],
        help="Source type (auto-detected if not specified)"
    )
    install_parser.add_argument(
        "--force", "-f",
        action="store_true",
        help="Force installation even if agent exists"
    )
    install_parser.add_argument(
        "--validate-only",
        action="store_true",
        help="Only validate without installing"
    )

    # Uninstall command
    uninstall_parser = subparsers.add_parser(
        "uninstall",
        help="Uninstall an agent"
    )
    uninstall_parser.add_argument(
        "agent_id",
        help="ID of the agent to uninstall"
    )
    uninstall_parser.add_argument(
        "--version", "-V",
        help="Specific version to uninstall (uninstalls all versions if not specified)"
    )
    uninstall_parser.add_argument(
        "--force", "-f",
        action="store_true",
        help="Force uninstallation"
    )

    # List command
    list_parser = subparsers.add_parser(
        "list",
        help="List installed agents"
    )
    list_parser.add_argument(
        "agent_id",
        nargs="?",
        help="Optional agent ID to filter by"
    )
    list_parser.add_argument(
        "--detailed", "-d",
        action="store_true",
        help="Show detailed information"
    )

    # Info command
    info_parser = subparsers.add_parser(
        "info",
        help="Show information about an installed agent"
    )
    info_parser.add_argument(
        "agent_id",
        help="ID of the agent"
    )
    info_parser.add_argument(
        "--version", "-V",
        help="Specific version (shows latest version if not specified)"
    )

    # Validate command
    validate_parser = subparsers.add_parser(
        "validate",
        help="Validate an agent contract"
    )
    validate_parser.add_argument(
        "contract_file",
        help="Path to contract file"
    )
    validate_parser.add_argument(
        "--strict",
        action="store_true",
        help="Treat warnings as errors"
    )

    return parser


async def main() -> int:
    """
    Main entry point for the CLI.

    Returns:
        Exit code.
    """
    parser = create_parser()
    args = parser.parse_args()

    # Disable rich if no-color is specified
    global RICH_AVAILABLE
    if args.no_color:
        RICH_AVAILABLE = False

    # Create CLI instance
    cli = AgentCLI(
        install_root=args.install_root,
        output_format=args.format,
        verbose=args.verbose
    )

    # Execute command
    if args.command == "install":
        return await cli.install(
            source=args.source,
            source_type=args.type,
            force=args.force,
            validate_only=args.validate_only
        )

    elif args.command == "uninstall":
        return await cli.uninstall(
            agent_id=args.agent_id,
            version=args.version,
            force=args.force
        )

    elif args.command == "list":
        return await cli.list_agents(
            agent_id=args.agent_id,
            detailed=args.detailed
        )

    elif args.command == "info":
        return await cli.info(
            agent_id=args.agent_id,
            version=args.version
        )

    elif args.command == "validate":
        return await cli.validate(
            contract_file=args.contract_file,
            strict=args.strict
        )

    else:
        parser.print_help()
        return 1


def run() -> None:
    """
    Run the CLI.
    """
    try:
        exit_code = asyncio.run(main())
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\nOperation cancelled by user", file=sys.stderr)
        sys.exit(130)
    except Exception as e:
        print(f"Unexpected error: {str(e)}", file=sys.stderr)
        if "--verbose" in sys.argv or "-v" in sys.argv:
            import traceback
            traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    run()
