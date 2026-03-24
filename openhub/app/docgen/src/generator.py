# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Documentation Generator Core
============================

This module provides the core functionality for generating documentation
from various source formats (Markdown, Python, YAML, JSON) into multiple
output formats (HTML, PDF, Markdown).

Features:
- Multi-format input support
- Template-based rendering with Jinja2
- File watching for automatic regeneration
- Navigation generation
- Search index creation
- Performance optimization with caching
"""

import asyncio
import hashlib
import json
import logging
import os
import re
import shutil
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple, Set
from dataclasses import dataclass, field
from enum import Enum

try:
    import markdown
    import yaml
    import jinja2
    from watchdog.observers import Observer
    from watchdog.events import FileSystemEventHandler, FileSystemEvent
    MARKDOWN_AVAILABLE = True
    YAML_AVAILABLE = True
    JINJA2_AVAILABLE = True
    WATCHDOG_AVAILABLE = True
except ImportError:
    MARKDOWN_AVAILABLE = False
    YAML_AVAILABLE = False
    JINJA2_AVAILABLE = False
    WATCHDOG_AVAILABLE = False


class FileType(Enum):
    """Supported file types for documentation generation."""
    MARKDOWN = "markdown"
    PYTHON = "python"
    YAML = "yaml"
    JSON = "json"
    TEXT = "text"
    UNKNOWN = "unknown"


class OutputFormat(Enum):
    """Supported output formats."""
    HTML = "html"
    PDF = "pdf"
    MARKDOWN = "markdown"


@dataclass
class FileMetadata:
    """Metadata for a source file."""
    path: Path
    file_type: FileType
    size: int
    modified_time: float
    checksum: str
    title: str = ""
    description: str = ""
    tags: List[str] = field(default_factory=list)
    dependencies: List[str] = field(default_factory=list)
    frontmatter: Dict[str, Any] = field(default_factory=dict)


@dataclass
class GenerationResult:
    """Result of documentation generation."""
    success: bool
    generated_files: List[Path] = field(default_factory=list)
    skipped_files: List[Path] = field(default_factory=list)
    failed_files: List[Tuple[Path, str]] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    errors: List[str] = field(default_factory=list)
    stats: Dict[str, Any] = field(default_factory=dict)
    
    def add_generated(self, path: Path) -> None:
        """Add a generated file to the result."""
        self.generated_files.append(path)
    
    def add_skipped(self, path: Path) -> None:
        """Add a skipped file to the result."""
        self.skipped_files.append(path)
    
    def add_failed(self, path: Path, error: str) -> None:
        """Add a failed file to the result."""
        self.failed_files.append((path, error))
        self.success = False
    
    def add_warning(self, warning: str) -> None:
        """Add a warning to the result."""
        self.warnings.append(warning)
    
    def add_error(self, error: str) -> None:
        """Add an error to the result."""
        self.errors.append(error)
        self.success = False


class DocumentationGenerator:
    """
    Core documentation generator for OpenHub projects.
    
    This class handles the complete documentation generation process,
    including file discovery, parsing, template rendering, and output
    generation.
    """
    
    def __init__(
        self,
        config: Dict[str, Any],
        logger: Optional[logging.Logger] = None
    ):
        """
        Initialize the documentation generator.
        
        Args:
            config: Configuration dictionary.
            logger: Logger instance. If None, creates a default logger.
        """
        self.config = config
        self._setup_logger(logger)
        self._setup_directories()
        self._setup_templates()
        self._setup_cache()
        
        # File type patterns
        self.file_patterns = {
            FileType.MARKDOWN: re.compile(r'\.(md|markdown)$', re.IGNORECASE),
            FileType.PYTHON: re.compile(r'\.py$', re.IGNORECASE),
            FileType.YAML: re.compile(r'\.(yaml|yml)$', re.IGNORECASE),
            FileType.JSON: re.compile(r'\.json$', re.IGNORECASE),
            FileType.TEXT: re.compile(r'\.txt$', re.IGNORECASE),
        }
        
        # Statistics
        self.stats = {
            "files_processed": 0,
            "files_generated": 0,
            "files_skipped": 0,
            "files_failed": 0,
            "start_time": 0,
            "end_time": 0,
            "duration": 0,
        }
    
    def _setup_logger(self, logger: Optional[logging.Logger]) -> None:
        """Setup logger for the generator."""
        if logger is None:
            self.logger = logging.getLogger(__name__)
            if not self.logger.handlers:
                handler = logging.StreamHandler()
                formatter = logging.Formatter(
                    '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
                )
                handler.setFormatter(formatter)
                self.logger.addHandler(handler)
                
                log_config = self.config.get("logging", {})
                level = log_config.get("level", "INFO")
                self.logger.setLevel(getattr(logging, level.upper()))
        else:
            self.logger = logger
    
    def _setup_directories(self) -> None:
        """Setup input and output directories."""
        self.input_dir = Path(self.config.get("input_dir", ".")).resolve()
        self.output_dir = Path(self.config.get("output_dir", "./docs")).resolve()
        self.template_dir = Path(self.config.get("template_dir", "./templates")).resolve()
        
        # Ensure directories exist
        self.input_dir.mkdir(parents=True, exist_ok=True)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.template_dir.mkdir(parents=True, exist_ok=True)
        
        self.logger.info(f"Input directory: {self.input_dir}")
        self.logger.info(f"Output directory: {self.output_dir}")
        self.logger.info(f"Template directory: {self.template_dir}")
    
    def _setup_templates(self) -> None:
        """Setup Jinja2 template environment."""
        if not JINJA2_AVAILABLE:
            raise ImportError("Jinja2 is required for template rendering")
        
        # Create template loader
        template_loader = jinja2.FileSystemLoader(searchpath=self.template_dir)
        
        # Create template environment
        self.template_env = jinja2.Environment(
            loader=template_loader,
            autoescape=jinja2.select_autoescape(['html', 'xml']),
            trim_blocks=True,
            lstrip_blocks=True,
        )
        
        # Add custom filters
        self.template_env.filters['slugify'] = self._slugify
        self.template_env.filters['markdown'] = self._markdown_filter
        self.template_env.filters['tojson'] = lambda x: json.dumps(x, indent=2)
        
        # Add global variables
        self.template_env.globals.update({
            'now': datetime.now,
            'config': self.config,
        })
    
    def _setup_cache(self) -> None:
        """Setup file cache for performance."""
        self.cache_enabled = self.config.get("cache_enabled", True)
        self.cache_ttl = self.config.get("cache_ttl", 3600)
        
        if self.cache_enabled:
            self.cache_dir = Path(tempfile.gettempdir()) / "openhub_docgen_cache"
            self.cache_dir.mkdir(parents=True, exist_ok=True)
            self.logger.info(f"Cache directory: {self.cache_dir}")
        else:
            self.cache_dir = None
    
    def _slugify(self, text: str) -> str:
        """
        Convert text to URL-friendly slug.
        
        Args:
            text: Text to slugify.
            
        Returns:
            Slugified text.
        """
        # Convert to lowercase
        text = text.lower()
        
        # Replace non-alphanumeric characters with hyphens
        text = re.sub(r'[^a-z0-9]+', '-', text)
        
        # Remove leading/trailing hyphens
        text = text.strip('-')
        
        return text
    
    def _markdown_filter(self, text: str) -> str:
        """
        Convert Markdown to HTML.
        
        Args:
            text: Markdown text.
            
        Returns:
            HTML text.
        """
        if not MARKDOWN_AVAILABLE:
            return text
        
        extensions = self.config.get("markdown_extensions", ["extra", "codehilite"])
        md = markdown.Markdown(extensions=extensions)
        return md.convert(text)
    
    def _get_file_type(self, path: Path) -> FileType:
        """
        Determine file type from extension.
        
        Args:
            path: File path.
            
        Returns:
            FileType enum.
        """
        for file_type, pattern in self.file_patterns.items():
            if pattern.search(str(path)):
                return file_type
        
        return FileType.UNKNOWN
    
    def _should_process_file(self, path: Path) -> bool:
        """
        Check if a file should be processed.
        
        Args:
            path: File path.
            
        Returns:
            True if file should be processed, False otherwise.
        """
        # Check if file exists
        if not path.exists() or not path.is_file():
            return False
        
        # Check include patterns
        include_patterns = self.config.get("include_patterns", [])
        if include_patterns:
            matched = False
            for pattern in include_patterns:
                if path.match(pattern):
                    matched = True
                    break
            if not matched:
                return False
        
        # Check exclude patterns
        exclude_patterns = self.config.get("exclude_patterns", [])
        for pattern in exclude_patterns:
            if path.match(pattern):
                return False
        
        # Check file type
        file_type = self._get_file_type(path)
        if file_type == FileType.UNKNOWN:
            return False
        
        return True
    
    def _calculate_checksum(self, path: Path) -> str:
        """
        Calculate file checksum.
        
        Args:
            path: File path.
            
        Returns:
            MD5 checksum.
        """
        hasher = hashlib.md5()
        with open(path, 'rb') as f:
            for chunk in iter(lambda: f.read(4096), b''):
                hasher.update(chunk)
        return hasher.hexdigest()
    
    def _get_file_metadata(self, path: Path) -> FileMetadata:
        """
        Extract metadata from a file.
        
        Args:
            path: File path.
            
        Returns:
            FileMetadata object.
        """
        stat = path.stat()
        checksum = self._calculate_checksum(path)
        file_type = self._get_file_type(path)
        
        metadata = FileMetadata(
            path=path,
            file_type=file_type,
            size=stat.st_size,
            modified_time=stat.st_mtime,
            checksum=checksum,
        )
        
        # Extract title and description based on file type
        if file_type == FileType.MARKDOWN:
            self._extract_markdown_metadata(path, metadata)
        elif file_type == FileType.PYTHON:
            self._extract_python_metadata(path, metadata)
        elif file_type == FileType.YAML:
            self._extract_yaml_metadata(path, metadata)
        elif file_type == FileType.JSON:
            self._extract_json_metadata(path, metadata)
        
        return metadata
    
    def _extract_markdown_metadata(self, path: Path, metadata: FileMetadata) -> None:
        """
        Extract metadata from Markdown file.
        
        Args:
            path: Markdown file path.
            metadata: FileMetadata object to update.
        """
        try:
            with open(path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Extract frontmatter (YAML at beginning of file)
            frontmatter_match = re.match(r'^---\n(.*?)\n---\n', content, re.DOTALL)
            if frontmatter_match:
                frontmatter_text = frontmatter_match.group(1)
                try:
                    metadata.frontmatter = yaml.safe_load(frontmatter_text) or {}
                except yaml.YAMLError:
                    metadata.frontmatter = {}
                
                # Extract title and description from frontmatter
                metadata.title = metadata.frontmatter.get('title', path.stem)
                metadata.description = metadata.frontmatter.get('description', '')
                metadata.tags = metadata.frontmatter.get('tags', [])
            
            # If no frontmatter, extract first heading as title
            if not metadata.title or metadata.title == path.stem:
                heading_match = re.search(r'^#\s+(.+)$', content, re.MULTILINE)
                if heading_match:
                    metadata.title = heading_match.group(1).strip()
            
            # Extract description from first paragraph
            if not metadata.description:
                # Remove frontmatter if present
                content_no_frontmatter = re.sub(r'^---\n.*?\n---\n', '', content, flags=re.DOTALL)
                
                # Find first paragraph
                paragraph_match = re.search(r'^\s*([^\n#].+?)\n\n', content_no_frontmatter, re.DOTALL)
                if paragraph_match:
                    metadata.description = paragraph_match.group(1).strip()[:200]
        
        except Exception as e:
            self.logger.warning(f"Failed to extract metadata from {path}: {str(e)}")
    
    def _extract_python_metadata(self, path: Path, metadata: FileMetadata) -> None:
        """
        Extract metadata from Python file.
        
        Args:
            path: Python file path.
            metadata: FileMetadata object to update.
        """
        try:
            with open(path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Extract module docstring
            docstring_match = re.search(r'^\"\"\"(.*?)\"\"\"', content, re.DOTALL)
            if docstring_match:
                docstring = docstring_match.group(1).strip()
                
                # Extract title (first line)
                lines = docstring.split('\n')
                if lines:
                    metadata.title = lines[0].strip()
                    
                    # Extract description (remaining lines)
                    if len(lines) > 1:
                        metadata.description = ' '.join(lines[1:]).strip()[:200]
            
            # If no docstring, use filename
            if not metadata.title:
                metadata.title = path.stem
            
            # Extract imports as dependencies
            import_pattern = r'^(?:from\s+(\S+)\s+import|import\s+(\S+))'
            imports = re.findall(import_pattern, content, re.MULTILINE)
            
            for import_match in imports:
                module = import_match[0] or import_match[1]
                if module and module not in metadata.dependencies:
                    metadata.dependencies.append(module)
        
        except Exception as e:
            self.logger.warning(f"Failed to extract metadata from {path}: {str(e)}")
    
    def _extract_yaml_metadata(self, path: Path, metadata: FileMetadata) -> None:
        """
        Extract metadata from YAML file.
        
        Args:
            path: YAML file path.
            metadata: FileMetadata object to update.
        """
        try:
            with open(path, 'r', encoding='utf-8') as f:
                content = yaml.safe_load(f)
            
            if isinstance(content, dict):
                metadata.frontmatter = content
                metadata.title = content.get('name', path.stem)
                metadata.description = content.get('description', '')
                metadata.tags = content.get('tags', [])
            else:
                metadata.title = path.stem
        
        except Exception as e:
            self.logger.warning(f"Failed to extract metadata from {path}: {str(e)}")
    
    def _extract_json_metadata(self, path: Path, metadata: FileMetadata) -> None:
        """
        Extract metadata from JSON file.
        
        Args:
            path: JSON file path.
            metadata: FileMetadata object to update.
        """
        try:
            with open(path, 'r', encoding='utf-8') as f:
                content = json.load(f)
            
            if isinstance(content, dict):
                metadata.frontmatter = content
                metadata.title = content.get('name', path.stem)
                metadata.description = content.get('description', '')
                metadata.tags = content.get('tags', [])
            else:
                metadata.title = path.stem
        
        except Exception as e:
            self.logger.warning(f"Failed to extract metadata from {path}: {str(e)}")
    
    def _is_cached(self, metadata: FileMetadata, output_format: OutputFormat) -> bool:
        """
        Check if file generation result is cached.
        
        Args:
            metadata: File metadata.
            output_format: Output format.
            
        Returns:
            True if cached and valid, False otherwise.
        """
        if not self.cache_enabled or not self.cache_dir:
            return False
        
        cache_key = f"{metadata.checksum}_{output_format.value}"
        cache_file = self.cache_dir / f"{cache_key}.cache"
        
        if not cache_file.exists():
            return False
        
        # Check cache TTL
        cache_stat = cache_file.stat()
        cache_age = time.time() - cache_stat.st_mtime
        
        if cache_age > self.cache_ttl:
            return False
        
        # Read cache metadata
        try:
            with open(cache_file, 'r', encoding='utf-8') as f:
                cache_data = json.load(f)
            
            # Verify cache matches current metadata
            return (
                cache_data.get('checksum') == metadata.checksum and
                cache_data.get('modified_time') == metadata.modified_time
            )
        
        except Exception:
            return False
    
    def _save_to_cache(self, metadata: FileMetadata, output_format: OutputFormat, content: str) -> None:
        """
        Save generation result to cache.
        
        Args:
            metadata: File metadata.
            output_format: Output format.
            content: Generated content.
        """
        if not self.cache_enabled or not self.cache_dir:
            return
        
        cache_key = f"{metadata.checksum}_{output_format.value}"
        cache_file = self.cache_dir / f"{cache_key}.cache"
        
        cache_data = {
            'checksum': metadata.checksum,
            'modified_time': metadata.modified_time,
            'output_format': output_format.value,
            'content': content,
            'timestamp': time.time(),
        }
        
        try:
            with open(cache_file, 'w', encoding='utf-8') as f:
                json.dump(cache_data, f)
        except Exception as e:
            self.logger.warning(f"Failed to save cache for {metadata.path}: {str(e)}")
    
    def _load_from_cache(self, metadata: FileMetadata, output_format: OutputFormat) -> Optional[str]:
        """
        Load generation result from cache.
        
        Args:
            metadata: File metadata.
            output_format: Output format.
            
        Returns:
            Cached content, or None if not found.
        """
        if not self.cache_enabled or not self.cache_dir:
            return None
        
        cache_key = f"{metadata.checksum}_{output_format.value}"
        cache_file = self.cache_dir / f"{cache_key}.cache"
        
        try:
            with open(cache_file, 'r', encoding='utf-8') as f:
                cache_data = json.load(f)
            
            return cache_data.get('content')
        
        except Exception:
            return None
    
    def _process_file(self, path: Path, output_formats: List[OutputFormat]) -> Tuple[List[Path], List[str]]:
        """
        Process a single file.
        
        Args:
            path: File path.
            output_formats: List of output formats to generate.
            
        Returns:
            Tuple of (generated_files, warnings).
        """
        generated_files = []
        warnings = []
        
        try:
            # Check if file should be processed
            if not self._should_process_file(path):
                return [], [f"Skipped file (excluded): {path}"]
            
            # Get file metadata
            metadata = self._get_file_metadata(path)
            
            # Process for each output format
            for output_format in output_formats:
                # Check cache
                if self._is_cached(metadata, output_format):
                    self.logger.debug(f"Using cache for {path} -> {output_format.value}")
                    continue
                
                # Generate content
                content = self._generate_content(metadata, output_format)
                if content is None:
                    warnings.append(f"Failed to generate {output_format.value} for {path}")
                    continue
                
                # Save to cache
                self._save_to_cache(metadata, output_format, content)
                
                # Determine output path
                output_path = self._get_output_path(metadata, output_format)
                
                # Write output file
                output_path.parent.mkdir(parents=True, exist_ok=True)
                with open(output_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                
                generated_files.append(output_path)
                self.logger.info(f"Generated {output_path}")
            
            return generated_files, warnings
        
        except Exception as e:
            error_msg = f"Failed to process {path}: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            return [], [error_msg]
    
    def _generate_content(self, metadata: FileMetadata, output_format: OutputFormat) -> Optional[str]:
        """
        Generate content for a file in the specified format.
        
        Args:
            metadata: File metadata.
            output_format: Output format.
            
        Returns:
            Generated content, or None on failure.
        """
        try:
            # Read file content
            with open(metadata.path, 'r', encoding='utf-8') as f:
                raw_content = f.read()
            
            # Prepare template context
            context = {
                'file': metadata,
                'content': raw_content,
                'relative_path': metadata.path.relative_to(self.input_dir),
                'output_format': output_format.value,
                'config': self.config,
            }
            
            # Add file-type specific processing
            if metadata.file_type == FileType.MARKDOWN:
                context['html_content'] = self._markdown_filter(raw_content)
            
            elif metadata.file_type == FileType.PYTHON:
                # Syntax highlighting for Python code
                context['code_content'] = raw_content
                context['language'] = 'python'
            
            elif metadata.file_type in [FileType.YAML, FileType.JSON]:
                # Pretty-print structured data
                try:
                    if metadata.file_type == FileType.YAML:
                        parsed = yaml.safe_load(raw_content)
                    else:
                        parsed = json.loads(raw_content)
                    
                    context['parsed_content'] = parsed
                    context['formatted_content'] = json.dumps(parsed, indent=2)
                except Exception:
                    context['parsed_content'] = None
                    context['formatted_content'] = raw_content
            
            # Get template name
            template_configs = self.config.get("output_formats", [])
            template_name = "default.html.j2"  # Default template
            
            for config in template_configs:
                if config.get('format') == output_format.value:
                    template_name = config.get('template', template_name)
                    break
            
            # Render template
            template = self.template_env.get_template(template_name)
            return template.render(**context)
        
        except Exception as e:
            self.logger.error(f"Failed to generate content for {metadata.path}: {str(e)}")
            return None
    
    def _get_output_path(self, metadata: FileMetadata, output_format: OutputFormat) -> Path:
        """
        Determine output path for a file.
        
        Args:
            metadata: File metadata.
            output_format: Output format.
            
        Returns:
            Output path.
        """
        # Get relative path from input directory
        rel_path = metadata.path.relative_to(self.input_dir)
        
        # Determine extension from config
        extension = f".{output_format.value}"  # Default extension
        
        template_configs = self.config.get("output_formats", [])
        for config in template_configs:
            if config.get('format') == output_format.value:
                extension = config.get('extension', extension)
                break
        
        # Construct output path
        output_name = rel_path.with_suffix(extension)
        return self.output_dir / output_name
    
    def _generate_navigation(self, all_metadata: List[FileMetadata]) -> Dict[str, Any]:
        """
        Generate navigation structure.
        
        Args:
            all_metadata: List of file metadata.
            
        Returns:
            Navigation structure.
        """
        navigation = {
            'sections': [],
            'breadcrumbs': {},
            'tree': {},
        }
        
        # Group by directory
        dir_structure = {}
        for metadata in all_metadata:
            rel_path = metadata.path.relative_to(self.input_dir)
            dir_parts = list(rel_path.parent.parts)
            
            # Build directory tree
            current = dir_structure
            for part in dir_parts:
                if part not in current:
                    current[part] = {'files': [], 'subdirs': {}}
                current = current[part]['subdirs']
            
            # Add file to parent directory
            parent = dir_structure
            for part in dir_parts:
                parent = parent[part]['subdirs']
            
            # Actually add to correct parent (need to backtrack)
            # Simplified: just collect all files
            pass
        
        # TODO: Implement proper navigation generation
        # This is a simplified version
        
        return navigation
    
    def _generate_search_index(self, all_metadata: List[FileMetadata]) -> Dict[str, Any]:
        """
        Generate search index.
        
        Args:
            all_metadata: List of file metadata.
            
        Returns:
            Search index.
        """
        index = {
            'version': '1.0.0',
            'files': [],
            'index': {},
        }
        
        for metadata in all_metadata:
            file_entry = {
                'path': str(metadata.path.relative_to(self.input_dir)),
                'title': metadata.title,
                'description': metadata.description,
                'tags': metadata.tags,
                'type': metadata.file_type.value,
            }
            
            index['files'].append(file_entry)
            
            # Add to search index (simplified)
            # TODO: Implement full-text search indexing
        
        return index
    
    async def generate(self) -> GenerationResult:
        """
        Generate documentation from input files.
        
        Returns:
            GenerationResult object.
        """
        result = GenerationResult(success=True)
        self.stats['start_time'] = time.time()
        
        try:
            self.logger.info("Starting documentation generation")
            
            # Get output formats from config
            output_format_configs = self.config.get("output_formats", [])
            output_formats = []
            
            for config in output_format_configs:
                format_name = config.get('format')
                try:
                    output_formats.append(OutputFormat(format_name))
                except ValueError:
                    self.logger.warning(f"Unknown output format: {format_name}")
            
            if not output_formats:
                output_formats = [OutputFormat.HTML]
                self.logger.info("No output formats specified, defaulting to HTML")
            
            # Find all files to process
            all_files = []
            for root, dirs, files in os.walk(self.input_dir):
                # Skip excluded directories
                exclude_patterns = self.config.get("exclude_patterns", [])
                dirs[:] = [d for d in dirs if not any(Path(d).match(p) for p in exclude_patterns)]
                
                for file in files:
                    file_path = Path(root) / file
                    if self._should_process_file(file_path):
                        all_files.append(file_path)
            
            self.logger.info(f"Found {len(all_files)} files to process")
            
            # Process files
            generated_files = []
            all_warnings = []
            all_metadata = []
            
            # Use parallel processing if enabled
            parallel = self.config.get("parallel_processing", True)
            max_workers = self.config.get("max_workers", 4)
            
            if parallel and len(all_files) > 1:
                with ThreadPoolExecutor(max_workers=max_workers) as executor:
                    futures = {
                        executor.submit(self._process_file, file_path, output_formats): file_path
                        for file_path in all_files
                    }
                    
                    for future in as_completed(futures):
                        file_path = futures[future]
                        try:
                            files, warnings = future.result()
                            generated_files.extend(files)
                            all_warnings.extend(warnings)
                            
                            # Collect metadata for processed files
                            if files:
                                metadata = self._get_file_metadata(file_path)
                                all_metadata.append(metadata)
                        
                        except Exception as e:
                            error_msg = f"Failed to process {file_path}: {str(e)}"
                            result.add_failed(file_path, error_msg)
                            self.logger.error(error_msg)
            else:
                # Sequential processing
                for file_path in all_files:
                    files, warnings = self._process_file(file_path, output_formats)
                    generated_files.extend(files)
                    all_warnings.extend(warnings)
                    
                    # Collect metadata for processed files
                    if files:
                        metadata = self._get_file_metadata(file_path)
                        all_metadata.append(metadata)
            
            # Add generated files to result
            for file_path in generated_files:
                result.add_generated(file_path)
            
            # Add warnings to result
            for warning in all_warnings:
                result.add_warning(warning)
            
            # Generate navigation if enabled
            if self.config.get("generate_navigation", True) and all_metadata:
                navigation = self._generate_navigation(all_metadata)
                nav_file = self.output_dir / "navigation.json"
                
                with open(nav_file, 'w', encoding='utf-8') as f:
                    json.dump(navigation, f, indent=2)
                
                result.add_generated(nav_file)
                self.logger.info(f"Generated navigation: {nav_file}")
            
            # Generate search index if enabled
            if self.config.get("enable_search", True) and all_metadata:
                search_index = self._generate_search_index(all_metadata)
                index_file = self.output_dir / "search_index.json"
                
                with open(index_file, 'w', encoding='utf-8') as f:
                    json.dump(search_index, f, indent=2)
                
                result.add_generated(index_file)
                self.logger.info(f"Generated search index: {index_file}")
            
            # Copy static assets if template directory exists
            static_dir = self.template_dir / "static"
            if static_dir.exists() and static_dir.is_dir():
                output_static = self.output_dir / "static"
                shutil.copytree(static_dir, output_static, dirs_exist_ok=True)
                self.logger.info(f"Copied static assets to: {output_static}")
            
            # Update statistics
            self.stats['end_time'] = time.time()
            self.stats['duration'] = self.stats['end_time'] - self.stats['start_time']
            self.stats['files_processed'] = len(all_files)
            self.stats['files_generated'] = len(generated_files)
            self.stats['files_skipped'] = len(all_files) - len(generated_files)
            self.stats['files_failed'] = len(result.failed_files)
            
            result.stats = self.stats.copy()
            
            self.logger.info(f"Documentation generation completed in {self.stats['duration']:.2f} seconds")
            self.logger.info(f"Processed: {self.stats['files_processed']}, "
                           f"Generated: {self.stats['files_generated']}, "
                           f"Skipped: {self.stats['files_skipped']}, "
                           f"Failed: {self.stats['files_failed']}")
        
        except Exception as e:
            error_msg = f"Documentation generation failed: {str(e)}"
            result.add_error(error_msg)
            self.logger.error(error_msg, exc_info=True)
        
        return result
    
    async def watch(self) -> None:
        """
        Watch for file changes and regenerate documentation automatically.
        
        This method runs indefinitely until interrupted.
        """
        if not WATCHDOG_AVAILABLE:
            self.logger.error("Watchdog is required for file watching")
            return
        
        self.logger.info("Starting file watcher")
        
        class ChangeHandler(FileSystemEventHandler):
            def __init__(self, generator):
                self.generator = generator
                self.last_event_time = 0
                self.debounce_interval = generator.config.get("watch_interval", 2.0)
            
            def on_any_event(self, event: FileSystemEvent) -> None:
                # Debounce events
                current_time = time.time()
                if current_time - self.last_event_time < self.debounce_interval:
                    return
                
                self.last_event_time = current_time
                
                # Check if event is relevant
                if event.is_directory:
                    return
                
                path = Path(event.src_path)
                if not self.generator._should_process_file(path):
                    return
                
                # Regenerate documentation
                self.generator.logger.info(f"File changed: {path}")
                asyncio.create_task(self.generator.generate())
        
        # Create and start observer
        event_handler = ChangeHandler(self)
        observer = Observer()
        observer.schedule(event_handler, str(self.input_dir), recursive=True)
        observer.start()
        
        try:
            self.logger.info(f"Watching directory: {self.input_dir}")
            self.logger.info("Press Ctrl+C to stop")
            
            # Keep running
            while True:
                await asyncio.sleep(1)
        
        except KeyboardInterrupt:
            self.logger.info("Stopping file watcher")
        
        finally:
            observer.stop()
            observer.join()


async def generate_documentation(config_path: str) -> GenerationResult:
    """
    Convenience function to generate documentation from config file.
    
    Args:
        config_path: Path to configuration file.
        
    Returns:
        GenerationResult object.
    """
    # Load configuration
    config = {}
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            if config_path.endswith('.yaml') or config_path.endswith('.yml'):
                config = yaml.safe_load(f)
            elif config_path.endswith('.json'):
                config = json.load(f)
            else:
                # Try YAML first, then JSON
                try:
                    config = yaml.safe_load(f)
                except yaml.YAMLError:
                    f.seek(0)
                    config = json.load(f)
    except Exception as e:
        print(f"Failed to load configuration: {str(e)}")
        return GenerationResult(success=False, errors=[str(e)])
    
    # Create generator
    generator = DocumentationGenerator(config)
    
    # Generate documentation
    return await generator.generate()


async def main():
    """Command-line interface for documentation generation."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="OpenHub Documentation Generator"
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
    
    args = parser.parse_args()
    
    # Load configuration
    config_path = Path(args.config)
    if not config_path.exists():
        print(f"Configuration file not found: {config_path}")
        return 1
    
    config = {}
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            if config_path.suffix in ['.yaml', '.yml']:
                config = yaml.safe_load(f)
            elif config_path.suffix == '.json':
                config = json.load(f)
            else:
                print(f"Unsupported configuration format: {config_path.suffix}")
                return 1
    except Exception as e:
        print(f"Failed to load configuration: {str(e)}")
        return 1
    
    # Update config with command-line options
    if args.verbose:
        config['logging'] = config.get('logging', {})
        config['logging']['level'] = 'DEBUG'
    
    # Create generator
    generator = DocumentationGenerator(config)
    
    try:
        if args.watch:
            # Start file watcher
            await generator.watch()
        else:
            # Generate documentation once
            result = await generator.generate()
            
            # Print results
            if result.success:
                print(f"✓ Documentation generation completed successfully")
                print(f"  Generated: {len(result.generated_files)} files")
                print(f"  Duration: {result.stats.get('duration', 0):.2f} seconds")
                
                if result.warnings:
                    print(f"  Warnings: {len(result.warnings)}")
                    for warning in result.warnings[:5]:  # Show first 5 warnings
                        print(f"    - {warning}")
                    if len(result.warnings) > 5:
                        print(f"    ... and {len(result.warnings) - 5} more")
                
                return 0
            else:
                print(f"✗ Documentation generation failed")
                
                if result.errors:
                    for error in result.errors:
                        print(f"  - {error}")
                
                if result.failed_files:
                    print(f"  Failed files:")
                    for file_path, error in result.failed_files[:5]:  # Show first 5
                        print(f"    - {file_path}: {error}")
                    if len(result.failed_files) > 5:
                        print(f"    ... and {len(result.failed_files) - 5} more")
                
                return 1
    
    except KeyboardInterrupt:
        print("\nOperation cancelled by user")
        return 130
    
    except Exception as e:
        print(f"Unexpected error: {str(e)}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        return 1


if __name__ == "__main__":
    asyncio.run(main())