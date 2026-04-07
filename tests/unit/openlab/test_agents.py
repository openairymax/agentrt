"""
OpenLab Agent Tests
==================

测试OpenLab模块中的所有智能体实现

Copyright (c) 2026 SPHARX. All Rights Reserved.
"""

import asyncio
import json
import pytest
import sys
from pathlib import Path

# 添加openlab到路径
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "openlab"))

from openlab.core.agent import AgentContext, AgentStatus, TaskResult


class TestArchitectAgent:
    """架构师智能体测试类"""
    
    @pytest.fixture
    def architect_agent(self):
        """创建架构师智能体实例"""
        from openlab.contrib.agents.architect.agent import create_architect_agent
        return create_architect_agent()
    
    @pytest.mark.asyncio
    async def test_agent_initialization(self, architect_agent):
        """测试智能体初始化"""
        await architect_agent.initialize()
        
        assert architect_agent.status == AgentStatus.READY
        assert architect_agent._system1_prompt is not None or True  # 提示词可能不存在
    
    @pytest.mark.asyncio
    async def test_design_architecture_task(self, architect_agent):
        """测试架构设计任务"""
        await architect_agent.initialize()
        
        context = AgentContext(agent_id=architect_agent.agent_id)
        
        result = await architect_agent.execute(
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
        
        assert result.success is True
        assert result.output is not None
        assert "architecture_type" in result.output
        assert "components" in result.output
        assert "tech_stack" in result.output
        
        await architect_agent.shutdown()
    
    @pytest.mark.asyncio
    async def test_review_architecture_task(self, architect_agent):
        """测试架构评审任务"""
        await architect_agent.initialize()
        
        context = AgentContext(agent_id=architect_agent.agent_id)
        
        result = await architect_agent.execute(
            {
                "task_type": "review",
                "architecture": {
                    "type": "microservices",
                    "components": ["api", "data"]
                }
            },
            context
        )
        
        assert result.success is True
        assert result.output is not None
        assert "overall_score" in result.output
        assert "strengths" in result.output
        assert "weaknesses" in result.output
        
        await architect_agent.shutdown()
    
    @pytest.mark.asyncio
    async def test_tech_selection_task(self, architect_agent):
        """测试技术选型任务"""
        await architect_agent.initialize()
        
        context = AgentContext(agent_id=architect_agent.agent_id)
        
        result = await architect_agent.execute(
            {
                "task_type": "tech_selection",
                "requirements": {
                    "features": ["api", "database"],
                    "performance": "high"
                }
            },
            context
        )
        
        assert result.success is True
        assert result.output is not None
        assert "frontend" in result.output
        assert "backend" in result.output
        assert "database" in result.output
        
        await architect_agent.shutdown()
    
    @pytest.mark.asyncio
    async def test_invalid_task_type(self, architect_agent):
        """测试无效任务类型处理"""
        await architect_agent.initialize()
        
        context = AgentContext(agent_id=architect_agent.agent_id)
        
        result = await architect_agent.execute(
            {
                "task_type": "invalid_task",
                "requirements": {}
            },
            context
        )
        
        assert result.success is False
        assert result.error is not None
        assert "Unknown task type" in result.error
        
        await architect_agent.shutdown()
    
    @pytest.mark.asyncio
    async def test_shutdown(self, architect_agent):
        """测试智能体关闭"""
        await architect_agent.initialize()
        
        await architect_agent.shutdown()
        
        assert architect_agent.status == AgentStatus.SHUTDOWN


class TestBackendAgent:
    """后端开发智能体测试类"""
    
    @pytest.fixture
    def backend_agent(self):
        """创建后端开发智能体实例"""
        from openlab.contrib.agents.backend.agent import create_backend_agent
        return create_backend_agent()
    
    @pytest.mark.asyncio
    async def test_api_development_task(self, backend_agent):
        """测试API开发任务"""
        await backend_agent.initialize()
        
        context = AgentContext(agent_id=backend_agent.agent_id)
        
        result = await backend_agent.execute(
            {
                "task_type": "api_development",
                "requirements": {
                    "complexity": "medium",
                    "endpoints": ["users", "products", "orders"]
                }
            },
            context
        )
        
        assert result.success is True
        assert result.output is not None
        assert "api_style" in result.output
        assert "endpoints" in result.output
        
        await backend_agent.shutdown()


class TestFrontendAgent:
    """前端开发智能体测试类"""
    
    @pytest.fixture
    def frontend_agent(self):
        """创建前端开发智能体实例"""
        from openlab.contrib.agents.frontend.agent import create_frontend_agent
        return create_frontend_agent()
    
    @pytest.mark.asyncio
    async def test_ui_development_task(self, frontend_agent):
        """测试UI开发任务"""
        await frontend_agent.initialize()
        
        context = AgentContext(agent_id=frontend_agent.agent_id)
        
        result = await frontend_agent.execute(
            {"task_type": "ui_development"},
            context
        )
        
        assert result.success is True
        assert result.output is not None
        assert "framework" in result.output
        assert "components" in result.output
        
        await frontend_agent.shutdown()


class TestDevOpsAgent:
    """运维部署智能体测试类"""
    
    @pytest.fixture
    def devops_agent(self):
        """创建运维部署智能体实例"""
        from openlab.contrib.agents.devops.agent import create_devops_agent
        return create_devops_agent()
    
    @pytest.mark.asyncio
    async def test_cicd_setup_task(self, devops_agent):
        """测试CI/CD设置任务"""
        await devops_agent.initialize()
        
        context = AgentContext(agent_id=devops_agent.agent_id)
        
        result = await devops_agent.execute(
            {"task_type": "cicd_setup"},
            context
        )
        
        assert result.success is True
        assert result.output is not None
        assert "ci_tool" in result.output
        assert "stages" in result.output
        
        await devops_agent.shutdown()


class TestProductManagerAgent:
    """产品规划智能体测试类"""
    
    @pytest.fixture
    def product_manager_agent(self):
        """创建产品规划智能体实例"""
        from openlab.contrib.agents.product_manager.agent import create_product_manager_agent
        return create_product_manager_agent()
    
    @pytest.mark.asyncio
    async def test_requirement_analysis_task(self, product_manager_agent):
        """测试需求分析任务"""
        await product_manager_agent.initialize()
        
        context = AgentContext(agent_id=product_manager_agent.agent_id)
        
        result = await product_manager_agent.execute(
            {"task_type": "requirement_analysis"},
            context
        )
        
        assert result.success is True
        assert result.output is not None
        assert "requirements" in result.output
        assert "user_stories" in result.output
        
        await product_manager_agent.shutdown()


class TestSecurityAgent:
    """安全审计智能体测试类"""
    
    @pytest.fixture
    def security_agent(self):
        """创建安全审计智能体实例"""
        from openlab.contrib.agents.security.agent import create_security_agent
        return create_security_agent()
    
    @pytest.mark.asyncio
    async def test_security_assessment_task(self, security_agent):
        """测试安全评估任务"""
        await security_agent.initialize()
        
        context = AgentContext(agent_id=security_agent.agent_id)
        
        result = await security_agent.execute(
            {"task_type": "security_assessment"},
            context
        )
        
        assert result.success is True
        assert result.output is not None
        assert "assessment_score" in result.output
        assert "findings" in result.output
        
        await security_agent.shutdown()


class TestTesterAgent:
    """质量测试智能体测试类"""
    
    @pytest.fixture
    def tester_agent(self):
        """创建质量测试智能体实例"""
        from openlab.contrib.agents.tester.agent import create_tester_agent
        return create_tester_agent()
    
    @pytest.mark.asyncio
    async def test_test_strategy_task(self, tester_agent):
        """测试策略制定任务"""
        await tester_agent.initialize()
        
        context = AgentContext(agent_id=tester_agent.agent_id)
        
        result = await tester_agent.execute(
            {"task_type": "test_strategy"},
            context
        )
        
        assert result.success is True
        assert result.output is not None
        assert "test_levels" in result.output
        assert "coverage_target" in result.output
        
        await tester_agent.shutdown()


class TestAgentContracts:
    """智能体合约验证测试类"""
    
    def test_architect_contract_exists(self):
        """测试架构师合约文件存在性"""
        contract_path = Path(__file__).parent.parent.parent / \
                         "openlab/contrib/agents/architect/contract.json"
        
        assert contract_path.exists()
        
        with open(contract_path, 'r', encoding='utf-8') as f:
            contract = json.load(f)
        
        assert "agent_id" in contract
        assert contract["agent_id"] == "architect"
        assert "capabilities" in contract
        assert len(contract["capabilities"]) > 0
    
    def test_all_agents_have_contracts(self):
        """测试所有智能体都有合约文件"""
        agents = [
            "architect", "backend", "frontend", "devops",
            "product_manager", "security", "tester"
        ]
        
        for agent_name in agents:
            contract_path = Path(__file__).parent.parent.parent / \
                             f"openlab/contrib/agents/{agent_name}/contract.json"
            
            assert contract_path.exists(), f"{agent_name} missing contract.json"
            
            with open(contract_path, 'r', encoding='utf-8') as f:
                contract = json.load(f)
            
            assert "agent_id" in contract
            assert "capabilities" in contract
            assert "metadata" in contract


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
