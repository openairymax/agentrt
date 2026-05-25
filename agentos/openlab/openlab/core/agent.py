'''OpenLab Core Agent Module - AgentOS 架构设计原则 V1.8'''



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

    """Agent 消息传递对象"""

    

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

    '''Agent 基类接口'''

    

    def __init__(

        self,

        agent_id: str,

        capabilities: Optional[Set[AgentCapability]] = None,

        manager: Optional[Any] = None,

        workbench_id: Optional[str] = None

    ):

        '''初始化 Agent 实例'''

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

        

        子类应重写此方法以实现 Agent 的初始化逻辑        """

        pass

    

    @abstractmethod

    async def execute(self, input_data: Any, context: AgentContext) -> TaskResult:

        """

        执行任务

        

        Args:

            input_data: 输入数据

            context: 执行上下文            

        Returns:

            TaskResult: 任务执行结果对象

        """

        pass

    

    @abstractmethod

    async def shutdown(self) -> None:

        """

        关闭 Agent

        

        清理资源并优雅关闭 Agent 实例        """

        pass

    

    async def handle_message(self, message: Message) -> Optional[Message]:

        """

        处理消息

        

        Args:

            message: 输入消息

            

        Returns:

            Optional[Message]: 响应消息，可为空

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

            tool: 工具函数或对象

        """

        self._tools[name] = tool

    

    def get_tool(self, name: str) -> Optional[Any]:

        """

        获取已注册的工具

        

        Args:

            name: 工具名称

            

        Returns:

            Optional[Any]: 工具函数或对象

        """

        return self._tools.get(name)

    

    def _update_activity(self) -> None:

        '''更新活动时间'''

        self._last_activity = time.time()

    

    def __repr__(self) -> str:

        return f"{self.__class__.__name__}(id={self.agent_id}, status={self.status})"





class AgentRegistry:

    '''Agent 注册表,线程安全的 Agent 注册与管理'''

    

    def __init__(self):

        self._agents: Dict[str, Agent] = {}

        self._lock = asyncio.Lock()

    

    async def register(self, agent: Agent) -> bool:

        '''注册 Agent'''

        async with self._lock:

            if agent.agent_id in self._agents:

                return False

            self._agents[agent.agent_id] = agent

            return True

    

    async def unregister(self, agent_id: str) -> bool:

        '''注销 Agent'''

        async with self._lock:

            if agent_id not in self._agents:

                return False

            del self._agents[agent_id]

            return True

    

    async def get(self, agent_id: str) -> Optional[Agent]:

        '''根据 ID 获取 Agent'''

        async with self._lock:

            return self._agents.get(agent_id)

    

    async def list_agents(self) -> List[Agent]:

        '''列出所有 Agent'''

        async with self._lock:

            return list(self._agents.values())

    

    async def count(self) -> int:

        '''获取 Agent 数量'''

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

