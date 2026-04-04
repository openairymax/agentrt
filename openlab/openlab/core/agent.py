"""
OpenLab Core Agent Module

Agent 管理与生命周期核心模块
遵循 AgentOS 架构设计原则 V1.8
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional, Set
import asyncio
import time


class AgentStatus(Enum):
    """Agent 状态枚举"""
    CREATED = "created"
    INITIALIZING = "initializing"
    READY = "ready"
    RUNNING = "running"
    PAUSED = "paused"
    SHUTTING_DOWN = "shutting_down"
    SHUTDOWN = "shutdown"
    ERROR = "error"


class AgentCapability(Enum):
    """Agent 能力枚举"""
    ARCHITECTURE_DESIGN = "architecture_design"
    CODE_GENERATION = "code_generation"
    TEST_GENERATION = "test_generation"
    DOCUMENTATION = "documentation"
    DEBUGGING = "debugging"
    OPTIMIZATION = "optimization"


@dataclass
class AgentContext:
    """Agent 执行上下文"""
    agent_id: str
    task_id: Optional[str] = None
    session_id: Optional[str] = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: float = field(default_factory=time.time)
    timeout: Optional[float] = None


@dataclass
class TaskResult:
    """任务执行结果"""
    success: bool
    output: Optional[Any] = None
    error: Optional[str] = None
    error_code: Optional[str] = None
    metrics: Dict[str, Any] = field(default_factory=dict)
    warnings: List[str] = field(default_factory=list)


class Message:
    """Agent 消息基类"""
    
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
    Agent 抽象基类
    
    遵循原则:
    - K-2 接口契约化：完整的 docstring 和类型注解
    - K-4 可插拔策略：支持运行时替换
    - E-3 资源确定性：明确的资源生命周期
    - E-6 错误可追溯：完整的错误链
    """
    
    def __init__(
        self,
        agent_id: str,
        capabilities: Optional[Set[AgentCapability]] = None,
        manager: Optional[Any] = None,
        workbench_id: Optional[str] = None
    ):
        """
        初始化 Agent
        
        Args:
            agent_id: Agent 唯一标识
            capabilities: Agent 能力集合
            manager: Agent 管理器引用
            workbench_id: 虚拟工位标识（cupolas 安全穹顶）
        """
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
        """获取 Agent 当前状态"""
        return self._status
    
    @status.setter
    def status(self, value: AgentStatus) -> None:
        """设置 Agent 状态"""
        self._status = value
        self._last_activity = time.time()
    
    @property
    def context(self) -> Optional[AgentContext]:
        """获取当前执行上下文"""
        return self._context
    
    @abstractmethod
    async def initialize(self) -> None:
        """
        初始化 Agent
        
        必须实现的方法，在 Agent 启动时调用
        """
        pass
    
    @abstractmethod
    async def execute(self, input_data: Any, context: AgentContext) -> TaskResult:
        """
        执行任务
        
        Args:
            input_data: 输入数据
            context: 执行上下文
            
        Returns:
            TaskResult: 任务执行结果
        """
        pass
    
    @abstractmethod
    async def shutdown(self) -> None:
        """
        关闭 Agent
        
        清理资源，优雅退出
        """
        pass
    
    async def handle_message(self, message: Message) -> Optional[Message]:
        """
        处理消息
        
        Args:
            message: 输入消息
            
        Returns:
            Optional[Message]: 响应消息
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
        注册工具
        
        Args:
            name: 工具名称
            tool: 工具实例
        """
        self._tools[name] = tool
    
    def get_tool(self, name: str) -> Optional[Any]:
        """
        获取工具
        
        Args:
            name: 工具名称
            
        Returns:
            Optional[Any]: 工具实例
        """
        return self._tools.get(name)
    
    def _update_activity(self) -> None:
        """更新最后活动时间"""
        self._last_activity = time.time()
    
    def __repr__(self) -> str:
        return f"{self.__class__.__name__}(id={self.agent_id}, status={self.status})"


class AgentRegistry:
    """
    Agent 注册表
    
    线程安全的 Agent 注册与管理
    """
    
    def __init__(self):
        self._agents: Dict[str, Agent] = {}
        self._lock = asyncio.Lock()
    
    async def register(self, agent: Agent) -> bool:
        """
        注册 Agent
        
        Args:
            agent: Agent 实例
            
        Returns:
            bool: 注册是否成功
        """
        async with self._lock:
            if agent.agent_id in self._agents:
                return False
            self._agents[agent.agent_id] = agent
            return True
    
    async def unregister(self, agent_id: str) -> bool:
        """
        注销 Agent
        
        Args:
            agent_id: Agent ID
            
        Returns:
            bool: 注销是否成功
        """
        async with self._lock:
            if agent_id not in self._agents:
                return False
            del self._agents[agent_id]
            return True
    
    async def get(self, agent_id: str) -> Optional[Agent]:
        """
        获取 Agent
        
        Args:
            agent_id: Agent ID
            
        Returns:
            Optional[Agent]: Agent 实例
        """
        async with self._lock:
            return self._agents.get(agent_id)
    
    async def list_agents(self) -> List[Agent]:
        """
        列出所有 Agent
        
        Returns:
            List[Agent]: Agent 列表
        """
        async with self._lock:
            return list(self._agents.values())
    
    async def count(self) -> int:
        """
        获取 Agent 数量
        
        Returns:
            int: Agent 数量
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
