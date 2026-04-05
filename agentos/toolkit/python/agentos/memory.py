# AgentOS Python SDK Memory
# Version: 3.0.0
# Last updated: 2026-04-04

"""
Memory class implementation for the AgentOS Python SDK.
"""

from typing import Optional, Dict, Any

class Memory:
    """Memory class for managing AgentOS memories.
    
    This class represents a memory stored in the AgentOS system.
    """
    
    def __init__(self, memory_id: str, content: str, created_at: str, metadata: Optional[Dict[str, Any]] = None):
        """
        Initialize a Memory object.
        
        Args:
            memory_id: The memory ID.
            content: The memory content.
            created_at: The creation timestamp.
            metadata: Optional metadata.
        """
        self.memory_id = memory_id
        self.content = content
        self.created_at = created_at
        self.metadata = metadata or {}
    
    def to_dict(self) -> Dict[str, Any]:
        """
        Convert the Memory object to a dictionary.
        
        Returns:
            A dictionary representation of the Memory object.
        """
        return {
            "memory_id": self.memory_id,
            "content": self.content,
            "created_at": self.created_at,
            "metadata": self.metadata
        }
    
    def __repr__(self) -> str:
        """
        Return a string representation of the Memory object.
        
        Returns:
            A string representation of the Memory object.
        """
        return f"Memory(id={self.memory_id!r}, content={self.content[:50]!r}..., created_at={self.created_at!r})"
