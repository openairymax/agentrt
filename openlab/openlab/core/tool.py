"""
OpenLab Core Tool Module

工具抽象与执行核心模块
遵循 AgentOS 架构设计原则 V1.8
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Dict, List, Optional, Set, Callable, Awaitable
import asyncio
import time


class ToolCategory(Enum):
    """工具类别枚举"""
    INPUT_OUTPUT = "input_output"      # 输入输出
    COMPUTATION = "computation"         # 计算
    COMMUNICATION = "communication"     # 通信
    DATA_ACCESS = "data_access"         # 数据访问
    SYSTEM = "system"                   # 系统
    CUSTOM = "custom"                   # 自定义


class ToolCapability(Enum):
    """工具能力枚举"""
    READ = "read"
    WRITE = "write"
    EXECUTE = "execute"
    QUERY = "query"
    TRANSFORM = "transform"
    ANALYZE = "analyze"


@dataclass
class ToolContext:
    """
    工具执行上下文
    
    遵循原则:
    - E-3 资源确定性：明确的资源管理
    - A-1 简约至上：最小必要信息
    """
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
    工具执行结果
    
    遵循原则:
    - E-6 错误可追溯：完整的错误信息
    - A-2 极致细节：详细的执行指标
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
    工具抽象基类
    
    遵循原则:
    - K-2 接口契约化：完整的 docstring 和类型注解
    - K-4 可插拔策略：支持运行时替换
    - E-1 安全内生：内置安全检查
    """
    
    # 类级别常量
    NAME: str = ""
    DESCRIPTION: str = ""
    CATEGORY: ToolCategory = ToolCategory.CUSTOM
    CAPABILITIES: Set[ToolCapability] = set()
    INPUT_SCHEMA: Optional[Dict[str, Any]] = None
    OUTPUT_SCHEMA: Optional[Dict[str, Any]] = None
    VERSION: str = "1.0.0"
    
    def __init__(self, tool_id: Optional[str] = None):
        """
        初始化工具
        
        Args:
            tool_id: 工具唯一标识
        """
        self.tool_id = tool_id or f"{self.NAME}_{id(self)}"
        self._enabled = True
        self._last_used: Optional[float] = None
        self._usage_count: int = 0
    
    @property
    def enabled(self) -> bool:
        """工具是否启用"""
        return self._enabled
    
    @enabled.setter
    def enabled(self, value: bool) -> None:
        """设置工具启用状态"""
        self._enabled = value
    
    @property
    def last_used(self) -> Optional[float]:
        """最后使用时间"""
        return self._last_used
    
    @property
    def usage_count(self) -> int:
        """使用次数"""
        return self._usage_count
    
    @abstractmethod
    async def _do_execute(
        self,
        parameters: Dict[str, Any],
        context: ToolContext
    ) -> ToolResult:
        """
        执行工具
        
        子类必须实现的核心方法
        
        Args:
            parameters: 执行参数
            context: 执行上下文
            
        Returns:
            ToolResult: 执行结果
        """
        pass
    
    async def execute(
        self,
        parameters: Dict[str, Any],
        context: Optional[ToolContext] = None
    ) -> ToolResult:
        """
        执行工具（公共接口）
        
        包含前置检查、执行、后置处理的完整流程
        
        Args:
            parameters: 执行参数
            context: 执行上下文
            
        Returns:
            ToolResult: 执行结果
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
        
        # 执行前检查（安全检查）
        security_result = await self._pre_execute_check(parameters, context)
        if not security_result[0]:
            return ToolResult(
                success=False,
                error=security_result[1],
                error_code="SECURITY_CHECK_FAILED"
            )
        
        # 执行
        start_time = time.time()
        try:
            result = await self._do_execute(parameters, context)
            result.execution_time = time.time() - start_time
            
            # 更新统计
            self._last_used = time.time()
            self._usage_count += 1
            
            # 输出验证
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
        验证输入参数
        
        Args:
            parameters: 输入参数
            
        Returns:
            tuple[bool, Optional[str]]: (是否有效，错误消息)
        """
        if not self.INPUT_SCHEMA:
            return True, None
        
        # 简化的 JSON Schema 验证
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
        验证输出
        
        Args:
            output: 输出数据
            
        Returns:
            tuple[bool, Optional[str]]: (是否有效，错误消息)
        """
        if not self.OUTPUT_SCHEMA:
            return True, None
        
        # 简化的验证逻辑
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
        执行前检查（安全检查）
        
        子类可以重写此方法添加自定义安全检查
        
        Args:
            parameters: 执行参数
            context: 执行上下文
            
        Returns:
            tuple[bool, Optional[str]]: (是否通过检查，错误消息)
        """
        # 默认实现：总是通过
        return True, None
    
    def get_info(self) -> Dict[str, Any]:
        """
        获取工具信息
        
        Returns:
            Dict[str, Any]: 工具信息
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
    工具注册表
    
    遵循原则:
    - K-4 可插拔策略：支持动态注册/注销
    - E-3 资源确定性：明确的资源管理
    """
    
    def __init__(self):
        self._tools: Dict[str, Tool] = {}
        self._lock = asyncio.Lock()
    
    async def register(self, tool: Tool) -> bool:
        """
        注册工具
        
        Args:
            tool: 工具实例
            
        Returns:
            bool: 注册是否成功
        """
        async with self._lock:
            if tool.tool_id in self._tools:
                return False
            self._tools[tool.tool_id] = tool
            return True
    
    async def unregister(self, tool_id: str) -> bool:
        """
        注销工具
        
        Args:
            tool_id: 工具 ID
            
        Returns:
            bool: 注销是否成功
        """
        async with self._lock:
            if tool_id not in self._tools:
                return False
            del self._tools[tool_id]
            return True
    
    async def get(self, tool_id: str) -> Optional[Tool]:
        """
        获取工具
        
        Args:
            tool_id: 工具 ID
            
        Returns:
            Optional[Tool]: 工具实例
        """
        async with self._lock:
            return self._tools.get(tool_id)
    
    async def list_tools(self) -> List[Tool]:
        """
        列出所有工具
        
        Returns:
            List[Tool]: 工具列表
        """
        async with self._lock:
            return list(self._tools.values())
    
    async def find_by_category(self, category: ToolCategory) -> List[Tool]:
        """
        按类别查找工具
        
        Args:
            category: 工具类别
            
        Returns:
            List[Tool]: 工具列表
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
        按能力查找工具
        
        Args:
            capability: 工具能力
            
        Returns:
            List[Tool]: 工具列表
        """
        async with self._lock:
            return [
                tool for tool in self._tools.values()
                if capability in tool.CAPABILITIES
            ]


