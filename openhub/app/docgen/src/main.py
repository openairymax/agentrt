# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Documentation Generator Main Entry Point
========================================

This module provides the main entry point for the OpenHub Documentation Generator.
It handles command-line argument parsing, configuration loading, and orchestrates
the documentation generation process.

Features:
- Command-line interface with comprehensive options
- Configuration validation and loading
- Integration with the core generator
- Error handling and reporting
- Progress display and logging
"""

import argparse
import asyncio
import json
import logging
import sys
from pathlib import Path
from typing import Dict, Any, Optional

try:
    import yaml
    YAML_AVAILABLE = True
except ImportError:
    YAML_AVAILABLE = False

# Import local modules
sys.path.insert(0, str(Path(__file__).parent.parent.parent))
try:
    from App.docgen.src.generator import (
        DocumentationGenerator,
        GenerationResult,
        generate_documentation
    )
except ImportError:
    # Fallback for direct execution
    from generator import (
        DocumentationGenerator,
        GenerationResult,
        generate_documentation
    )


def setup_logging(level: str = "INFO", log_file: Optional[str] = None) -> logging.Logger:
    """
    Setup logging configuration.
    
    Args:
        level: Logging level (DEBUG, INFO, WARNING, ERROR, CRITICAL).
        log_file: Optional log file path.
        
    Returns:
        Configured logger instance.
    """
    logger = logging.getLogger("docgen")
    logger.setLevel(getattr(logging, level.upper()))
    
    # Remove existing handlers
    logger.handlers.clear()
    
    # Create formatter
    formatter = logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )
    
    # Console handler
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)
    
    # File handler (if specified)
    if log_file:
        file_handler = logging.FileHandler(log_file, encoding='utf-8')
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)
    
    return logger


def load_config(config_path: str) -> Dict[str, Any]:
    """
    Load configuration from file.
    
    Args:
        config_path: Path to configuration file.
        
    Returns:
        Configuration dictionary.
        
    Raises:
        FileNotFoundError: If config file does not exist.
        ValueError: If config file format is unsupported or invalid.
    """
    path = Path(config_path)
    
    if not path.exists():
        raise FileNotFoundError(f"Configuration file not found: {config_path}")
    
    try:
        with open(path, 'r', encoding='utf-8') as f:
            if path.suffix in ['.yaml', '.yml']:
                if not YAML_AVAILABLE:
                    raise ImportError("PyYAML is required for YAML configuration files")
                config = yaml.safe_load(f)
            elif path.suffix == '.json':
                config = json.load(f)
            else:
                # Try to auto-detect format
                content = f.read()
                try:
                    config = json.loads(content)
                except json.JSONDecodeError:
                    if YAML_AVAILABLE:
                        config = yaml.safe_load(content)
                    else:
                        raise ValueError(
                            f"Unsupported configuration format: {path.suffix}. "
                            "Supported formats: .json, .yaml, .yml"
                        )
        
        if config is None:
            config = {}
        
        return config
    
    except Exception as e:
        raise ValueError(f"Failed to load configuration from {config_path}: {str(e)}")


def validate_config(config: Dict[str, Any]) -> None:
    """
    Validate configuration.
    
    Args:
        config: Configuration dictionary.
        
    Raises:
        ValueError: If configuration is invalid.
    """
    # Check required fields
    required_fields = ['input_dir', 'output_dir']
    for field in required_fields:
        if field not in config:
            raise ValueError(f"Missing required configuration field: {field}")
    
    # Validate input directory
    input_dir = Path(config['input_dir'])
    if not input_dir.exists():
        raise ValueError(f"Input directory does not exist: {input_dir}")
    
    # Validate output directory (will be created if it doesn't exist)
    output_dir = Path(config['output_dir'])
    
    # Validate template directory if specified
    if 'template_dir' in config:
        template_dir = Path(config['template_dir'])
        if not template_dir.exists():
            raise ValueError(f"Template directory does not exist: {template_dir}")
    
    # Validate include patterns
    if 'include_patterns' in config:
        include_patterns = config['include_patterns']
        if not isinstance(include_patterns, list):
            raise ValueError("include_patterns must be a list")
    
    # Validate exclude patterns
    if 'exclude_patterns' in config:
        exclude_patterns = config['exclude_patterns']
        if not isinstance(exclude_patterns, list):
            raise ValueError("exclude_patterns must be a list")
    
    # Validate output formats
    if 'output_formats' in config:
        output_formats = config['output_formats']
        if not isinstance(output_formats, list):
            raise ValueError("output_formats must be a list")
        
        for fmt in output_formats:
            if not isinstance(fmt, dict):
                raise ValueError("Each output format must be a dictionary")
            
            if 'format' not in fmt:
                raise ValueError("Output format missing 'format' field")
    
    # Validate logging configuration
    if 'logging' in config:
        logging_config = config['logging']
        if not isinstance(logging_config, dict):
            raise ValueError("logging must be a dictionary")
        
        if 'level' in logging_config:
            level = logging_config['level']
            valid_levels = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
            if level.upper() not in valid_levels:
                raise ValueError(f"Invalid logging level: {level}. Valid levels: {', '.join(valid_levels)}")


def print_result(result: GenerationResult, verbose: bool = False) -> None:
    """
    Print generation results.
    
    Args:
        result: Generation result object.
        verbose: Enable verbose output.
    """
    if result.success:
        print(f"✓ Documentation generation completed successfully")
        print(f"  Generated: {len(result.generated_files)} files")
        print(f"  Duration: {result.stats.get('duration', 0):.2f} seconds")
        
        if result.warnings:
            print(f"  Warnings: {len(result.warnings)}")
            if verbose:
                for warning in result.warnings:
                    print(f"    - {warning}")
            else:
                for warning in result.warnings[:5]:  # Show first 5 warnings
                    print(f"    - {warning}")
                if len(result.warnings) > 5:
                    print(f"    ... and {len(result.warnings) - 5} more")
        
        if verbose:
            print(f"\nGenerated files:")
            for file_path in result.generated_files[:10]:  # Show first 10 files
                print(f"  - {file_path}")
            if len(result.generated_files) > 10:
                print(f"  ... and {len(result.generated_files) - 10} more")
            
            if result.skipped_files:
                print(f"\nSkipped files: {len(result.skipped_files)}")
                for file_path in result.skipped_files[:5]:  # Show first 5 skipped files
                    print(f"  - {file_path}")
                if len(result.skipped_files) > 5:
                    print(f"  ... and {len(result.skipped_files) - 5} more")
    
    else:
        print(f"✗ Documentation generation failed")
        
        if result.errors:
            print(f"  Errors:")
            for error in result.errors:
                print(f"    - {error}")
        
        if result.failed_files:
            print(f"  Failed files:")
            for file_path, error in result.failed_files[:5]:  # Show first 5
                print(f"    - {file_path}: {error}")
            if len(result.failed_files) > 5:
                print(f"    ... and {len(result.failed_files) - 5} more")
        
        if result.warnings:
            print(f"  Warnings: {len(result.warnings)}")
            for warning in result.warnings[:3]:  # Show first 3 warnings
                print(f"    - {warning}")
            if len(result.warnings) > 3:
                print(f"    ... and {len(result.warnings) - 3} more")


async def run_generation(config_path: str, watch: bool = False, verbose: bool = False) -> int:
    """
    Run documentation generation.
    
    Args:
        config_path: Path to configuration file.
        watch: Enable file watching mode.
        verbose: Enable verbose output.
        
    Returns:
        Exit code (0 for success, non-zero for failure).
    """
    try:
        # Load configuration
        config = load_config(config_path)
        
        # Validate configuration
        validate_config(config)
        
        # Setup logging
        log_config = config.get('logging', {})
        log_level = log_config.get('level', 'DEBUG' if verbose else 'INFO')
        log_file = log_config.get('file')
        
        logger = setup_logging(log_level, log_file)
        logger.info(f"Starting documentation generator with config: {config_path}")
        
        # Create generator
        generator = DocumentationGenerator(config, logger)
        
        if watch:
            # Start file watcher
            logger.info("Starting file watcher")
            print("Starting file watcher. Press Ctrl+C to stop.")
            
            try:
                await generator.watch()
            except KeyboardInterrupt:
                logger.info("File watcher stopped by user")
                print("\nFile watcher stopped.")
                return 0
        
        else:
            # Generate documentation once
            logger.info("Starting documentation generation")
            result = await generator.generate()
            
            # Print results
            print_result(result, verbose)
            
            # Log results
            if result.success:
                logger.info(
                    f"Documentation generation completed successfully. "
                    f"Generated {len(result.generated_files)} files in "
                    f"{result.stats.get('duration', 0):.2f} seconds."
                )
                
                if result.warnings:
                    logger.warning(f"Generation completed with {len(result.warnings)} warnings")
                    for warning in result.warnings:
                        logger.warning(warning)
            else:
                logger.error(
                    f"Documentation generation failed with {len(result.errors)} errors "
                    f"and {len(result.failed_files)} failed files."
                )
                
                for error in result.errors:
                    logger.error(error)
                
                for file_path, error in result.failed_files:
                    logger.error(f"Failed to process {file_path}: {error}")
            
            return 0 if result.success else 1
    
    except FileNotFoundError as e:
        print(f"Error: {str(e)}", file=sys.stderr)
        return 1
    
    except ValueError as e:
        print(f"Configuration error: {str(e)}", file=sys.stderr)
        return 1
    
    except ImportError as e:
        print(f"Import error: {str(e)}", file=sys.stderr)
        print("Please install required dependencies:", file=sys.stderr)
        print("  pip install markdown pyyaml jinja2 watchdog", file=sys.stderr)
        return 1
    
    except KeyboardInterrupt:
        print("\nOperation cancelled by user", file=sys.stderr)
        return 130
    
    except Exception as e:
        print(f"Unexpected error: {str(e)}", file=sys.stderr)
        if verbose:
            import traceback
            traceback.print_exc()
        return 1


def create_parser() -> argparse.ArgumentParser:
    """
    Create argument parser.
    
    Returns:
        Configured argument parser.
    """
    parser = argparse.ArgumentParser(
        description="OpenHub Documentation Generator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s config.yaml                    # Generate documentation once
  %(prog)s config.yaml --watch            # Watch for changes and regenerate
  %(prog)s config.yaml --verbose          # Show detailed output
  %(prog)s --validate config.yaml         # Validate configuration only
  
Configuration File Format:
  The configuration file can be in YAML (.yaml, .yml) or JSON (.json) format.
  See config.yaml.example for a complete example.
        """
    )
    
    parser.add_argument(
        "config",
        nargs="?",
        default="config.yaml",
        help="Path to configuration file (default: config.yaml)"
    )
    
    parser.add_argument(
        "--watch", "-w",
        action="store_true",
        help="Watch for file changes and regenerate automatically"
    )
    
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose output"
    )
    
    parser.add_argument(
        "--validate",
        action="store_true",
        help="Validate configuration only (don't generate documentation)"
    )
    
    parser.add_argument(
        "--version",
        action="version",
        version="OpenHub Documentation Generator 1.0.0"
    )
    
    return parser


