"""
Backend Agent Implementation
============================

后端开发智能体 - 负责后端系统开发、API设计和数据库设计

遵循 AgentOS 架构设计原则:
- K-2 接口契约化：完整的 docstring 和类型注解
- K-4 可插拔策略：支持运行时替换
- E-3 资源确定性：明确的资源生命周期
- E-6 错误可追溯：完整的错误链

Copyright (c) 2026 SPHARX. All Rights Reserved.
"""

import asyncio
import json
import logging
import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Set

import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / "openlab"))

from openlab.core.agent import Agent, AgentCapability, AgentContext, AgentStatus, TaskResult


logger = logging.getLogger(__name__)


@dataclass
class BackendContext:
    """后端开发智能体上下文"""
    project_type: Optional[str] = None
    framework: Optional[str] = None
    database: Optional[str] = None
    api_style: Optional[str] = None


class BackendAgent(Agent):
    """
    后端开发智能体
    
    核心职责:
    1. API设计与开发 - RESTful/GraphQL API 设计与实现
    2. 数据库设计 - 数据模型、查询优化、迁移管理
    3. 后端服务架构 - 微服务、消息队列、缓存策略
    4. 性能优化 - 响应时间优化、并发处理、资源管理
    
    能力:
    - CODE_GENERATION: 代码生成能力
    - DEBUGGING: 调试能力
    - OPTIMIZATION: 优化能力
    """
    
    def __init__(
        self,
        agent_id: str = "backend-001",
        capabilities: Optional[Set[AgentCapability]] = None,
        manager: Optional[Any] = None,
        workbench_id: Optional[str] = None
    ):
        """
        初始化后端开发智能体
        
        Args:
            agent_id: Agent 唯一标识
            capabilities: Agent 能力集合
            manager: Agent 管理器引用
            workbench_id: 虚拟工位标识
        """
        default_capabilities = {
            AgentCapability.CODE_GENERATION,
            AgentCapability.DEBUGGING,
            AgentCapability.OPTIMIZATION
        }
        
        super().__init__(
            agent_id=agent_id,
            capabilities=capabilities or default_capabilities,
            manager=manager,
            workbench_id=workbench_id
        )
        
        self._backend_context: Optional[BackendContext] = None
        self._prompts_dir = Path(__file__).parent / "prompts"
        
        logger.info(f"BackendAgent initialized with ID: {agent_id}")
    
    async def initialize(self) -> None:
        """
        初始化后端开发智能体
        
        加载提示词模板和配置
        """
        logger.info(f"Initializing BackendAgent: {self.agent_id}")
        
        self.status = AgentStatus.INITIALIZING
        
        try:
            await self._load_prompts()
            self._backend_context = BackendContext()
            self.status = AgentStatus.READY
            logger.info(f"BackendAgent {self.agent_id} initialized successfully")
        except Exception as e:
            self.status = AgentStatus.ERROR
            logger.error(f"Failed to initialize BackendAgent {self.agent_id}: {e}")
            raise
    
    async def _load_prompts(self) -> None:
        """加载提示词模板"""
        self._system1_prompt = None
        self._system2_prompt = None
        
        system1_path = self._prompts_dir / "system1.md"
        system2_path = self._prompts_dir / "system2.md"
        
        if system1_path.exists():
            with open(system1_path, 'r', encoding='utf-8') as f:
                self._system1_prompt = f.read()
        
        if system2_path.exists():
            with open(system2_path, 'r', encoding='utf-8') as f:
                self._system2_prompt = f.read()
    
    async def execute(self, input_data: Any, context: AgentContext) -> TaskResult:
        """
        执行后端开发任务
        
        Args:
            input_data: 输入数据，包含任务类型和参数
            context: 执行上下文
            
        Returns:
            TaskResult: 任务执行结果
        """
        logger.info(f"BackendAgent {self.agent_id} executing task")
        
        self.status = AgentStatus.RUNNING
        self._context = context
        
        start_time = time.time()
        
        try:
            if not isinstance(input_data, dict):
                raise ValueError("input_data must be a dictionary")
            
            task_type = input_data.get("task_type", "api_development")
            
            if task_type == "api_development":
                result = await self._develop_api(input_data)
            elif task_type == "database_design":
                result = await self._design_database(input_data)
            elif task_type == "service_architecture":
                result = await self._design_service_architecture(input_data)
            elif task_type == "performance_optimization":
                result = await self._optimize_performance(input_data)
            else:
                raise ValueError(f"Unknown task type: {task_type}")
            
            execution_time = time.time() - start_time
            
            self.status = AgentStatus.READY
            
            return TaskResult(
                success=True,
                output=result,
                metrics={
                    "execution_time": execution_time,
                    "task_type": task_type
                }
            )
            
        except Exception as e:
            execution_time = time.time() - start_time
            self.status = AgentStatus.ERROR
            
            logger.error(f"BackendAgent {self.agent_id} execution failed: {e}")
            
            return TaskResult(
                success=False,
                error=str(e),
                error_code="BACKEND_EXECUTION_ERROR",
                metrics={
                    "execution_time": execution_time
                }
            )
    
    async def _develop_api(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        开发API接口
        
        Args:
            input_data: 包含API需求信息
            
        Returns:
            API设计方案和代码
        """
        logger.info("Developing API")
        
        api_requirements = input_data.get("requirements", {})
        endpoints = api_requirements.get("endpoints", [])
        
        api_design = {
            "api_style": self._determine_api_style(api_requirements),
            "endpoints": self._design_endpoints(endpoints),
            "authentication": self._design_auth(api_requirements),
            "validation": self._design_validation(api_requirements),
            "error_handling": self._design_error_handling(),
            "documentation": self._generate_api_docs(),
            "code_examples": self._generate_code_examples(endpoints)
        }
        
        return api_design
    
    async def _design_database(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        设计数据库
        
        Args:
            input_data: 包含数据需求信息
            
        Returns:
            数据库设计方案
        """
        logger.info("Designing database")
        
        db_requirements = input_data.get("requirements", {})
        entities = db_requirements.get("entities", [])
        
        db_design = {
            "database_type": self._select_database(db_requirements),
            "schema": self._design_schema(entities),
            "indexes": self._design_indexes(entities),
            "relationships": self._define_relationships(entities),
            "migrations": self._generate_migrations(),
            "optimization": self._suggest_db_optimizations()
        }
        
        return db_design
    
    async def _design_service_architecture(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        设计服务架构
        
        Args:
            input_data: 包含架构需求信息
            
        Returns:
        服务架构设计方案
        """
        logger.info("Designing service architecture")
        
        arch_requirements = input_data.get("requirements", {})
        
        service_arch = {
            "architecture_pattern": self._select_architecture_pattern(arch_requirements),
            "services": self._define_services(arch_requirements),
            "communication": self._design_communication(arch_requirements),
            "data_management": self._design_data_strategy(arch_requirements),
            "scalability": self._design_scalability(arch_requirements),
            "monitoring": self._design_monitoring()
        }
        
        return service_arch
    
    async def _optimize_performance(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        优化性能
        
        Args:
            input_data: 包含性能问题和目标
            
        Returns:
            优化建议和实施方案
        """
        logger.info("Optimizing performance")
        
        perf_issues = input_data.get("issues", [])
        targets = input_data.get("targets", {})
        
        optimization_plan = {
            "current_performance": self._analyze_current_perf(perf_issues),
            "bottlenecks": self._identify_bottlenecks(perf_issues),
            "optimizations": self._propose_optimizations(perf_issues, targets),
            "implementation_steps": self._create_implementation_plan(),
            "expected_improvements": self._estimate_improvements(targets)
        }
        
        return optimization_plan
    
    def _determine_api_style(self, requirements: Dict[str, Any]) -> str:
        """确定API风格"""
        complexity = requirements.get("complexity", "medium")
        
        if complexity == "high":
            return "graphql"
        else:
            return "restful"
    
    def _design_endpoints(self, endpoints: List[str]) -> List[Dict[str, Any]]:
        """设计端点"""
        designed_endpoints = []
        
        for endpoint in endpoints:
            designed_endpoints.append({
                "path": f"/api/v1/{endpoint}",
                "methods": ["GET", "POST", "PUT", "DELETE"],
                "auth_required": True,
                "rate_limiting": True
            })
        
        return designed_endpoints
    
    def _design_auth(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """设计认证方案"""
        return {
            "method": "JWT",
            "token_expiry": 3600,
            "refresh_token": True,
            "oauth_support": True
        }
    
    def _design_validation(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """设计验证机制"""
        return {
            "input_validation": "Pydantic models",
            "output_validation": "Response schemas",
            "sanitization": "Input sanitization"
        }
    
    def _design_error_handling(self) -> Dict[str, Any]:
        """设计错误处理"""
        return {
            "error_format": "RFC 7807 Problem Details",
            "logging": "Structured logging",
            "retry_mechanism": "Exponential backoff"
        }
    
    def _generate_api_docs(self) -> Dict[str, Any]:
        """生成API文档"""
        return {
            "format": "OpenAPI 3.0",
            "tools": ["Swagger UI", "ReDoc"],
            "auto_generation": True
        }
    
    def _generate_code_examples(self, endpoints: List[str]) -> List[Dict[str, str]]:
        """生成代码示例"""
        examples = []
        
        for endpoint in endpoints[:3]:  # 只生成前3个示例
            examples.append({
                "endpoint": endpoint,
                "example_request": f"GET /api/v1/{endpoint}",
                "example_response": '{"status": "success"}'
            })
        
        return examples
    
    def _select_database(self, requirements: Dict[str, Any]) -> str:
        """选择数据库类型"""
        scale = requirements.get("scale", "medium")
        
        if scale == "large":
            return "PostgreSQL"
        else:
            return "SQLite"
    
    def _design_schema(self, entities: List[str]) -> List[Dict[str, Any]]:
        """设计数据库模式"""
        schema = []
        
        for entity in entities:
            schema.append({
                "table_name": entity.lower(),
                "columns": [
                    {"name": "id", "type": "INTEGER", "primary_key": True},
                    {"name": "created_at", "type": "TIMESTAMP"},
                    {"name": "updated_at", "type": "TIMESTAMP"}
                ]
            })
        
        return schema
    
    def _design_indexes(self, entities: List[str]) -> List[Dict[str, Any]]:
        """设计索引"""
        indexes = []
        
        for entity in entities:
            indexes.append({
                "table": entity.lower(),
                "columns": ["id"],
                "type": "B-tree"
            })
        
        return indexes
    
    def _define_relationships(self, entities: List[str]) -> List[Dict[str, Any]]:
        """定义关系"""
        relationships = []
        
        if len(entities) > 1:
            relationships.append({
                "from": entities[0].lower(),
                "to": entities[1].lower(),
                "type": "one-to-many"
            })
        
        return relationships
    
    def _generate_migrations(self) -> Dict[str, Any]:
        """生成迁移脚本"""
        return {
            "tool": "Alembic",
            "version_control": True,
            "rollback_support": True
        }
    
    def _suggest_db_optimizations(self) -> List[str]:
        """建议数据库优化"""
        return [
            "Use connection pooling",
            "Implement query caching",
            "Add read replicas for heavy read workloads"
        ]
    
    def _select_architecture_pattern(self, requirements: Dict[str, Any]) -> str:
        """选择架构模式"""
        scale = requirements.get("scale", "medium")
        
        if scale == "large":
            return "microservices"
        else:
            return "monolithic"
    
    def _define_services(self, requirements: Dict[str, Any]) -> List[Dict[str, Any]]:
        """定义服务"""
        services = [
            {
                "name": "user-service",
                "responsibilities": ["user management", "authentication"]
            },
            {
                "name": "data-service",
                "responsibilities": ["data processing", "storage"]
            }
        ]
        
        return services
    
    def _design_communication(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """设计通信方式"""
        return {
            "protocol": "HTTP/REST",
            "message_queue": "RabbitMQ",
            "service_discovery": "Consul"
        }
    
    def _design_data_strategy(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """设计数据策略"""
        return {
            "pattern": "Database per Service",
            "event_sourcing": False,
            "cqrs": False
        }
    
    def _design_scalability(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """设计可扩展性"""
        return {
            "horizontal_scaling": True,
            "load_balancing": "Nginx",
            "auto_scaling": "Kubernetes HPA"
        }
    
    def _design_monitoring(self) -> Dict[str, Any]:
        """设计监控"""
        return {
            "metrics": "Prometheus",
            "tracing": "Jaeger",
            "logging": "ELK Stack"
        }
    
    def _analyze_current_perf(self, issues: List[str]) -> Dict[str, float]:
        """分析当前性能"""
        return {
            "response_time_p50": 200,
            "response_time_p95": 1000,
            "throughput": 500,
            "error_rate": 0.01
        }
    
    def _identify_bottlenecks(self, issues: List[str]) -> List[str]:
        """识别瓶颈"""
        return [
            "Database query performance",
            "Memory usage optimization",
            "Connection pool sizing"
        ]
    
    def _propose_optimizations(self, issues: List[str], targets: Dict[str, Any]) -> List[Dict[str, Any]]:
        """提出优化建议"""
        optimizations = [
            {
                "area": "database",
                "optimization": "Add query indexes and optimize queries",
                "expected_impact": "high",
                "effort": "medium"
            },
            {
                "area": "caching",
                "optimization": "Implement Redis caching layer",
                "expected_impact": "high",
                "effort": "low"
            },
            {
                "area": "connection_pool",
                "optimization": "Tune connection pool parameters",
                "expected_impact": "medium",
                "effort": "low"
            }
        ]
        
        return optimizations
    
    def _create_implementation_plan(self) -> List[Dict[str, Any]]:
        """创建实施计划"""
        return [
            {
                "phase": 1,
                "tasks": ["Performance baseline measurement"],
                "duration": "1 day"
            },
            {
                "phase": 2,
                "tasks": ["Implement caching layer"],
                "duration": "2 days"
            },
            {
                "phase": 3,
                "tasks": ["Database query optimization"],
                "duration": "3 days"
            }
        ]
    
    def _estimate_improvements(self, targets: Dict[str, Any]) -> Dict[str, float]:
        """估计改进效果"""
        return {
            "response_time_reduction": 0.5,
            "throughput_increase": 2.0,
            "error_rate_reduction": 0.8
        }
    
    async def shutdown(self) -> None:
        """关闭智能体"""
        logger.info(f"Shutting down BackendAgent: {self.agent_id}")
        self.status = AgentStatus.SHUTTING_DOWN
        
        self._backend_context = None
        self._context = None
        
        self.status = AgentStatus.SHUTDOWN
        logger.info(f"BackendAgent {self.agent_id} shutdown complete")


def create_backend_agent(
    agent_id: str = "backend-001",
    manager: Optional[Any] = None,
    workbench_id: Optional[str] = None
) -> BackendAgent:
    """
    创建后端开发智能体实例
    
    Args:
        agent_id: Agent 唯一标识
        manager: Agent 管理器引用
        workbench_id: 虚拟工位标识
        
    Returns:
        BackendAgent 实例
    """
    return BackendAgent(
        agent_id=agent_id,
        manager=manager,
        workbench_id=workbench_id
    )


if __name__ == "__main__":
    async def test_backend_agent():
        """测试后端开发智能体"""
        agent = create_backend_agent()
        await agent.initialize()
        
        context = AgentContext(agent_id=agent.agent_id)
        
        result = await agent.execute(
            {
                "task_type": "api_development",
                "requirements": {
                    "complexity": "medium",
                    "endpoints": ["users", "products", "orders"]
                }
            },
            context
        )
        
        print(f"Task Result: {json.dumps(result.output, indent=2)}")
        
        await agent.shutdown()
    
    asyncio.run(test_backend_agent())
