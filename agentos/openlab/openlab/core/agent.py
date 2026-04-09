"""
OpenLab Core Agent Module

Agent 绠＄悊涓庣敓鍛藉懆鏈熸牳蹇冩ā鍧?閬靛惊 AgentOS 鏋舵瀯璁捐鍘熷垯 V1.8
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional, Set
import asyncio
import time


class AgentStatus(Enum):
    """Agent 鐘舵€佹灇涓?""
    CREATED = "created"
    INITIALIZING = "initializing"
    READY = "ready"
    RUNNING = "running"
    PAUSED = "paused"
    SHUTTING_DOWN = "shutting_down"
    SHUTDOWN = "shutdown"
    ERROR = "error"


class AgentCapability(Enum):
    """Agent 鑳藉姏鏋氫妇"""
    ARCHITECTURE_DESIGN = "architecture_design"
    CODE_GENERATION = "code_generation"
    TEST_GENERATION = "test_generation"
    DOCUMENTATION = "documentation"
    DEBUGGING = "debugging"
    OPTIMIZATION = "optimization"


@dataclass
class AgentContext:
    """Agent 鎵ц涓婁笅鏂?""
    agent_id: str
    task_id: Optional[str] = None
    session_id: Optional[str] = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: float = field(default_factory=time.time)
    timeout: Optional[float] = None


@dataclass
class TaskResult:
    """浠诲姟鎵ц缁撴灉"""
    success: bool
    output: Optional[Any] = None
    error: Optional[str] = None
    error_code: Optional[str] = None
    metrics: Dict[str, Any] = field(default_factory=dict)
    warnings: List[str] = field(default_factory=list)


class Message:
    """Agent 娑堟伅鍩虹被"""
    
    def __init__(
        self,
        message_type: str,
        content: Any,
        sender: Optional[str] = None,
        receiver: Optional[str] = None,
        metadata: Optional[Dict[str, Any]] = None
    ):
        self.type = message_type
        self.content = content
        self.sender = sender
        self.receiver = receiver
        self.metadata = metadata or {}
        self.timestamp = time.time()
    
    def __repr__(self) -> str:
        return f"Message(type={self.type}, sender={self.sender}, receiver={self.receiver})"


class Agent(ABC):
    """
    Agent 鎶借薄鍩虹被
    
    閬靛惊鍘熷垯:
    - K-2 鎺ュ彛濂戠害鍖栵細瀹屾暣鐨?docstring 鍜岀被鍨嬫敞瑙?    - K-4 鍙彃鎷旂瓥鐣ワ細鏀寔杩愯鏃舵浛鎹?    - E-3 璧勬簮纭畾鎬э細鏄庣‘鐨勮祫婧愮敓鍛藉懆鏈?    - E-6 閿欒鍙拷婧細瀹屾暣鐨勯敊璇摼
    """
    
    def __init__(
        self,
        agent_id: str,
        capabilities: Optional[Set[AgentCapability]] = None,
        manager: Optional[Any] = None,
        workbench_id: Optional[str] = None
    ):
        """
        鍒濆鍖?Agent
        
        Args:
            agent_id: Agent 鍞竴鏍囪瘑
            capabilities: Agent 鑳藉姏闆嗗悎
            manager: Agent 绠＄悊鍣ㄥ紩鐢?            workbench_id: 铏氭嫙宸ヤ綅鏍囪瘑锛坈upolas 瀹夊叏绌归《锛?        """
        self.agent_id = agent_id
        self.capabilities = capabilities or set()
        self.manager = manager
        self.workbench_id = workbench_id
        self._status = AgentStatus.CREATED
        self._context: Optional[AgentContext] = None
        self._tools: Dict[str, Any] = {}
        self._tool_executor: Optional[Any] = None
        self._created_at = time.time()
        self._last_activity = time.time()
    
    @property
    def status(self) -> AgentStatus:
        """鑾峰彇 Agent 褰撳墠鐘舵€?""
        return self._status
    
    @status.setter
    def status(self, value: AgentStatus) -> None:
        """璁剧疆 Agent 鐘舵€?""
        self._status = value
        self._last_activity = time.time()
    
    @property
    def context(self) -> Optional[AgentContext]:
        """鑾峰彇褰撳墠鎵ц涓婁笅鏂?""
        return self._context
    
    @abstractmethod
    async def initialize(self) -> None:
        """
        鍒濆鍖?Agent
        
        蹇呴』瀹炵幇鐨勬柟娉曪紝鍦?Agent 鍚姩鏃惰皟鐢?        """
        pass
    
    @abstractmethod
    async def execute(self, input_data: Any, context: AgentContext) -> TaskResult:
        """
        鎵ц浠诲姟
        
        Args:
            input_data: 杈撳叆鏁版嵁
            context: 鎵ц涓婁笅鏂?            
        Returns:
            TaskResult: 浠诲姟鎵ц缁撴灉
        """
        pass
    
    @abstractmethod
    async def shutdown(self) -> None:
        """
        鍏抽棴 Agent
        
        娓呯悊璧勬簮锛屼紭闆呴€€鍑?        """
        pass
    
    async def handle_message(self, message: Message) -> Optional[Message]:
        """
        澶勭悊娑堟伅
        
        Args:
            message: 杈撳叆娑堟伅
            
        Returns:
            Optional[Message]: 鍝嶅簲娑堟伅
        """
        if message.type == "task":
            result = await self.execute(message.content, self._context)
            return Message(
                message_type="result",
                content=result,
                sender=self.agent_id,
                receiver=message.sender
            )
        return None
    
    def register_tool(self, name: str, tool: Any) -> None:
        """
        娉ㄥ唽宸ュ叿
        
        Args:
            name: 宸ュ叿鍚嶇О
            tool: 宸ュ叿瀹炰緥
        """
        self._tools[name] = tool
    
    def get_tool(self, name: str) -> Optional[Any]:
        """
        鑾峰彇宸ュ叿
        
        Args:
            name: 宸ュ叿鍚嶇О
            
        Returns:
            Optional[Any]: 宸ュ叿瀹炰緥
        """
        return self._tools.get(name)
    
    def _update_activity(self) -> None:
        """鏇存柊鏈€鍚庢椿鍔ㄦ椂闂?""
        self._last_activity = time.time()
    
    def __repr__(self) -> str:
        return f"{self.__class__.__name__}(id={self.agent_id}, status={self.status})"


class AgentRegistry:
    """
    Agent 娉ㄥ唽琛?    
    绾跨▼瀹夊叏鐨?Agent 娉ㄥ唽涓庣鐞?    """
    
    def __init__(self):
        self._agents: Dict[str, Agent] = {}
        self._lock = asyncio.Lock()
    
    async def register(self, agent: Agent) -> bool:
        """
        娉ㄥ唽 Agent
        
        Args:
            agent: Agent 瀹炰緥
            
        Returns:
            bool: 娉ㄥ唽鏄惁鎴愬姛
        """
        async with self._lock:
            if agent.agent_id in self._agents:
                return False
            self._agents[agent.agent_id] = agent
            return True
    
    async def unregister(self, agent_id: str) -> bool:
        """
        娉ㄩ攢 Agent
        
        Args:
            agent_id: Agent ID
            
        Returns:
            bool: 娉ㄩ攢鏄惁鎴愬姛
        """
        async with self._lock:
            if agent_id not in self._agents:
                return False
            del self._agents[agent_id]
            return True
    
    async def get(self, agent_id: str) -> Optional[Agent]:
        """
        鑾峰彇 Agent
        
        Args:
            agent_id: Agent ID
            
        Returns:
            Optional[Agent]: Agent 瀹炰緥
        """
        async with self._lock:
            return self._agents.get(agent_id)
    
    async def list_agents(self) -> List[Agent]:
        """
        鍒楀嚭鎵€鏈?Agent
        
        Returns:
            List[Agent]: Agent 鍒楄〃
        """
        async with self._lock:
            return list(self._agents.values())
    
    async def count(self) -> int:
        """
        鑾峰彇 Agent 鏁伴噺
        
        Returns:
            int: Agent 鏁伴噺
        """
        async with self._lock:
            return len(self._agents)


__all__ = [
    "Agent",
    "AgentStatus",
    "AgentCapability",
    "AgentContext",
    "AgentRegistry",
    "TaskResult",
    "Message",
]
