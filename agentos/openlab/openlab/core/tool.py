'''OpenLab Core Tool Module - AgentOS 架构设计原则 V1.8'''

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional, Set, Callable, Awaitable
import asyncio
import time


class ToolCategory(Enum):
    '''工具类别枚举'''
    INPUT_OUTPUT = "input_output"      # 输入输出
    COMPUTATION = "computation"         # 计算
    COMMUNICATION = "communication"     # 通信
    DATA_ACCESS = "data_access"         # 数据访问
    SYSTEM = "system"                   # 系统
    CUSTOM = "custom"                   # 自定义

class ToolCapability(Enum):
    '''工具能力枚举'''
    READ = "read"
    WRITE = "write"
    EXECUTE = "execute"
    QUERY = "query"
    TRANSFORM = "transform"
    ANALYZE = "analyze"


@dataclass
class ToolContext:
    """
    宸ュ叿鎵ц涓婁笅鏂?    
    閬靛惊鍘熷垯:
    - E-3 璧勬簮纭畾鎬э細鏄庣‘鐨勮祫婧愮鐞?    - A-1 绠€绾﹁嚦涓婏細鏈€灏忓繀瑕佷俊鎭?    """
    tool_id: str
    agent_id: Optional[str] = None
    task_id: Optional[str] = None
    session_id: Optional[str] = None
    timeout: Optional[float] = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: float = field(default_factory=time.time)


@dataclass
class ToolResult:
    """
    宸ュ叿鎵ц缁撴灉
    
    閬靛惊鍘熷垯:
    - E-6 閿欒鍙拷婧細瀹屾暣鐨勯敊璇俊鎭?    - A-2 鏋佽嚧缁嗚妭锛氳缁嗙殑鎵ц鎸囨爣
    """
    success: bool
    output: Optional[Any] = None
    error: Optional[str] = None
    error_code: Optional[str] = None
    execution_time: float = 0.0
    metrics: Dict[str, Any] = field(default_factory=dict)
    warnings: List[str] = field(default_factory=list)