class ToolExecutor:
    """
    工具执行器
    
    负责工具的调度和执行管理
    
    遵循原则:
    - E-3 资源确定性：并发控制和超时管理
    - S-1 反馈闭环：完整的执行反馈
    """
    
    def __init__(
        self,
        max_concurrent: int = 50,
        default_timeout: float = 60.0
    ):
        """
        初始化工具执行器
        
        Args:
            max_concurrent: 最大并发执行数
            default_timeout: 默认超时时间（秒）
        """
        self.max_concurrent = max_concurrent
        self.default_timeout = default_timeout
        self._semaphore = asyncio.Semaphore(max_concurrent)
        self._registry = ToolRegistry()
        self._execution_history: List[Dict[str, Any]] = []
        self._shutdown = False
    
    async def register_tool(self, tool: Tool) -> bool:
        """
        注册工具
        
        Args:
            tool: 工具实例
            
        Returns:
            bool: 注册是否成功
        """
        return await self._registry.register(tool)
    
    async def execute(
        self,
        tool_id: str,
        parameters: Dict[str, Any],
        context: Optional[ToolContext] = None
    ) -> ToolResult:
        """
        执行工具
        
        Args:
            tool_id: 工具 ID
            parameters: 执行参数
            context: 执行上下文
            
        Returns:
            ToolResult: 执行结果
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
            
            # 设置超时
            if context is None:
                context = ToolContext(tool_id=tool_id)
            if context.timeout is None:
                context.timeout = self.default_timeout
            
            # 执行
            try:
                result = await asyncio.wait_for(
                    tool.execute(parameters, context),
                    timeout=context.timeout
                )
                
                # 记录执行历史
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
        关闭执行器
        
        Args:
            wait: 是否等待运行中的任务完成
        """
        self._shutdown = True
        
        if wait:
            # 等待所有并发任务完成
            await self._semaphore.acquire()
            self._semaphore.release()
    
    def get_stats(self) -> Dict[str, Any]:
        """
        获取执行器统计信息
        
        Returns:
            Dict[str, Any]: 统计信息
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
