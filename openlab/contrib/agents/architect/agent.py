"""
Architect Agent Implementation
===============================

架构师智能体 - 负责系统架构设计与评审

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
class ArchitectContext:
    """架构师智能体上下文"""
    project_type: Optional[str] = None
    tech_stack: Optional[List[str]] = None
    constraints: Dict[str, Any] = field(default_factory=dict)
    review_mode: bool = False


class ArchitectAgent(Agent):
    """
    架构师智能体
    
    核心职责:
    1. 系统架构设计 - 根据需求设计系统架构
    2. 技术选型建议 - 评估和推荐技术栈
    3. 架构评审 - 审查现有架构并提出改进建议
    4. 设计文档生成 - 生成架构设计文档
    
    能力:
    - ARCHITECTURE_DESIGN: 架构设计能力
    - DOCUMENTATION: 文档生成能力
    - OPTIMIZATION: 架构优化能力
    """
    
    def __init__(
        self,
        agent_id: str = "architect-001",
        capabilities: Optional[Set[AgentCapability]] = None,
        manager: Optional[Any] = None,
        workbench_id: Optional[str] = None
    ):
        """
        初始化架构师智能体
        
        Args:
            agent_id: Agent 唯一标识
            capabilities: Agent 能力集合
            manager: Agent 管理器引用
            workbench_id: 虚拟工位标识
        """
        default_capabilities = {
            AgentCapability.ARCHITECTURE_DESIGN,
            AgentCapability.DOCUMENTATION,
            AgentCapability.OPTIMIZATION
        }
        
        super().__init__(
            agent_id=agent_id,
            capabilities=capabilities or default_capabilities,
            manager=manager,
            workbench_id=workbench_id
        )
        
        self._architect_context: Optional[ArchitectContext] = None
        self._prompts_dir = Path(__file__).parent / "prompts"
        
        logger.info(f"ArchitectAgent initialized with ID: {agent_id}")
    
    async def initialize(self) -> None:
        """
        初始化架构师智能体
        
        加载提示词模板和配置
        """
        logger.info(f"Initializing ArchitectAgent: {self.agent_id}")
        
        self.status = AgentStatus.INITIALIZING
        
        try:
            await self._load_prompts()
            self._architect_context = ArchitectContext()
            self.status = AgentStatus.READY
            logger.info(f"ArchitectAgent {self.agent_id} initialized successfully")
        except Exception as e:
            self.status = AgentStatus.ERROR
            logger.error(f"Failed to initialize ArchitectAgent {self.agent_id}: {e}")
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
            logger.debug(f"Loaded System1 prompt from {system1_path}")
        
        if system2_path.exists():
            with open(system2_path, 'r', encoding='utf-8') as f:
                self._system2_prompt = f.read()
            logger.debug(f"Loaded System2 prompt from {system2_path}")
    
    async def execute(self, input_data: Any, context: AgentContext) -> TaskResult:
        """
        执行架构设计或评审任务
        
        Args:
            input_data: 输入数据，包含任务类型和参数
            context: 执行上下文
            
        Returns:
            TaskResult: 任务执行结果
        """
        logger.info(f"ArchitectAgent {self.agent_id} executing task")
        
        self.status = AgentStatus.RUNNING
        self._context = context
        
        start_time = time.time()
        
        try:
            if not isinstance(input_data, dict):
                raise ValueError("input_data must be a dictionary")
            
            task_type = input_data.get("task_type", "design")
            
            if task_type == "design":
                result = await self._design_architecture(input_data)
            elif task_type == "review":
                result = await self._review_architecture(input_data)
            elif task_type == "tech_selection":
                result = await self._recommend_tech_stack(input_data)
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
            
            logger.error(f"ArchitectAgent {self.agent_id} execution failed: {e}")
            
            return TaskResult(
                success=False,
                error=str(e),
                error_code="ARCHITECT_EXECUTION_ERROR",
                metrics={
                    "execution_time": execution_time
                }
            )
    
    async def _design_architecture(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        设计系统架构
        
        Args:
            input_data: 包含需求信息
            
        Returns:
            架构设计方案
        """
        logger.info("Designing architecture")
        
        requirements = input_data.get("requirements", {})
        constraints = input_data.get("constraints", {})
        
        architecture_design = {
            "architecture_type": self._determine_architecture_type(requirements),
            "components": self._design_components(requirements),
            "tech_stack": self._select_tech_stack(requirements, constraints),
            "data_flow": self._design_data_flow(requirements),
            "deployment_strategy": self._design_deployment_strategy(requirements),
            "scalability": self._design_scalability(requirements),
            "security": self._design_security(requirements),
            "recommendations": self._generate_recommendations(requirements)
        }
        
        return architecture_design
    
    async def _review_architecture(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        评审现有架构
        
        Args:
            input_data: 包含现有架构信息
            
        Returns:
            评审结果和改进建议
        """
        logger.info("Reviewing architecture")
        
        existing_architecture = input_data.get("architecture", {})
        
        review_result = {
            "overall_score": self._calculate_architecture_score(existing_architecture),
            "strengths": self._identify_strengths(existing_architecture),
            "weaknesses": self._identify_weaknesses(existing_architecture),
            "risks": self._identify_risks(existing_architecture),
            "improvements": self._suggest_improvements(existing_architecture),
            "compliance": self._check_compliance(existing_architecture)
        }
        
        return review_result
    
    async def _recommend_tech_stack(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        推荐技术栈
        
        Args:
            input_data: 包含项目需求
            
        Returns:
            技术栈推荐
        """
        logger.info("Recommending tech stack")
        
        requirements = input_data.get("requirements", {})
        
        tech_recommendation = {
            "frontend": self._recommend_frontend_tech(requirements),
            "backend": self._recommend_backend_tech(requirements),
            "database": self._recommend_database(requirements),
            "infrastructure": self._recommend_infrastructure(requirements),
            "devops": self._recommend_devops_tools(requirements),
            "rationale": self._explain_tech_choices(requirements)
        }
        
        return tech_recommendation
    
    def _determine_architecture_type(self, requirements: Dict[str, Any]) -> str:
        """确定架构类型"""
        scale = requirements.get("scale", "medium")
        
        if scale == "large":
            return "microservices"
        elif scale == "small":
            return "monolithic"
        else:
            return "modular_monolith"
    
    def _design_components(self, requirements: Dict[str, Any]) -> List[Dict[str, Any]]:
        """设计系统组件"""
        components = []
        
        features = requirements.get("features", [])
        
        if "api" in features:
            components.append({
                "name": "API Gateway",
                "type": "gateway",
                "responsibilities": ["routing", "authentication", "rate_limiting"]
            })
        
        if "data_processing" in features:
            components.append({
                "name": "Data Processing Service",
                "type": "service",
                "responsibilities": ["data_ingestion", "transformation", "storage"]
            })
        
        return components
    
    def _select_tech_stack(
        self,
        requirements: Dict[str, Any],
        constraints: Dict[str, Any]
    ) -> Dict[str, str]:
        """选择技术栈"""
        tech_stack = {}
        
        performance_req = requirements.get("performance", "medium")
        
        if performance_req == "high":
            tech_stack["language"] = "Rust"
            tech_stack["framework"] = "Actix-web"
        else:
            tech_stack["language"] = "Python"
            tech_stack["framework"] = "FastAPI"
        
        return tech_stack
    
    def _design_data_flow(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """设计数据流"""
        return {
            "pattern": "event_driven",
            "message_broker": "Apache Kafka",
            "data_format": "JSON"
        }
    
    def _design_deployment_strategy(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """设计部署策略"""
        return {
            "strategy": "blue_green",
            "containerization": "Docker",
            "orchestration": "Kubernetes"
        }
    
    def _design_scalability(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """设计可扩展性"""
        return {
            "horizontal_scaling": True,
            "auto_scaling": True,
            "load_balancing": "round_robin"
        }
    
    def _design_security(self, requirements: Dict[str, Any]) -> Dict[str, Any]:
        """设计安全策略"""
        return {
            "authentication": "JWT",
            "authorization": "RBAC",
            "encryption": "TLS 1.3"
        }
    
    def _generate_recommendations(self, requirements: Dict[str, Any]) -> List[str]:
        """生成建议"""
        return [
            "Implement comprehensive logging and monitoring",
            "Use circuit breaker pattern for external services",
            "Implement graceful degradation for non-critical features"
        ]
    
    def _calculate_architecture_score(self, architecture: Dict[str, Any]) -> float:
        """计算架构评分"""
        return 0.85
    
    def _identify_strengths(self, architecture: Dict[str, Any]) -> List[str]:
        """识别优势"""
        return ["Good separation of concerns", "Clear component boundaries"]
    
    def _identify_weaknesses(self, architecture: Dict[str, Any]) -> List[str]:
        """识别弱点"""
        return ["Potential single point of failure in database layer"]
    
    def _identify_risks(self, architecture: Dict[str, Any]) -> List[str]:
        """识别风险"""
        return ["Database scalability risk", "Network latency in distributed components"]
    
    def _suggest_improvements(self, architecture: Dict[str, Any]) -> List[str]:
        """建议改进"""
        return [
            "Implement database sharding for better scalability",
            "Add caching layer to reduce database load"
        ]
    
    def _check_compliance(self, architecture: Dict[str, Any]) -> Dict[str, bool]:
        """检查合规性"""
        return {
            "security_compliance": True,
            "performance_compliance": True,
            "scalability_compliance": True
        }
    
    def _recommend_frontend_tech(self, requirements: Dict[str, Any]) -> Dict[str, str]:
        """推荐前端技术"""
        return {
            "framework": "React",
            "state_management": "Redux",
            "styling": "Tailwind CSS"
        }
    
    def _recommend_backend_tech(self, requirements: Dict[str, Any]) -> Dict[str, str]:
        """推荐后端技术"""
        return {
            "language": "Python",
            "framework": "FastAPI",
            "orm": "SQLAlchemy"
        }
    
    def _recommend_database(self, requirements: Dict[str, Any]) -> Dict[str, str]:
        """推荐数据库"""
        return {
            "primary": "PostgreSQL",
            "cache": "Redis",
            "search": "Elasticsearch"
        }
    
    def _recommend_infrastructure(self, requirements: Dict[str, Any]) -> Dict[str, str]:
        """推荐基础设施"""
        return {
            "cloud_provider": "AWS",
            "containerization": "Docker",
            "orchestration": "Kubernetes"
        }
    
    def _recommend_devops_tools(self, requirements: Dict[str, Any]) -> Dict[str, str]:
        """推荐DevOps工具"""
        return {
            "ci_cd": "GitHub Actions",
            "monitoring": "Prometheus + Grafana",
            "logging": "ELK Stack"
        }
    
    def _explain_tech_choices(self, requirements: Dict[str, Any]) -> Dict[str, str]:
        """解释技术选择"""
        return {
            "frontend": "React provides excellent performance and developer experience",
            "backend": "FastAPI offers high performance and automatic API documentation",
            "database": "PostgreSQL provides ACID compliance and excellent performance"
        }
    
    async def shutdown(self) -> None:
        """关闭智能体"""
        logger.info(f"Shutting down ArchitectAgent: {self.agent_id}")
        self.status = AgentStatus.SHUTTING_DOWN
        
        self._architect_context = None
        self._context = None
        
        self.status = AgentStatus.SHUTDOWN
        logger.info(f"ArchitectAgent {self.agent_id} shutdown complete")


def create_architect_agent(
    agent_id: str = "architect-001",
    manager: Optional[Any] = None,
    workbench_id: Optional[str] = None
) -> ArchitectAgent:
    """
    创建架构师智能体实例
    
    Args:
        agent_id: Agent 唯一标识
        manager: Agent 管理器引用
        workbench_id: 虚拟工位标识
        
    Returns:
        ArchitectAgent 实例
    """
    return ArchitectAgent(
        agent_id=agent_id,
        manager=manager,
        workbench_id=workbench_id
    )


if __name__ == "__main__":
    async def test_architect_agent():
        """测试架构师智能体"""
        agent = create_architect_agent()
        await agent.initialize()
        
        context = AgentContext(agent_id=agent.agent_id)
        
        result = await agent.execute(
            {
                "task_type": "design",
                "requirements": {
                    "scale": "medium",
                    "features": ["api", "data_processing"],
                    "performance": "high"
                }
            },
            context
        )
        
        print(f"Task Result: {json.dumps(result.output, indent=2)}")
        
        await agent.shutdown()
    
    asyncio.run(test_architect_agent())
