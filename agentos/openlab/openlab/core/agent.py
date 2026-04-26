'''OpenLab Core Agent Module - AgentOS 架构设计原则 V1.8'''



from abc import ABC, abstractmethod

from dataclasses import dataclass, field

from enum import Enum

from typing import Any, Dict, List, Optional, Set

import asyncio

import time





class AgentStatus(Enum):

    """Agent 鐘舵佹灇与"""

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

    """Agent 鎵ц与婁笅鏂"""

    agent_id: str

    task_id: Optional[str] = None

    session_id: Optional[str] = None

    metadata: Dict[str, Any] = field(default_factory=dict)

    created_at: float = field(default_factory=time.time)

    timeout: Optional[float] = None





@dataclass

class TaskResult:

    """浠诲姟鎵ц缁撴灉"""

    success: bool

    output: Optional[Any] = None

    error: Optional[str] = None

    error_code: Optional[str] = None

    metrics: Dict[str, Any] = field(default_factory=dict)

    warnings: List[str] = field(default_factory=list)





class Message:

    """Agent 濞戝牊浼呴崺铏硅"""

    

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

        """鑾峰彇 Agent 褰撳墠鐘舵"""

        return self._status

    

    @status.setter

    def status(self, value: AgentStatus) -> None:

        """璁剧疆 Agent 鐘舵"""

        self._status = value

        self._last_activity = time.time()

    

    @property

    def context(self) -> Optional[AgentContext]:

        """鑾峰彇褰撳墠鎵ц与婁笅鏂"""

        return self._context

    

    @abstractmethod

    async def initialize(self) -> None:

        """

        閸掓繂顫愰崠?Agent

        

        韫囧懘銆忕圭偟骞囬惃鍕煙濞夋洩绱濋崷?Agent 閸氼垰濮╅弮鎯扮殶閻?        """

        pass

    

    @abstractmethod

    async def execute(self, input_data: Any, context: AgentContext) -> TaskResult:

        """

        閹笛嗩攽娴犺濮

        

        Args:

            input_data: 鏉堟挸鍙嗛弫鐗堝祦

            context: 閹笛嗩攽娑撳﹣绗呴弬?            

        Returns:

            TaskResult: 娴犺濮熼幍褑顢戠紒鎾寸亯

        """

        pass

    

    @abstractmethod

    async def shutdown(self) -> None:

        """

        閸忔娊妫 Agent

        

        濞撳懐鎮婄挧鍕爱閿涘奔绱梿鍛粹偓鈧崙?        """

        pass

    

    async def handle_message(self, message: Message) -> Optional[Message]:

        """

        婢跺嫮鎮婂☉鍫熶紖

        

        Args:

            message: 鏉堟挸鍙嗗☉鍫熶紖

            

        Returns:

            Optional[Message]: 閸濆秴绨插☉鍫熶紖

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

        濞夈劌鍞藉銉ュ徔

        

        Args:

            name: 瀹搞儱鍙块崥宥囆

            tool: 瀹搞儱鍙跨圭偘绶

        """

        self._tools[name] = tool

    

    def get_tool(self, name: str) -> Optional[Any]:

        """

        閼惧嘲褰囧銉ュ徔

        

        Args:

            name: 瀹搞儱鍙块崥宥囆

            

        Returns:

            Optional[Any]: 瀹搞儱鍙跨圭偘绶

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

