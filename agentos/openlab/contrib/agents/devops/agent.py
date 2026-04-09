"""
DevOps Agent Implementation
==========================

杩愮淮閮ㄧ讲鏅鸿兘浣?- 璐熻矗CI/CD銆佸鍣ㄥ寲閮ㄧ讲銆佸熀纭€璁炬柦绠＄悊

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


class DevOpsAgent(Agent):
    """
    杩愮淮閮ㄧ讲鏅鸿兘浣?    
    鏍稿績鑱岃矗:
    1. CI/CD娴佹按绾胯璁?- 鑷姩鍖栨瀯寤恒€佹祴璇曘€侀儴缃?    2. 瀹瑰櫒鍖栫鐞?- Docker/Kubernetes閰嶇疆浼樺寲
    3. 鍩虹璁炬柦鍗充唬鐮?- Terraform/Ansible鑷姩鍖?    4. 鐩戞帶鍛婅 - 绯荤粺鐩戞帶銆佹棩蹇楄仛鍚堛€佸憡璀︾瓥鐣?    
    鑳藉姏:
    - CODE_GENERATION: 閰嶇疆浠ｇ爜鐢熸垚鑳藉姏
    - DEBUGGING: 闂璇婃柇鑳藉姏
    - OPTIMIZATION: 鎬ц兘璋冧紭鑳藉姏
    """
    
    def __init__(
        self,
        agent_id: str = "devops-001",
        capabilities: Optional[Set[AgentCapability]] = None,
        manager: Optional[Any] = None,
        workbench_id: Optional[str] = None
    ):
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
        
        self._prompts_dir = Path(__file__).parent / "prompts"
        logger.info(f"DevOpsAgent initialized with ID: {agent_id}")
    
    async def initialize(self) -> None:
        """鍒濆鍖栬繍缁撮儴缃叉櫤鑳戒綋"""
        self.status = AgentStatus.INITIALIZING
        
        try:
            await self._load_prompts()
            self.status = AgentStatus.READY
            logger.info(f"DevOpsAgent {self.agent_id} initialized successfully")
        except Exception as e:
            self.status = AgentStatus.ERROR
            raise e
    
    async def _load_prompts(self) -> None:
        """鍔犺浇鎻愮ず璇嶆ā鏉?""
        system1_path = self._prompts_dir / "system1.md"
        system2_path = self._prompts_dir / "system2.md"
        
        if system1_path.exists():
            with open(system1_path, 'r', encoding='utf-8') as f:
                pass
        
        if system2_path.exists():
            with open(system2_path, 'r', encoding='utf-8') as f:
                pass
    
    async def execute(self, input_data: Any, context: AgentContext) -> TaskResult:
        """鎵ц杩愮淮浠诲姟"""
        self.status = AgentStatus.RUNNING
        start_time = time.time()
        
        try:
            if not isinstance(input_data, dict):
                raise ValueError("input_data must be a dictionary")
            
            task_type = input_data.get("task_type", "cicd_setup")
            
            if task_type == "cicd_setup":
                result = await self._setup_cicd(input_data)
            elif task_type == "containerization":
                result = await self._setup_containerization(input_data)
            elif task_type == "monitoring":
                result = await self._setup_monitoring(input_data)
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
                error_code="DEVOPS_EXECUTION_ERROR"
            )
    
    async def _setup_cicd(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """璁剧疆CI/CD娴佹按绾?""
        return {
            "ci_tool": "GitHub Actions",
            "stages": ["build", "test", "security_scan", "deploy"],
            "artifacts_management": True,
            "parallel_execution": True
        }
    
    async def _setup_containerization(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """璁剧疆瀹瑰櫒鍖?""
        return {
            "container_runtime": "Docker",
            "orchestration": "Kubernetes",
            "image_optimization": "Multi-stage builds",
            "resource_limits": True
        }
    
    async def _setup_monitoring(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """璁剧疆鐩戞帶绯荤粺"""
        return {
            "metrics": "Prometheus",
            "visualization": "Grafana",
            "logging": "ELK Stack",
            "alerting": "AlertManager"
        }
    
    async def shutdown(self) -> None:
        """鍏抽棴鏅鸿兘浣?""
        self.status = AgentStatus.SHUTTING_DOWN
        self.status = AgentStatus.SHUTDOWN


def create_devops_agent(
    agent_id: str = "devops-001",
    manager: Optional[Any] = None,
    workbench_id: Optional[str] = None
) -> DevOpsAgent:
    """鍒涘缓杩愮淮閮ㄧ讲鏅鸿兘浣撳疄渚?""
    return DevOpsAgent(
        agent_id=agent_id,
        manager=manager,
        workbench_id=workbench_id
    )


if __name__ == "__main__":
    async def test_devops_agent():
        agent = create_devops_agent()
        await agent.initialize()
        context = AgentContext(agent_id=agent.agent_id)
        result = await agent.execute({"task_type": "cicd_setup"}, context)
        print(json.dumps(result.output, indent=2))
        await agent.shutdown()
    
    asyncio.run(test_devops_agent())
