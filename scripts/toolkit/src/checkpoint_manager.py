#!/usr/bin/env python3
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS Checkpoint Manager
# Migrated from scripts/operations/checkpoint_manager.py

"""
AgentOS State Checkpoint Manager

Manages persistent state snapshots for AgentOS agents:
- Create, list, restore, and delete checkpoints
- Automatic checkpoint rotation
- JSON-based state serialization

Usage:
    from scripts.toolkit import CheckpointManager
    
    mgr = CheckpointManager()
    mgr.create_checkpoint("agent_1", {"state": "data"})
"""

import json
import os
import shutil
from dataclasses import dataclass, field, asdict
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional


@dataclass
class CheckpointMetadata:
    """Checkpoint metadata"""
    checkpoint_id: str
    agent_id: str
    created_at: str
    size_bytes: int = 0
    version: str = "1.0"
    tags: List[str] = field(default_factory=list)


@dataclass
class Checkpoint:
    """Complete checkpoint with metadata and state"""
    metadata: CheckpointMetadata
    state: Dict[str, Any]


class CheckpointManager:
    """
    Manages agent state checkpoints with automatic rotation.
    
    Features:
    - JSON-based persistence
    - Configurable max checkpoints per agent
    - Tag-based filtering
    """

    def __init__(
        self,
        storage_dir: Optional[str] = None,
        max_checkpoints_per_agent: int = 10,
        auto_compress: bool = False
    ):
        self.storage_dir = storage_dir or os.path.join(
            os.path.expanduser("~"), ".agentos", "checkpoints"
        )
        self.max_per_agent = max_checkpoints_per_agent
        self.auto_compress = auto_compress
        
        Path(self.storage_dir).mkdir(parents=True, exist_ok=True)

    def _agent_dir(self, agent_id: str) -> str:
        return os.path.join(self.storage_dir, agent_id)

    def create_checkpoint(
        self,
        agent_id: str,
        state: Dict[str, Any],
        tags: Optional[List[str]] = None,
        version: str = "1.0"
    ) -> Checkpoint:
        """Create a new checkpoint for an agent"""
        timestamp = datetime.now().isoformat()
        cp_id = f"cp_{timestamp.replace(':', '-').replace('.', '-')}"
        
        metadata = CheckpointMetadata(
            checkpoint_id=cp_id,
            agent_id=agent_id,
            created_at=timestamp,
            version=version,
            tags=tags or []
        )
        
        checkpoint = Checkpoint(metadata=metadata, state=state)
        
        agent_dir = self._agent_dir(agent_id)
        os.makedirs(agent_dir, exist_ok=True)
        
        filepath = os.path.join(agent_dir, f"{cp_id}.json")
        content = json.dumps(asdict(checkpoint), indent=2, ensure_ascii=False)
        
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(content)
        
        metadata.size_bytes = len(content.encode("utf-8"))
        
        existing = self.list_checkpoints(agent_id)
        if len(existing) > self.max_per_agent:
            self._rotate_checkpoints(agent_id, existing)
        
        return checkpoint

    def get_checkpoint(
        self,
        agent_id: str,
        checkpoint_id: str
    ) -> Optional[Checkpoint]:
        """Retrieve a specific checkpoint"""
        filepath = os.path.join(
            self._agent_dir(agent_id), f"{checkpoint_id}.json"
        )
        if not os.path.exists(filepath):
            return None
        
        with open(filepath, "r", encoding="utf-8") as f:
            data = json.load(f)
        
        return Checkpoint(
            metadata=CheckpointMetadata(**data["metadata"]),
            state=data["state"]
        )

    def list_checkpoints(
        self,
        agent_id: str,
        tag_filter: Optional[str] = None
    ) -> List[CheckpointMetadata]:
        """List all checkpoints for an agent"""
        agent_dir = self._agent_dir(agent_id)
        if not os.path.exists(agent_dir):
            return []
        
        checkpoints = []
        for fname in sorted(os.listdir(agent_dir)):
            if not fname.endswith(".json"):
                continue
            
            try:
                with open(os.path.join(agent_dir, fname), "r") as f:
                    data = json.load(f)
                meta = CheckpointMetadata(**data["metadata"])
                
                if tag_filter and tag_filter not in meta.tags:
                    continue
                
                checkpoints.append(meta)
            except (json.JSONDecodeError, KeyError):
                continue
        
        return sorted(checkpoints, key=lambda c: c.created_at, reverse=True)

    def restore_checkpoint(
        self,
        agent_id: str,
        checkpoint_id: str
    ) -> Optional[Dict[str, Any]]:
        """Restore state from a checkpoint"""
        cp = self.get_checkpoint(agent_id, checkpoint_id)
        if cp is None:
            return None
        return cp.state.copy()

    def delete_checkpoint(
        self,
        agent_id: str,
        checkpoint_id: str
    ) -> bool:
        """Delete a specific checkpoint"""
        filepath = os.path.join(
            self._agent_dir(agent_id), f"{checkpoint_id}.json"
        )
        if os.path.exists(filepath):
            os.remove(filepath)
            return True
        return False

    def delete_all_checkpoints(self, agent_id: str) -> int:
        """Delete all checkpoints for an agent"""
        agent_dir = self._agent_dir(agent_id)
        if not os.path.exists(agent_dir):
            return 0
        
        count = 0
        for fname in os.listdir(agent_dir):
            if fname.endswith(".json"):
                os.remove(os.path.join(agent_dir, fname))
                count += 1
        return count

    def _rotate_checkpoints(
        self,
        agent_id: str,
        existing: List[CheckpointMetadata]
    ) -> int:
        """Remove oldest checkpoints exceeding limit"""
        to_remove = len(existing) - self.max_per_agent
        removed = 0
        
        for meta in existing[-to_remove:]:
            if self.delete_checkpoint(agent_id, meta.checkpoint_id):
                removed += 1
        
        return removed

    def get_storage_stats(self) -> Dict[str, Any]:
        """Get storage usage statistics"""
        total_files = 0
        total_size = 0
        agent_counts: Dict[str, int] = {}
        
        if os.path.exists(self.storage_dir):
            for agent_id in os.listdir(self.storage_dir):
                agent_dir = os.path.join(self.storage_dir, agent_id)
                if not os.path.isdir(agent_dir):
                    continue
                
                count = 0
                for fname in os.listdir(agent_dir):
                    if fname.endswith(".json"):
                        fpath = os.path.join(agent_dir, fname)
                        total_size += os.path.getsize(fpath)
                        total_files += 1
                        count += 1
                agent_counts[agent_id] = count
        
        return {
            "total_checkpoints": total_files,
            "total_size_bytes": total_size,
            "agents": agent_counts,
            "storage_dir": self.storage_dir
        }
