#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Memory Manager
# Migrated from scripts/operations/memory_manager.py

"""
AgentOS Memory Manager

Manages the multi-layer memory system for AgentOS agents:
- L1 Raw Volume: Raw sensory data storage
- L2 Feature: Extracted features and embeddings
- L3 Structural: Knowledge graphs and relationships
- L4 Pattern: Learned patterns and abstractions

Usage:
    from scripts.toolkit import MemoryManager
    
    manager = MemoryManager()
    info = manager.get_memory_info()
"""

import json
import os
import logging
from dataclasses import dataclass, field, asdict
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional

logger = logging.getLogger(__name__)


class MemoryLayer(Enum):
    """Memory layer identifiers"""
    L1_RAW = "l1_raw"
    L2_FEATURE = "l2_feature"
    L3_STRUCTURAL = "l3_structural"
    L4_PATTERN = "l4_pattern"


@dataclass
class MemoryStats:
    """Memory statistics"""
    layer: str
    total_entries: int = 0
    total_size_bytes: int = 0
    utilization_pct: float = 0.0
    oldest_entry: Optional[str] = None
    newest_entry: Optional[str] = None


@dataclass
class MemoryConfig:
    """Memory configuration"""
    max_memory_mb: int = 512
    gc_threshold_mb: int = 256
    l1_max_size_mb: int = 64
    l2_max_size_mb: int = 128
    l3_max_size_mb: int = 256
    l4_max_size_mb: int = 64
    retention_seconds: Dict[str, int] = field(default_factory=lambda: {
        "l1_raw": 3600,
        "l2_feature": 7200,
        "l3_structural": 86400,
        "l4_pattern": 604800,
    })
    enabled_layers: List[str] = field(default_factory=lambda: [
        "l1_raw", "l2_feature", "l3_structural", "l4_pattern"
    ])


class MemoryManager:
    """
    AgentOS Multi-Layer Memory Manager
    
    Manages memory allocation, garbage collection, and statistics
    across four cognitive layers.
    """

    def __init__(self, config: Optional[MemoryConfig] = None):
        self.config = config or MemoryConfig()
        self._layer_stats: Dict[str, MemoryStats] = {}
        self._initialized = False
        
        for layer in MemoryLayer:
            self._layer_stats[layer.value] = MemoryStats(layer=layer.value)

    def initialize(self) -> bool:
        """Initialize memory system (mock implementation)"""
        self._initialized = True
        logger.info("Memory system initialized")
        return True

    def get_memory_info(self) -> Dict[str, Any]:
        """Get comprehensive memory information"""
        total_entries = sum(s.total_entries for s in self._layer_stats.values())
        total_size = sum(s.total_size_bytes for s in self._layer_stats.values())
        
        return {
            "status": "active" if self._initialized else "inactive",
            "config": asdict(self.config),
            "layers": {k: asdict(v) for k, v in self._layer_stats.items()},
            "summary": {
                "total_entries": total_entries,
                "total_size_bytes": total_size,
                "total_size_mb": round(total_size / (1024 * 1024), 2),
            }
        }

    def get_layer_stats(self, layer: MemoryLayer) -> MemoryStats:
        """Get statistics for a specific layer"""
        return self._layer_stats.get(layer.value, MemoryStats(layer=layer.value))

    def record_access(self, layer: MemoryLayer, entry_id: str,
                      size_bytes: int = 0) -> None:
        """Record a memory access event"""
        stats = self._layer_stats[layer.value]
        stats.total_entries += 1
        stats.total_size_bytes += size_bytes
        if not stats.newest_entry or entry_id > stats.newest_entry:
            stats.newest_entry = entry_id

    def cleanup(self) -> Dict[str, int]:
        """Run garbage collection across layers"""
        cleaned = {}
        for layer in MemoryLayer:
            stats = self._layer_stats[layer.value]
            before = stats.total_entries
            stats.total_entries = max(0, before // 10)
            cleaned[layer.value] = before - stats.total_entries
        logger.info(f"Memory cleanup completed: {cleaned}")
        return cleaned

    def reset(self) -> None:
        """Reset all memory statistics"""
        for layer in MemoryLayer:
            self._layer_stats[layer.value] = MemoryStats(layer=layer.value)
        logger.info("Memory statistics reset")

    def health_check(self) -> Dict[str, Any]:
        """Perform memory health check"""
        issues = []
        
        total_size = sum(s.total_size_bytes for s in self._layer_stats.values())
        total_mb = total_size / (1024 * 1024)
        
        if total_mb > self.config.max_memory_mb:
            issues.append({
                "severity": "critical",
                "message": f"Memory usage ({total_mb:.1f}MB) exceeds limit ({self.config.max_memory_mb}MB)"
            })
        
        if not self._initialized:
            issues.append({
                "severity": "warning",
                "message": "Memory system not initialized"
            })

        return {
            "healthy": len(issues) == 0,
            "issues": issues,
            "utilization_pct": (total_mb / self.config.max_memory_mb * 100) if self.config.max_memory_mb > 0 else 0
        }


def get_memory_manager(config: Optional[MemoryConfig] = None) -> MemoryManager:
    global _memory_manager
    if '_memory_manager' not in globals():
        _memory_manager = MemoryManager(config=config)
    return _memory_manager
