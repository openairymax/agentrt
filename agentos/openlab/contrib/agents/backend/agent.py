"""
Backend Agent Implementation
============================

鍚庣寮€鍙戞櫤鑳戒綋 - 璐熻矗鍚庣绯荤粺寮€鍙戙€丄PI璁捐鍜屾暟鎹簱璁捐

閬靛惊 AgentOS 鏋舵瀯璁捐鍘熷垯:
- K-2 鎺ュ彛濂戠害鍖栵細瀹屾暣鐨?docstring 鍜岀被鍨嬫敞瑙?- K-4 鍙彃鎷旂瓥鐣ワ細鏀寔杩愯鏃舵浛鎹?- E-3 璧勬簮纭畾鎬э細鏄庣‘鐨勮祫婧愮敓鍛藉懆鏈?- E-6 閿欒鍙拷婧細瀹屾暣鐨勯敊璇摼

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
    """鍚庣寮€鍙戞櫤鑳戒綋涓婁笅鏂?""
    project_type: Optional[str] = None
    framework: Optional[str] = None
    database: Optional[str] = None
    api_style: Optional[str] = None


class BackendAgent(Agent):
    """
    鍚庣寮€鍙戞櫤鑳戒綋
    
    鏍稿績鑱岃矗:
    1. API璁捐涓庡紑鍙?- RESTful/GraphQL API 璁捐涓庡疄鐜?    2. 鏁版嵁搴撹璁?- 鏁版嵁妯″瀷銆佹煡璇紭鍖栥€佽縼绉荤鐞?    3. 鍚庣鏈嶅姟鏋舵瀯 - 寰湇鍔°€佹秷鎭槦鍒椼€佺紦瀛樼瓥鐣?    4. 鎬ц兘浼樺寲 - 鍝嶅簲鏃堕棿浼樺寲銆佸苟鍙戝鐞嗐€佽祫婧愮鐞?    
    鑳藉姏:
    - CODE_GENERATION: 浠ｇ爜鐢熸垚鑳藉姏
    - DEBUGGING: 璋冭瘯鑳藉姏
    - OPTIMIZATION: 浼樺寲鑳藉姏
    """
    
    def __init__(
        self,
        agent_id: str = "backend-001",
        capabilities: Optional[Set[AgentCapability]] = None,
        manager: Optional[Any] = None,
        workbench_id: Optional[str] = None
    ):
        """
        鍒濆鍖栧悗绔紑鍙戞櫤鑳戒綋
        
        Args:
            agent_id: Agent 鍞竴鏍囪瘑
            capabilities: Agent 鑳藉姏闆嗗悎
            manager: Agent 绠＄悊鍣ㄥ紩鐢?            workbench_id: 铏氭嫙宸ヤ綅鏍囪瘑
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
        鍒濆鍖栧悗绔紑鍙戞櫤鑳戒綋
        
        鍔犺浇鎻愮ず璇嶆ā鏉垮拰閰嶇疆
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
        """鍔犺浇鎻愮ず璇嶆ā鏉?""
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
        鎵ц鍚庣寮€鍙戜换鍔?        
        Args:
            input_data: 杈撳叆鏁版嵁锛屽寘鍚换鍔＄被鍨嬪拰鍙傛暟
            context: 鎵ц涓婁笅鏂?            
        Returns:
            TaskResult: 浠诲姟鎵ц缁撴灉
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
        寮€鍙慉PI鎺ュ彛
        
        Args:
            input_data: 鍖呭惈API闇€姹備俊鎭?            
        Returns:
            API璁捐鏂规鍜屼唬鐮?        """
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
        璁捐鏁版嵁搴?        
        Args:
            input_data: 鍖呭惈鏁版嵁闇€姹備俊鎭?            
        Returns:
            鏁版嵁搴撹璁℃柟妗?        """
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
        璁捐鏈嶅姟鏋舵瀯
        
        Args:
            input_data: 鍖呭惈鏋舵瀯闇€姹備俊鎭?            
        Returns:
        鏈嶅姟鏋舵瀯璁捐鏂规
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
        浼樺寲鎬ц兘
        
        Args:
            input_data: 鍖呭惈鎬ц兘闂鍜岀洰鏍?            
        Returns:
            浼樺寲寤鸿鍜屽疄鏂芥柟妗?        """
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
        """纭畾API椋庢牸"""
        complexity = requirements.get("complexity", "medium")
        
        if complexity == "high":
            return "graphql"
        else:
            return "restful"
    
    def _design_endpoints(self, endpoints: List[str]) -> List[Dict[str, Any]]:
        """璁捐绔偣"""
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
        """璁捐璁よ瘉鏂规"""
        return {
            "method": "JWT",
            "token_expiry": 3600,
            "refresh_token": True,
            "oauth_support": True
        }
    
    def _design_validation(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """璁捐楠岃瘉鏈哄埗"""
        return {
            "input_validation": "Pydantic models",
            "output_validation": "Response schemas",
            "sanitization": "Input sanitization"
        }
    
    def _design_error_handling(self) -> Dict[str, Any]:
        """璁捐閿欒澶勭悊"""
        return {
            "error_format": "RFC 7807 Problem Details",
            "logging": "Structured logging",
            "retry_mechanism": "Exponential backoff"
        }
    
    def _generate_api_docs(self) -> Dict[str, Any]:
        """鐢熸垚API鏂囨。"""
        return {
            "format": "OpenAPI 3.0",
            "tools": ["Swagger UI", "ReDoc"],
            "auto_generation": True
        }
    
    def _generate_code_examples(self, endpoints: List[str]) -> List[Dict[str, str]]:
        """鐢熸垚浠ｇ爜绀轰緥"""
        examples = []
        
        for endpoint in endpoints[:3]:  # 鍙敓鎴愬墠3涓ず渚?            examples.append({
                "endpoint": endpoint,
                "example_request": f"GET /api/v1/{endpoint}",
                "example_response": '{"status": "success"}'
            })
        
        return examples
    
    def _select_database(self, requirements: Dict[str, Any]) -> str:
        """閫夋嫨鏁版嵁搴撶被鍨?""
        scale = requirements.get("scale", "medium")
        
        if scale == "large":
            return "PostgreSQL"
        else:
            return "SQLite"
    
    def _design_schema(self, entities: List[str]) -> List[Dict[str, Any]]:
        """璁捐鏁版嵁搴撴ā寮?""
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
        """璁捐绱㈠紩"""
        indexes = []
        
        for entity in entities:
            indexes.append({
                "table": entity.lower(),
                "columns": ["id"],
                "type": "B-tree"
            })
        
        return indexes
    
    def _define_relationships(self, entities: List[str]) -> List[Dict[str, Any]]:
        """瀹氫箟鍏崇郴"""
        relationships = []
        
        if len(entities) > 1:
            relationships.append({
                "from": entities[0].lower(),
                "to": entities[1].lower(),
                "type": "one-to-many"
            })
        
        return relationships
    
    def _generate_migrations(self) -> Dict[str, Any]:
        """鐢熸垚杩佺Щ鑴氭湰"""
        return {
            "tool": "Alembic",
            "version_control": True,
            "rollback_support": True
        }
    
    def _suggest_db_optimizations(self) -> List[str]:
        """寤鸿鏁版嵁搴撲紭鍖?""
        return [
            "Use connection pooling",
            "Implement query caching",
            "Add read replicas for heavy read workloads"
        ]
    
    def _select_architecture_pattern(self, requirements: Dict[str, Any]) -> str:
        """閫夋嫨鏋舵瀯妯″紡"""
        scale = requirements.get("scale", "medium")
        
        if scale == "large":
            return "microservices"
        else:
            return "monolithic"
    
    def _define_services(self, requirements: Dict[str, Any]) -> List[Dict[str, Any]]:
        """瀹氫箟鏈嶅姟"""
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
        """璁捐閫氫俊鏂瑰紡"""
        return {
            "protocol": "HTTP/REST",
            "message_queue": "RabbitMQ",
            "service_discovery": "Consul"
        }
    
    def _design_data_strategy(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """璁捐鏁版嵁绛栫暐"""
        return {
            "pattern": "Database per Service",
            "event_sourcing": False,
            "cqrs": False
        }
    
    def _design_scalability(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """璁捐鍙墿灞曟€?""
        return {
            "horizontal_scaling": True,
            "load_balancing": "Nginx",
            "auto_scaling": "Kubernetes HPA"
        }
    
    def _design_monitoring(self) -> Dict[str, Any]:
        """璁捐鐩戞帶"""
        return {
            "metrics": "Prometheus",
            "tracing": "Jaeger",
            "logging": "ELK Stack"
        }
    
    def _analyze_current_perf(self, issues: List[str]) -> Dict[str, float]:
        """鍒嗘瀽褰撳墠鎬ц兘"""
        return {
            "response_time_p50": 200,
            "response_time_p95": 1000,
            "throughput": 500,
            "error_rate": 0.01
        }
    
    def _identify_bottlenecks(self, issues: List[str]) -> List[str]:
        """璇嗗埆鐡堕"""
        return [
            "Database query performance",
            "Memory usage optimization",
            "Connection pool sizing"
        ]
    
    def _propose_optimizations(self, issues: List[str], targets: Dict[str, Any]) -> List[Dict[str, Any]]:
        """鎻愬嚭浼樺寲寤鸿"""
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
        """鍒涘缓瀹炴柦璁″垝"""
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
        """浼拌鏀硅繘鏁堟灉"""
        return {
            "response_time_reduction": 0.5,
            "throughput_increase": 2.0,
            "error_rate_reduction": 0.8
        }
    
    async def shutdown(self) -> None:
        """鍏抽棴鏅鸿兘浣?""
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
    鍒涘缓鍚庣寮€鍙戞櫤鑳戒綋瀹炰緥
    
    Args:
        agent_id: Agent 鍞竴鏍囪瘑
        manager: Agent 绠＄悊鍣ㄥ紩鐢?        workbench_id: 铏氭嫙宸ヤ綅鏍囪瘑
        
    Returns:
        BackendAgent 瀹炰緥
    """
    return BackendAgent(
        agent_id=agent_id,
        manager=manager,
        workbench_id=workbench_id
    )


if __name__ == "__main__":
    async def test_backend_agent():
        """娴嬭瘯鍚庣寮€鍙戞櫤鑳戒綋"""
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
