"""
Product Manager Agent Implementation
====================================

产品规划智能体 - 负责产品需求分析、路线图规划和用户研究

Copyright (c) 2026 SPHARX. All Rights Reserved.
"""

import asyncio
import json
import logging
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Set

import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / "openlab"))

from openlab.core.agent import Agent, AgentCapability, AgentContext, AgentStatus, TaskResult


logger = logging.getLogger(__name__)


class ProductManagerAgent(Agent):
    """
    产品规划智能体
    
    核心职责:
    1. 需求分析 - 用户需求收集、优先级排序
    2. 路线图规划 - 产品迭代计划、里程碑管理
    3. 用户研究 - 用户画像、用户旅程设计
    4. 数据分析 - 产品指标、A/B测试
    
    能力:
    - DOCUMENTATION: 文档生成能力
    - OPTIMIZATION: 优化能力
    """
    
    def __init__(
        self,
        agent_id: str = "product_manager-001",
        capabilities: Optional[Set[AgentCapability]] = None,
        manager: Optional[Any] = None,
        workbench_id: Optional[str] = None
    ):
        default_capabilities = {
            AgentCapability.DOCUMENTATION,
            AgentCapability.OPTIMIZATION
        }
        
        super().__init__(
            agent_id=agent_id,
            capabilities=capabilities or default_capabilities,
            manager=manager,
            workbench_id=workbench_id
        )
        
        self._prompts_dir = Path(__file__).parent / "prompts"
        logger.info(f"ProductManagerAgent initialized with ID: {agent_id}")
    
    async def initialize(self) -> None:
        """初始化产品规划智能体"""
        self.status = AgentStatus.INITIALIZING
        
        try:
            await self._load_prompts()
            self.status = AgentStatus.READY
            logger.info(f"ProductManagerAgent {self.agent_id} initialized successfully")
        except Exception as e:
            self.status = AgentStatus.ERROR
            raise e
    
    async def _load_prompts(self) -> None:
        """加载提示词模板"""
        system1_path = self._prompts_dir / "system1.md"
        system2_path = self._prompts_dir / "system2.md"
        
        if system1_path.exists():
            with open(system1_path, 'r', encoding='utf-8') as f:
                pass
        
        if system2_path.exists():
            with open(system2_path, 'r', encoding='utf-8') as f:
                pass
    
    async def execute(self, input_data: Any, context: AgentContext) -> TaskResult:
        """执行产品规划任务"""
        self.status = AgentStatus.RUNNING
        start_time = time.time()
        
        try:
            if not isinstance(input_data, dict):
                raise ValueError("input_data must be a dictionary")
            
            task_type = input_data.get("task_type", "requirement_analysis")
            
            if task_type == "requirement_analysis":
                result = await self._analyze_requirements(input_data)
            elif task_type == "roadmap_planning":
                result = await self._plan_roadmap(input_data)
            elif task_type == "user_research":
                result = await self._conduct_user_research(input_data)
            else:
                raise ValueError(f"Unknown task type: {task_type}")
            
            execution_time = time.time() - start_time
            
            return TaskResult(
                success=True,
                output=result,
                metrics={"execution_time": execution_time}
            )
            
        except Exception as e:
            return TaskResult(
                success=False,
                error=str(e),
                error_code="PRODUCT_MANAGER_EXECUTION_ERROR"
            )
    
    async def _analyze_requirements(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """分析需求"""
        return {
            "requirements": [
                {"id": "REQ-001", "title": "User Authentication", "priority": "high"},
                {"id": "REQ-002", "title": "Dashboard", "priority": "medium"}
            ],
            "user_stories": ["As a user, I want to login securely"],
            "acceptance_criteria": ["Given user credentials, When login Then access granted"]
        }
    
    async def _plan_roadmap(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """规划路线图"""
        return {
            "phases": [
                {"name": "Phase 1", "duration": "Q1 2026", "features": ["MVP"]},
                {"name": "Phase 2", "duration": "Q2 2026", "features": ["Advanced features"]}
            ],
            "milestones": ["Alpha release", "Beta release", "GA release"]
        }
    
    async def _conduct_user_research(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """进行用户研究"""
        return {
            "personas": [{"name": "Developer", "goals": ["Build faster"]}],
            "journey_maps": ["Discovery → Evaluation → Adoption"],
            "feedback_channels": ["Surveys", "Interviews"]
        }
    
    async def shutdown(self) -> None:
        """关闭智能体"""
        self.status = AgentStatus.SHUTTING_DOWN
        self.status = AgentStatus.SHUTDOWN


def create_product_manager_agent(
    agent_id: str = "product_manager-001",
    manager: Optional[Any] = None,
    workbench_id: Optional[str] = None
) -> ProductManagerAgent:
    """创建产品规划智能体实例"""
    return ProductManagerAgent(
        agent_id=agent_id,
        manager=manager,
        workbench_id=workbench_id
    )


if __name__ == "__main__":
    async def test_product_manager_agent():
        agent = create_product_manager_agent()
        await agent.initialize()
        context = AgentContext(agent_id=agent.agent_id)
        result = await agent.execute({"task_type": "requirement_analysis"}, context)
        print(json.dumps(result.output, indent=2))
        await agent.shutdown()
    
    asyncio.run(test_product_manager_agent())