def validate_config_only(config_path: str) -> int:
    """
    Validate configuration only.
    
    Args:
        config_path: Path to configuration file.
        
    Returns:
        Exit code (0 for valid, non-zero for invalid).
    """
    try:
        config = load_config(config_path)
        validate_config(config)
        
        print(f"✓ Configuration is valid: {config_path}")
        print(f"  Input directory: {config.get('input_dir')}")
        print(f"  Output directory: {config.get('output_dir')}")
        
        if 'template_dir' in config:
            print(f"  Template directory: {config.get('template_dir')}")
        
        if 'include_patterns' in config:
            print(f"  Include patterns: {len(config['include_patterns'])}")
        
        if 'exclude_patterns' in config:
            print(f"  Exclude patterns: {len(config['exclude_patterns'])}")
        
        if 'output_formats' in config:
            formats = [fmt.get('format', 'unknown') for fmt in config['output_formats']]
            print(f"  Output formats: {', '.join(formats)}")
        
        return 0
    
    except Exception as e:
        print(f"✗ Configuration is invalid: {str(e)}", file=sys.stderr)
        return 1


async def main() -> int:
    """
    Main entry point.
    
    Returns:
        Exit code.
    """
    parser = create_parser()
    args = parser.parse_args()
    
    if args.validate:
        return validate_config_only(args.config)
    else:
        return await run_generation(args.config, args.watch, args.verbose)


def run() -> None:
    """
    Run the documentation generator.
    """
    try:
        exit_code = asyncio.run(main())
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\nOperation cancelled by user", file=sys.stderr)
        sys.exit(130)


if __name__ == "__main__":
    run()