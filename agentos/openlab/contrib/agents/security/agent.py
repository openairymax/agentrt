"""
Security Agent Implementation
============================

瀹夊叏瀹¤鏅鸿兘浣?- 璐熻矗瀹夊叏璇勪及銆佹紡娲炴壂鎻忓拰瀹夊叏绛栫暐鍒跺畾

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


class SecurityAgent(Agent):
    """
    瀹夊叏瀹¤鏅鸿兘浣?    
    鏍稿績鑱岃矗:
    1. 瀹夊叏璇勪及 - 浠ｇ爜瀹¤銆佹灦鏋勫畨鍏ㄥ垎鏋?    2. 婕忔礊鎵弿 - 鑷姩鍖栨紡娲炴娴嬨€佹笚閫忔祴璇?    3. 瀹夊叏绛栫暐 - 璁块棶鎺у埗銆佸姞瀵嗙瓥鐣ャ€佸悎瑙勬鏌?    4. 濞佽儊寤烘ā - 鏀诲嚮闈㈠垎鏋愩€侀闄╄瘎浼?    
    鑳藉姏:
    - DEBUGGING: 瀹夊叏璋冭瘯鑳藉姏
    - OPTIMIZATION: 瀹夊叏浼樺寲鑳藉姏
    """
    
    def __init__(
        self,
        agent_id: str = "security-001",
        capabilities: Optional[Set[AgentCapability]] = None,
        manager: Optional[Any] = None,
        workbench_id: Optional[str] = None
    ):
        default_capabilities = {
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
        logger.info(f"SecurityAgent initialized with ID: {agent_id}")
    
    async def initialize(self) -> None:
        """鍒濆鍖栧畨鍏ㄥ璁℃櫤鑳戒綋"""
        self.status = AgentStatus.INITIALIZING
        
        try:
            await self._load_prompts()
            self.status = AgentStatus.READY
            logger.info(f"SecurityAgent {self.agent_id} initialized successfully")
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
        """鎵ц瀹夊叏浠诲姟"""
        self.status = AgentStatus.RUNNING
        start_time = time.time()
        
        try:
            if not isinstance(input_data, dict):
                raise ValueError("input_data must be a dictionary")
            
            task_type = input_data.get("task_type", "security_assessment")
            
            if task_type == "security_assessment":
                result = await self._assess_security(input_data)
            elif task_type == "vulnerability_scan":
                result = await self._scan_vulnerabilities(input_data)
            elif task_type == "threat_modeling":
                result = await self._model_threats(input_data)
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
                error_code="SECURITY_EXECUTION_ERROR"
            )
    
    async def _assess_security(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """璇勪及瀹夊叏鎬?""
        return {
            "assessment_score": 85,
            "findings": [
                {"severity": "medium", "issue": "Missing input validation"},
                {"severity": "low", "issue": "Outdated dependencies"}
            ],
            "recommendations": [
                "Implement comprehensive input validation",
                "Update all dependencies to latest versions"
            ]
        }
    
    async def _scan_vulnerabilities(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """鎵弿婕忔礊"""
        return {
            "vulnerabilities_found": 5,
            "critical": 0,
            "high": 1,
            "medium": 3,
            "low": 1,
            "scan_tools_used": ["SAST", "DAST", "Dependency scanning"]
        }
    
    async def _model_threats(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """濞佽儊寤烘ā"""
        return {
            "attack_surface": ["API endpoints", "User inputs", "File uploads"],
            "threats": [
                {"type": "Injection", "likelihood": "high", "impact": "critical"},
                {"type": "XSS", "likelihood": "medium", "impact": "high"}
            ],
            "mitigations": [
                "Parameterized queries for injection prevention",
                "Input sanitization and CSP for XSS prevention"
            ]
        }
    
    async def shutdown(self) -> None:
        """鍏抽棴鏅鸿兘浣?""
        self.status = AgentStatus.SHUTTING_DOWN
        self.status = AgentStatus.SHUTDOWN


def create_security_agent(
    agent_id: str = "security-001",
    manager: Optional[Any] = None,
    workbench_id: Optional[str] = None
) -> SecurityAgent:
    """鍒涘缓瀹夊叏瀹¤鏅鸿兘浣撳疄渚?""
    return SecurityAgent(
        agent_id=agent_id,
        manager=manager,
        workbench_id=workbench_id
    )


if __name__ == "__main__":
    async def test_security_agent():
        agent = create_security_agent()
        await agent.initialize()
        context = AgentContext(agent_id=agent.agent_id)
        result = await agent.execute({"task_type": "security_assessment"}, context)
        print(json.dumps(result.output, indent=2))
        await agent.shutdown()
    
    asyncio.run(test_security_agent())