class Tool(ABC):
    """
    宸ュ叿鎶借薄鍩虹被
    
    閬靛惊鍘熷垯:
    - K-2 鎺ュ彛濂戠害鍖栵細瀹屾暣鐨?docstring 鍜岀被鍨嬫敞瑙?    - K-4 鍙彃鎷旂瓥鐣ワ細鏀寔杩愯鏃舵浛鎹?    - E-1 瀹夊叏鍐呯敓锛氬唴缃畨鍏ㄦ鏌?    """
    
    # 绫荤骇鍒父閲?    NAME: str = ""
    DESCRIPTION: str = ""
    CATEGORY: ToolCategory = ToolCategory.CUSTOM
    CAPABILITIES: Set[ToolCapability] = set()
    INPUT_SCHEMA: Optional[Dict[str, Any]] = None
    OUTPUT_SCHEMA: Optional[Dict[str, Any]] = None
    VERSION: str = "1.0.0"
    
    def __init__(self, tool_id: Optional[str] = None):
        """
        鍒濆鍖栧伐鍏?        
        Args:
            tool_id: 宸ュ叿鍞竴鏍囪瘑
        """
        self.tool_id = tool_id or f"{self.NAME}_{id(self)}"
        self._enabled = True
        self._last_used: Optional[float] = None
        self._usage_count: int = 0
    
    @property
    def enabled(self) -> bool:
        """宸ュ叿鏄惁鍚敤"""
        return self._enabled
    
    @enabled.setter
    def enabled(self, value: bool) -> None:
        '''设置工具启用状态'''
        self._enabled = value
    
    @property
    def last_used(self) -> Optional[float]:
        '''最后使用时间'''
        return self._last_used
    
    @property
    def usage_count(self) -> int:
        '''使用次数'''
        return self._usage_count
    
    @abstractmethod
    async def _do_execute(
        self,
        parameters: Dict[str, Any],
        context: ToolContext
    ) -> ToolResult:
        """
        鎵ц宸ュ叿
        
        瀛愮被蹇呴』瀹炵幇鐨勬牳蹇冩柟娉?        
        Args:
            parameters: 鎵ц鍙傛暟
            context: 鎵ц涓婁笅鏂?            
        Returns:
            ToolResult: 鎵ц缁撴灉
        """
        pass
    
    async def execute(
        self,
        parameters: Dict[str, Any],
        context: Optional[ToolContext] = None
    ) -> ToolResult:
        """
        鎵ц宸ュ叿锛堝叕鍏辨帴鍙ｏ級
        
        鍖呭惈鍓嶇疆妫€鏌ャ€佹墽琛屻€佸悗缃鐞嗙殑瀹屾暣娴佺▼
        
        Args:
            parameters: 鎵ц鍙傛暟
            context: 鎵ц涓婁笅鏂?            
        Returns:
            ToolResult: 鎵ц缁撴灉
        """
        # 检查工具是否启用
        if not self._enabled:
            return ToolResult(
                success=False,
                error="Tool is disabled",
                error_code="TOOL_DISABLED"
            )
        
        # 创建上下文
        if context is None:
            context = ToolContext(tool_id=self.tool_id)
        
        # 参数验证
        if self.INPUT_SCHEMA:
            validation_result = self._validate_input(parameters)
            if not validation_result[0]:
                return ToolResult(
                    success=False,
                    error=validation_result[1],
                    error_code="INVALID_INPUT"
                )
        
        # 鎵ц鍓嶆鏌ワ紙瀹夊叏妫€鏌ワ級
        security_result = await self._pre_execute_check(parameters, context)
        if not security_result[0]:
            return ToolResult(
                success=False,
                error=security_result[1],
                error_code="SECURITY_CHECK_FAILED"
            )
        
        # 鎵ц
        start_time = time.time()
        try:
            result = await self._do_execute(parameters, context)
            result.execution_time = time.time() - start_time
            
            # 鏇存柊缁熻
            self._last_used = time.time()
            self._usage_count += 1
            
            # 杈撳嚭楠岃瘉
            if result.success and self.OUTPUT_SCHEMA:
                validation_result = self._validate_output(result.output)
                if not validation_result[0]:
                    result.warnings.append(
                        f"Output validation failed: {validation_result[1]}"
                    )
            
            return result
            
        except Exception as e:
            execution_time = time.time() - start_time
            return ToolResult(
                success=False,
                error=str(e),
                error_code="EXECUTION_ERROR",
                execution_time=execution_time
            )
    
    def _validate_input(
        self,
        parameters: Dict[str, Any]
    ) -> tuple[bool, Optional[str]]:
        """
        楠岃瘉杈撳叆鍙傛暟
        
        Args:
            parameters: 杈撳叆鍙傛暟
            
        Returns:
            tuple[bool, Optional[str]]: (鏄惁鏈夋晥锛岄敊璇秷鎭?
        """
        if not self.INPUT_SCHEMA:
            return True, None
        
        # 绠€鍖栫殑 JSON Schema 楠岃瘉
        required = self.INPUT_SCHEMA.get("required", [])
        properties = self.INPUT_SCHEMA.get("properties", {})
        
        for key in required:
            if key not in parameters:
                return False, f"Missing required parameter: {key}"
        
        for key, value in parameters.items():
            if key in properties:
                prop_schema = properties[key]
                expected_type = prop_schema.get("type")
                
                if expected_type == "string" and not isinstance(value, str):
                    return False, f"Parameter {key} must be a string"
                elif expected_type == "number" and not isinstance(value, (int, float)):
                    return False, f"Parameter {key} must be a number"
                elif expected_type == "boolean" and not isinstance(value, bool):
                    return False, f"Parameter {key} must be a boolean"
                elif expected_type == "array" and not isinstance(value, list):
                    return False, f"Parameter {key} must be an array"
                elif expected_type == "object" and not isinstance(value, dict):
                    return False, f"Parameter {key} must be an object"
        
        return True, None
    
    def _validate_output(self, output: Any) -> tuple[bool, Optional[str]]:
        """
        楠岃瘉杈撳嚭
        
        Args:
            output: 杈撳嚭鏁版嵁
            
        Returns:
            tuple[bool, Optional[str]]: (鏄惁鏈夋晥锛岄敊璇秷鎭?
        """
        if not self.OUTPUT_SCHEMA:
            return True, None
        
        # 绠€鍖栫殑楠岃瘉閫昏緫
        expected_type = self.OUTPUT_SCHEMA.get("type")
        
        if expected_type == "string" and not isinstance(output, str):
            return False, "Output must be a string"
        elif expected_type == "number" and not isinstance(output, (int, float)):
            return False, "Output must be a number"
        elif expected_type == "array" and not isinstance(output, list):
            return False, "Output must be an array"
        elif expected_type == "object" and not isinstance(output, dict):
            return False, "Output must be an object"
        
        return True, None
    
    async def _pre_execute_check(
        self,
        parameters: Dict[str, Any],
        context: ToolContext
    ) -> tuple[bool, Optional[str]]:
        """
        鎵ц鍓嶆鏌ワ紙瀹夊叏妫€鏌ワ級
        
        瀛愮被鍙互閲嶅啓姝ゆ柟娉曟坊鍔犺嚜瀹氫箟瀹夊叏妫€鏌?        
        Args:
            parameters: 鎵ц鍙傛暟
            context: 鎵ц涓婁笅鏂?            
        Returns:
            tuple[bool, Optional[str]]: (鏄惁閫氳繃妫€鏌ワ紝閿欒娑堟伅)
        """
        # 榛樿瀹炵幇锛氭€绘槸閫氳繃
        return True, None
    
    def get_info(self) -> Dict[str, Any]:
        """
        鑾峰彇宸ュ叿淇℃伅
        
        Returns:
            Dict[str, Any]: 宸ュ叿淇℃伅
        """
        return {
            "tool_id": self.tool_id,
            "name": self.NAME,
            "description": self.DESCRIPTION,
            "category": self.CATEGORY.value,
            "capabilities": [c.value for c in self.CAPABILITIES],
            "version": self.VERSION,
            "enabled": self._enabled,
            "usage_count": self._usage_count,
            "last_used": self._last_used,
        }
    
    def __repr__(self) -> str:
        return f"{self.__class__.__name__}(name={self.NAME}, id={self.tool_id})"


class ToolRegistry:
    """
    宸ュ叿娉ㄥ唽琛?    
    閬靛惊鍘熷垯:
    - K-4 鍙彃鎷旂瓥鐣ワ細鏀寔鍔ㄦ€佹敞鍐?娉ㄩ攢
    - E-3 璧勬簮纭畾鎬э細鏄庣‘鐨勮祫婧愮鐞?    """
    
    def __init__(self):
        self._tools: Dict[str, Tool] = {}
        self._lock = asyncio.Lock()
    
    async def register(self, tool: Tool) -> bool:
        """
        娉ㄥ唽宸ュ叿
        
        Args:
            tool: 宸ュ叿瀹炰緥
            
        Returns:
            bool: 娉ㄥ唽鏄惁鎴愬姛
        """
        async with self._lock:
            if tool.tool_id in self._tools:
                return False
            self._tools[tool.tool_id] = tool
            return True
    
    async def unregister(self, tool_id: str) -> bool:
        """
        娉ㄩ攢宸ュ叿
        
        Args:
            tool_id: 宸ュ叿 ID
            
        Returns:
            bool: 娉ㄩ攢鏄惁鎴愬姛
        """
        async with self._lock:
            if tool_id not in self._tools:
                return False
            del self._tools[tool_id]
            return True
    
    async def get(self, tool_id: str) -> Optional[Tool]:
        """
        鑾峰彇宸ュ叿
        
        Args:
            tool_id: 宸ュ叿 ID
            
        Returns:
            Optional[Tool]: 宸ュ叿瀹炰緥
        """
        async with self._lock:
            return self._tools.get(tool_id)
    
    async def list_tools(self) -> List[Tool]:
        """
        鍒楀嚭鎵€鏈夊伐鍏?        
        Returns:
            List[Tool]: 宸ュ叿鍒楄〃
        """
        async with self._lock:
            return list(self._tools.values())
    
    async def find_by_category(self, category: ToolCategory) -> List[Tool]:
        """
        鎸夌被鍒煡鎵惧伐鍏?        
        Args:
            category: 宸ュ叿绫诲埆
            
        Returns:
            List[Tool]: 宸ュ叿鍒楄〃
        """
        async with self._lock:
            return [
                tool for tool in self._tools.values()
                if tool.CATEGORY == category
            ]
    
    async def find_by_capability(
        self,
        capability: ToolCapability
    ) -> List[Tool]:
        """
        鎸夎兘鍔涙煡鎵惧伐鍏?        
        Args:
            capability: 宸ュ叿鑳藉姏
            
        Returns:
            List[Tool]: 宸ュ叿鍒楄〃
        """
        async with self._lock:
            return [
                tool for tool in self._tools.values()
                if capability in tool.CAPABILITIES
            ]


class ToolExecutor:
    """
    宸ュ叿鎵ц鍣?    
    璐熻矗宸ュ叿鐨勮皟搴﹀拰鎵ц绠＄悊
    
    閬靛惊鍘熷垯:
    - E-3 璧勬簮纭畾鎬э細骞跺彂鎺у埗鍜岃秴鏃剁鐞?    - S-1 鍙嶉闂幆锛氬畬鏁寸殑鎵ц鍙嶉
    """
    
    def __init__(
        self,
        max_concurrent: int = 50,
        default_timeout: float = 60.0
    ):
        """
        鍒濆鍖栧伐鍏锋墽琛屽櫒
        
        Args:
            max_concurrent: 鏈€澶у苟鍙戞墽琛屾暟
            default_timeout: 榛樿瓒呮椂鏃堕棿锛堢锛?        """
        self.max_concurrent = max_concurrent
        self.default_timeout = default_timeout
        self._semaphore = asyncio.Semaphore(max_concurrent)
        self._registry = ToolRegistry()
        self._execution_history: List[Dict[str, Any]] = []
        self._shutdown = False
    
    async def register_tool(self, tool: Tool) -> bool:
        """
        娉ㄥ唽宸ュ叿
        
        Args:
            tool: 宸ュ叿瀹炰緥
            
        Returns:
            bool: 娉ㄥ唽鏄惁鎴愬姛
        """
        return await self._registry.register(tool)
    
    async def execute(
        self,
        tool_id: str,
        parameters: Dict[str, Any],
        context: Optional[ToolContext] = None
    ) -> ToolResult:
        """
        鎵ц宸ュ叿
        
        Args:
            tool_id: 宸ュ叿 ID
            parameters: 鎵ц鍙傛暟
            context: 鎵ц涓婁笅鏂?            
        Returns:
            ToolResult: 鎵ц缁撴灉
        """
        if self._shutdown:
            return ToolResult(
                success=False,
                error="Executor is shutting down",
                error_code="EXECUTOR_SHUTDOWN"
            )
        
        async with self._semaphore:
            tool = await self._registry.get(tool_id)
            if not tool:
                return ToolResult(
                    success=False,
                    error=f"Tool not found: {tool_id}",
                    error_code="TOOL_NOT_FOUND"
                )
            
            # 璁剧疆瓒呮椂
            if context is None:
                context = ToolContext(tool_id=tool_id)
            if context.timeout is None:
                context.timeout = self.default_timeout
            
            # 鎵ц
            try:
                result = await asyncio.wait_for(
                    tool.execute(parameters, context),
                    timeout=context.timeout
                )
                
                # 璁板綍鎵ц鍘嗗彶
                self._execution_history.append({
                    "tool_id": tool_id,
                    "parameters": parameters,
                    "result": result,
                    "timestamp": time.time(),
                })
                
                return result
                
            except asyncio.TimeoutError:
                return ToolResult(
                    success=False,
                    error="Tool execution timeout",
                    error_code="TIMEOUT"
                )
    
    async def shutdown(self, wait: bool = True) -> None:
        """
        鍏抽棴鎵ц鍣?        
        Args:
            wait: 鏄惁绛夊緟杩愯涓殑浠诲姟瀹屾垚
        """
        self._shutdown = True
        
        if wait:
            # 绛夊緟鎵€鏈夊苟鍙戜换鍔″畬鎴?            await self._semaphore.acquire()
            self._semaphore.release()
    
    def get_stats(self) -> Dict[str, Any]:
        """
        鑾峰彇鎵ц鍣ㄧ粺璁′俊鎭?        
        Returns:
            Dict[str, Any]: 缁熻淇℃伅
        """
        return {
            "max_concurrent": self.max_concurrent,
            "default_timeout": self.default_timeout,
            "registered_tools": len(self._registry._tools),
            "execution_history_size": len(self._execution_history),
            "shutdown": self._shutdown,
        }


__all__ = [
    "Tool",
    "ToolCategory",
    "ToolCapability",
    "ToolContext",
    "ToolResult",
    "ToolRegistry",
    "ToolExecutor",
]
